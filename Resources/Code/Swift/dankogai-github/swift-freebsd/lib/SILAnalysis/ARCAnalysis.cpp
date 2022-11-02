//===-------------- ARCAnalysis.cpp - SIL ARC Analysis --------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "sil-arc-analysis"
#include "swift/SILAnalysis/ARCAnalysis.h"
#include "swift/Basic/Fallthrough.h"
#include "swift/SIL/SILFunction.h"
#include "swift/SIL/SILInstruction.h"
#include "swift/SILAnalysis/AliasAnalysis.h"
#include "swift/SILAnalysis/RCIdentityAnalysis.h"
#include "swift/SILAnalysis/ValueTracking.h"
#include "swift/SILPasses/Utils/Local.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/Debug.h"

using namespace swift;

//===----------------------------------------------------------------------===//
//                             Decrement Analysis
//===----------------------------------------------------------------------===//

static bool isKnownToNotDecrementRefCount(FunctionRefInst *FRI) {
   return llvm::StringSwitch<bool>(FRI->getReferencedFunction()->getName())
     .Case("swift_keepAlive", true)
     .Default(false);
}

static bool canApplyDecrementRefCount(OperandValueArrayRef Ops, SILValue Ptr,
                                      AliasAnalysis *AA) {
  // Ok, this apply *MAY* decrement ref counts. Now our strategy is to attempt
  // to use properties of the pointer, the function's arguments, and the
  // function itself to prove that the pointer cannot have its ref count
  // affected by the applied function.

  // TODO: Put in function property check section here when we get access to
  // such information.

  // First make sure that the underlying object of ptr is a local object which
  // does not escape. This prevents the apply from indirectly via the global
  // affecting the reference count of the pointer.
  if (!isNonEscapingLocalObject(getUnderlyingObject(Ptr)))
    return true;

  // Now that we know that the function can not affect the pointer indirectly,
  // make sure that the apply can not affect the pointer directly via the
  // applies arguments by proving that the pointer can not alias any of the
  // functions arguments.
  for (auto Op : Ops) {
    for (int i = 0, e = Ptr->getNumTypes(); i < e; i++) {
      if (!AA->isNoAlias(Op, SILValue(Ptr.getDef(), i)))
        return true;
    }
  }

  // Success! The apply inst can not affect the reference count of ptr!
  return false;
}

static bool canApplyDecrementRefCount(ApplyInst *AI, SILValue Ptr,
                                      AliasAnalysis *AA) {
  // Ignore any thick functions for now due to us not handling the ref-counted
  // nature of its context.
  if (auto FTy = AI->getCallee().getType().getAs<SILFunctionType>())
    if (FTy->getExtInfo().hasContext())
      return true;

  // Treat applications of @noreturn functions as decrementing ref counts. This
  // causes the apply to become a sink barrier for ref count increments.
  if (AI->getCallee().getType().getAs<SILFunctionType>()->isNoReturn())
    return true;

  // swift_keepAlive can not retain values. Remove this when we get rid of that.
  if (auto *FRI = dyn_cast<FunctionRefInst>(AI->getCallee()))
    if (isKnownToNotDecrementRefCount(FRI))
      return false;

  return canApplyDecrementRefCount(AI->getArgumentsWithoutIndirectResult(),
                                   Ptr, AA);
}

static bool canBuiltinNeverDecrementRefCounts(BuiltinInst *BI) {
  // If we have a builtin that is side effect free, we can commute the
  // builtin and the retain.
  if (!BI->mayHaveSideEffects())
    return true;

  // If this is an instruction which might have side effect, but its side
  // effects do not cause reference counts to be decremented, return false.
  //
  // If this is expanded, refactor it into a method with a string switch.
  if (auto Kind = BI->getBuiltinKind()) {
    switch (Kind.getValue()) {
    case BuiltinValueKind::CopyArray:
      return true;
    default:
      break;
    }
  }

  if (auto ID = BI->getIntrinsicID()) {
    switch (ID.getValue()) {
    case llvm::Intrinsic::memcpy:
    case llvm::Intrinsic::memmove:
    case llvm::Intrinsic::memset:
      return true;
    default:
      break;
    }
  }

  return false;
}

static bool canApplyDecrementRefCount(BuiltinInst *BI, SILValue Ptr,
                                      AliasAnalysis *AA) {
  if (canBuiltinNeverDecrementRefCounts(BI))
    return false;

  return canApplyDecrementRefCount(BI->getArguments(), Ptr, AA);
}

