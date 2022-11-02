//===--- Local.cpp - Functions that perform local SIL transformations. ---===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===---------------------------------------------------------------------===//
#include "swift/SILPasses/Utils/Local.h"
#include "swift/SILAnalysis/Analysis.h"
#include "swift/SILAnalysis/ARCAnalysis.h"
#include "swift/SILAnalysis/DominanceAnalysis.h"
#include "swift/SIL/DynamicCasts.h"
#include "swift/SIL/SILArgument.h"
#include "swift/SIL/SILBuilder.h"
#include "swift/SIL/SILModule.h"
#include "swift/SIL/SILUndef.h"
#include "swift/SIL/TypeLowering.h"
#include "swift/SIL/DebugUtils.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/CommandLine.h"
#include <deque>

using namespace swift;

/// \brief Perform a fast local check to see if the instruction is dead.
///
/// This routine only examines the state of the instruction at hand.
bool
swift::isInstructionTriviallyDead(SILInstruction *I) {
  // At Onone, consider all uses, including the debug_info.
  // This way, debug_info is preserved at Onone.
  if (!I->use_empty() &&
      I->getModule().getOptions().Optimization <= SILOptions::SILOptMode::None)
    return false;

  if (!hasNoUsesExceptDebug(I) || isa<TermInst>(I))
    return false;

  if (auto *BI = dyn_cast<BuiltinInst>(I)) {
    return !BI->mayHaveSideEffects();
  }

  // condfail instructions that obviously can't fail are dead.
  if (auto *CFI = dyn_cast<CondFailInst>(I))
    if (auto *ILI = dyn_cast<IntegerLiteralInst>(CFI->getOperand()))
      if (!ILI->getValue())
        return true;

  // mark_uninitialized is never dead.
  if (isa<MarkUninitializedInst>(I))
    return false;

  if (isa<DebugValueInst>(I) || isa<DebugValueAddrInst>(I))
    return false;

  // These invalidate enums so "write" memory, but that is not an essential
  // operation so we can remove these if they are trivially dead.
  if (isa<UncheckedTakeEnumDataAddrInst>(I))
    return true;
  
  if (!I->mayHaveSideEffects())
    return true;

  return false;
}

namespace {
  using CallbackTy = std::function<void(SILInstruction *)>;
} // end anonymous namespace

void swift::
recursivelyDeleteTriviallyDeadInstructions(ArrayRef<SILInstruction *> IA,
                                           bool Force, CallbackTy Callback) {
  // Delete these instruction and others that become dead after it's deleted.
  llvm::SmallPtrSet<SILInstruction *, 8> DeadInsts;
  for (auto I : IA) {
    // If the instruction is not dead and force is false, do nothing.
    if (Force || isInstructionTriviallyDead(I))
      DeadInsts.insert(I);
  }
  llvm::SmallPtrSet<SILInstruction *, 8> NextInsts;
  while (!DeadInsts.empty()) {
    for (auto I : DeadInsts) {
      // Call the callback before we mutate the to be deleted instruction in any
      // way.
      Callback(I);

      // Check if any of the operands will become dead as well.
      MutableArrayRef<Operand> Ops = I->getAllOperands();
      for (Operand &Op : Ops) {
        SILValue OpVal = Op.get();
        if (!OpVal)
          continue;

        // Remove the reference from the instruction being deleted to this
        // operand.
        Op.drop();

        // If the operand is an instruction that is only used by the instruction
        // being deleted, delete it.
        if (SILInstruction *OpValInst = dyn_cast<SILInstruction>(OpVal))
          if (!DeadInsts.count(OpValInst) &&
              isInstructionTriviallyDead(OpValInst))
            NextInsts.insert(OpValInst);
      }

      // If we have a function ref inst, we need to especially drop its function
      // argument so that it gets a proper ref decement.
      auto *FRI = dyn_cast<FunctionRefInst>(I);
      if (FRI && FRI->getReferencedFunction())
        FRI->dropReferencedFunction();
    }

    for (auto I : DeadInsts) {
      // This will remove this instruction and all its uses.
      
      eraseFromParentWithDebugInsts(I);
    }

    NextInsts.swap(DeadInsts);
    NextInsts.clear();
  }
}

/// \brief If the given instruction is dead, delete it along with its dead
/// operands.
///
/// \param I The instruction to be deleted.
/// \param Force If Force is set, don't check if the top level instruction is
///        considered dead - delete it regardless.
void swift::recursivelyDeleteTriviallyDeadInstructions(SILInstruction *I,
                                                       bool Force,
                                                       CallbackTy Callback) {

  ArrayRef<SILInstruction *> AI = ArrayRef<SILInstruction *>(I);
  recursivelyDeleteTriviallyDeadInstructions(AI, Force, Callback);
}

void swift::eraseUsesOfInstruction(SILInstruction *Inst,
                                   CallbackTy Callback) {
  for (auto UI = Inst->use_begin(), E = Inst->use_end(); UI != E;) {
    auto *User = UI->getUser();
    UI++;

    // If the instruction itself has any uses, recursively zap them so that
    // nothing uses this instruction.
    eraseUsesOfInstruction(User, Callback);

    // Walk through the operand list and delete any random instructions that
    // will become trivially dead when this instruction is removed.

    for (auto &Op : User->getAllOperands()) {
      if (auto *OpI = dyn_cast<SILInstruction>(Op.get())) {
        // Don't recursively delete the pointer we're getting in.
        if (OpI != Inst) {
          Op.drop();
          recursivelyDeleteTriviallyDeadInstructions(OpI, false, Callback);
        }
      }
    }
    Callback(User);
    User->eraseFromParent();
  }
}

void swift::eraseUsesOfValue(SILValue V) {
  for (auto UI = V.use_begin(), E = V.use_end(); UI != E;) {
    auto *User = UI->getUser();
    UI++;

    // If the instruction itself has any uses, recursively zap them so that
    // nothing uses this instruction.
    eraseUsesOfValue(User);

    // Walk through the operand list and delete any random instructions that
    // will become trivially dead when this instruction is removed.
    for (auto &Op : User->getAllOperands()) {
      if (auto *OpI = dyn_cast<SILInstruction>(Op.get())) {
        // Don't recursively delete the pointer we're getting in.
        if (Op.get() != V) {
          Op.drop();
          recursivelyDeleteTriviallyDeadInstructions(OpI, false,
                                                     [](SILInstruction *){});
        }
      }
    }

    User->eraseFromParent();
  }
}

// Devirtualization of functions with covariant return types produces
// a result that is not an apply, but takes an apply as an
// argument. Attempt to dig the apply out from this result.
FullApplySite swift::findApplyFromDevirtualizedResult(SILInstruction *I) {
  if (!I)
    return FullApplySite();

  if (auto Apply = FullApplySite::isa(I))
    return Apply;

  if (!I->getNumOperands())
    return FullApplySite();

  if (isa<UpcastInst>(I) || isa<EnumInst>(I) || isa<UncheckedRefCastInst>(I))
    return findApplyFromDevirtualizedResult(
        dyn_cast<SILInstruction>(I->getOperand(0).getDef()));

  return FullApplySite();
}

// Replace a dead apply with a new instruction that computes the same
// value, and delete the old apply.
void swift::replaceDeadApply(ApplySite Old, ValueBase *New) {
  auto *OldApply = Old.getInstruction();
  if (!isa<TryApplyInst>(OldApply))
    OldApply->replaceAllUsesWith(New);
  recursivelyDeleteTriviallyDeadInstructions(OldApply, true);
}

bool swift::hasUnboundGenericTypes(TypeSubstitutionMap &SubsMap) {
  // Check whether any of the substitutions are dependent.
  for (auto &entry : SubsMap)
    if (entry.second->getCanonicalType()->hasArchetype())
      return true;

  return false;
}

bool swift::hasUnboundGenericTypes(ArrayRef<Substitution> Subs) {
  // Check whether any of the substitutions are dependent.
  for (auto &sub : Subs)
    if (sub.getReplacement()->getCanonicalType()->hasArchetype())
      return true;
  return false;
}

bool swift::hasDynamicSelfTypes(TypeSubstitutionMap &SubsMap) {
  // Check whether any of the substitutions are refer to dynamic self.
  for (auto &entry : SubsMap)
    if (entry.second->getCanonicalType()->hasDynamicSelfType())
      return true;

  return false;
}

bool swift::hasDynamicSelfTypes(ArrayRef<Substitution> Subs) {
  // Check whether any of the substitutions are refer to dynamic self.
  for (auto &sub : Subs)
    if (sub.getReplacement()->getCanonicalType()->hasDynamicSelfType())
      return true;

  return false;
}

bool swift::computeMayBindDynamicSelf(SILFunction *F) {
  for (auto &BB : *F)
    for (auto &I : BB)
      if (auto Apply = FullApplySite::isa(&I))
        if (hasDynamicSelfTypes(Apply.getSubstitutions()))
          return true;
  return false;
}

/// Find a new position for an ApplyInst's FuncRef so that it dominates its
/// use. Not that FuncionRefInsts may be shared by multiple ApplyInsts.
void swift::placeFuncRef(ApplyInst *AI, DominanceInfo *DT) {
  FunctionRefInst *FuncRef = cast<FunctionRefInst>(AI->getCallee());
  SILBasicBlock *DomBB =
    DT->findNearestCommonDominator(AI->getParent(), FuncRef->getParent());
  if (DomBB == AI->getParent() && DomBB != FuncRef->getParent())
    // Prefer to place the FuncRef immediately before the call. Since we're
    // moving FuncRef up, this must be the only call to it in the block.
    FuncRef->moveBefore(AI);
  else
    // Otherwise, conservatively stick it at the beginning of the block.
    FuncRef->moveBefore(&*DomBB->begin());
}

/// \brief Add an argument, \p val, to the branch-edge that is pointing into
/// block \p Dest. Return a new instruction and do not erase the old
/// instruction.
TermInst *swift::addArgumentToBranch(SILValue Val, SILBasicBlock *Dest,
                                     TermInst *Branch) {
  SILBuilderWithScope Builder(Branch);

  if (CondBranchInst *CBI = dyn_cast<CondBranchInst>(Branch)) {
    SmallVector<SILValue, 8> TrueArgs;
    SmallVector<SILValue, 8> FalseArgs;

    for (auto A : CBI->getTrueArgs())
      TrueArgs.push_back(A);

    for (auto A : CBI->getFalseArgs())
      FalseArgs.push_back(A);

    if (Dest == CBI->getTrueBB()) {
      TrueArgs.push_back(Val);
      assert(TrueArgs.size() == Dest->getNumBBArg());
    } else {
      FalseArgs.push_back(Val);
      assert(FalseArgs.size() == Dest->getNumBBArg());
    }

    return Builder.createCondBranch(CBI->getLoc(), CBI->getCondition(),
                                    CBI->getTrueBB(), TrueArgs,
                                    CBI->getFalseBB(), FalseArgs);
  }

  if (BranchInst *BI = dyn_cast<BranchInst>(Branch)) {
    SmallVector<SILValue, 8> Args;

    for (auto A : BI->getArgs())
      Args.push_back(A);

    Args.push_back(Val);
    assert(Args.size() == Dest->getNumBBArg());
    return Builder.createBranch(BI->getLoc(), BI->getDestBB(), Args);
  }

  llvm_unreachable("unsupported terminator");
}

