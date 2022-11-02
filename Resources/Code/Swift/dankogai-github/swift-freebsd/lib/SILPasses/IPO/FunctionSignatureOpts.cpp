//===-- FunctionSignatureOpts.cpp - Optimizes function signatures ---------===//
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

#define DEBUG_TYPE "sil-function-signature-opts"
#include "swift/SILPasses/Passes.h"
#include "swift/SILAnalysis/BasicCalleeAnalysis.h"
#include "swift/SILAnalysis/FunctionOrder.h"
#include "swift/SILAnalysis/RCIdentityAnalysis.h"
#include "swift/SILAnalysis/ARCAnalysis.h"
#include "swift/SILPasses/Transforms.h"
#include "swift/SILPasses/Utils/Local.h"
#include "swift/Basic/LLVM.h"
#include "swift/Basic/BlotMapVector.h"
#include "swift/Basic/Range.h"
#include "swift/SIL/Mangle.h"
#include "swift/SIL/Projection.h"
#include "swift/SIL/SILFunction.h"
#include "swift/SIL/SILCloner.h"
#include "swift/SIL/SILArgument.h"
#include "swift/SIL/SILValue.h"
#include "swift/SIL/SILDebugScope.h"
#include "swift/SIL/DebugUtils.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/Debug.h"
#include <type_traits>

using namespace swift;

STATISTIC(NumFunctionSignaturesOptimized, "Total func sig optimized");
STATISTIC(NumDeadArgsEliminated, "Total dead args eliminated");
STATISTIC(NumOwnedConvertedToGuaranteed, "Total owned args -> guaranteed args");
STATISTIC(NumCallSitesOptimized, "Total call sites optimized");
STATISTIC(NumSROAArguments, "Total SROA argumments optimized");

//===----------------------------------------------------------------------===//
//                                  Utility
//===----------------------------------------------------------------------===//

typedef SmallVector<FullApplySite, 8> ApplyList;

/// Returns true if I is a release instruction.
static bool isRelease(SILInstruction *I) {
  switch (I->getKind()) {
  case ValueKind::StrongReleaseInst:
  case ValueKind::ReleaseValueInst:
    return true;
  default:
    return false;
  }
}

/// Returns .Some(I) if I is a release that is the only non-debug instruction
/// with side-effects in the use-def graph originating from Arg. Returns
/// .Some(nullptr), if all uses from the arg were either debug insts or do not
/// have side-effects. Returns .None if there were any non-release instructions
/// with side-effects in the use-def graph from Arg or if there were multiple
/// release instructions with side-effects in the use-def graph from Arg.
static llvm::Optional<NullablePtr<SILInstruction>>
getNonTrivialNonDebugReleaseUse(SILArgument *Arg) {
  llvm::SmallVector<SILInstruction *, 8> Worklist;
  llvm::SmallPtrSet<SILInstruction *, 8> SeenInsts;
  llvm::Optional<SILInstruction *> Result;

  for (Operand *I : getNonDebugUses(SILValue(Arg)))
    Worklist.push_back(I->getUser());

  while (!Worklist.empty()) {
    SILInstruction *U = Worklist.pop_back_val();
    if (!SeenInsts.insert(U).second)
      continue;

    // If U is a terminator inst, return false.
    if (isa<TermInst>(U))
      return None;

    // If U has side effects...
    if (U->mayHaveSideEffects()) {
      // And is not a release_value, return None.
      if (!isRelease(U))
        return None;

      // If we have already seen a release of some sort, bail.
      if (Result.hasValue())
        return None;

      // Otherwise, set result to that value.
      Result = U;
      continue;
    }

    // Otherwise add all non-debug uses of I to the worklist.
    for (Operand *I : getNonDebugUses(*U))
      Worklist.push_back(I->getUser());
  }

  if (!Result.hasValue())
    return NullablePtr<SILInstruction>();
  return NullablePtr<SILInstruction>(Result.getValue());
}

//===----------------------------------------------------------------------===//
//                             Argument Analysis
//===----------------------------------------------------------------------===//

namespace {

/// A structure that maintains all of the information about a specific
/// SILArgument that we are tracking.
struct ArgumentDescriptor {

  /// The argument that we are tracking original data for.
  SILArgument *Arg;

  /// The original index of this argument.
  unsigned Index;

  /// The original parameter info of this argument.
  SILParameterInfo ParameterInfo;

  /// The original decl of this Argument.
  const ValueDecl *Decl;

  /// Was this parameter originally dead?
  bool IsDead;

  /// If non-null, this is the release in the return block of the callee, which
  /// is associated with this parameter if it is @owned. If the parameter is not
  /// @owned or we could not find such a release in the callee, this is null.
  SILInstruction *CalleeRelease;

  /// The same as CalleeRelease, but the release in the throw block, if it is a
  /// function which has a throw block.
  SILInstruction *CalleeReleaseInThrowBlock;

  /// The projection tree of this arguments.
  ProjectionTree ProjTree;

  ArgumentDescriptor() = delete;