/// Is the may have side effects user by the definition of its value kind unable
/// to decrement ref counts.
///
/// Although is_unique will never decrement a refcount, it must appear to do so
/// from the perspective of the optimizer. This forces a separate retain to be
/// preserved for any original source level copy.
static bool canDecrementRefCountsByValueKind(SILInstruction *User) {
  assert(User->getMemoryBehavior()
           ==  SILInstruction::MemoryBehavior::MayHaveSideEffects &&
         "Invalid argument. Function is only applicable to isntructions with "
         "side effects.");
  switch (User->getKind()) {
  case ValueKind::DeallocStackInst:
  case ValueKind::StrongRetainInst:
  case ValueKind::StrongRetainAutoreleasedInst:
  case ValueKind::StrongRetainUnownedInst:
  case ValueKind::UnownedRetainInst:
  case ValueKind::RetainValueInst:
  case ValueKind::PartialApplyInst:
  case ValueKind::FixLifetimeInst:
  case ValueKind::CopyBlockInst:
  case ValueKind::CondFailInst:
  case ValueKind::StrongPinInst:
    return false;

  case ValueKind::CopyAddrInst:
    // We only decrement ref counts if we are not initializing dest.
    return !cast<CopyAddrInst>(User)->isInitializationOfDest();

  case ValueKind::CheckedCastAddrBranchInst: {
    // If we do not take on success, we do not touch ref counts.
    auto *CCABI = cast<CheckedCastAddrBranchInst>(User);
    return shouldTakeOnSuccess(CCABI->getConsumptionKind());
  }
  default:
    return true;
  }
}

bool swift::mayDecrementRefCount(SILInstruction *User,
                                 SILValue Ptr, AliasAnalysis *AA) {
  // If we have an instruction that does not have *pure* side effects, it can
  // not affect ref counts.
  //
  // This distinguishes in between a "write" side effect and ref count side
  // effects.
  if (User->getMemoryBehavior() !=
      SILInstruction::MemoryBehavior::MayHaveSideEffects)
    return false;

  // Ok, we know that this instruction's generic behavior is
  // "MayHaveSideEffects". That is a criterion (it has effects not represented
  // by use-def chains) that is broader than ours (does it effect a particular
  // pointers ref counts). Thus begin by attempting to prove that the type of
  // instruction that the user is by definition can not decrement ref counts.
  if (!canDecrementRefCountsByValueKind(User))
    return false;

  // Ok, this instruction may have ref counts. If it is an apply, attempt to
  // prove that the callee is unable to affect Ptr.
  if (auto *AI = dyn_cast<ApplyInst>(User))
    return canApplyDecrementRefCount(AI, Ptr, AA);
  if (auto *BI = dyn_cast<BuiltinInst>(User))
    return canApplyDecrementRefCount(BI, Ptr, AA);

  // We can not conservatively prove that this instruction can not decrement the
  // ref count of Ptr. So assume that it does.
  return true;
}

bool swift::mayCheckRefCount(SILInstruction *User) {
  return isa<IsUniqueInst>(User) || isa<IsUniqueOrPinnedInst>(User);
}

// Attempt to prove conservatively that Inst can never decrement reference
// counts
bool swift::canNeverDecrementRefCounts(SILInstruction *Inst) {
  if (Inst->getMemoryBehavior() !=
      SILInstruction::MemoryBehavior::MayHaveSideEffects)
    return true;

  if (!canDecrementRefCountsByValueKind(Inst))
    return true;

  if (auto *BI = dyn_cast<BuiltinInst>(Inst))
    return canBuiltinNeverDecrementRefCounts(BI);

  // We can not prove that Inst can never decrement or use ref counts. Be
  // conservative.
  return false;
}

//===----------------------------------------------------------------------===//
//                                Use Analysis
//===----------------------------------------------------------------------===//