SILLinkage swift::getSpecializedLinkage(SILFunction *F, SILLinkage L) {
  switch (L) {
  case SILLinkage::Public:
  case SILLinkage::PublicExternal:
  case SILLinkage::Shared:
  case SILLinkage::SharedExternal:
  case SILLinkage::Hidden:
  case SILLinkage::HiddenExternal:
    // Specializations of public or hidden symbols can be shared by all TUs
    // that specialize the definition.
    // Treat stdlib_binary_only specially. We don't serialize the body of
    // stdlib_binary_only functions so we can't mark them as Shared (making
    // their visibility in the dylib hidden).
    return F->hasSemanticsString("stdlib_binary_only") ? SILLinkage::Public
                                                       : SILLinkage::Shared;

  case SILLinkage::Private:
  case SILLinkage::PrivateExternal:
    // Specializations of private symbols should remain so.
    // TODO: maybe PrivateExternals should get SharedExternal (these are private
    // functions from the stdlib which are specialized in another module).
    return SILLinkage::Private;
  }
}

/// Remove all instructions in the body of \p BB in safe manner by using
/// undef.
void swift::clearBlockBody(SILBasicBlock *BB) {
  // Instructions in the dead block may be used by other dead blocks.  Replace
  // any uses of them with undef values.
  while (!BB->empty()) {
    // Grab the last instruction in the BB.
    auto *Inst = &BB->back();

    // Replace any still-remaining uses with undef values and erase.
    Inst->replaceAllUsesWithUndef();
    Inst->eraseFromParent();
  }
}

// Handle the mechanical aspects of removing an unreachable block.
void swift::removeDeadBlock(SILBasicBlock *BB) {
  // Clear the body of BB.
  clearBlockBody(BB);

  // Now that the BB is empty, eliminate it.
  BB->eraseFromParent();
}

/// Cast a value into the expected, ABI compatible type if necessary.
/// This may happen e.g. when:
/// - a type of the return value is a subclass of the expected return type.
/// - actual return type and expected return type differ in optionality.
/// - both types are tuple-types and some of the elements need to be casted.
///
/// If CheckOnly flag is set, then this function only checks if the
/// required casting is possible. If it is not possible, then None
/// is returned.
///
/// If CheckOnly is not set, then a casting code is generated and the final
/// casted value is returned.
///
/// NOTE: We intentionally combine the checking of the cast's handling possibility
/// and the transformation performing the cast in the same function, to avoid
/// any divergence between the check and the implementation in the future.
///
/// NOTE: The implementation of this function is very closely related to the
/// rules checked by SILVerifier::requireABICompatibleFunctionTypes.
Optional<SILValue> swift::castValueToABICompatibleType(SILBuilder *B, SILLocation Loc,
                                             SILValue Value,
                                             SILType SrcTy, SILType DestTy,
                                             bool CheckOnly) {

  // No cast is required if types are the same.
  if (SrcTy == DestTy)
    return Value;

  SILValue CastedValue;

  auto &M = B->getModule();
  OptionalTypeKind SrcOTK;
  OptionalTypeKind DestOTK;

  // Check if src and dest types are optional.
  auto OptionalSrcTy = SrcTy.getSwiftRValueType()
                            .getAnyOptionalObjectType(SrcOTK);
  auto OptionalDestTy = DestTy.getSwiftRValueType()
                              .getAnyOptionalObjectType(DestOTK);

  // If both types are classes and dest is the superclass of src,
  // simply perform an upcast.
  if (SrcTy.getSwiftRValueType()->mayHaveSuperclass() &&
      DestTy.getSwiftRValueType()->mayHaveSuperclass() &&
      DestTy.isSuperclassOf(SrcTy)) {
    if (CheckOnly)
      return Value;
    CastedValue = B->createUpcast(Loc, Value, DestTy);
    return CastedValue;
  }

  SILType OptionalSrcLoweredTy;
  SILType OptionalDestLoweredTy;

  if (OptionalSrcTy)
    OptionalSrcLoweredTy = M.Types.getLoweredType(OptionalSrcTy);

  if (OptionalDestTy)
    OptionalDestLoweredTy = M.Types.getLoweredType(OptionalDestTy);

  // Both types are optional.
  if (OptionalDestTy && OptionalSrcTy) {
    // If both wrapped types are classes and dest is the superclass of src,
    // simply perform an upcast.
    if (OptionalDestTy->mayHaveSuperclass() &&
        OptionalSrcTy->mayHaveSuperclass() &&
        OptionalDestLoweredTy.isSuperclassOf(OptionalSrcLoweredTy)) {
      // Insert upcast.
      if (CheckOnly)
        return Value;
      CastedValue = B->createUpcast(Loc, Value, DestTy);
      return CastedValue;
    }

    if (CheckOnly)
      return castValueToABICompatibleType(B, Loc, Value,
          OptionalSrcLoweredTy,
          OptionalDestLoweredTy, CheckOnly);

    // Unwrap the original optional value.
    auto *SomeDecl = B->getASTContext().getOptionalSomeDecl(SrcOTK);
    auto *NoneBB = B->getFunction().createBasicBlock();
    auto *SomeBB = B->getFunction().createBasicBlock();
    auto *CurBB = B->getInsertionPoint()->getParent();

    auto *ContBB = CurBB->splitBasicBlock(B->getInsertionPoint());
    ContBB->createBBArg(DestTy,nullptr);

    SmallVector<std::pair<EnumElementDecl *, SILBasicBlock *>, 1> CaseBBs;
    CaseBBs.push_back(std::make_pair(SomeDecl, SomeBB));
    B->setInsertionPoint(CurBB);
    B->createSwitchEnum(Loc, Value, NoneBB, CaseBBs);

    // Handle the Some case.
    B->setInsertionPoint(SomeBB);
    SILValue UnwrappedValue =  B->createUncheckedEnumData(Loc, Value,
                                                          SomeDecl);
    // Cast the unwrapped value.
    auto CastedUnwrappedValue =
        castValueToABICompatibleType(B, Loc, UnwrappedValue,
                                     OptionalSrcLoweredTy,
                                     OptionalDestLoweredTy).getValue();
    // Wrap into optional.
    CastedValue =  B->createOptionalSome(Loc, CastedUnwrappedValue,
                                        DestOTK, DestTy);
    B->createBranch(Loc, ContBB, {CastedValue});

    // Handle the None case.
    B->setInsertionPoint(NoneBB);
    CastedValue = B->createOptionalNone(Loc, DestTy);
    B->createBranch(Loc, ContBB, {CastedValue});
    B->setInsertionPoint(ContBB->begin());

    CastedValue = ContBB->getBBArg(0);
    return CastedValue;
  }

  // Src is not optional, but dest is optional.
  if (!OptionalSrcTy && OptionalDestTy) {
    auto OptionalSrcCanTy = OptionalType::get(DestOTK,
        SrcTy.getSwiftRValueType()).getCanonicalTypeOrNull();
    auto LoweredOptionalSrcType = M.Types.getLoweredType(OptionalSrcCanTy);
    if (CheckOnly)
      return castValueToABICompatibleType(B, Loc, Value,
                                          LoweredOptionalSrcType, DestTy,
                                          CheckOnly);

    // Wrap the source value into an optional first.
    SILValue WrappedValue = B->createOptionalSome(Loc, Value,
                                                  DestOTK,
                                                  LoweredOptionalSrcType);
    // Cast the wrapped value.
    return castValueToABICompatibleType(B, Loc, WrappedValue,
                                        WrappedValue.getType(),
                                        DestTy);
  }

  // Both types are not optional.
  if (SrcTy.getSwiftRValueType()->mayHaveSuperclass() &&
      DestTy.getSwiftRValueType()->mayHaveSuperclass()) {
    if (CheckOnly)
      return Value;

    if (DestTy.isSuperclassOf(SrcTy)) {
      // Insert upcast.
      CastedValue = B->createUpcast(Loc, Value, DestTy);
      return CastedValue;
    }

    // Cast the reference.
    CastedValue = B->createUncheckedBitCast(Loc, Value, DestTy);
    return CastedValue;
  }

  // If B.Type needs to be casted to A.Type and
  // A is a superclass of B, then it can be done by means
  // of a simple upcast.
  if (isa<AnyMetatypeType>(SrcTy.getSwiftRValueType()) &&
      isa<AnyMetatypeType>(DestTy.getSwiftRValueType()) &&
      SrcTy.isClassOrClassMetatype() && DestTy.isClassOrClassMetatype() &&
      DestTy.getMetatypeInstanceType(M).isSuperclassOf(
          SrcTy.getMetatypeInstanceType(M))) {
    if (CheckOnly)
      return Value;
    CastedValue = B->createUpcast(Loc, Value, DestTy);
    return CastedValue;
  }

  if (SrcTy.isAddress() && DestTy.isAddress()) {
    if (CheckOnly)
      return Value;
    CastedValue = B->createUncheckedAddrCast(Loc, Value, DestTy);
    return CastedValue;
  }

  if (SrcTy.isHeapObjectReferenceType() && DestTy.isHeapObjectReferenceType()) {
    if (CheckOnly)
      return Value;
    CastedValue = B->createUncheckedRefCast(Loc, Value, DestTy);
    return CastedValue;
  }

  // Handle tuple types.
  // Extract elements, cast each of them, create a new tuple.
  if (auto tup = SrcTy.getAs<TupleType>()) {
    SmallVector<CanType, 1> aElements, bElements;
    auto atypes = tup.getElementTypes();
    aElements.append(atypes.begin(), atypes.end());
    auto btypes = DestTy.getAs<TupleType>().getElementTypes();
    bElements.append(btypes.begin(), btypes.end());

    if (CheckOnly && aElements.size() != bElements.size())
      return None;

    assert (aElements.size() == bElements.size() &&
          "Tuple types should have the same number of elements");

    SmallVector<SILValue, 8> ExpectedTuple;
    for (unsigned i : indices(aElements)) {
      auto aa = M.Types.getLoweredType(aElements[i]),
           bb = M.Types.getLoweredType(bElements[i]);

      if (CheckOnly) {
        if (!castValueToABICompatibleType(B, Loc, Value, aa, bb, CheckOnly).hasValue())
          return None;
        continue;
      }

      SILValue Element = B->createTupleExtract(Loc, Value, i);
      // Cast the value if necessary.
      Element = castValueToABICompatibleType(B, Loc, Element, aa, bb).getValue();
      ExpectedTuple.push_back(Element);
    }

    if (CheckOnly)
      return Value;

    CastedValue = B->createTuple(Loc, DestTy, ExpectedTuple);
    return CastedValue;
  }

  // Function types are interchangeable if they're also ABI-compatible.
  if (SrcTy.getAs<SILFunctionType>()) {
    if (DestTy.getAs<SILFunctionType>()) {
      if (CheckOnly)
        return Value;
      // Insert convert_function.
      CastedValue = B->createConvertFunction(Loc, Value, DestTy);
      return CastedValue;
    }
  }

  if (CheckOnly)
    return None;
  llvm_unreachable("Unknown combination of types for casting");
  return SILValue();
}