  /// Initialize this argument descriptor with all information from A that we
  /// use in our optimization.
  ///
  /// *NOTE* We cache a lot of data from the argument and maintain a reference
  /// to the original argument. The reason why we do this is to make sure we
  /// have access to the original argument's state if we modify the argument
  /// when optimizing.
  ArgumentDescriptor(llvm::BumpPtrAllocator &BPA, SILArgument *A)
    : Arg(A), Index(A->getIndex()), ParameterInfo(A->getParameterInfo()),
      Decl(A->getDecl()), IsDead(false), CalleeRelease(),
      CalleeReleaseInThrowBlock(), ProjTree(A->getModule(), BPA, A->getType()) {
    ProjTree.computeUsesAndLiveness(A);
  }

  ArgumentDescriptor(const ArgumentDescriptor &) = delete;
  ArgumentDescriptor(ArgumentDescriptor &&) = default;
  ArgumentDescriptor &operator=(const ArgumentDescriptor &) = delete;
  ArgumentDescriptor &operator=(ArgumentDescriptor &&) = default;

  /// \returns true if this argument's ParameterConvention is P.
  bool hasConvention(ParameterConvention P) const {
    return Arg->hasConvention(P);
  }

  /// Convert the potentially multiple interface params associated with this
  /// argument.
  void
  computeOptimizedInterfaceParams(SmallVectorImpl<SILParameterInfo> &Out) const;

  /// Add potentially multiple new arguments to NewArgs from the caller's apply
  /// or try_apply inst.
  void addCallerArgs(SILBuilder &Builder, FullApplySite FAS,
                     SmallVectorImpl<SILValue> &NewArgs) const;

  /// Add potentially multiple new arguments to NewArgs from the thunk's
  /// function arguments.
  void addThunkArgs(SILBuilder &Builder, SILBasicBlock *BB,
                    SmallVectorImpl<SILValue> &NewArgs) const;

  /// Optimize the argument at ArgOffset and return the index of the next
  /// argument to be optimized.
  ///
  /// The return value makes it easy to SROA arguments since we can return the
  /// amount of SROAed arguments we created.
  unsigned
  updateOptimizedBBArgs(SILBuilder &Builder, SILBasicBlock *BB,
                        unsigned ArgOffset);

  bool canOptimizeLiveArg() const {
    return ParameterInfo.getSILType().isObject();
  }

  /// Return true if it's both legal and a good idea to explode this argument.
  bool shouldExplode() const {
    // We cannot optimize the argument.
    if (!canOptimizeLiveArg())
      return false;

    // See if the projection tree consists of potentially multiple levels of
    // structs containing one field. In such a case, there is no point in
    // exploding the argument.
    if (ProjTree.isSingleton())
      return false;

    size_t explosionSize = ProjTree.liveLeafCount();
    return explosionSize >= 1 && explosionSize <= 3;
  }
};

} // end anonymous namespace

void
ArgumentDescriptor::
computeOptimizedInterfaceParams(SmallVectorImpl<SILParameterInfo> &Out) const {
  DEBUG(llvm::dbgs() << "        Computing Interface Params\n");
  // If we have a dead argument, bail.
  if (IsDead) {
    DEBUG(llvm::dbgs() << "            Dead!\n");
    return;
  }

  // If this argument is live, but we can not optimize it.
  if (!canOptimizeLiveArg()) {
    DEBUG(llvm::dbgs() << "            Can not optimize live arg!\n");
    Out.push_back(ParameterInfo);
    return;
  }

  // If we can not explode this value, handle callee release and return.
  if (!shouldExplode()) {
    DEBUG(llvm::dbgs() << "            ProjTree can not explode arg.\n");
    // If we found a release in the callee in the last BB on an @owned
    // parameter, change the parameter to @guaranteed and continue...
    if (CalleeRelease) {
      DEBUG(llvm::dbgs() << "            Has callee release.\n");
      assert(ParameterInfo.getConvention() == ParameterConvention::Direct_Owned &&
             "Can only transform @owned => @guaranteed in this code path");
      SILParameterInfo NewInfo(ParameterInfo.getType(),
                               ParameterConvention::Direct_Guaranteed);
      Out.push_back(NewInfo);
      return;
    }

    DEBUG(llvm::dbgs() << "            Does not have callee release.\n");
    // Otherwise just propagate through the parameter info.
    Out.push_back(ParameterInfo);
    return;
  }

  DEBUG(llvm::dbgs() << "            ProjTree can explode arg.\n");
  // Ok, we need to use the projection tree. Iterate over the leafs of the
  // tree...
  llvm::SmallVector<SILType, 8> LeafTypes;
  ProjTree.getLeafTypes(LeafTypes);
  DEBUG(llvm::dbgs() << "            Leafs:\n");
  for (SILType Ty : LeafTypes) {
    DEBUG(llvm::dbgs() << "                " << Ty << "\n");
    // If Ty is trivial, just pass it directly.
    if (Ty.isTrivial(Arg->getModule())) {
      SILParameterInfo NewInfo(Ty.getSwiftRValueType(),
                               ParameterConvention::Direct_Unowned);
      Out.push_back(NewInfo);
      continue;
    }

    // If Ty is guaranteed, just pass it through.
    ParameterConvention Conv = ParameterInfo.getConvention();
    if (Conv == ParameterConvention::Direct_Guaranteed) {
      assert(!CalleeRelease && "Guaranteed parameter should not have a callee "
             "release.");
      SILParameterInfo NewInfo(Ty.getSwiftRValueType(),
                               ParameterConvention::Direct_Guaranteed);
      Out.push_back(NewInfo);
      continue;
    }

    // If Ty is not trivial and we found a callee release, pass it as
    // guaranteed.
    assert(ParameterInfo.getConvention() == ParameterConvention::Direct_Owned
           && "Can only transform @owned => @guaranteed in this code path");
    if (CalleeRelease) {
      SILParameterInfo NewInfo(Ty.getSwiftRValueType(),
                               ParameterConvention::Direct_Guaranteed);
      Out.push_back(NewInfo);
      continue;
    }

    // Otherwise, just add Ty as an @owned parameter.
    SILParameterInfo NewInfo(Ty.getSwiftRValueType(),
                             ParameterConvention::Direct_Owned);
    Out.push_back(NewInfo);
  }
}