/// Returns true if a builtin apply can not use reference counted values.
///
/// The main case that this handles here are builtins that via read none imply
/// that they can not read globals and at the same time do not take any
/// non-trivial types via the arguments. The reason why we care about taking
/// non-trivial types as arguments is that we want to be careful in the face of
/// intrinsics that may be equivalent to bitcast and inttoptr operations.
static bool canApplyOfBuiltinUseNonTrivialValues(BuiltinInst *BInst) {
  SILModule &Mod = BInst->getModule();

  auto &II = BInst->getIntrinsicInfo();
  if (II.ID != llvm::Intrinsic::not_intrinsic) {
    if (II.hasAttribute(llvm::Attribute::ReadNone)) {
      for (auto &Op : BInst->getAllOperands()) {
        if (!Op.get().getType().isTrivial(Mod)) {
          return false;
        }
      }
    }

    return true;
  }

  auto &BI = BInst->getBuiltinInfo();
  if (BI.isReadNone()) {
    for (auto &Op : BInst->getAllOperands()) {
      if (!Op.get().getType().isTrivial(Mod)) {
        return false;
      }
    }
  }

  return true;
}

/// Returns true if Inst is a function that we know never uses ref count values.
bool swift::canNeverUseValues(SILInstruction *Inst) {
  switch (Inst->getKind()) {
  // These instructions do not use other values.
  case ValueKind::FunctionRefInst:
  case ValueKind::IntegerLiteralInst:
  case ValueKind::FloatLiteralInst:
  case ValueKind::StringLiteralInst:
  case ValueKind::AllocStackInst:
  case ValueKind::AllocRefInst:
  case ValueKind::AllocRefDynamicInst:
  case ValueKind::AllocBoxInst:
  case ValueKind::MetatypeInst:
  case ValueKind::WitnessMethodInst:
    return true;

  // DeallocStackInst do not use reference counted values, only local storage
  // handles.
  case ValueKind::DeallocStackInst:
    return true;

  // Debug values do not use referenced counted values in a manner we care
  // about.
  case ValueKind::DebugValueInst:
  case ValueKind::DebugValueAddrInst:
    return true;

  // Casts do not use pointers in a manner that we care about since we strip
  // them during our analysis. The reason for this is if the cast is not dead
  // then there must be some other use after the cast that we will protect if a
  // release is not in between the cast and the use.
  case ValueKind::UpcastInst:
  case ValueKind::AddressToPointerInst:
  case ValueKind::PointerToAddressInst:
  case ValueKind::UncheckedRefCastInst:
  case ValueKind::UncheckedRefCastAddrInst:
  case ValueKind::UncheckedAddrCastInst:
  case ValueKind::RefToRawPointerInst:
  case ValueKind::RawPointerToRefInst:
  case ValueKind::UnconditionalCheckedCastInst:
  case ValueKind::UncheckedBitwiseCastInst:
    return true;

  // If we have a trivial bit cast between trivial types, it is not something
  // that can use ref count ops in a way we care about. We do need to be careful
  // with uses with ref count inputs. In such a case, we assume conservatively
  // that the bit cast could use it.
  //
  // The reason why this is different from the ref bitcast is b/c the use of a
  // ref bit cast is still a ref typed value implying that our ARC dataflow will
  // properly handle its users. A conversion of a reference count value to a
  // trivial value though could be used as a trivial value in ways that ARC
  // dataflow will not understand implying we need to treat it as a use to be
  // safe.
  case ValueKind::UncheckedTrivialBitCastInst: {
    SILValue Op = cast<UncheckedTrivialBitCastInst>(Inst)->getOperand();
    return Op.getType().isTrivial(Inst->getModule());
  }

  // Typed GEPs do not use pointers. The user of the typed GEP may but we will
  // catch that via the dataflow.
  case ValueKind::StructExtractInst:
  case ValueKind::TupleExtractInst:
  case ValueKind::StructElementAddrInst:
  case ValueKind::TupleElementAddrInst:
  case ValueKind::UncheckedTakeEnumDataAddrInst:
  case ValueKind::RefElementAddrInst:
  case ValueKind::UncheckedEnumDataInst:
  case ValueKind::IndexAddrInst:
  case ValueKind::IndexRawPointerInst:
      return true;

  // Aggregate formation by themselves do not create new uses since it is their
  // users that would create the appropriate uses.
  case ValueKind::EnumInst:
  case ValueKind::StructInst:
  case ValueKind::TupleInst:
    return true;

  // Only uses non reference counted values.
  case ValueKind::CondFailInst:
    return true;

  case ValueKind::BuiltinInst: {
    auto *BI = cast<BuiltinInst>(Inst);

    // Certain builtin function refs we know can never use non-trivial values.
    return canApplyOfBuiltinUseNonTrivialValues(BI);
  }

  default:
    return false;
  }
}