bool swift::canCastValueToABICompatibleType(SILModule &M,
                                            SILType SrcTy, SILType DestTy) {
  SILBuilder B(*M.begin());
  SILLocation Loc = ArtificialUnreachableLocation();
  auto Result = castValueToABICompatibleType(&B, Loc, SILValue(),
                                             SrcTy, DestTy,
                                             /* CheckOnly */ true);
  return Result.hasValue();
}


//===----------------------------------------------------------------------===//
//                       String Concatenation Optimizer
//===----------------------------------------------------------------------===//

namespace {
/// This is a helper class that performs optimization of string literals
/// concatenation.
class StringConcatenationOptimizer {
  /// Apply instruction being optimized.
  ApplyInst *AI;
  /// Builder to be used for creation of new instructions.
  SILBuilder &Builder;
  /// Left string literal operand of a string concatenation.
  StringLiteralInst *SLILeft = nullptr;
  /// Right string literal operand of a string concatenation.
  StringLiteralInst *SLIRight = nullptr;
  /// Function used to construct the left string literal.
  FunctionRefInst *FRILeft = nullptr;
  /// Function used to construct the right string literal.
  FunctionRefInst *FRIRight = nullptr;
  /// Apply instructions used to construct left string literal.
  ApplyInst *AILeft = nullptr;
  /// Apply instructions used to construct right string literal.
  ApplyInst *AIRight = nullptr;
  /// String literal conversion function to be used.
  FunctionRefInst *FRIConvertFromBuiltin = nullptr;
  /// Result type of a function producing the concatenated string literal.
  SILValue FuncResultType;

  /// Internal helper methods
  bool extractStringConcatOperands();
  void adjustEncodings();
  APInt getConcatenatedLength();
  bool isAscii() const;

public:
  StringConcatenationOptimizer(ApplyInst *AI, SILBuilder &Builder)
      : AI(AI), Builder(Builder) {}

  /// Tries to optimize a given apply instruction if it is a
  /// concatenation of string literals.
  ///
  /// Returns a new instruction if optimization was possible.
  SILInstruction *optimize();
};

} // end anonymous namespace

/// Checks operands of a string concatenation operation to see if
/// optimization is applicable.
///
/// Returns false if optimization is not possible.
/// Returns true and initializes internal fields if optimization is possible.
bool StringConcatenationOptimizer::extractStringConcatOperands() {
  auto *Fn = AI->getCalleeFunction();
  if (!Fn)
    return false;

  if (AI->getNumOperands() != 3 ||
      !Fn->hasSemanticsString("string.concat"))
    return false;

  // Left and right operands of a string concatenation operation.
  AILeft = dyn_cast<ApplyInst>(AI->getOperand(1));
  AIRight = dyn_cast<ApplyInst>(AI->getOperand(2));

  if (!AILeft || !AIRight)
    return false;

  FRILeft = dyn_cast<FunctionRefInst>(AILeft->getCallee());
  FRIRight = dyn_cast<FunctionRefInst>(AIRight->getCallee());

  if (!FRILeft || !FRIRight)
    return false;

  auto *FRILeftFun = FRILeft->getReferencedFunction();
  auto *FRIRightFun = FRIRight->getReferencedFunction();

  if (FRILeftFun->getEffectsKind() >= EffectsKind::ReadWrite ||
      FRIRightFun->getEffectsKind() >= EffectsKind::ReadWrite)
    return false;

  if (!FRILeftFun->hasDefinedSemantics() ||
      !FRIRightFun->hasDefinedSemantics())
    return false;

  auto SemanticsLeft = FRILeftFun->getSemanticsString();
  auto SemanticsRight = FRIRightFun->getSemanticsString();
  auto AILeftOperandsNum = AILeft->getNumOperands();
  auto AIRightOperandsNum = AIRight->getNumOperands();

  // makeUTF16 should have following parameters:
  // (start: RawPointer, numberOfCodeUnits: Word)
  // makeUTF8 should have following parameters:
  // (start: RawPointer, byteSize: Word, isASCII: Int1)
  if (!((SemanticsLeft == "string.makeUTF16" && AILeftOperandsNum == 4) ||
        (SemanticsLeft == "string.makeUTF8" && AILeftOperandsNum == 5) ||
        (SemanticsRight == "string.makeUTF16" && AIRightOperandsNum == 4) ||
        (SemanticsRight == "string.makeUTF8" && AIRightOperandsNum == 5)))
    return false;

  SLILeft = dyn_cast<StringLiteralInst>(AILeft->getOperand(1));
  SLIRight = dyn_cast<StringLiteralInst>(AIRight->getOperand(1));

  if (!SLILeft || !SLIRight)
    return false;

  // Only UTF-8 and UTF-16 encoded string literals are supported by this
  // optimization.
  if (SLILeft->getEncoding() != StringLiteralInst::Encoding::UTF8 &&
      SLILeft->getEncoding() != StringLiteralInst::Encoding::UTF16)
    return false;

  if (SLIRight->getEncoding() != StringLiteralInst::Encoding::UTF8 &&
      SLIRight->getEncoding() != StringLiteralInst::Encoding::UTF16)
    return false;

  return true;
}

/// Ensures that both string literals to be concatenated use the same
/// UTF encoding. Converts UTF-8 into UTF-16 if required.
void StringConcatenationOptimizer::adjustEncodings() {
  if (SLILeft->getEncoding() == SLIRight->getEncoding()) {
    FRIConvertFromBuiltin = FRILeft;
    if (SLILeft->getEncoding() == StringLiteralInst::Encoding::UTF8) {
      FuncResultType = AILeft->getOperand(4);
    } else {
      FuncResultType = AILeft->getOperand(3);
    }
    return;
  }

  Builder.setCurrentDebugScope(AI->getDebugScope());

  // If one of the string literals is UTF8 and another one is UTF16,
  // convert the UTF8-encoded string literal into UTF16-encoding first.
  if (SLILeft->getEncoding() == StringLiteralInst::Encoding::UTF8 &&
      SLIRight->getEncoding() == StringLiteralInst::Encoding::UTF16) {
    FuncResultType = AIRight->getOperand(3);
    FRIConvertFromBuiltin = FRIRight;
    // Convert UTF8 representation into UTF16.
    SLILeft = Builder.createStringLiteral(AI->getLoc(), SLILeft->getValue(),
                                          StringLiteralInst::Encoding::UTF16);
  }

  if (SLIRight->getEncoding() == StringLiteralInst::Encoding::UTF8 &&
      SLILeft->getEncoding() == StringLiteralInst::Encoding::UTF16) {
    FuncResultType = AILeft->getOperand(3);
    FRIConvertFromBuiltin = FRILeft;
    // Convert UTF8 representation into UTF16.
    SLIRight = Builder.createStringLiteral(AI->getLoc(), SLIRight->getValue(),
                                           StringLiteralInst::Encoding::UTF16);
  }

  // It should be impossible to have two operands with different
  // encodings at this point.
  assert(SLILeft->getEncoding() == SLIRight->getEncoding() &&
        "Both operands of string concatenation should have the same encoding");
}

/// Computes the length of a concatenated string literal.
APInt StringConcatenationOptimizer::getConcatenatedLength() {
  // Real length of string literals computed based on its contents.
  // Length is in code units.
  auto SLILenLeft = SLILeft->getCodeUnitCount();
  (void) SLILenLeft;
  auto SLILenRight = SLIRight->getCodeUnitCount();
  (void) SLILenRight;

  // Length of string literals as reported by string.make functions.
  auto *LenLeft = dyn_cast<IntegerLiteralInst>(AILeft->getOperand(2));
  auto *LenRight = dyn_cast<IntegerLiteralInst>(AIRight->getOperand(2));

  // Real and reported length should be the same.
  assert(SLILenLeft == LenLeft->getValue() &&
         "Size of string literal in @_semantics(string.make) is wrong");

  assert(SLILenRight == LenRight->getValue() &&
         "Size of string literal in @_semantics(string.make) is wrong");


  // Compute length of the concatenated literal.
  return LenLeft->getValue() + LenRight->getValue();
}

/// Computes the isAscii flag of a concatenated UTF8-encoded string literal.
bool StringConcatenationOptimizer::isAscii() const{
  // Add the isASCII argument in case of UTF8.
  // IsASCII is true only if IsASCII of both literals is true.
  auto *AsciiLeft = dyn_cast<IntegerLiteralInst>(AILeft->getOperand(3));
  auto *AsciiRight = dyn_cast<IntegerLiteralInst>(AIRight->getOperand(3));
  auto IsAsciiLeft = AsciiLeft->getValue() == 1;
  auto IsAsciiRight = AsciiRight->getValue() == 1;
  return IsAsciiLeft && IsAsciiRight;
}

SILInstruction *StringConcatenationOptimizer::optimize() {
  // Bail out if string literals concatenation optimization is
  // not possible.
  if (!extractStringConcatOperands())
    return nullptr;

  // Perform string literal encodings adjustments if needed.
  adjustEncodings();

  // Arguments of the new StringLiteralInst to be created.
  SmallVector<SILValue, 4> Arguments;

  // Encoding to be used for the concatenated string literal.
  auto Encoding = SLILeft->getEncoding();

  // Create a concatenated string literal.
  Builder.setCurrentDebugScope(AI->getDebugScope());
  auto LV = SLILeft->getValue();
  auto RV = SLIRight->getValue();
  auto *NewSLI =
      Builder.createStringLiteral(AI->getLoc(), LV + Twine(RV), Encoding);
  Arguments.push_back(NewSLI);

  // Length of the concatenated literal according to its encoding.
  auto *Len = Builder.createIntegerLiteral(
      AI->getLoc(), AILeft->getOperand(2).getType(), getConcatenatedLength());
  Arguments.push_back(Len);

  // isAscii flag for UTF8-encoded string literals.
  if (Encoding == StringLiteralInst::Encoding::UTF8) {
    bool IsAscii = isAscii();
    auto ILType = AILeft->getOperand(3).getType();
    auto *Ascii =
        Builder.createIntegerLiteral(AI->getLoc(), ILType, intmax_t(IsAscii));
    Arguments.push_back(Ascii);
  }

  // Type.
  Arguments.push_back(FuncResultType);

  auto FnTy = FRIConvertFromBuiltin->getType();
  auto STResultType = FnTy.castTo<SILFunctionType>()->getResult().getSILType();
  return Builder.createApply(AI->getLoc(), FRIConvertFromBuiltin, FnTy,
                             STResultType, ArrayRef<Substitution>(), Arguments,
                             false);
}

/// Top level entry point
SILInstruction *swift::tryToConcatenateStrings(ApplyInst *AI, SILBuilder &B) {
  return StringConcatenationOptimizer(AI, B).optimize();
}

//===----------------------------------------------------------------------===//
//                              Closure Deletion
//===----------------------------------------------------------------------===//