void
ArgumentDescriptor::
addCallerArgs(SILBuilder &B, FullApplySite FAS,
              llvm::SmallVectorImpl<SILValue> &NewArgs) const {
  if (IsDead)
    return;

  SILValue Arg = FAS.getArgument(Index);
  if (!shouldExplode()) {
    NewArgs.push_back(Arg);
    return;
  }

  ProjTree.createTreeFromValue(B, FAS.getLoc(), Arg, NewArgs);
}

void
ArgumentDescriptor::
addThunkArgs(SILBuilder &Builder, SILBasicBlock *BB,
             llvm::SmallVectorImpl<SILValue> &NewArgs) const {
  if (IsDead)
    return;

  if (!shouldExplode()) {
    NewArgs.push_back(BB->getBBArg(Index));
    return;
  }

  ProjTree.createTreeFromValue(Builder, BB->getParent()->getLocation(),
                               BB->getBBArg(Index), NewArgs);
}

unsigned
ArgumentDescriptor::
updateOptimizedBBArgs(SILBuilder &Builder, SILBasicBlock *BB,
                      unsigned ArgOffset) {
  // If this argument is completely dead, delete this argument and return
  // ArgOffset.
  if (IsDead) {
    // If we have a callee release and we are dead, set the callee release's
    // operand to undef. We do not need it to have the argument anymore, but we
    // do need the instruction to be non-null.
    //
    // TODO: This should not be necessary.
    if (CalleeRelease) {
      SILType CalleeReleaseTy = CalleeRelease->getOperand(0).getType();
      CalleeRelease->setOperand(0, SILUndef::get(CalleeReleaseTy,
                                                 Builder.getModule()));
      
      // TODO: Currently we cannot mark arguments as dead if they are released
      // in a throw block. But as soon as we can do this, we have to handle
      // CalleeReleaseInThrowBlock as well.
      assert(!CalleeReleaseInThrowBlock &&
             "released arg in throw block cannot be dead");
    }

    // We should be able to recursively delete all of the remaining
    // instructions.
    SILArgument *Arg = BB->getBBArg(ArgOffset);
    eraseUsesOfValue(Arg);
    BB->eraseBBArg(ArgOffset);
    return ArgOffset;
  }

  // If this argument is not dead and we did not perform SROA, increment the
  // offset and return.
  if (!shouldExplode()) {
    return ArgOffset + 1;
  }

  // Create values for the leaf types.
  llvm::SmallVector<SILValue, 8> LeafValues;

  // Create a reference to the old arg offset and increment arg offset so we can
  // create the new arguments.
  unsigned OldArgOffset = ArgOffset++;

  // We do this in the same order as leaf types since ProjTree expects that the
  // order of leaf values matches the order of leaf types.
  {
    llvm::SmallVector<SILType, 8> LeafTypes;
    ProjTree.getLeafTypes(LeafTypes);
    for (auto Ty : LeafTypes) {
      LeafValues.push_back(BB->insertBBArg(ArgOffset++, Ty, BB->getBBArg(OldArgOffset)->getDecl()));
    }
  }

  // Then go through the projection tree constructing aggregates and replacing
  // uses.
  //
  // TODO: What is the right location to use here?
  ProjTree.replaceValueUsesWithLeafUses(Builder, BB->getParent()->getLocation(),
                                        LeafValues);

  // Replace all uses of the original arg with undef so it does not have any
  // uses.
  SILValue OrigArg = SILValue(BB->getBBArg(OldArgOffset));
  OrigArg.replaceAllUsesWith(SILUndef::get(OrigArg.getType(),
                                           BB->getModule()));

  // Now erase the old argument since it does not have any uses. We also
  // decrement ArgOffset since we have one less argument now.
  BB->eraseBBArg(OldArgOffset);
  --ArgOffset;

  return ArgOffset;
}