static bool doOperandsAlias(ArrayRef<Operand> Ops, SILValue Ptr,
                            AliasAnalysis *AA) {
  // If any are not no alias, we have a use.
  return std::any_of(Ops.begin(), Ops.end(),
                     [&AA, &Ptr](const Operand &Op) -> bool {
                       return !AA->isNoAlias(Ptr, Op.get());
                     });
}

static bool canTerminatorUseValue(TermInst *TI, SILValue Ptr,
                                  AliasAnalysis *AA) {
  if (auto *BI = dyn_cast<BranchInst>(TI)) {
    return doOperandsAlias(BI->getAllOperands(), Ptr, AA);
  }

  if (auto *CBI = dyn_cast<CondBranchInst>(TI)) {
    bool First = doOperandsAlias(CBI->getTrueOperands(), Ptr, AA);
    bool Second = doOperandsAlias(CBI->getFalseOperands(), Ptr, AA);
    return First || Second;
  }

  if (auto *SWEI = dyn_cast<SwitchEnumInst>(TI)) {
    return doOperandsAlias(SWEI->getAllOperands(), Ptr, AA);
  }

  if (auto *SWVI = dyn_cast<SwitchValueInst>(TI)) {
    return doOperandsAlias(SWVI->getAllOperands(), Ptr, AA);
  }

  auto *CCBI = dyn_cast<CheckedCastBranchInst>(TI);
  // If we don't have this last case, be conservative and assume that we can use
  // the value.
  if (!CCBI)
    return true;

  // Otherwise, look at the operands.
  return doOperandsAlias(CCBI->getAllOperands(), Ptr, AA);
}

bool swift::mayUseValue(SILInstruction *User, SILValue Ptr,
                        AliasAnalysis *AA) {
  // If Inst is an instruction that we know can never use values with reference
  // semantics, return true.
  if (canNeverUseValues(User))
    return false;

  // If the user is a load or a store and we can prove that it does not access
  // the object then return true.
  // Notice that we need to check all of the values of the object.
  if (isa<StoreInst>(User)) {
    for (int i = 0, e = Ptr->getNumTypes(); i < e; i++) {
      if (AA->mayWriteToMemory(User, SILValue(Ptr.getDef(), i)))
        return true;
    }
    return false;
  }

  if (isa<LoadInst>(User) ) {
    for (int i = 0, e = Ptr->getNumTypes(); i < e; i++) {
      if (AA->mayReadFromMemory(User, SILValue(Ptr.getDef(), i)))
        return true;
    }
    return false;
  }

  // If we have a terminator instruction, see if it can use ptr. This currently
  // means that we first show that TI can not indirectly use Ptr and then use
  // alias analysis on the arguments.
  if (auto *TI = dyn_cast<TermInst>(User))
    return canTerminatorUseValue(TI, Ptr, AA);

  // TODO: If we add in alias analysis support here for apply inst, we will need
  // to check that the pointer does not escape.

  // Otherwise, assume that Inst can use Target.
  return true;
}

//===----------------------------------------------------------------------===//
//                             Must Use Analysis
//===----------------------------------------------------------------------===//

/// Returns true if User must use Ptr.
///
/// In terms of ARC this means that if we do not remove User, all releases post
/// dominated by User are known safe.
bool swift::mustUseValue(SILInstruction *User, SILValue Ptr,
                         AliasAnalysis *AA) {
  // Right now just pattern match applies.
  auto *AI = dyn_cast<ApplyInst>(User);
  if (!AI)
    return false;

  // If any of AI's arguments must alias Ptr, return true.
  for (SILValue Arg : AI->getArguments())
    if (AA->isMustAlias(Arg, Ptr))
      return true;
  return false;
}

/// Returns true if User must use Ptr in a guaranteed way.
///
/// This means that assuming that everything is conservative, we can ignore the
/// ref count effects of User on Ptr since we will only remove things over
/// guaranteed parameters if we are known safe in both directions.
bool swift::mustGuaranteedUseValue(SILInstruction *User, SILValue Ptr,
                                   AliasAnalysis *AA) {
  // Right now just pattern match applies.
  auto *AI = dyn_cast<ApplyInst>(User);
  if (!AI)
    return false;

  // For now just look for guaranteed self.
  //
  // TODO: Expand this to handle *any* guaranteed parameter.
  if (!AI->hasGuaranteedSelfArgument())
    return false;

  // Return true if Ptr alias's self.
  return AA->isMustAlias(AI->getSelfArgument(), Ptr);
}