static bool useDoesNotKeepClosureAlive(const SILInstruction *I) {
  switch (I->getKind()) {
  case ValueKind::StrongRetainInst:
  case ValueKind::StrongReleaseInst:
  case ValueKind::RetainValueInst:
  case ValueKind::ReleaseValueInst:
  case ValueKind::DebugValueInst:
    return true;
  default:
    return false;
  }
}

void swift::releasePartialApplyCapturedArg(SILBuilder &Builder, SILLocation Loc,
                                           SILValue Arg, SILParameterInfo PInfo,
                                           InstModCallbacks Callbacks) {
  // If we have a non-trivial type and the argument is passed in @inout, we do
  // not need to destroy it here. This is something that is implicit in the
  // partial_apply design that will be revisited when partial_apply is
  // redesigned.
  if (PInfo.isIndirectInOut())
    return;

  if (isa<AllocStackInst>(Arg)) {
    return;
  }

  // If we have a trivial type, we do not need to put in any extra releases.
  if (Arg.getType().isTrivial(Builder.getModule()))
    return;

  // Otherwise, we need to destroy the argument.
  if (Arg.getType().isObject()) {
    if (Arg.getType().hasReferenceSemantics()) {
      auto U = Builder.emitStrongRelease(Loc, Arg);
      if (U.isNull())
        return;

      if (auto *SRI = U.dyn_cast<StrongRetainInst *>()) {
        Callbacks.DeleteInst(SRI);
        return;
      }

      Callbacks.CreatedNewInst(U.get<StrongReleaseInst *>());
      return;
    }

    auto U = Builder.emitReleaseValue(Loc, Arg);
    if (U.isNull())
      return;

    if (auto *RVI = U.dyn_cast<RetainValueInst *>()) {
      Callbacks.DeleteInst(RVI);
      return;
    }

    Callbacks.CreatedNewInst(U.get<ReleaseValueInst *>());
    return;
  }

  SILInstruction *NewInst = Builder.emitDestroyAddrAndFold(Loc, Arg);
  Callbacks.CreatedNewInst(NewInst);
}

/// For each captured argument of PAI, decrement the ref count of the captured
/// argument as appropriate at each of the post dominated release locations
/// found by Tracker.
static void releaseCapturedArgsOfDeadPartialApply(PartialApplyInst *PAI,
                                                  ReleaseTracker &Tracker,
                                                  InstModCallbacks Callbacks) {
  SILBuilderWithScope Builder(PAI);
  SILLocation Loc = PAI->getLoc();
  CanSILFunctionType PAITy =
      dyn_cast<SILFunctionType>(PAI->getCallee().getType().getSwiftType());

  // Emit a destroy value for each captured closure argument.
  ArrayRef<SILParameterInfo> Params = PAITy->getParameters();
  auto Args = PAI->getArguments();
  unsigned Delta = Params.size() - Args.size();
  assert(Delta <= Params.size() && "Error, more Args to partial apply than "
                                   "params in its interface.");

  for (auto *FinalRelease : Tracker.getFinalReleases()) {
    Builder.setInsertionPoint(FinalRelease);
    for (unsigned AI = 0, AE = Args.size(); AI != AE; ++AI) {
      SILValue Arg = Args[AI];
      SILParameterInfo Param = Params[AI + Delta];

      releasePartialApplyCapturedArg(Builder, Loc, Arg, Param, Callbacks);
    }
  }
}

/// TODO: Generalize this to general objects.
bool swift::tryDeleteDeadClosure(SILInstruction *Closure,
                                 InstModCallbacks Callbacks) {
  // We currently only handle locally identified values that do not escape. We
  // also assume that the partial apply does not capture any addresses.
  if (!isa<PartialApplyInst>(Closure) && !isa<ThinToThickFunctionInst>(Closure))
    return false;

  // We only accept a user if it is an ARC object that can be removed if the
  // object is dead. This should be expanded in the future. This also ensures
  // that we are locally identified and non-escaping since we only allow for
  // specific ARC users.
  ReleaseTracker Tracker([](const SILInstruction *I) -> bool {
    return useDoesNotKeepClosureAlive(I);
  });

  // Find the ARC Users and the final retain, release.
  if (!getFinalReleasesForValue(SILValue(Closure), Tracker))
    return false;

  // If we have a partial_apply, release each captured argument at each one of
  // the final release locations of the partial apply.
  if (auto *PAI = dyn_cast<PartialApplyInst>(Closure))
    releaseCapturedArgsOfDeadPartialApply(PAI, Tracker, Callbacks);

  // Then delete all user instructions.
  for (auto *User : Tracker.getTrackedUsers()) {
    assert(User->getNumTypes() == 0 && "We expect only ARC operations without "
                                       "results. This is true b/c of "
                                       "isARCOperationRemovableIfObjectIsDead");
    Callbacks.DeleteInst(User);
  }

  // Finally delete the closure.
  Callbacks.DeleteInst(Closure);

  return true;
}

//===----------------------------------------------------------------------===//
//                             Value Lifetime
//===----------------------------------------------------------------------===//

// Record a use by marking it's block and adding it to the initial LiveIn set.
void ValueLifetimeAnalysis::visitUser(SILInstruction *User) {
  auto *BB = User->getParent();
  UseBlocks.insert(BB);
  if (BB != DefValue->getParentBB())
    LiveIn.insert(BB);
}