//===----------------------------------------------------------------------===//
//                             Function Analyzer
//===----------------------------------------------------------------------===//

namespace {

template <typename T1, typename T2>
inline T1 getFirstPairElt(const std::pair<T1, T2> &P) { return P.first; }

/// A class that contains all analysis information we gather about our
/// function. Also provides utility methods for creating the new empty function.
class FunctionAnalyzer {
  llvm::BumpPtrAllocator &Allocator;

  RCIdentityFunctionInfo *RCIA;

  /// The function that we are analyzing.
  SILFunction *F;

  /// Does any call inside the given function may bind dynamic 'Self' to a
  /// generic argument of the callee.
  bool MayBindDynamicSelf;

  /// Did we ascertain that we can optimize this function?
  bool ShouldOptimize;

  /// Did we change the self argument. If so we need to change the calling
  /// convention 'method' to 'freestanding'.
  bool HaveModifiedSelfArgument;

  /// A list of structures which present a "view" of precompiled information on
  /// an argument that we will use during our optimization.
  llvm::SmallVector<ArgumentDescriptor, 8> ArgDescList;

public:
  ArrayRef<ArgumentDescriptor> getArgList() const { return ArgDescList; }
  FunctionAnalyzer() = delete;
  FunctionAnalyzer(const FunctionAnalyzer &) = delete;
  FunctionAnalyzer(FunctionAnalyzer &&) = delete;

  FunctionAnalyzer(llvm::BumpPtrAllocator &Allocator,
                   RCIdentityFunctionInfo *RCIA,
                   SILFunction *F)
    : Allocator(Allocator), RCIA(RCIA), F(F),
      MayBindDynamicSelf(computeMayBindDynamicSelf(F)),
      ShouldOptimize(false), HaveModifiedSelfArgument(false), ArgDescList() {}

  /// Analyze the given function.
  bool analyze();

  /// Returns the managled name of the function that should be generated from
  /// this function analyzer.
  llvm::SmallString<64> getOptimizedName();

  /// Create a new empty function with the optimized signature found by this
  /// analysis.
  ///
  /// *NOTE* This occurs in the same module as F.
  SILFunction *createEmptyFunctionWithOptimizedSig(llvm::SmallString<64> &Name);

  ArrayRef<ArgumentDescriptor> getArgDescList() const { return ArgDescList; }
  MutableArrayRef<ArgumentDescriptor> getArgDescList() { return ArgDescList; }

  /// Is the given argument required by the ABI?
  ///
  /// Metadata arguments may be required if dynamic Self is bound to any generic
  /// parameters within this function's call sites.
  bool isArgumentABIRequired(SILArgument *Arg) {
    // This implicitly asserts that a function binding dynamic self has a self
    // metadata argument or object from which self metadata can be obtained.
    return MayBindDynamicSelf && (F->getSelfMetadataArgument() == Arg);
  }

private:
  /// Compute the CanSILFunctionType for the optimized function.
  CanSILFunctionType createOptimizedSILFunctionType();
};

} // end anonymous namespace

/// This function goes through the arguments of F and sees if we have anything
/// to optimize in which case it returns true. If we have nothing to optimize,
/// it returns false.
bool
FunctionAnalyzer::analyze() {
  // For now ignore functions with indirect results.
  if (F->getLoweredFunctionType()->hasIndirectResult())
    return false;

  ArrayRef<SILArgument *> Args = F->begin()->getBBArgs();

  // A map from consumed SILArguments to the release associated with an
  // argument.
  ConsumedArgToEpilogueReleaseMatcher ArgToReturnReleaseMap(RCIA, F);
  ConsumedArgToEpilogueReleaseMatcher ArgToThrowReleaseMap;
  auto ThrowBBIter = F->findThrowBB();
  if (ThrowBBIter != F->end())
    ArgToThrowReleaseMap.findMatchingReleases(RCIA, &*ThrowBBIter);

  for (unsigned i = 0, e = Args.size(); i != e; ++i) {
    ArgumentDescriptor A(Allocator, Args[i]);
    bool HaveOptimizedArg = false;

    bool isABIRequired = isArgumentABIRequired(Args[i]);
    auto OnlyRelease = getNonTrivialNonDebugReleaseUse(Args[i]);

    // If this argument is not ABI required and has not uses except for debug
    // instructions, remove it.
    if (!isABIRequired && OnlyRelease && OnlyRelease.getValue().isNull()) {
      A.IsDead = true;
      HaveOptimizedArg = true;
      ++NumDeadArgsEliminated;
    }

    // See if we can find a ref count equivalent strong_release or release_value
    // at the end of this function if our argument is an @owned parameter.
    if (A.hasConvention(ParameterConvention::Direct_Owned)) {
      if (auto *Release = ArgToReturnReleaseMap.releaseForArgument(A.Arg)) {
        SILInstruction *ReleaseInThrow = nullptr;
        
        // If the function has a throw block we must also find a matching
        // release in the throw block.
        if (ThrowBBIter == F->end() ||
            (ReleaseInThrow = ArgToThrowReleaseMap.releaseForArgument(A.Arg))) {
        
          // TODO: accept a second release in the throw block to let the
          // argument be dead.
          if (OnlyRelease && OnlyRelease.getValue().getPtrOrNull() == Release) {
            A.IsDead = true;
          }
          A.CalleeRelease = Release;
          A.CalleeReleaseInThrowBlock = ReleaseInThrow;
          HaveOptimizedArg = true;
          ++NumOwnedConvertedToGuaranteed;
        }
      }
    }

    if (A.shouldExplode()) {
      HaveOptimizedArg = true;
      ++NumSROAArguments;
    }

    if (HaveOptimizedArg) {
      ShouldOptimize = true;
      // Store that we have modified the self argument. We need to change the
      // calling convention later.
      if (Args[i]->isSelf())
        HaveModifiedSelfArgument = true;
    }

    // Add the argument to our list.
    ArgDescList.push_back(std::move(A));
  }

  return ShouldOptimize;
}