//===----------------------------------------------------------------------===//
// Utility Methods for determining use, decrement of values in a contiguous
// instruction range in one BB.
//===----------------------------------------------------------------------===//

/// If \p Op has arc uses in the instruction range [Start, End), return the
/// first such instruction. Otherwise return None. We assume that
/// Start and End are both in the same basic block.
Optional<SILBasicBlock::iterator>
swift::
valueHasARCUsesInInstructionRange(SILValue Op,
                                  SILBasicBlock::iterator Start,
                                  SILBasicBlock::iterator End,
                                  AliasAnalysis *AA) {
  assert(Start->getParent() == End->getParent() &&
         "Start and End should be in the same basic block");

  // If Start == End, then we have an empty range, return false.
  if (Start == End)
    return None;

  // Otherwise, until Start != End.
  while (Start != End) {
    // Check if Start can use Op in an ARC relevant way. If so, return true.
    if (mayUseValue(&*Start, Op, AA))
      return Start;

    // Otherwise, increment our iterator.
    ++Start;
  }

  // If all such instructions can not use Op, return false.
  return None;
}

/// If \p Op has arc uses in the instruction range (Start, End], return the
/// first such instruction. Otherwise return None. We assume that Start and End
/// are both in the same basic block.
Optional<SILBasicBlock::iterator>
swift::valueHasARCUsesInReverseInstructionRange(SILValue Op,
                                                SILBasicBlock::iterator Start,
                                                SILBasicBlock::iterator End,
                                                AliasAnalysis *AA) {
  assert(Start->getParent() == End->getParent() &&
         "Start and End should be in the same basic block");
  assert(End != End->getParent()->end() &&
         "End should be mapped to an actual instruction");

  // If Start == End, then we have an empty range, return false.
  if (Start == End)
    return None;

  // Otherwise, until End == Start.
  while (Start != End) {
    // Check if Start can use Op in an ARC relevant way. If so, return true.
    if (mayUseValue(&*End, Op, AA))
      return End;

    // Otherwise, decrement our iterator.
    --End;
  }

  // If all such instructions can not use Op, return false.
  return None;
}

/// If \p Op has instructions in the instruction range (Start, End] which may
/// decrement it, return the first such instruction. Returns None
/// if no such instruction exists. We assume that Start and End are both in the
/// same basic block.
Optional<SILBasicBlock::iterator>
swift::
valueHasARCDecrementOrCheckInInstructionRange(SILValue Op,
                                              SILBasicBlock::iterator Start,
                                              SILBasicBlock::iterator End,
                                              AliasAnalysis *AA) {
  assert(Start->getParent() == End->getParent() &&
         "Start and End should be in the same basic block");

  // If Start == End, then we have an empty range, return nothing.
  if (Start == End)
    return None;

  // Otherwise, until Start != End.
  while (Start != End) {
    // Check if Start can decrement or check Op's ref count. If so, return
    // Start. Ref count checks do not have side effects, but are barriers for
    // retains.
    if (mayDecrementRefCount(&*Start, Op, AA) || mayCheckRefCount(&*Start))
      return Start;
    // Otherwise, increment our iterator.
    ++Start;
  }

  // If all such instructions can not decrement Op, return nothing.
  return None;
}

bool
swift::
mayGuaranteedUseValue(SILInstruction *User, SILValue Ptr, AliasAnalysis *AA) {
  // Only full apply sites can require a guaranteed lifetime. If we don't have
  // one, bail.
  if (!isa<FullApplySite>(User))
    return false;

  FullApplySite FAS(User);

  // Ok, we have a full apply site. If the apply has no arguments, we don't need
  // to worry about any guaranteed parameters.
  if (!FAS.getNumArguments())
    return false;

  // Ok, we have an apply site with arguments. Look at the function type and
  // iterate through the function parameters. If any of the parameters are
  // guaranteed, attempt to prove that the passed in parameter can not alias
  // Ptr. If we fail, return true.
  CanSILFunctionType FType = FAS.getSubstCalleeType();
  auto Params = FType->getParameters();
  for (unsigned i : indices(Params)) {    
    if (!Params[i].isGuaranteed())
      continue;
    SILValue Op = FAS.getArgument(i);
    for (int i = 0, e = Ptr->getNumTypes(); i < e; i++)
      if (!AA->isNoAlias(Op, SILValue(Ptr.getDef(), i)))
        return true;
  }

  // Ok, we were able to prove that all arguments to the apply that were
  // guaranteed do not alias Ptr. Return false.
  return false;
}