// Generate ValueLifetimeAnalysis::liveIn.
//
// Propagate liveness backwards from an initial set of blocks in our
// liveIn set.
void ValueLifetimeAnalysis::propagateLiveness() {
  // First populate a worklist of predecessors.
  llvm::SmallVector<SILBasicBlock *, 64> Worklist;
  for (auto *BB : LiveIn)
    for (auto Pred : BB->getPreds())
      Worklist.push_back(Pred);

  // Now propagate liveness backwards until we hit the block that
  // defines the value.
  auto DefBB = DefValue->getParentBB();
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
bool ValueLifetimeAnalysis::successorHasLiveIn(SILBasicBlock *BB) {

  for (auto &Succ : BB->getSuccessors())
    if (LiveIn.count(Succ))
      return true;

  return false;
}

// Walk backwards in BB looking for last use of value DefValue.
SILInstruction *ValueLifetimeAnalysis::
findLastDirectUseInBlock(SILBasicBlock *BB) {
  for (auto II = BB->rbegin(); II != BB->rend(); ++II) {
    assert(DefValue.getDef() != &*II && "Found def before finding use!");

    for (auto &Oper : II->getAllOperands()) {
      if (Oper.get() != DefValue)
        continue;

      return &*II;
    }
  }
  llvm_unreachable("Expected to find use of value in block!");
}

// Walk backwards in BB looking for last use specified in the provided use
// list. This likely includes transitive uses of defValue.
SILInstruction *ValueLifetimeAnalysis::
findLastSpecifiedUseInBlock(SILBasicBlock *BB) {
  for (auto II = BB->rbegin(); II != BB->rend(); ++II) {
    assert(DefValue.getDef() != &*II && "Found def before finding use!");

    if (UserSet.count(&*II))
      return &*II;
  }
  llvm_unreachable("Expected to find use of value in block!");
}

/// Generate ValueLifetime::lastUsers from ValueLifetimeAnalysis.
ValueLifetime ValueLifetimeAnalysis::computeLastUsers() {
  ValueLifetime Lifetime;
  for (auto *BB : UseBlocks) {
    if (successorHasLiveIn(BB))
      continue;
    Lifetime.LastUsers.insert(UserSet.empty()
                              ? findLastDirectUseInBlock(BB)
                              : findLastSpecifiedUseInBlock(BB));
  }
  return Lifetime;
}

//===----------------------------------------------------------------------===//
//                    Casts Optimization and Simplification
//===----------------------------------------------------------------------===//

/// \brief  Get a substitution corresponding to the type witness.
/// Inspired by ProtocolConformance::getTypeWitnessByName.
static const Substitution *
getTypeWitnessByName(ProtocolConformance *conformance, Identifier name) {
  // Find the named requirement.
  AssociatedTypeDecl *assocType = nullptr;
  assert(conformance && "Missing conformance information");
  auto members = conformance->getProtocol()->lookupDirect(name);
  for (auto member : members) {
    assocType = dyn_cast<AssociatedTypeDecl>(member);
    if (assocType)
      break;
  }

  if (!assocType)
    return nullptr;

  if (!conformance->hasTypeWitness(assocType, nullptr)) {
    return nullptr;
  }
  return &conformance->getTypeWitness(assocType, nullptr);
}

/// Check if is a bridging cast, i.e. one of the sides is
/// a bridged type.
static bool isBridgingCast(CanType SourceType, CanType TargetType) {
  // Bridging casts cannot be further simplified.
  auto TargetIsBridgeable = TargetType->isBridgeableObjectType();
  auto SourceIsBridgeable = SourceType->isBridgeableObjectType();

  if (TargetIsBridgeable != SourceIsBridgeable)
    return true;

  return false;
}

/// If target is a Swift type bridging to an ObjC type,
/// return the ObjC type it bridges to.
/// If target is an ObjC type, return this type.
static Type getCastFromObjC(SILModule &M, CanType source, CanType target) {
  Optional<Type> BridgedTy = M.getASTContext().getBridgedToObjC(
      M.getSwiftModule(), target, nullptr);
  if (!BridgedTy.hasValue() || !BridgedTy.getValue())
    return Type();
  return BridgedTy.getValue();
}

/// Create a call of _forceBridgeFromObjectiveC_bridgeable or
/// _conditionallyBridgeFromObjectiveC_bridgeable  which converts an an ObjC
/// instance into a corresponding Swift type, conforming to
/// _ObjectiveCBridgeable.
SILInstruction *
CastOptimizer::
optimizeBridgedObjCToSwiftCast(SILInstruction *Inst,
                     bool isConditional,
                     SILValue Src,
                     SILValue Dest,
                     CanType Source,
                     CanType Target,
                     Type BridgedSourceTy,
                     Type BridgedTargetTy,
                     SILBasicBlock *SuccessBB,
                     SILBasicBlock *FailureBB) {
  auto &M = Inst->getModule();
  auto Loc = Inst->getLoc();

  // The conformance to _BridgedToObjectiveC is statically known.
  // Retrieve the  bridging operation to be used if a static conformance
  // to _BridgedToObjectiveC can be proven.
  FuncDecl *BridgeFuncDecl =
      isConditional
          ? M.getASTContext().getConditionallyBridgeFromObjectiveCBridgeable(nullptr)
          : M.getASTContext().getForceBridgeFromObjectiveCBridgeable(nullptr);

  assert(BridgeFuncDecl && "_forceBridgeFromObjectiveC should exist");

  SILDeclRef FuncDeclRef(BridgeFuncDecl, SILDeclRef::Kind::Func);

  // Lookup a function from the stdlib.
  SILFunction *BridgedFunc = M.getOrCreateFunction(
      Loc, FuncDeclRef, ForDefinition_t::NotForDefinition);

  if (!BridgedFunc)
    return nullptr;

  CanType CanBridgedTy(BridgedTargetTy);
  SILType SILBridgedTy = SILType::getPrimitiveObjectType(CanBridgedTy);

  SILBuilderWithScope Builder(Inst);
  SILValue SrcOp;
  SILInstruction *NewI = nullptr;

  assert(Src.getType().isAddress() && "Source should have an address type");
  assert(Dest.getType().isAddress() && "Source should have an address type");

  if (SILBridgedTy != Src.getType()) {
    // Check if we can simplify a cast into:
    // - ObjCTy to _ObjectiveCBridgeable._ObjectiveCType.
    // - then convert _ObjectiveCBridgeable._ObjectiveCType to
    // a Swift type using _forceBridgeFromObjectiveC.

    // Generate a load for the source argument.
    auto *Load = Builder.createLoad(Loc, Src);
    // Try to convert the source into the expected ObjC type first.


    if (Load->getType() == SILBridgedTy) {
      // If type of the source and the expected ObjC type are
      // equal, there is no need to generate the conversion
      // from ObjCTy to _ObjectiveCBridgeable._ObjectiveCType.
      if (isConditional) {
        SILBasicBlock *CastSuccessBB = Inst->getFunction()->createBasicBlock();
        CastSuccessBB->createBBArg(SILBridgedTy);
        Builder.createBranch(Loc, CastSuccessBB, SILValue(Load,0));
        Builder.setInsertionPoint(CastSuccessBB);
        SrcOp = SILValue(CastSuccessBB->getBBArg(0), 0);
      } else {
        SrcOp = Load;
      }
    } else if (isConditional) {
      SILBasicBlock *CastSuccessBB = Inst->getFunction()->createBasicBlock();
      CastSuccessBB->createBBArg(SILBridgedTy);
      NewI = Builder.createCheckedCastBranch(Loc, false, SILValue(Load, 0),
                                             SILBridgedTy, CastSuccessBB,
                                             FailureBB);
      Builder.setInsertionPoint(CastSuccessBB);
      SrcOp = SILValue(CastSuccessBB->getBBArg(0), 0);
    } else {
      NewI = Builder.createUnconditionalCheckedCast(Loc, SILValue(Load, 0),
                                                    SILBridgedTy);
      SrcOp = SILValue(NewI, 0);
    }
  } else {
    SrcOp = Src;
  }

  // Now emit the a cast from the casted ObjC object into a target type.
  // This is done by means of calling _forceBridgeFromObjectiveC or
  // _conditionallyBridgeFromObjectiveC_birdgeable from the Target type.
  // Lookup the required function in the Target type.

  // Lookup the _ObjectiveCBridgeable protocol.
  auto BridgedProto =
      M.getASTContext().getProtocol(KnownProtocolKind::ObjectiveCBridgeable);
  auto Conf =
      M.getSwiftModule()->lookupConformance(Target, BridgedProto, nullptr);
  assert(Conf.getInt() == ConformanceKind::Conforms &&
         "_ObjectiveCBridgeable conformance should exist");

  auto *Conformance = Conf.getPointer();

  auto ParamTypes = BridgedFunc->getLoweredFunctionType()
                               ->getParametersWithoutIndirectResult();

  auto *FuncRef = Builder.createFunctionRef(Loc, BridgedFunc);

  auto MetaTy = MetatypeType::get(Target, MetatypeRepresentation::Thick);
  auto SILMetaTy = M.Types.getTypeLowering(MetaTy, 0).getLoweredType();
  auto *MetaTyVal = Builder.createMetatype(Loc, SILMetaTy);
  SmallVector<SILValue, 1> Args;

  auto PolyFuncTy = BridgeFuncDecl->getType()->getAs<PolymorphicFunctionType>();
  ArrayRef<ArchetypeType *> Archetypes =
      PolyFuncTy->getGenericParams().getAllArchetypes();

  // Add substitutions
  SmallVector<Substitution, 2> Subs;
  auto Conformances = M.getASTContext().Allocate<ProtocolConformance *>(1);
  Conformances[0] = Conformance;
  Subs.push_back(Substitution(Archetypes[0], Target, Conformances));
  const Substitution *DepTypeSubst = getTypeWitnessByName(
      Conformance, M.getASTContext().getIdentifier("_ObjectiveCType"));
  Subs.push_back(Substitution(Archetypes[1], DepTypeSubst->getReplacement(),
                              DepTypeSubst->getConformances()));
  auto SILFnTy = FuncRef->getType();
  SILType SubstFnTy = SILFnTy.substGenericArgs(M, Subs);
  SILType ResultTy = SubstFnTy.castTo<SILFunctionType>()->getSILResult();

  // Temporary to hold the intermediate result.
  AllocStackInst *Tmp = nullptr;
  CanType OptionalTy;
  OptionalTypeKind OTK;
  SILValue InOutOptionalParam;
  if (isConditional) {
    // Create a temporary
    OptionalTy = OptionalType::get(Dest.getType().getSwiftRValueType())
                     ->getImplementationType()
                     .getCanonicalTypeOrNull();
    OptionalTy.getAnyOptionalObjectType(OTK);
    Tmp = Builder.createAllocStack(Loc,
                                   SILType::getPrimitiveObjectType(OptionalTy));
    InOutOptionalParam = SILValue(Tmp, 1);
  } else {
    InOutOptionalParam = Dest;
  }

  (void) ParamTypes;
  assert(ParamTypes[0].getConvention() == ParameterConvention::Direct_Owned &&
         "Parameter should be @owned");

  // Emit a retain.
  Builder.createRetainValue(Loc, SrcOp);

  Args.push_back(InOutOptionalParam);
  Args.push_back(SrcOp);
  Args.push_back(SILValue(MetaTyVal, 0));

  auto *AI = Builder.createApply(Loc, FuncRef, SubstFnTy, ResultTy, Subs, Args,
                                 false);

  // If the source of a cast should be destroyed, emit a release.
  if (auto *UCCAI = dyn_cast<UnconditionalCheckedCastAddrInst>(Inst)) {
    assert(UCCAI->getConsumptionKind() == CastConsumptionKind::TakeAlways);
    if (UCCAI->getConsumptionKind() == CastConsumptionKind::TakeAlways) {
      Builder.createReleaseValue(Loc, SrcOp);
    }
  }

  if (auto *CCABI = dyn_cast<CheckedCastAddrBranchInst>(Inst)) {
    if (CCABI->getConsumptionKind() == CastConsumptionKind::TakeAlways) {
      Builder.createReleaseValue(Loc, SrcOp);
    } else if (CCABI->getConsumptionKind() ==
               CastConsumptionKind::TakeOnSuccess) {
      // Insert a release in the success BB.
      Builder.setInsertionPoint(SuccessBB->begin());
      Builder.createReleaseValue(Loc, SrcOp);
    }
  }

  // Results should be checked in case we process a conditional
  // case. E.g. casts from NSArray into [SwiftType] may fail, i.e. return .None.
  if (isConditional) {
    // Copy the temporary into Dest.
    // Load from the optional.
    auto *SomeDecl = Builder.getASTContext().getOptionalSomeDecl(OTK);

    SILBasicBlock *ConvSuccessBB = Inst->getFunction()->createBasicBlock();
    SmallVector<std::pair<EnumElementDecl *, SILBasicBlock*>, 1> CaseBBs;
    CaseBBs.push_back(std::make_pair(M.getASTContext().getOptionalNoneDecl(), FailureBB));
    Builder.createSwitchEnumAddr(Loc, InOutOptionalParam, ConvSuccessBB, CaseBBs);

    Builder.setInsertionPoint(FailureBB->begin());
    Builder.createDeallocStack(Loc, SILValue(Tmp, 0));

    Builder.setInsertionPoint(ConvSuccessBB);
    auto Addr = Builder.createUncheckedTakeEnumDataAddr(Loc, InOutOptionalParam,
                                                        SomeDecl);
    auto LoadFromOptional = Builder.createLoad(Loc, SILValue(Addr, 0));

    // Store into Dest
    Builder.createStore(Loc, LoadFromOptional, Dest);

    Builder.createDeallocStack(Loc, SILValue(Tmp, 0));
    SmallVector<SILValue, 1> SuccessBBArgs;
    Builder.createBranch(Loc, SuccessBB, SuccessBBArgs);
  }

  EraseInstAction(Inst);
  return (NewI) ? NewI : AI;
}

bool swift::isValidLinkageForFragileRef(SILLinkage linkage) {
  switch (linkage) {
  case SILLinkage::Private:
  case SILLinkage::PrivateExternal:
  case SILLinkage::Hidden:
  case SILLinkage::HiddenExternal:
    return false;

  case SILLinkage::Shared:
  case SILLinkage::SharedExternal:
      // This handles some kind of generated functions, like constructors
      // of clang imported types.
      // TODO: check why those functions are not fragile anyway and make
      // a less conservative check here.
      return true;

  case SILLinkage::Public:
  case SILLinkage::PublicExternal:
    return true;
  }
}

/// Create a call of _bridgeToObjectiveC which converts an _ObjectiveCBridgeable
/// instance into a bridged ObjC type.
SILInstruction *
CastOptimizer::
optimizeBridgedSwiftToObjCCast(SILInstruction *Inst,
                     bool isConditional,
                     SILValue Src,
                     SILValue Dest,
                     CanType Source,
                     CanType Target,
                     Type BridgedSourceTy,
                     Type BridgedTargetTy,
                     SILBasicBlock *SuccessBB,
                     SILBasicBlock *FailureBB) {

  auto &M = Inst->getModule();
  auto Loc = Inst->getLoc();

  // Find the _BridgedToObjectiveC protocol.
  auto BridgedProto =
      M.getASTContext().getProtocol(KnownProtocolKind::ObjectiveCBridgeable);

  auto Conf =
      M.getSwiftModule()->lookupConformance(Source, BridgedProto, nullptr);

  assert(Conf.getInt() == ConformanceKind::Conforms &&
         "_ObjectiveCBridgeable conformance should exist");
  (void) Conf;

  bool isCurrentModuleBridgeToObjectiveC = false;

  // Generate code to invoke _bridgeToObjectiveC
  SILBuilderWithScope Builder(Inst);

  auto Members = Source.getNominalOrBoundGenericNominal()->lookupDirect(
      M.getASTContext().Id_bridgeToObjectiveC);
  assert(Members.size() == 1 &&
         "There should be exactly one implementation of _bridgeToObjectiveC");
  auto BridgeFuncDecl = Members.front();
  auto BridgeFuncDeclRef = SILDeclRef(BridgeFuncDecl);
  Module *Mod = M.getASTContext().getLoadedModule(
      M.getASTContext().Id_Foundation);
  if (!Mod)
    return nullptr;
  SmallVector<ValueDecl *, 2> Results;
  Mod->lookupMember(Results, Source.getNominalOrBoundGenericNominal(),
                    M.getASTContext().Id_bridgeToObjectiveC, Identifier());
  ArrayRef<ValueDecl *> ResultsRef(Results);
  if (ResultsRef.empty()) {
    M.getSwiftModule()->lookupMember(Results, Source.getNominalOrBoundGenericNominal(),
                      M.getASTContext().Id_bridgeToObjectiveC, Identifier());
    ResultsRef = Results;
    isCurrentModuleBridgeToObjectiveC = true;
  }
  if (ResultsRef.size() != 1)
    return nullptr;

  auto MemberDeclRef = SILDeclRef(Results.front());
  auto Linkage = (isCurrentModuleBridgeToObjectiveC)
                     ? ForDefinition_t::ForDefinition
                     : ForDefinition_t::NotForDefinition;
  auto *BridgedFunc = M.getOrCreateFunction(Loc, MemberDeclRef, Linkage);
  assert(BridgedFunc &&
         "Implementation of _bridgeToObjectiveC could not be found");

  if (Inst->getFunction()->isFragile() &&
      !(BridgedFunc->isFragile() ||
        isValidLinkageForFragileRef(BridgedFunc->getLinkage()) ||
        BridgedFunc->isExternalDeclaration()))
    return nullptr;

  auto ParamTypes = BridgedFunc->getLoweredFunctionType()
                               ->getParametersWithoutIndirectResult();

  auto SILFnTy = SILType::getPrimitiveObjectType(
      M.Types.getConstantFunctionType(BridgeFuncDeclRef));
  ArrayRef<Substitution> Subs;
  if (Source.getNominalOrBoundGenericNominal()->getGenericSignature()) {
    // Get substitutions, if source is a bound generic type.
    Subs = Source->castTo<BoundGenericType>()->getSubstitutions(
        M.getSwiftModule(), nullptr);
  }

  SILType SubstFnTy = SILFnTy.substGenericArgs(M, Subs);
  SILType ResultTy = SubstFnTy.castTo<SILFunctionType>()->getSILResult();

  auto FnRef = Builder.createFunctionRef(Loc, BridgedFunc);
  if (Src.getType().isAddress()) {
    // Create load
    Src = SILValue(Builder.createLoad(Loc, Src), 0);
  }

  if(ParamTypes[0].getConvention() == ParameterConvention::Direct_Guaranteed)
    Builder.createRetainValue(Loc, Src);

  // Generate a code to invoke the bridging function.
  auto *NewAI = Builder.createApply(Loc, FnRef, SubstFnTy, ResultTy, Subs, Src,
                                    false);

  if(ParamTypes[0].getConvention() == ParameterConvention::Direct_Guaranteed)
    Builder.createReleaseValue(Loc, Src);

  SILInstruction *NewI = NewAI;

  if (Dest) {
    // If it is addr cast then store the result.
    auto ConvTy = NewAI->getType();
    auto DestTy = Dest.getType().getObjectType();
    assert((ConvTy == DestTy || DestTy.isSuperclassOf(ConvTy)) &&
           "Destination should have the same type or be a superclass "
           "of the source operand");
    auto CastedValue = SILValue(
        (ConvTy == DestTy) ? NewI : Builder.createUpcast(Loc, NewAI, DestTy),
        0);
    NewI = Builder.createStore(Loc, CastedValue, Dest);
  }

  if (Dest) {
    EraseInstAction(Inst);
  }

  return NewI;
}

/// Make use of the fact that some of these casts cannot fail.
/// For example, if the ObjC type is exactly the expected
/// _ObjectiveCType type, then it would always succeed for
/// NSString, NSNumber, etc.
/// Casts from NSArray, NSDictionary and NSSet may fail.
///
/// If ObjC class is not exactly _ObjectiveCType, then
/// its conversion to a required _ObjectiveCType may fail.
SILInstruction *
CastOptimizer::
optimizeBridgedCasts(SILInstruction *Inst,
                     bool isConditional,
                     SILValue Src,
                     SILValue Dest,
                     CanType source,
                     CanType target,
                     SILBasicBlock *SuccessBB,
                     SILBasicBlock *FailureBB) {

  auto &M = Inst->getModule();

  // To apply the bridged optimizations, we should
  // ensure that types are not existential,
  // and that one of the types is a class and another
  // one is a struct.
  if (source.isAnyExistentialType() ||
      target.isAnyExistentialType() ||
      (source.getClassOrBoundGenericClass() &&
       !target.getStructOrBoundGenericStruct()) ||
      (target.getClassOrBoundGenericClass() &&
       !source.getStructOrBoundGenericStruct()))
    return nullptr;

  // Casts involving non-bound generic types cannot be optimized.
  if (source->hasArchetype() || target->hasArchetype())
    return nullptr;

  auto BridgedTargetTy = getCastFromObjC(M, source, target);
  if (!BridgedTargetTy)
    return nullptr;

  auto BridgedSourceTy = getCastFromObjC(M, target, source);

  CanType CanBridgedTargetTy(BridgedTargetTy);
  CanType CanBridgedSourceTy(BridgedSourceTy);

  if (CanBridgedSourceTy == source && CanBridgedTargetTy == target) {
    // Both source and target type are ObjC types.
    return nullptr;
  }

  if (CanBridgedSourceTy != source && CanBridgedTargetTy != target) {
    // Both source and target type are Swift types.
    return nullptr;
  }

  if (CanBridgedSourceTy || CanBridgedTargetTy) {
    // Check what kind of conversion it is? ObjC->Swift or Swift-ObjC?
    if (CanBridgedTargetTy != target) {
      // This is an ObjC to Swift cast.
      return optimizeBridgedObjCToSwiftCast(Inst, isConditional, Src, Dest, source,
          target, BridgedSourceTy, BridgedTargetTy, SuccessBB, FailureBB);
    } else {
      // This is a Swift to ObjC cast
      return optimizeBridgedSwiftToObjCCast(Inst, isConditional, Src, Dest, source,
          target, BridgedSourceTy, BridgedTargetTy, SuccessBB, FailureBB);
    }
  }

  llvm_unreachable("Unknown kind of bridging");
}


SILInstruction *
CastOptimizer::
simplifyCheckedCastAddrBranchInst(CheckedCastAddrBranchInst *Inst) {
  if (auto *I = optimizeCheckedCastAddrBranchInst(Inst))
    Inst = dyn_cast<CheckedCastAddrBranchInst>(I);

  if (!Inst)
    return nullptr;

  auto Loc = Inst->getLoc();
  auto Src = Inst->getSrc();
  auto Dest = Inst->getDest();
  auto SourceType = Inst->getSourceType();
  auto TargetType = Inst->getTargetType();
  auto *SuccessBB = Inst->getSuccessBB();
  auto *FailureBB = Inst->getFailureBB();
  auto &Mod = Inst->getModule();

  SILBuilderWithScope Builder(Inst);

  // Try to determine the outcome of the cast from a known type
  // to a protocol type at compile-time.
  bool isSourceTypeExact = isa<MetatypeInst>(Inst->getSrc());

  // Check if we can statically predict the outcome of the cast.
  auto Feasibility = classifyDynamicCast(Mod.getSwiftModule(),
                          Src.getType().getSwiftRValueType(),
                          Dest.getType().getSwiftRValueType(),
                          isSourceTypeExact,
                          Mod.isWholeModule());

  if (Feasibility == DynamicCastFeasibility::MaySucceed) {
    return nullptr;
  }

  if (Feasibility == DynamicCastFeasibility::WillFail) {
    if (shouldDestroyOnFailure(Inst->getConsumptionKind())) {
      auto &srcTL = Builder.getModule().getTypeLowering(Src.getType());
      srcTL.emitDestroyAddress(Builder, Loc, Src);
    }
    auto NewI = Builder.createBranch(Loc, FailureBB);
    EraseInstAction(Inst);
    WillFailAction();
    return NewI;
  }

  // Cast will succeed

  // Replace by unconditional_addr_cast, followed by a branch.
  // The unconditional_addr_cast can be skipped, if the result of a cast
  // is not used afterwards.
  bool ResultNotUsed = isa<AllocStackInst>(Dest.getDef());
  for (auto Use : Dest.getUses()) {
    auto *User = Use->getUser();
    if (isa<DeallocStackInst>(User) || User == Inst)
      continue;
    ResultNotUsed = false;
    break;
  }

  auto *BB = Inst->getParent();

  if (!ResultNotUsed) {
    SILInstruction *BridgedI = nullptr;

    // To apply the bridged optimizations, we should
    // ensure that types are not existential,
    // and that not both types are classes.
    BridgedI = optimizeBridgedCasts(Inst, true, Src, Dest, SourceType,
                                    TargetType, SuccessBB, FailureBB);

    if (!BridgedI) {
      // Since it is an addr cast, only address types are handled here.
      if (!Src.getType().isAddress() || !Dest.getType().isAddress()) {
        return nullptr;
      } else if (!emitSuccessfulIndirectUnconditionalCast(
                     Builder, Mod.getSwiftModule(), Loc,
                     Inst->getConsumptionKind(), Src, SourceType, Dest,
                     TargetType, Inst)) {
        // No optimization was possible.
        return nullptr;
      }
      EraseInstAction(Inst);
    }
    SILInstruction *NewI = &BB->back();
    if (!isa<TermInst>(NewI)) {
      Builder.setInsertionPoint(BB);
      NewI = Builder.createBranch(Loc, SuccessBB);
    }
    WillSucceedAction();
    return NewI;
  } else {
    // Result is not used.
    EraseInstAction(Inst);
    Builder.setInsertionPoint(BB);
    auto *NewI = Builder.createBranch(Loc, SuccessBB);
    WillSucceedAction();
    return NewI;
  }
}

SILInstruction *
CastOptimizer::simplifyCheckedCastBranchInst(CheckedCastBranchInst *Inst) {
  if (Inst->isExact()) {
    auto *ARI = dyn_cast<AllocRefInst>(Inst->getOperand().stripUpCasts());
    if (!ARI)
      return nullptr;

    // We know the dynamic type of the operand.
    SILBuilderWithScope Builder(Inst);
    auto Loc = Inst->getLoc();
    auto *SuccessBB = Inst->getSuccessBB();
    auto *FailureBB = Inst->getFailureBB();

    if (ARI->getType() == Inst->getCastType()) {
      // This exact cast will succeed.
      SmallVector<SILValue, 1> Args;
      Args.push_back(ARI);
      auto *NewI = Builder.createBranch(Loc, SuccessBB, Args);
      EraseInstAction(Inst);
      WillSucceedAction();
      return NewI;
    }

    // This exact cast will fail.
    auto *NewI = Builder.createBranch(Loc, FailureBB);
    EraseInstAction(Inst);
    WillFailAction();
    return NewI;
  }

  if (auto *I = optimizeCheckedCastBranchInst(Inst))
    Inst = dyn_cast<CheckedCastBranchInst>(I);

  if (!Inst)
    return nullptr;

  auto LoweredSourceType = Inst->getOperand().getType();
  auto LoweredTargetType = Inst->getCastType();
  auto SourceType = LoweredSourceType.getSwiftRValueType();
  auto TargetType = LoweredTargetType.getSwiftRValueType();
  auto Loc = Inst->getLoc();
  auto *SuccessBB = Inst->getSuccessBB();
  auto *FailureBB = Inst->getFailureBB();
  auto Op = Inst->getOperand();
  auto &Mod = Inst->getModule();
  bool isSourceTypeExact = isa<MetatypeInst>(Op);


  // Check if we can statically predict the outcome of the cast.
  auto Feasibility = classifyDynamicCast(Mod.getSwiftModule(),
                          SourceType,
                          TargetType,
                          isSourceTypeExact);

  if (Feasibility == DynamicCastFeasibility::MaySucceed) {
    return nullptr;
  }

  SILBuilderWithScope Builder(Inst);

  if (Feasibility == DynamicCastFeasibility::WillFail) {
    auto *NewI = Builder.createBranch(Loc, FailureBB);
    EraseInstAction(Inst);
    WillFailAction();
    return NewI;
  }

  // Casting will succeed.

  // Replace by unconditional_cast, followed by a branch.
  // The unconditional_cast can be skipped, if the result of a cast
  // is not used afterwards.
  bool ResultNotUsed = SuccessBB->getBBArg(0)->use_empty();
  SILValue CastedValue;
  if (Op.getType() != LoweredTargetType) {
    if (!ResultNotUsed) {
      auto Src = Inst->getOperand();
      auto Dest = SILValue();
      // To apply the bridged casts optimizations.
      auto BridgedI = optimizeBridgedCasts(Inst, false, Src, Dest, SourceType,
          TargetType, nullptr, nullptr);

      if (BridgedI) {
        CastedValue = SILValue(BridgedI, 0);
      } else {
        if (!canUseScalarCheckedCastInstructions(Mod, SourceType, TargetType))
          return nullptr;

        CastedValue = emitSuccessfulScalarUnconditionalCast(
          Builder, Mod.getSwiftModule(), Loc, Op, LoweredTargetType,
          SourceType, TargetType, Inst);
      }

      if (!CastedValue)
        CastedValue =
            Builder.createUnconditionalCheckedCast(Loc, Op, LoweredTargetType);
    } else {
      CastedValue = SILUndef::get(LoweredTargetType, Mod);
    }
  } else {
    // No need to cast.
    CastedValue = Op;
  }

  auto *NewI = Builder.createBranch(Loc, SuccessBB, CastedValue);
  EraseInstAction(Inst);
  WillSucceedAction();
  return NewI;
}

SILInstruction *
CastOptimizer::
optimizeCheckedCastAddrBranchInst(CheckedCastAddrBranchInst *Inst) {
  auto Loc = Inst->getLoc();
  auto Src = Inst->getSrc();
  auto Dest = Inst->getDest();
  auto *SuccessBB = Inst->getSuccessBB();
  auto *FailureBB = Inst->getFailureBB();

  // If there is an unbound generic type involved in the cast, bail.
  if (Src.getType().hasArchetype() || Dest.getType().hasArchetype())
    return nullptr;

  // %1 = metatype $A.Type
  // [%2 = init_existential_metatype %1 ...]
  // %3 = alloc_stack
  // store %1 to %3 or store %2 to %3
  // checked_cast_addr_br %3 to ...
  // ->
  // %1 = metatype $A.Type
  // %c = checked_cast_br %1 to ...
  // store %c to %3 (if successful)
  if (auto *ASI = dyn_cast<AllocStackInst>(Src.getDef())) {
    // Check if the value of this alloc_stack is set only once by a store
    // instruction, used only by CCABI and then deallocated.
    bool isLegal = true;
    StoreInst *Store = nullptr;
    for (auto Use : ASI->getUses()) {
      auto *User = Use->getUser();
      if (isa<DeallocStackInst>(User) || User == Inst)
        continue;
      if (auto *SI = dyn_cast<StoreInst>(User)) {
        if (!Store) {
          Store = SI;
          continue;
        }
      }
      isLegal = false;
      break;
    }

    if (isLegal && Store) {
      // Check what was the value stored in the allocated stack slot.
      auto Src = Store->getSrc();
      MetatypeInst *MI = nullptr;
      if (auto *IEMI = dyn_cast<InitExistentialMetatypeInst>(Src)) {
        MI = dyn_cast<MetatypeInst>(IEMI->getOperand());
      }

      if (!MI)
        MI = dyn_cast<MetatypeInst>(Src);

      if (MI) {
        if (SuccessBB->getSinglePredecessor()
            && canUseScalarCheckedCastInstructions(Inst->getModule(),
                MI->getType().getSwiftRValueType(),
                Dest.getType().getObjectType().getSwiftRValueType())) {
          SILBuilderWithScope B(Inst);
          auto NewI = B.createCheckedCastBranch(
              Loc, false /*isExact*/, SILValue(MI, 0),
              Dest.getType().getObjectType(), SuccessBB, FailureBB);
          SuccessBB->createBBArg(Dest.getType().getObjectType(), nullptr);
          B.setInsertionPoint(SuccessBB->begin());
          // Store the result
          B.createStore(Loc, SuccessBB->getBBArg(0), Dest);
          EraseInstAction(Inst);
          return NewI;
        }
      }
    }
  }
  return nullptr;
}

SILInstruction *
CastOptimizer::optimizeCheckedCastBranchInst(CheckedCastBranchInst *Inst) {
  if (Inst->isExact())
    return nullptr;

  auto LoweredTargetType = Inst->getCastType();
  auto Loc = Inst->getLoc();
  auto *SuccessBB = Inst->getSuccessBB();
  auto *FailureBB = Inst->getFailureBB();
  auto Op = Inst->getOperand();

  // Try to simplify checked_cond_br instructions using existential
  // metatypes by propagating a concrete type whenever it can be
  // determined statically.

  // %0 = metatype $A.Type
  // %1 = init_existential_metatype ..., %0: $A
  // checked_cond_br %1, ....
  // ->
  // %1 = metatype $A.Type
  // checked_cond_br %1, ....
  if (auto *IEMI = dyn_cast<InitExistentialMetatypeInst>(Op)) {
    if (auto *MI = dyn_cast<MetatypeInst>(IEMI->getOperand())) {
      SILBuilderWithScope B(Inst);
      auto *NewI = B.createCheckedCastBranch(Loc, /* isExact */ false, MI,
                                LoweredTargetType,
                                SuccessBB,
                                FailureBB);
      EraseInstAction(Inst);
      return NewI;
    }
  }

  if (auto *EMI = dyn_cast<ExistentialMetatypeInst>(Op)) {
    // Operand of the existential_metatype instruction.
    auto Op = EMI->getOperand();
    auto EmiTy = EMI->getType();

    // %0 = alloc_stack ..
    // %1 = init_existential_addr %0: $A
    // %2 = existential_metatype %0, ...
    // checked_cond_br %2, ....
    // ->
    // %1 = metatype $A.Type
    // checked_cond_br %1, ....

    if (auto *ASI = dyn_cast<AllocStackInst>(Op)) {
      // Should be in the same BB.
      if (ASI->getParent() != EMI->getParent())
        return nullptr;
      // Check if this alloc_stac is is only initialized once by means of
      // single init_existential_addr.
      bool isLegal = true;
      // init_existental instruction used to initialize this alloc_stack.
      InitExistentialAddrInst *FoundIEI = nullptr;
      for (auto Use: getNonDebugUses(*ASI)) {
        auto *User = Use->getUser();
        if (isa<ExistentialMetatypeInst>(User) ||
            isa<DestroyAddrInst>(User) ||
            isa<DeallocStackInst>(User))
           continue;
        if (auto *IEI = dyn_cast<InitExistentialAddrInst>(User)) {
          if (!FoundIEI) {
            FoundIEI = IEI;
            continue;
          }
        }
        isLegal = false;
        break;
      }

      if (isLegal && FoundIEI) {
        // Should be in the same BB.
        if (FoundIEI->getParent() != EMI->getParent())
          return nullptr;
        // Get the type used to initialize the existential.
        auto LoweredConcreteTy = FoundIEI->getLoweredConcreteType();
        if (LoweredConcreteTy.isAnyExistentialType())
          return nullptr;
        // Get the metatype of this type.
        auto EMT = dyn_cast<AnyMetatypeType>(EmiTy.getSwiftRValueType());
        auto *MetaTy = MetatypeType::get(LoweredConcreteTy.getSwiftRValueType(),
                                         EMT->getRepresentation());
        auto CanMetaTy = CanMetatypeType::CanTypeWrapper(MetaTy);
        auto SILMetaTy = SILType::getPrimitiveObjectType(CanMetaTy);
        SILBuilderWithScope B(Inst);
        auto *MI = B.createMetatype(FoundIEI->getLoc(), SILMetaTy);

        auto *NewI = B.createCheckedCastBranch(Loc, /* isExact */ false, MI,
                                  LoweredTargetType,
                                  SuccessBB,
                                  FailureBB);
        EraseInstAction(Inst);
        return NewI;
      }
    }

    // %0 = alloc_ref $A
    // %1 = init_existential_ref %0: $A, $...
    // %2 = existential_metatype ..., %1 :  ...
    // checked_cond_br %2, ....
    // ->
    // %1 = metatype $A.Type
    // checked_cond_br %1, ....
    if (auto *FoundIERI = dyn_cast<InitExistentialRefInst>(Op)) {
      auto *ASRI = dyn_cast<AllocRefInst>(FoundIERI->getOperand());
      if (!ASRI)
        return nullptr;
      // Should be in the same BB.
      if (ASRI->getParent() != EMI->getParent())
        return nullptr;
      // Check if this alloc_stac is is only initialized once by means of
      // a single initt_existential_ref.
      bool isLegal = true;
      for (auto Use: getNonDebugUses(*ASRI)) {
        auto *User = Use->getUser();
        if (isa<ExistentialMetatypeInst>(User) || isa<StrongReleaseInst>(User))
           continue;
        if (auto *IERI = dyn_cast<InitExistentialRefInst>(User)) {
          if (IERI == FoundIERI) {
            continue;
          }
        }
        isLegal = false;
        break;
      }

      if (isLegal && FoundIERI) {
        // Should be in the same BB.
        if (FoundIERI->getParent() != EMI->getParent())
          return nullptr;
        // Get the type used to initialize the existential.
        auto ConcreteTy = FoundIERI->getFormalConcreteType();
        if (ConcreteTy.isAnyExistentialType())
          return nullptr;
        // Get the SIL metatype of this type.
        auto EMT = dyn_cast<AnyMetatypeType>(EMI->getType().getSwiftRValueType());
        auto *MetaTy = MetatypeType::get(ConcreteTy, EMT->getRepresentation());
        auto CanMetaTy = CanMetatypeType::CanTypeWrapper(MetaTy);
        auto SILMetaTy = SILType::getPrimitiveObjectType(CanMetaTy);
        SILBuilderWithScope B(Inst);
        auto *MI = B.createMetatype(FoundIERI->getLoc(), SILMetaTy);

        auto *NewI = B.createCheckedCastBranch(Loc, /* isExact */ false, MI,
                                  LoweredTargetType,
                                  SuccessBB,
                                  FailureBB);
        EraseInstAction(Inst);
        return NewI;
      }
    }
  }

  return nullptr;
}

ValueBase *
CastOptimizer::
optimizeUnconditionalCheckedCastInst(UnconditionalCheckedCastInst *Inst) {
  auto LoweredSourceType = Inst->getOperand().getType();
  auto LoweredTargetType = Inst->getType();
  auto Loc = Inst->getLoc();
  auto Op = Inst->getOperand();
  auto &Mod = Inst->getModule();

  bool isSourceTypeExact = isa<MetatypeInst>(Op);

  // Check if we can statically predict the outcome of the cast.
  auto Feasibility = classifyDynamicCast(Mod.getSwiftModule(),
                          LoweredSourceType.getSwiftRValueType(),
                          LoweredTargetType.getSwiftRValueType(),
                          isSourceTypeExact);

  if (Feasibility == DynamicCastFeasibility::MaySucceed) {
    return nullptr;
  }

  if (Feasibility == DynamicCastFeasibility::WillFail) {
    // Remove the cast and insert a trap, followed by an
    // unreachable instruction.
    SILBuilderWithScope Builder(Inst);
    auto *Trap = Builder.createBuiltinTrap(Loc);
    Inst->replaceAllUsesWithUndef();
    EraseInstAction(Inst);
    Builder.setInsertionPoint(std::next(SILBasicBlock::iterator(Trap)));
    Builder.createUnreachable(ArtificialUnreachableLocation());
    WillFailAction();
    return Trap;
  }

  if (Feasibility == DynamicCastFeasibility::WillSucceed) {

    if (Inst->use_empty()) {
      EraseInstAction(Inst);
      WillSucceedAction();
      return nullptr;
    }

    SILBuilderWithScope Builder(Inst);

    // Try to apply the bridged casts optimizations
    auto SourceType = LoweredSourceType.getSwiftRValueType();
    auto TargetType = LoweredTargetType.getSwiftRValueType();
    auto Src = Inst->getOperand();
    auto NewI = optimizeBridgedCasts(Inst, false, Src, SILValue(), SourceType,
        TargetType, nullptr, nullptr);
    if (NewI) {
      ReplaceInstUsesAction(Inst, NewI);
      EraseInstAction(Inst);
      WillSucceedAction();
      return NewI;
    }

    if (isBridgingCast(SourceType, TargetType))
      return nullptr;

    auto Result = emitSuccessfulScalarUnconditionalCast(Builder,
                      Mod.getSwiftModule(), Loc, Op,
                      LoweredTargetType,
                      LoweredSourceType.getSwiftRValueType(),
                      LoweredTargetType.getSwiftRValueType(),
                      Inst);

    if (!Result) {
      // No optimization was possible.
      return nullptr;
    }

    ReplaceInstUsesAction(Inst, Result.getDef());
    EraseInstAction(Inst);
    WillSucceedAction();
    return Result.getDef();
  }

  return nullptr;
}


SILInstruction *
CastOptimizer::
optimizeUnconditionalCheckedCastAddrInst(UnconditionalCheckedCastAddrInst *Inst) {
  auto Loc = Inst->getLoc();
  auto Src = Inst->getSrc();
  auto Dest = Inst->getDest();
  auto SourceType = Inst->getSourceType();
  auto TargetType = Inst->getTargetType();
  auto &Mod = Inst->getModule();

  bool isSourceTypeExact = isa<MetatypeInst>(Src);

  // Check if we can statically predict the outcome of the cast.
  auto Feasibility = classifyDynamicCast(Mod.getSwiftModule(), SourceType,
                                         TargetType, isSourceTypeExact);

  if (Feasibility == DynamicCastFeasibility::MaySucceed) {
    // Forced bridged casts can be still simplified here.
    // If they fail, they fail inside the conversion function.
    if (!isBridgingCast(SourceType, TargetType))
      return nullptr;
  }

  if (Feasibility == DynamicCastFeasibility::WillFail) {
    // Remove the cast and insert a trap, followed by an
    // unreachable instruction.
    SILBuilderWithScope Builder(Inst);
    SILInstruction *NewI = Builder.createBuiltinTrap(Loc);
    // mem2reg's invariants get unhappy if we don't try to
    // initialize a loadable result.
    auto DestType = Dest.getType();
    auto &resultTL = Mod.Types.getTypeLowering(DestType);
    if (!resultTL.isAddressOnly()) {
      auto undef = SILValue(SILUndef::get(DestType.getObjectType(),
                                          Builder.getModule()));
      NewI = Builder.createStore(Loc, undef, Dest);
    }
    Inst->replaceAllUsesWithUndef();
    EraseInstAction(Inst);
    Builder.setInsertionPoint(std::next(SILBasicBlock::iterator(NewI)));
    Builder.createUnreachable(ArtificialUnreachableLocation());
    WillFailAction();
  }

  if (Feasibility == DynamicCastFeasibility::WillSucceed ||
      Feasibility == DynamicCastFeasibility::MaySucceed) {

    bool ResultNotUsed = isa<AllocStackInst>(Dest.getDef());
    for (auto Use : Dest.getUses()) {
      auto *User = Use->getUser();
      if (isa<DeallocStackInst>(User) || User == Inst)
        continue;
      ResultNotUsed = false;
      break;
    }

    if (ResultNotUsed) {
      EraseInstAction(Inst);
      WillSucceedAction();
      return nullptr;
    }

    // Try to apply the bridged casts optimizations
    auto NewI = optimizeBridgedCasts(Inst, false, Src, Dest, SourceType,
                                         TargetType, nullptr, nullptr);
    if (NewI) {
        WillSucceedAction();
        return nullptr;
    }

    if (isBridgingCast(SourceType, TargetType))
      return nullptr;

    SILBuilderWithScope Builder(Inst);
    if (!emitSuccessfulIndirectUnconditionalCast(Builder, Mod.getSwiftModule(),
                                            Loc, Inst->getConsumptionKind(),
                                            Src, SourceType,
                                            Dest, TargetType, Inst)) {
      // No optimization was possible.
      return nullptr;
    }

    Inst->replaceAllUsesWithUndef();
    EraseInstAction(Inst);
    WillSucceedAction();
  }

  return nullptr;
}

bool swift::simplifyUsers(SILInstruction *I) {
  bool Changed = false;

  for (auto UI = I->use_begin(), UE = I->use_end(); UI != UE; ) {
    SILInstruction *User = UI->getUser();
    ++UI;

    if (User->getNumTypes() != 1)
      continue;
    SILValue S = simplifyInstruction(User);
    if (!S)
      continue;

    SILValue(User).replaceAllUsesWith(S);
    User->eraseFromParent();
    Changed = true;
  }

  return Changed;
}

/// Some support functions for the global-opt and let-properties-opts

/// Check if a given type is a simple type, i.e. a builtin
/// integer or floating point type or a struct/tuple whose members
/// are of simple types.
/// TODO: Cache the "simple" flag for types to avoid repeating checks.
bool swift::isSimpleType(SILType SILTy, SILModule& Module) {
  // Classes can never be initialized statically at compile-time.
  if (SILTy.getClassOrBoundGenericClass()) {
    return false;
  }

  if (!SILTy.isTrivial(Module))
    return false;

  return true;
}

/// Check if the value of V is computed by means of a simple initialization.
/// Store the actual SILValue into Val and the reversed list of instructions
/// initializing it in Insns.
/// The check is performed by recursively walking the computation of the
/// SIL value being analyzed.
/// TODO: Move into utils.
bool
swift::analyzeStaticInitializer(SILValue V,
                                SmallVectorImpl<SILInstruction *> &Insns) {
  auto I = dyn_cast<SILInstruction>(V);
  if (!I)
    return false;

  while (true) {
    Insns.push_back(I);
    if (auto *SI = dyn_cast<StructInst>(I)) {
      // If it is not a struct which is a simple type, bail.
      if (!isSimpleType(SI->getType(), I->getModule()))
        return false;
      for (auto &Op: SI->getAllOperands()) {
        // If one of the struct instruction operands is not
        // a simple initializer, bail.
        if (!analyzeStaticInitializer(Op.get(), Insns))
          return false;
      }
      return true;
    } if (auto *TI = dyn_cast<TupleInst>(I)) {
      // If it is not a tuple which is a simple type, bail.
      if (!isSimpleType(TI->getType(), I->getModule()))
        return false;
      for (auto &Op: TI->getAllOperands()) {
        // If one of the struct instruction operands is not
        // a simple initializer, bail.
        if (!analyzeStaticInitializer(Op.get(), Insns))
          return false;
      }
      return true;
    } else {
      if (auto *bi = dyn_cast<BuiltinInst>(I)) {
        switch (bi->getBuiltinInfo().ID) {
        case BuiltinValueKind::FPTrunc:
          if (auto *LI = dyn_cast<LiteralInst>(bi->getArguments()[0])) {
            I = LI;
            continue;
          }
          break;
        default:
          return false;
        }
      }

      if (I->getKind() == ValueKind::IntegerLiteralInst
          || I->getKind() == ValueKind::FloatLiteralInst
          || I->getKind() == ValueKind::StringLiteralInst)
        return true;
      return false;
    }
  }
  return false;
}

/// Replace load sequence which may contian
/// a chain of struct_element_addr followed by a load.
/// The sequence is travered inside out, i.e.
/// starting with the innermost struct_element_addr
/// Move into utils.
void swift::replaceLoadSequence(SILInstruction *I,
                                SILInstruction *Value,
                                SILBuilder &B) {
  if (auto *LI = dyn_cast<LoadInst>(I)) {
    LI->replaceAllUsesWith(Value);
    return;
  }

  // It is a series of struct_element_addr followed by load.
  if (auto *SEAI = dyn_cast<StructElementAddrInst>(I)) {
    auto *SEI = B.createStructExtract(SEAI->getLoc(), Value, SEAI->getField());
    for (auto SEAIUse : SEAI->getUses()) {
      replaceLoadSequence(SEAIUse->getUser(), SEI, B);
    }
    return;
  }

  if (auto *TEAI = dyn_cast<TupleElementAddrInst>(I)) {
    auto *TEI = B.createTupleExtract(TEAI->getLoc(), Value, TEAI->getFieldNo());
    for (auto TEAIUse : TEAI->getUses()) {
      replaceLoadSequence(TEAIUse->getUser(), TEI, B);
    }
    return;
  }

  llvm_unreachable("Unknown instruction sequence for reading from a global");
}

/// Are the callees that could be called through Decl statically
/// knowable based on the Decl and the compilation mode?
bool swift::calleesAreStaticallyKnowable(SILModule &M, SILDeclRef Decl) {
  if (Decl.isForeign)
    return false;

  const DeclContext *AssocDC = M.getAssociatedContext();
  if (!AssocDC)
    return false;

  auto *AFD = Decl.getAbstractFunctionDecl();
  assert(AFD && "Expected abstract function decl!");

  // Only handle members defined within the SILModule's associated context.
  if (!AFD->isChildContextOf(AssocDC))
    return false;

  if (AFD->isDynamic())
    return false;

  // Only consider 'private' members, unless we are in whole-module compilation.
  switch (AFD->getEffectiveAccess()) {
  case Accessibility::Public:
    return false;
  case Accessibility::Internal:
    return M.isWholeModule();
  case Accessibility::Private:
    return true;
  }
}