//===----------------------------------------------------------------------===//
//                         Creating the New Function
//===----------------------------------------------------------------------===//

CanSILFunctionType
FunctionAnalyzer::createOptimizedSILFunctionType() {
  const ASTContext &Ctx = F->getModule().getASTContext();
  CanSILFunctionType FTy = F->getLoweredFunctionType();

  // The only way that we modify the arity of function parameters is here for
  // dead arguments. Doing anything else is unsafe since by definition non-dead
  // arguments will have SSA uses in the function. We would need to be smarter
  // in our moving to handle such cases.
  llvm::SmallVector<SILParameterInfo, 8> InterfaceParams;
  for (auto &ArgDesc : ArgDescList) {
    ArgDesc.computeOptimizedInterfaceParams(InterfaceParams);
  }

  SILResultInfo InterfaceResult = FTy->getResult();
  auto InterfaceErrorResult = FTy->getOptionalErrorResult();
  auto ExtInfo = FTy->getExtInfo();

  // Don't use a method representation if we modified self.
  if (HaveModifiedSelfArgument)
    ExtInfo = ExtInfo.withRepresentation(SILFunctionTypeRepresentation::Thin);

  return SILFunctionType::get(FTy->getGenericSignature(),
                              ExtInfo,
                              FTy->getCalleeConvention(),
                              InterfaceParams, InterfaceResult,
                              InterfaceErrorResult, Ctx);
}

SILFunction *
FunctionAnalyzer::
createEmptyFunctionWithOptimizedSig(llvm::SmallString<64> &NewFName) {
  SILModule &M = F->getModule();

  // Create the new optimized function type.
  CanSILFunctionType NewFTy = createOptimizedSILFunctionType();

  // Create the new function.
  SILFunction *NewF = SILFunction::create(
      M, F->getLinkage(), NewFName, NewFTy, nullptr, F->getLocation(),
      F->isBare(), F->isTransparent(), F->isFragile(), F->isThunk(),
      F->getClassVisibility(),
      F->getInlineStrategy(), F->getEffectsKind(), 0, F->getDebugScope(),
      F->getDeclContext());

  NewF->setDeclCtx(F->getDeclContext());

  // Array semantic clients rely on the signature being as in the original
  // version.
  if (!F->getSemanticsAttr().startswith("array."))
    NewF->setSemanticsAttr(F->getSemanticsAttr());

  return NewF;
}

//===----------------------------------------------------------------------===//
//                                  Mangling
//===----------------------------------------------------------------------===//

llvm::SmallString<64> FunctionAnalyzer::getOptimizedName() {
  llvm::SmallString<64> Name;

  {
    llvm::raw_svector_ostream buffer(Name);
    Mangle::Mangler M(buffer);
    auto P = Mangle::SpecializationPass::FunctionSignatureOpts;
    Mangle::FunctionSignatureSpecializationMangler FSSM(P, M, F);

    for (unsigned i : indices(ArgDescList)) {
      const ArgumentDescriptor &Arg = ArgDescList[i];
      if (Arg.IsDead) {
        FSSM.setArgumentDead(i);
      }

      // If we have an @owned argument and found a callee release for it,
      // convert the argument to guaranteed.
      if (Arg.CalleeRelease) {
        FSSM.setArgumentOwnedToGuaranteed(i);
      }

      // If this argument is not dead and we can explode it, add 's' to the
      // mangling.
      if (Arg.shouldExplode() && !Arg.IsDead) {
        FSSM.setArgumentSROA(i);
      }
    }

    FSSM.mangle();
  }

  return Name;
}

//===----------------------------------------------------------------------===//
//                                Main Routine
//===----------------------------------------------------------------------===//