//===----------------------------------------------------------------------===//
//           Utilities for recognizing trap BBs that are ARC inert
//===----------------------------------------------------------------------===//

static bool ignoreableApplyInstInUnreachableBlock(ApplyInst *AI) {
  const char *fatalName =
    "_TFs18_fatalErrorMessageFTVs12StaticStringS_S_Su_T_";
  auto *Fn = AI->getCalleeFunction();

  // We use endswith here since if we specialize fatal error we will always
  // prepend the specialization records to fatalName.
  if (!Fn || !Fn->getName().endswith(fatalName))
    return false;

  return true;
}

static bool ignoreableBuiltinInstInUnreachableBlock(BuiltinInst *BI) {
  const BuiltinInfo &BInfo = BI->getBuiltinInfo();
  if (BInfo.ID == BuiltinValueKind::CondUnreachable)
    return true;

  const IntrinsicInfo &IInfo = BI->getIntrinsicInfo();
  if (IInfo.ID == llvm::Intrinsic::trap)
    return true;
  
  return false;
}

/// Match a call to a trap BB with no ARC relevant side effects.
bool swift::isARCInertTrapBB(SILBasicBlock *BB) {
  // Do a quick check at the beginning to make sure that our terminator is
  // actually an unreachable. This ensures that in many cases this function will
  // exit early and quickly.
  auto II = BB->rbegin();
  if (!isa<UnreachableInst>(*II))
    return false;

  auto IE = BB->rend();
  while (II != IE) {
    // Ignore any instructions without side effects.
    if (!II->mayHaveSideEffects()) {
      ++II;
      continue;
    }

    // Ignore cond fail.
    if (isa<CondFailInst>(*II)) {
      ++II;
      continue;
    }

    // Check for apply insts that we can ignore.
    if (auto *AI = dyn_cast<ApplyInst>(&*II)) {
      if (ignoreableApplyInstInUnreachableBlock(AI)) {
        ++II;
        continue;
      }
    }

    // Check for builtins that we can ignore.
    if (auto *BI = dyn_cast<BuiltinInst>(&*II)) {
      if (ignoreableBuiltinInstInUnreachableBlock(BI)) {
        ++II;
        continue;
      }
    }

    // If we can't ignore the instruction, return false.
    return false;
  }

  // Otherwise, we have an unreachable and every instruction is inert from an
  // ARC perspective in an unreachable BB.
  return true;
}

//===----------------------------------------------------------------------===//
//                          Owned Argument Utilities
//===----------------------------------------------------------------------===//

ConsumedArgToEpilogueReleaseMatcher::
ConsumedArgToEpilogueReleaseMatcher(RCIdentityFunctionInfo *RCIA,
                                    SILFunction *F) {
  // Find the return BB of F. If we fail, then bail.
  auto ReturnBB = F->findReturnBB();
  if (ReturnBB != F->end())
    findMatchingReleases(RCIA, &*ReturnBB);
}

void ConsumedArgToEpilogueReleaseMatcher::
findMatchingReleases(RCIdentityFunctionInfo *RCIA, SILBasicBlock *BB) {
  for (auto II = std::next(BB->rbegin()), IE = BB->rend();
       II != IE; ++II) {
    // If we do not have a release_value or strong_release...
    if (!isa<ReleaseValueInst>(*II) && !isa<StrongReleaseInst>(*II)) {
      // And the object can not use values in a manner that will keep the object
      // alive, continue. We may be able to find additional releases.
      if (canNeverUseValues(&*II))
        continue;

      // Otherwise, we need to stop computing since we do not want to reduce the
      // lifetime of objects.
      return;
    }

    // Ok, we have a release_value or strong_release. Grab Target and find the
    // RC identity root of its operand.
    SILInstruction *Target = &*II;
    SILValue Op = RCIA->getRCIdentityRoot(Target->getOperand(0));

    // If Op is not a consumed argument, we must break since this is not an Op
    // that is a part of a return sequence. We are being conservative here since
    // we could make this more general by allowing for intervening non-arg
    // releases in the sense that we do not allow for race conditions in between
    // destructors.
    auto *Arg = dyn_cast<SILArgument>(Op);
    if (!Arg || !Arg->isFunctionArg() ||
        !Arg->hasConvention(ParameterConvention::Direct_Owned))
      return;

    // Ok, we have a release on a SILArgument that is direct owned. Attempt to
    // put it into our arc opts map. If we already have it, we have exited the
    // return value sequence so break. Otherwise, continue looking for more arc
    // operations.
    if (!ArgInstMap.insert({Arg, Target}).second)
      return;
  }
}