/// This function takes in OldF and all callsites of OldF and rewrites the
/// callsites to call the new function.
static void
rewriteApplyInstToCallNewFunction(FunctionAnalyzer &Analyzer,
                                  SILFunction *NewF,
                                  const ApplyList &CallSites) {
  for (auto FAS : CallSites) {
    auto *AI = FAS.getInstruction();

    SILBuilderWithScope Builder(AI);

    FunctionRefInst *FRI = Builder.createFunctionRef(AI->getLoc(), NewF);

    // Create the args for the new apply, ignoring any dead arguments.
    llvm::SmallVector<SILValue, 8> NewArgs;
    ArrayRef<ArgumentDescriptor> ArgDescs = Analyzer.getArgDescList();
    for (auto &ArgDesc : ArgDescs) {
      ArgDesc.addCallerArgs(Builder, FAS, NewArgs);
    }

    // We are ignoring generic functions and functions with out parameters for
    // now.
    SILType LoweredType = NewF->getLoweredType();
    SILType ResultType = LoweredType.getFunctionInterfaceResultType();
    SILLocation Loc = AI->getLoc();

    // Create the new apply.
    SILInstruction *NewAI;
    if (ApplyInst *RealAI = dyn_cast<ApplyInst>(AI)) {
      NewAI = Builder.createApply(Loc, FRI, LoweredType, ResultType,
                                           ArrayRef<Substitution>(), NewArgs,
                                           RealAI->isNonThrowing());
      // Replace all uses of the old apply with the new apply.
      AI->replaceAllUsesWith(NewAI);
    } else {
      auto *TAI = cast<TryApplyInst>(AI);
      NewAI = Builder.createTryApply(Loc, FRI, LoweredType,
                                  ArrayRef<Substitution>(), NewArgs,
                                  TAI->getNormalBB(), TAI->getErrorBB());

      Builder.setInsertionPoint(TAI->getErrorBB(), TAI->getErrorBB()->begin());
      // If we have any arguments that were consumed but are now guaranteed,
      // insert a release_value in the error block.
      for (auto &ArgDesc : ArgDescs) {
        if (!ArgDesc.CalleeRelease)
          continue;
        Builder.createReleaseValue(Loc, FAS.getArgument(ArgDesc.Index));
      }
      // Also insert release_value in the normal block (done below).
      Builder.setInsertionPoint(TAI->getNormalBB(), TAI->getNormalBB()->begin());
    }

    // If we have any arguments that were consumed but are now guaranteed,
    // insert a release_value.
    for (auto &ArgDesc : ArgDescs) {
      if (!ArgDesc.CalleeRelease)
        continue;
      Builder.createReleaseValue(Loc, FAS.getArgument(ArgDesc.Index));
    }

    // Erase the old apply and its callee.
    recursivelyDeleteTriviallyDeadInstructions(AI, true,
                                               [](SILInstruction *){});

    ++NumCallSitesOptimized;
  }
}

static void createThunkBody(SILBasicBlock *BB, SILFunction *NewF,
                            FunctionAnalyzer &Analyzer) {
  // TODO: What is the proper location to use here?
  SILLocation Loc = BB->getParent()->getLocation();
  SILBuilder Builder(BB);
  Builder.setCurrentDebugScope(BB->getParent()->getDebugScope());

  FunctionRefInst *FRI = Builder.createFunctionRef(Loc, NewF);

  // Create the args for the thunk's apply, ignoring any dead arguments.
  llvm::SmallVector<SILValue, 8> ThunkArgs;
  ArrayRef<ArgumentDescriptor> ArgDescs = Analyzer.getArgDescList();
  for (auto &ArgDesc : ArgDescs) {
    ArgDesc.addThunkArgs(Builder, BB, ThunkArgs);
  }

  // We are ignoring generic functions and functions with out parameters for
  // now.
  SILType LoweredType = NewF->getLoweredType();
  SILType ResultType = LoweredType.getFunctionInterfaceResultType();
  SILValue ReturnValue;
  auto FunctionTy = LoweredType.castTo<SILFunctionType>();
  if (FunctionTy->hasErrorResult()) {
    // We need a try_apply to call a function with an error result.
    SILFunction *Thunk = BB->getParent();
    SILBasicBlock *NormalBlock = Thunk->createBasicBlock();
    ReturnValue = NormalBlock->createBBArg(ResultType, 0);
    SILBasicBlock *ErrorBlock = Thunk->createBasicBlock();
    SILType ErrorType = SILType::getPrimitiveObjectType(
                                      FunctionTy->getErrorResult().getType());
    auto *ErrorArg = ErrorBlock->createBBArg(ErrorType, 0);
    Builder.createTryApply(Loc, FRI, LoweredType,
                                   ArrayRef<Substitution>(), ThunkArgs,
                                   NormalBlock, ErrorBlock);
    
    // If we have any arguments that were consumed but are now guaranteed,
    // insert a release_value in the error block.
    Builder.setInsertionPoint(ErrorBlock);
    for (auto &ArgDesc : ArgDescs) {
      if (!ArgDesc.CalleeRelease)
        continue;
      Builder.createReleaseValue(Loc, BB->getBBArg(ArgDesc.Index));
    }
    Builder.createThrow(Loc, ErrorArg);

    // Also insert release_value in the normal block (done below).
    Builder.setInsertionPoint(NormalBlock);
  } else {
    ReturnValue = Builder.createApply(Loc, FRI, LoweredType, ResultType,
                                               ArrayRef<Substitution>(),
                                               ThunkArgs, false);
  }

  // If we have any arguments that were consumed but are now guaranteed,
  // insert a release_value.
  for (auto &ArgDesc : ArgDescs) {
    if (!ArgDesc.CalleeRelease)
      continue;
    Builder.createReleaseValue(Loc, BB->getBBArg(ArgDesc.Index));
  }

  // Function that are marked as @NoReturn must be followed by an 'unreachable'
  // instruction.
  if (NewF->getLoweredFunctionType()->isNoReturn()) {
    Builder.createUnreachable(Loc);
    return;
  }

  Builder.createReturn(Loc, ReturnValue);
}

static SILFunction *
moveFunctionBodyToNewFunctionWithName(SILFunction *F,
                                      llvm::SmallString<64> &NewFName,
                                      FunctionAnalyzer &Analyzer) {
  // First we create an empty function (i.e. no BB) whose function signature has
  // had its arity modified.
  //
  // We only do this to remove dead arguments. All other function signature
  // optimization is done later by modifying the function signature elements
  // themselves.
  SILFunction *NewF = Analyzer.createEmptyFunctionWithOptimizedSig(NewFName);
  // Then we transfer the body of F to NewF. At this point, the arguments of the
  // first BB will not match.
  NewF->spliceBody(F);
  // Do the same with the call graph.

  // Then perform any updates to the arguments of NewF.
  SILBasicBlock *NewFEntryBB = &*NewF->begin();
  MutableArrayRef<ArgumentDescriptor> ArgDescs = Analyzer.getArgDescList();
  unsigned ArgOffset = 0;
  SILBuilder Builder(NewFEntryBB->begin());
  Builder.setCurrentDebugScope(NewFEntryBB->getParent()->getDebugScope());
  for (auto &ArgDesc : ArgDescs) {
    // We always need to reset the insertion point in case we delete the first
    // instruction.
    Builder.setInsertionPoint(NewFEntryBB->begin());
    DEBUG(llvm::dbgs() << "Updating arguments at ArgOffset: " << ArgOffset
          << " for: " << *ArgDesc.Arg);
    ArgOffset = ArgDesc.updateOptimizedBBArgs(Builder, NewFEntryBB, ArgOffset);
  }

  // Otherwise generate the thunk body just in case.
  SILBasicBlock *ThunkBody = F->createBasicBlock();
  for (auto &ArgDesc : ArgDescs) {
    ThunkBody->createBBArg(ArgDesc.ParameterInfo.getSILType(),
                           ArgDesc.Decl);
  }
  createThunkBody(ThunkBody, NewF, Analyzer);
  
  F->setThunk(IsThunk);
  assert(F->getDebugScope()->SILFn != NewF->getDebugScope()->SILFn);

  return NewF;
}

/// This function takes in a SILFunction F and its callsites in the current
/// module and produces a new SILFunction that has the body of F but with
/// optimized function arguments. F is changed to be a thunk that calls NewF to
/// reduce code duplication in cases where we missed a callsite to F. The
/// function returns true if we were successful in creating the new function and
/// returns false otherwise.
static bool
optimizeFunctionSignature(llvm::BumpPtrAllocator &BPA,
                          RCIdentityFunctionInfo *RCIA,
                          SILFunction *F,
                          const ApplyList &CallSites) {
  DEBUG(llvm::dbgs() << "Optimizing Function Signature of " << F->getName()
                     << "\n");

  assert(!CallSites.empty() && "Unexpected empty set of call sites!");

  // An array containing our ArgumentDescriptor objects that contain information
  // from our analysis.
  llvm::SmallVector<ArgumentDescriptor, 8> Arguments;

  // Analyze function arguments. If there is no work to be done, exit early.
  FunctionAnalyzer Analyzer(BPA, RCIA, F);
  if (!Analyzer.analyze()) {
    DEBUG(llvm::dbgs() << "    Has no optimizable arguments... "
                          "bailing...\n");
    return false;
  }

  DEBUG(llvm::dbgs() << "    Has optimizable arguments... Performing "
                        "optimizations...\n");

  ++NumFunctionSignaturesOptimized;

  llvm::SmallString<64> NewFName = Analyzer.getOptimizedName();

  // If we already have a specialized version of this function, do not
  // respecialize. For now just bail.
  //
  // TODO: Improve this. I do not expect this to occur often so I am fine for
  // now avoiding this issue. The main things I am worried about are assumptions
  // that we make about the callee and caller being violated. That said, this is
  // just a fear.
  if (F->getModule().lookUpFunction(NewFName))
    return false;

  // Otherwise, move F over to NewF.
  SILFunction *NewF = moveFunctionBodyToNewFunctionWithName(F, NewFName,
                                                            Analyzer);

  // And remove all Callee releases that we found and made redundant via owned
  // to guaranteed conversion.
  //
  // TODO: If more stuff needs to be placed here, refactor into its own method.
  for (auto &A : Analyzer.getArgDescList()) {
    if (A.CalleeRelease) {
      A.CalleeRelease->eraseFromParent();
      if (A.CalleeReleaseInThrowBlock) {
        A.CalleeReleaseInThrowBlock->eraseFromParent();
      }
    }
  }

  // Rewrite all apply insts calling F to call NewF. Update each call site as
  // appropriate given the form of function signature optimization performed.
  rewriteApplyInstToCallNewFunction(Analyzer, NewF, CallSites);

  return true;
}