//===----------------------------------------------------------------------===//
//                    Code for Determining Final Releases
//===----------------------------------------------------------------------===//

// Propagate liveness backwards from an initial set of blocks in our
// LiveIn set.
static void propagateLiveness(llvm::SmallPtrSetImpl<SILBasicBlock *> &LiveIn,
                              SILBasicBlock *DefBB) {
  // First populate a worklist of predecessors.
  llvm::SmallVector<SILBasicBlock *, 64> Worklist;
  for (auto *BB : LiveIn)
    for (auto Pred : BB->getPreds())
      Worklist.push_back(Pred);

  // Now propagate liveness backwards until we hit the alloc_box.
  while (!Worklist.empty()) {
    auto *BB = Worklist.pop_back_val();

    // If it's already in the set, then we've already queued and/or
    // processed the predecessors.
    if (BB == DefBB || !LiveIn.insert(BB).second)
      continue;

    for (auto Pred : BB->getPreds())
      Worklist.push_back(Pred);
  }
}

// Is any successor of BB in the LiveIn set?
static bool successorHasLiveIn(SILBasicBlock *BB,
                               llvm::SmallPtrSetImpl<SILBasicBlock *> &LiveIn) {
  for (auto &Succ : BB->getSuccessors())
    if (LiveIn.count(Succ))
      return true;

  return false;
}

// Walk backwards in BB looking for the last use of a given
// value, and add it to the set of release points.
static bool addLastUse(SILValue V, SILBasicBlock *BB,
                       ReleaseTracker &Tracker) {
  for (auto I = BB->rbegin(); I != BB->rend(); ++I) {
    for (auto &Op : I->getAllOperands())
      if (Op.get().getDef() == V.getDef()) {
        Tracker.trackLastRelease(&*I);
        return true;
      }
  }

  llvm_unreachable("BB is expected to have a use of a closure");
  return false;
}

/// TODO: Refactor this code so the decision on whether or not to accept an
/// instruction.
bool swift::getFinalReleasesForValue(SILValue V, ReleaseTracker &Tracker) {
  llvm::SmallPtrSet<SILBasicBlock *, 16> LiveIn;
  llvm::SmallPtrSet<SILBasicBlock *, 16> UseBlocks;

  // First attempt to get the BB where this value resides.
  auto *DefBB = V->getParentBB();
  if (!DefBB)
    return false;

  bool seenRelease = false;
  SILInstruction *OneRelease = nullptr;

  // We'll treat this like a liveness problem where the value is the def. Each
  // block that has a use of the value has the value live-in unless it is the
  // block with the value.
  for (auto *UI : V.getUses()) {
    auto *User = UI->getUser();
    auto *BB = User->getParent();

    if (!Tracker.isUserAcceptable(User))
      return false;
    Tracker.trackUser(User);

    if (BB != DefBB)
      LiveIn.insert(BB);

    // Also keep track of the blocks with uses.
    UseBlocks.insert(BB);

    // Try to speed up the trivial case of single release/dealloc.
    if (isa<StrongReleaseInst>(User) || isa<DeallocBoxInst>(User)) {
      if (!seenRelease)
        OneRelease = User;
      else
        OneRelease = nullptr;

      seenRelease = true;
    }
  }

  // Only a single release/dealloc? We're done!
  if (OneRelease) {
    Tracker.trackLastRelease(OneRelease);
    return true;
  }

  propagateLiveness(LiveIn, DefBB);

  // Now examine each block we saw a use in. If it has no successors
  // that are in LiveIn, then the last use in the block is the final
  // release/dealloc.
  for (auto *BB : UseBlocks)
    if (!successorHasLiveIn(BB, LiveIn))
      if (!addLastUse(V, BB, Tracker))
        return false;

  return true;
}