//===----------------------------------------------------------------------===//
//                              Top Level Driver
//===----------------------------------------------------------------------===//

static bool isSpecializableRepresentation(SILFunctionTypeRepresentation Rep) {
  switch (Rep) {
  case SILFunctionTypeRepresentation::Method:
  case SILFunctionTypeRepresentation::Thin:
  case SILFunctionTypeRepresentation::Thick:
  case SILFunctionTypeRepresentation::CFunctionPointer:
    return true;
  case SILFunctionTypeRepresentation::WitnessMethod:
  case SILFunctionTypeRepresentation::ObjCMethod:
  case SILFunctionTypeRepresentation::Block:
    return false;
  }
}

/// Returns true if F is a function which the pass know show to specialize
/// function signatures for.
static bool canSpecializeFunction(SILFunction *F) {
  // Do not specialize the signature of SILFunctions that are external
  // declarations since there is no body to optimize.
  if (F->isExternalDeclaration())
    return false;

  // Do not specialize functions that are available externally. If an external
  // function was able to be specialized, it would have been specialized in its
  // own module. We will inline the original function as a thunk. The thunk will
  // call the specialized function.
  if (F->isAvailableExternally())
    return false;

  // Do not specialize the signature of always inline functions. We
  // will just inline them and specialize each one of the individual
  // functions that these sorts of functions are inlined into.
  if (F->getInlineStrategy() == Inline_t::AlwaysInline)
    return false;

  // For now ignore generic functions to keep things simple...
  if (F->getLoweredFunctionType()->isPolymorphic())
    return false;

  // Make sure F has a linkage that we can optimize.
  if (!isSpecializableRepresentation(F->getRepresentation()))
    return false;

  return true;
}

namespace {

class FunctionSignatureOpts : public SILModuleTransform {
public:
  FunctionSignatureOpts() {}

  void run() override {
    SILModule *M = getModule();
    auto *BCA = getAnalysis<BasicCalleeAnalysis>();
    auto *RCIA = getAnalysis<RCIdentityAnalysis>();
    llvm::BumpPtrAllocator Allocator;

    DEBUG(llvm::dbgs() << "**** Optimizing Function Signatures ****\n\n");

    // Construct a map from Callee -> Call Site Set.

    // Process each function in the callgraph that we are able to optimize.
    //
    // TODO: Determine if it is profitable to always perform this optimization
    // even if a function is not called locally. As far as we can tell. Down the
    // line more calls may be exposed and the inliner might be able to handle
    // those calls.
    bool Changed = false;

    // The CallerMap maps functions to the list of call sites that call that
    // function..
    llvm::DenseMap<SILFunction *, ApplyList> CallerMap;

    for (auto &F : *M) {
      // Don't optimize callers that are marked as 'no.optimize'.
      if (!F.shouldOptimize()) continue;

      // Scan the whole module and search Apply sites.
      for (auto &BB : F) {
        for (auto &II : BB) {
          if (auto Apply = FullApplySite::isa(&II)) {
            SILValue Callee = Apply.getCallee();

            //  Strip ThinToThickFunctionInst.
            if (auto TTTF = dyn_cast<ThinToThickFunctionInst>(Callee)) {
              Callee = TTTF->getOperand();
            }

            // Find the target function.
            auto *FRI = dyn_cast<FunctionRefInst>(Callee);
            if (!FRI) continue;

            SILFunction *F = FRI->getReferencedFunction();
            CallerMap[F].push_back(Apply);
          }
        }
      }
    }

    BottomUpFunctionOrder BottomUpOrder(*M, BCA);
    for (auto *F : BottomUpOrder.getFunctions()) {
      // Don't optimize callees that should not be optimized.
      if (!F->shouldOptimize())
        continue;

      // Check the signature of F to make sure that it is a function that we
      // can specialize. These are conditions independent of the call graph.
      if (!canSpecializeFunction(F))
        continue;

      // Now that we have our call graph, grab the CallSites of F.
      ApplyList &CallSites = CallerMap[F];

      // If this function is not called anywhere, for now don't do anything.
      //
      // TODO: If it is public, it may still make sense to specialize since if
      // we link in the public function in another module, we may be able to
      // inline it and access the specialized version.
      if (CallSites.empty())
        continue;

      // Otherwise, try to optimize the function signature of F.
      Changed |= optimizeFunctionSignature(Allocator, RCIA->get(F), F,
                                           CallSites);
    }

    // If we changed anything, invalidate the call graph.
    if (Changed) {
      invalidateAnalysis(SILAnalysis::InvalidationKind::Everything);
    }
  }

  StringRef getName() override { return "Function Signature Optimization"; }
};
} // end anonymous namespace

SILTransform *swift::createFunctionSignatureOpts() {
  return new FunctionSignatureOpts();
}
