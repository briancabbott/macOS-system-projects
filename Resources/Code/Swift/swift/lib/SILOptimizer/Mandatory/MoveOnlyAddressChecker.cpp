//===--- MoveOnlyAddressChecker.cpp ---------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2022 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
///
/// Move Only Checking of Addresses
/// -------------------------------
///
/// In this file, we implement move checking of addresses. This allows for the
/// compiler to perform move checking of address only lets, vars, inout args,
/// and mutating self.
///
/// Algorithm At a High Level
/// -------------------------
///
/// At a high level, this algorithm can be conceptualized as attempting to
/// completely classify the memory behavior of the recursive uses of a move only
/// marked address and then seek to transform those uses such that they are in
/// "simple move only address" form. We define "simple move only address" to
/// mean that along any path from an init of an address to a consume, all uses
/// are guaranteed to be semantically "borrow uses". If we find that any such
/// owned uses can not be turned into a "borrow" use, then we emit an error
/// since we would need to insert a copy there.
///
/// To implement this, our algorithm works in 4 stages: a use classification
/// stage, a dataflow stage, and then depending on success/failure one of two
/// transform stagesWe describe them below.
///
/// Use Classification Stage
/// ~~~~~~~~~~~~~~~~~~~~~~~~
///
/// Here we use an AccessPath based analysis to transitively visit all uses of
/// our marked address and classify a use as one of the following kinds of uses:
///
/// * init - store [init], copy_addr [init] %dest.
/// * destroy - destroy_addr.
/// * pureTake - load [take], copy_addr [take] %src.
/// * copyTransformableToTake - certain load [copy], certain copy_addr ![take]
/// %src of a temporary %dest.
/// * reinit - store [assign], copy_addr ![init] %dest
/// * borrow - load_borror, a load [copy] without consuming uses.
/// * livenessOnly - a read only use of the address.
///
/// We classify these by adding them to several disjoint SetVectors which track
/// membership.
///
/// When we classify an instruction as copyTransformableToTake, we perform some
/// extra preprocessing to determine if we can actually transform this copy to a
/// take. This means that we:
///
/// 1. For loads, we perform object move only checking. If we find a need for
/// multiple copies, we emit an error. If we find no extra copies needed, we
/// classify the load [copy] as a take if it has any last consuming uses and a
/// borrow if it only has destroy_addr consuming uses.
///
/// 2. For copy_addr, we pattern match if a copy_addr is initializing a "simple
/// temporary" (an alloc_stack with only one use that initializes it, a
/// copy_addr [init] in the same block). In this case, if the copy_addr only has
/// destroy_addr consuming uses, we treat it as a borrow... otherwise, we treat
/// it as a take. If we find any extra initializations, we fail the visitor so
/// we emit a "I don't understand this error" so that users report this case and
/// we can extend it as appropriate.
///
/// If we fail in either case, if we emit an error, we bail early with success
/// so we can assume invariants later in the dataflow stages that make the
/// dataflow easier.
///
/// Dataflow Stage
/// ~~~~~~~~~~~~~~
///
/// To perform our dataflow, we do the following:
///
/// 1. We walk each block from top to bottom performing the single block version
/// of the algorithm and preparing field sensitive pruned liveness.
///
/// 2. If we need to, we then use field sensitive pruned liveness to perform
/// global dataflow to determine if any of our takeOrCopies are within the
/// boundary lifetime implying a violation.
///
/// Success Transformation
/// ~~~~~~~~~~~~~~~~~~~~~~
///
/// Upon success Now that we know that we can change our address into "simple
/// move only address form", we transform the IR in the following way:
///
/// 1. Any load [copy] that are classified as borrows are changed to
/// load_borrow.
/// 2. Any load [copy] that are classified as takes are changed to load [take].
/// 3. Any copy_addr [init] temporary allocation are eliminated with their
///    destroy_addr. All uses are placed on the source address.
/// 4. Any destroy_addr that is paired with a copyTransformableToTake is
///    eliminated.
///
/// Fail Transformation
/// ~~~~~~~~~~~~~~~~~~~
///
/// If we emit any diagnostics, we loop through the function one last time after
/// we are done processing and convert all load [copy]/copy_addr of move only
/// types into their explicit forms. We take a little more compile time, but we
/// are going to fail anyways at this point, so it is ok to do so since we will
/// fail before attempting to codegen into LLVM IR.
///
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "sil-move-only-checker"

#include "swift/AST/DiagnosticEngine.h"
#include "swift/AST/DiagnosticsSIL.h"
#include "swift/Basic/Defer.h"
#include "swift/Basic/FrozenMultiMap.h"
#include "swift/SIL/ApplySite.h"
#include "swift/SIL/BasicBlockBits.h"
#include "swift/SIL/BasicBlockData.h"
#include "swift/SIL/BasicBlockDatastructures.h"
#include "swift/SIL/BasicBlockUtils.h"
#include "swift/SIL/Consumption.h"
#include "swift/SIL/DebugUtils.h"
#include "swift/SIL/InstructionUtils.h"
#include "swift/SIL/MemAccessUtils.h"
#include "swift/SIL/OwnershipUtils.h"
#include "swift/SIL/PostOrder.h"
#include "swift/SIL/PrunedLiveness.h"
#include "swift/SIL/SILArgument.h"
#include "swift/SIL/SILArgumentConvention.h"
#include "swift/SIL/SILBasicBlock.h"
#include "swift/SIL/SILBuilder.h"
#include "swift/SIL/SILFunction.h"
#include "swift/SIL/SILInstruction.h"
#include "swift/SIL/SILUndef.h"
#include "swift/SIL/SILValue.h"
#include "swift/SILOptimizer/Analysis/ClosureScope.h"
#include "swift/SILOptimizer/Analysis/DeadEndBlocksAnalysis.h"
#include "swift/SILOptimizer/Analysis/DominanceAnalysis.h"
#include "swift/SILOptimizer/Analysis/NonLocalAccessBlockAnalysis.h"
#include "swift/SILOptimizer/Analysis/PostOrderAnalysis.h"
#include "swift/SILOptimizer/PassManager/Transforms.h"
#include "swift/SILOptimizer/Utils/CanonicalOSSALifetime.h"
#include "swift/SILOptimizer/Utils/InstructionDeleter.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"

using namespace swift;

//===----------------------------------------------------------------------===//
//                              MARK: Utilities
//===----------------------------------------------------------------------===//

template <typename... T, typename... U>
static void diagnose(ASTContext &Context, SourceLoc loc, Diag<T...> diag,
                     U &&...args) {
  Context.Diags.diagnose(loc, diag, std::forward<U>(args)...);
}

static StringRef getVariableNameForValue(MarkMustCheckInst *mmci) {
  if (auto *allocInst = dyn_cast<AllocationInst>(mmci->getOperand())) {
    DebugVarCarryingInst debugVar(allocInst);
    if (auto varInfo = debugVar.getVarInfo()) {
      return varInfo->Name;
    } else {
      if (auto *decl = debugVar.getDecl()) {
        return decl->getBaseName().userFacingName();
      }
    }
  }

  if (auto *use = getSingleDebugUse(mmci)) {
    DebugVarCarryingInst debugVar(use->getUser());
    if (auto varInfo = debugVar.getVarInfo()) {
      return varInfo->Name;
    } else {
      if (auto *decl = debugVar.getDecl()) {
        return decl->getBaseName().userFacingName();
      }
    }
  }

  return "unknown";
}

namespace {

struct DiagnosticEmitter {
  // Track any violating uses we have emitted a diagnostic for so we don't emit
  // multiple diagnostics for the same use.
  SmallPtrSet<SILInstruction *, 8> useWithDiagnostic;

  void clear() { useWithDiagnostic.clear(); }

  void emitAddressDiagnostic(MarkMustCheckInst *markedValue,
                             SILInstruction *lastLiveUse,
                             SILInstruction *violatingUse, bool isUseConsuming,
                             bool isInOutEndOfFunction);
  void emitInOutEndOfFunctionDiagnostic(MarkMustCheckInst *markedValue,
                                        SILInstruction *violatingUse);
  void emitAddressDiagnosticNoCopy(MarkMustCheckInst *markedValue,
                                   SILInstruction *consumingUse);
};

} // namespace

void DiagnosticEmitter::emitAddressDiagnostic(
    MarkMustCheckInst *markedValue, SILInstruction *lastLiveUse,
    SILInstruction *violatingUse, bool isUseConsuming,
    bool isInOutEndOfFunction = false) {
  if (!useWithDiagnostic.insert(violatingUse).second)
    return;

  auto &astContext = markedValue->getFunction()->getASTContext();
  StringRef varName = getVariableNameForValue(markedValue);

  LLVM_DEBUG(llvm::dbgs() << "Emitting error!\n");
  LLVM_DEBUG(llvm::dbgs() << "    Mark: " << *markedValue);
  LLVM_DEBUG(llvm::dbgs() << "    Last Live Use: " << *lastLiveUse);
  LLVM_DEBUG(llvm::dbgs() << "    Last Live Use Is Consuming? "
                          << (isUseConsuming ? "yes" : "no") << '\n');
  LLVM_DEBUG(llvm::dbgs() << "    Violating Use: " << *violatingUse);

  // If our liveness use is the same as our violating use, then we know that we
  // had a loop. Give a better diagnostic.
  if (lastLiveUse == violatingUse) {
    diagnose(astContext,
             markedValue->getDefiningInstruction()->getLoc().getSourceLoc(),
             diag::sil_moveonlychecker_value_consumed_in_a_loop, varName);
    diagnose(astContext, violatingUse->getLoc().getSourceLoc(),
             diag::sil_moveonlychecker_consuming_use_here);
    return;
  }

  if (isInOutEndOfFunction) {
    diagnose(
        astContext,
        markedValue->getDefiningInstruction()->getLoc().getSourceLoc(),
        diag::
            sil_moveonlychecker_inout_not_reinitialized_before_end_of_function,
        varName);
    diagnose(astContext, violatingUse->getLoc().getSourceLoc(),
             diag::sil_moveonlychecker_consuming_use_here);
    return;
  }

  // First if we are consuming emit an error for no implicit copy semantics.
  if (isUseConsuming) {
    diagnose(astContext,
             markedValue->getDefiningInstruction()->getLoc().getSourceLoc(),
             diag::sil_moveonlychecker_owned_value_consumed_more_than_once,
             varName);
    diagnose(astContext, violatingUse->getLoc().getSourceLoc(),
             diag::sil_moveonlychecker_consuming_use_here);
    diagnose(astContext, lastLiveUse->getLoc().getSourceLoc(),
             diag::sil_moveonlychecker_consuming_use_here);
    return;
  }

  // Otherwise, use the "used after consuming use" error.
  diagnose(astContext,
           markedValue->getDefiningInstruction()->getLoc().getSourceLoc(),
           diag::sil_moveonlychecker_value_used_after_consume, varName);
  diagnose(astContext, violatingUse->getLoc().getSourceLoc(),
           diag::sil_moveonlychecker_consuming_use_here);
  diagnose(astContext, lastLiveUse->getLoc().getSourceLoc(),
           diag::sil_moveonlychecker_nonconsuming_use_here);
}

void DiagnosticEmitter::emitInOutEndOfFunctionDiagnostic(
    MarkMustCheckInst *markedValue, SILInstruction *violatingUse) {
  if (!useWithDiagnostic.insert(violatingUse).second)
    return;

  assert(cast<SILFunctionArgument>(markedValue->getOperand())
             ->getArgumentConvention()
             .isInoutConvention() &&
         "Expected markedValue to be on an inout");

  auto &astContext = markedValue->getFunction()->getASTContext();
  StringRef varName = getVariableNameForValue(markedValue);

  LLVM_DEBUG(llvm::dbgs() << "Emitting inout error error!\n");
  LLVM_DEBUG(llvm::dbgs() << "    Mark: " << *markedValue);
  LLVM_DEBUG(llvm::dbgs() << "    Violating Use: " << *violatingUse);

  // Otherwise, we need to do no implicit copy semantics. If our last use was
  // consuming message:
  diagnose(
      astContext,
      markedValue->getDefiningInstruction()->getLoc().getSourceLoc(),
      diag::sil_moveonlychecker_inout_not_reinitialized_before_end_of_function,
      varName);
  diagnose(astContext, violatingUse->getLoc().getSourceLoc(),
           diag::sil_moveonlychecker_consuming_use_here);
}

void DiagnosticEmitter::emitAddressDiagnosticNoCopy(
    MarkMustCheckInst *markedValue, SILInstruction *consumingUse) {
  if (!useWithDiagnostic.insert(consumingUse).second)
    return;

  auto &astContext = markedValue->getFunction()->getASTContext();
  StringRef varName = getVariableNameForValue(markedValue);

  LLVM_DEBUG(llvm::dbgs() << "Emitting no copy error!\n");
  LLVM_DEBUG(llvm::dbgs() << "    Mark: " << *markedValue);
  LLVM_DEBUG(llvm::dbgs() << "    Consuming Use: " << *consumingUse);

  // Otherwise, we need to do no implicit copy semantics. If our last use was
  // consuming message:
  diagnose(astContext,
           markedValue->getDefiningInstruction()->getLoc().getSourceLoc(),
           diag::sil_moveonlychecker_guaranteed_value_consumed, varName);
  diagnose(astContext, consumingUse->getLoc().getSourceLoc(),
           diag::sil_moveonlychecker_consuming_use_here);
}

static bool memInstMustInitialize(Operand *memOper) {
  SILValue address = memOper->get();

  SILInstruction *memInst = memOper->getUser();

  switch (memInst->getKind()) {
  default:
    return false;

  case SILInstructionKind::CopyAddrInst: {
    auto *CAI = cast<CopyAddrInst>(memInst);
    return CAI->getDest() == address && CAI->isInitializationOfDest();
  }
  case SILInstructionKind::ExplicitCopyAddrInst: {
    auto *CAI = cast<ExplicitCopyAddrInst>(memInst);
    return CAI->getDest() == address && CAI->isInitializationOfDest();
  }
  case SILInstructionKind::MarkUnresolvedMoveAddrInst: {
    return cast<MarkUnresolvedMoveAddrInst>(memInst)->getDest() == address;
  }
  case SILInstructionKind::InitExistentialAddrInst:
  case SILInstructionKind::InitEnumDataAddrInst:
  case SILInstructionKind::InjectEnumAddrInst:
    return true;

  case SILInstructionKind::BeginApplyInst:
  case SILInstructionKind::TryApplyInst:
  case SILInstructionKind::ApplyInst: {
    FullApplySite applySite(memInst);
    return applySite.isIndirectResultOperand(*memOper);
  }
  case SILInstructionKind::StoreInst: {
    auto qual = cast<StoreInst>(memInst)->getOwnershipQualifier();
    return qual == StoreOwnershipQualifier::Init ||
           qual == StoreOwnershipQualifier::Trivial;
  }

#define NEVER_OR_SOMETIMES_LOADABLE_CHECKED_REF_STORAGE(Name, ...)             \
  case SILInstructionKind::Store##Name##Inst:                                  \
    return cast<Store##Name##Inst>(memInst)->isInitializationOfDest();
#include "swift/AST/ReferenceStorage.def"
  }
}

static bool memInstMustReinitialize(Operand *memOper) {
  SILValue address = memOper->get();

  SILInstruction *memInst = memOper->getUser();

  switch (memInst->getKind()) {
  default:
    return false;

  case SILInstructionKind::CopyAddrInst: {
    auto *CAI = cast<CopyAddrInst>(memInst);
    return CAI->getDest() == address && !CAI->isInitializationOfDest();
  }
  case SILInstructionKind::ExplicitCopyAddrInst: {
    auto *CAI = cast<ExplicitCopyAddrInst>(memInst);
    return CAI->getDest() == address && !CAI->isInitializationOfDest();
  }
  case SILInstructionKind::YieldInst: {
    auto *yield = cast<YieldInst>(memInst);
    return yield->getYieldInfoForOperand(*memOper).isIndirectInOut();
  }
  case SILInstructionKind::BeginApplyInst:
  case SILInstructionKind::TryApplyInst:
  case SILInstructionKind::ApplyInst: {
    FullApplySite applySite(memInst);
    return applySite.getArgumentOperandConvention(*memOper).isInoutConvention();
  }
  case SILInstructionKind::StoreInst:
    return cast<StoreInst>(memInst)->getOwnershipQualifier() ==
           StoreOwnershipQualifier::Assign;

#define NEVER_OR_SOMETIMES_LOADABLE_CHECKED_REF_STORAGE(Name, ...)             \
  case SILInstructionKind::Store##Name##Inst:                                  \
    return !cast<Store##Name##Inst>(memInst)->isInitializationOfDest();
#include "swift/AST/ReferenceStorage.def"
  }
}

static bool memInstMustConsume(Operand *memOper) {
  SILValue address = memOper->get();

  SILInstruction *memInst = memOper->getUser();

  switch (memInst->getKind()) {
  default:
    return false;

  case SILInstructionKind::CopyAddrInst: {
    auto *CAI = cast<CopyAddrInst>(memInst);
    return (CAI->getSrc() == address && CAI->isTakeOfSrc()) ||
           (CAI->getDest() == address && !CAI->isInitializationOfDest());
  }
  case SILInstructionKind::ExplicitCopyAddrInst: {
    auto *CAI = cast<CopyAddrInst>(memInst);
    return (CAI->getSrc() == address && CAI->isTakeOfSrc()) ||
           (CAI->getDest() == address && !CAI->isInitializationOfDest());
  }
  case SILInstructionKind::BeginApplyInst:
  case SILInstructionKind::TryApplyInst:
  case SILInstructionKind::ApplyInst: {
    FullApplySite applySite(memInst);
    return applySite.getArgumentOperandConvention(*memOper).isOwnedConvention();
  }
  case SILInstructionKind::PartialApplyInst: {
    // If we are on the stack, we do not consume. Otherwise, we do.
    return !cast<PartialApplyInst>(memInst)->isOnStack();
  }
  case SILInstructionKind::DestroyAddrInst:
    return true;
  case SILInstructionKind::LoadInst:
    return cast<LoadInst>(memInst)->getOwnershipQualifier() ==
           LoadOwnershipQualifier::Take;
  }
}

//===----------------------------------------------------------------------===//
//                              MARK: Use State
//===----------------------------------------------------------------------===//

namespace {

struct UseState {
  SILValue address;
  SmallSetVector<DestroyAddrInst *, 4> destroys;
  llvm::SmallMapVector<SILInstruction *, TypeTreeLeafTypeRange, 4> livenessUses;
  llvm::SmallMapVector<SILInstruction *, TypeTreeLeafTypeRange, 4> borrows;
  llvm::SmallMapVector<SILInstruction *, TypeTreeLeafTypeRange, 4> takeInsts;
  llvm::SmallMapVector<SILInstruction *, TypeTreeLeafTypeRange, 4> initInsts;
  llvm::SmallMapVector<SILInstruction *, TypeTreeLeafTypeRange, 4> reinitInsts;

  SILFunction *getFunction() const { return address->getFunction(); }
  void clear() {
    address = SILValue();
    destroys.clear();
    livenessUses.clear();
    borrows.clear();
    takeInsts.clear();
    initInsts.clear();
    reinitInsts.clear();
  }
};

} // namespace

//===----------------------------------------------------------------------===//
//                          MARK: Liveness Processor
//===----------------------------------------------------------------------===//

namespace {

struct PerBlockLivenessState {
  using InstLeafTypePair = std::pair<SILInstruction *, TypeTreeLeafTypeRange>;
  SmallVector<InstLeafTypePair, 32> initDownInsts;
  SmallVector<InstLeafTypePair, 32> takeUpInsts;
  SmallVector<InstLeafTypePair, 32> livenessUpInsts;
  llvm::DenseMap<SILBasicBlock *,
                 std::pair<SILInstruction *, TypeTreeLeafTypeRange>>
      blockToState;
};

struct BlockState {
  using Map = llvm::DenseMap<SILBasicBlock *, BlockState>;

  /// This is either the liveness up or take up inst that projects
  /// up. We set this state according to the following rules:
  ///
  /// 1. If we are tracking a takeUp, we always take it even if we have a
  /// livenessUp.
  ///
  /// 2. If we have a livenessUp and do not have a take up, we track that
  /// instead.
  ///
  /// The reason why we do this is that we want to catch use after frees when
  /// non-consuming uses are later than a consuming use.
  SILInstruction *userUp;

  /// If we are init down, then we know that we can not transfer our take
  /// through this block and should stop traversing.
  bool isInitDown;

  BlockState() : userUp(nullptr) {}

  BlockState(SILInstruction *userUp, bool isInitDown)
      : userUp(userUp), isInitDown(isInitDown) {}
};

/// Post process the found liveness and emit errors if needed. TODO: Better
/// name.
struct LivenessChecker {
  MarkMustCheckInst *markedAddress;
  FieldSensitiveAddressPrunedLiveness &liveness;
  SmallBitVector livenessVector;
  bool hadAnyErrorUsers = false;
  SmallPtrSetImpl<SILInstruction *> &inoutTermUsers;
  BlockState::Map &blockToState;

  DiagnosticEmitter &diagnosticEmitter;

  LivenessChecker(MarkMustCheckInst *markedAddress,
                  FieldSensitiveAddressPrunedLiveness &liveness,
                  SmallPtrSetImpl<SILInstruction *> &inoutTermUsers,
                  BlockState::Map &blockToState,
                  DiagnosticEmitter &diagnosticEmitter)
      : markedAddress(markedAddress), liveness(liveness),
        inoutTermUsers(inoutTermUsers), blockToState(blockToState),
        diagnosticEmitter(diagnosticEmitter) {}

  /// Returns true if we emitted any errors.
  bool
  compute(SmallVectorImpl<std::pair<SILInstruction *, TypeTreeLeafTypeRange>>
              &takeUpInsts,
          SmallVectorImpl<std::pair<SILInstruction *, TypeTreeLeafTypeRange>>
              &takeDownInsts);

  void clear() {
    livenessVector.clear();
    hadAnyErrorUsers = false;
  }

  std::pair<bool, bool> testInstVectorLiveness(
      SmallVectorImpl<std::pair<SILInstruction *, TypeTreeLeafTypeRange>>
          &takeInsts);
};

} // namespace

std::pair<bool, bool> LivenessChecker::testInstVectorLiveness(
    SmallVectorImpl<std::pair<SILInstruction *, TypeTreeLeafTypeRange>>
        &takeInsts) {
  bool emittedDiagnostic = false;
  bool foundSingleBlockTakeDueToInitDown = false;

  for (auto takeInstAndValue : takeInsts) {
    LLVM_DEBUG(llvm::dbgs() << "    Checking: " << *takeInstAndValue.first);

    // Check if we are in the boundary...
    liveness.isWithinBoundary(takeInstAndValue.first, livenessVector);

    // If the bit vector does not contain any set bits, then we know that we did
    // not have any boundary violations for any leaf node of our root value.
    if (!livenessVector.any()) {
      // TODO: Today, we don't tell the user the actual field itself where the
      // violation occured and just instead just shows the two instructions. We
      // could be more specific though...
      LLVM_DEBUG(llvm::dbgs() << "        Not within the boundary.\n");
      continue;
    }
    LLVM_DEBUG(llvm::dbgs()
               << "        Within the boundary! Emitting an error\n");

    // Ok, we have an error and via the bit vector know which specific leaf
    // elements of our root type were within the per field boundary. We need to
    // go find the next reachable use that overlap with its sub-element. We only
    // emit a single error per use even if we get multiple sub elements that
    // match it. That helps reduce the amount of errors.
    //
    // DISCUSSION: It is important to note that this follows from the separation
    // of concerns behind this pass: we have simplified how we handle liveness
    // by losing this information. That being said, since we are erroring it is
    // ok that we are taking a little more time since we are not going to
    // codegen this code.
    //
    // That being said, set the flag that we saw at least one error, so we can
    // exit early after this loop.
    hadAnyErrorUsers = true;

    // B/c of the separation of concerns with our liveness, we now need to walk
    // blocks to go find the specific later takes that are reachable from this
    // take. It is ok that we are doing a bit more work here since we are going
    // to exit and not codegen.
    auto *errorUser = takeInstAndValue.first;

    // Before we do anything, grab the state for our errorUser's block from the
    // blockState and check if it is an init block. If so, we have no further
    // work to do since we already found correctness due to our single basic
    // block check. So, we have nothing further to do.
    if (blockToState.find(errorUser->getParent())->second.isInitDown) {
      // Set the flag that we saw an init down so that our assert later that we
      // actually emitted an error doesn't trigger.
      foundSingleBlockTakeDueToInitDown = true;
      continue;
    }

    BasicBlockWorklist worklist(errorUser->getFunction());
    for (auto *succBlock : errorUser->getParent()->getSuccessorBlocks())
      worklist.pushIfNotVisited(succBlock);

    LLVM_DEBUG(llvm::dbgs() << "Performing forward traversal from errorUse "
                               "looking for the cause of liveness!\n");

    while (auto *block = worklist.pop()) {
      LLVM_DEBUG(llvm::dbgs()
                 << "Visiting block: bb" << block->getDebugID() << "\n");
      auto iter = blockToState.find(block);
      if (iter == blockToState.end()) {
        LLVM_DEBUG(llvm::dbgs() << "    No State! Skipping!\n");
        for (auto *succBlock : block->getSuccessorBlocks())
          worklist.pushIfNotVisited(succBlock);
        continue;
      }

      auto *blockUser = iter->second.userUp;

      if (blockUser) {
        LLVM_DEBUG(llvm::dbgs() << "    Found userUp: " << *blockUser);
        auto info = liveness.isInterestingUser(blockUser);
        // Make sure that it overlaps with our range...
        if (info.second->contains(*info.second)) {
          LLVM_DEBUG(llvm::dbgs() << "    Emitted diagnostic for it!\n");
          // and if it does... emit our diagnostic and continue to see if we
          // find errors along other paths.
          // if (invalidUsesWithDiagnostics.insert(errorUser
          bool isConsuming =
              info.first ==
              FieldSensitiveAddressPrunedLiveness::LifetimeEndingUse;
          diagnosticEmitter.emitAddressDiagnostic(
              markedAddress, blockUser, errorUser, isConsuming,
              inoutTermUsers.count(blockUser));
          emittedDiagnostic = true;
          continue;
        }
      }

      // Otherwise, add successors and continue! We didn't overlap with this
      // use.
      LLVM_DEBUG(llvm::dbgs() << "    Does not overlap at the type level, no "
                                 "diagnostic! Visiting successors!\n");
      for (auto *succBlock : block->getSuccessorBlocks())
        worklist.pushIfNotVisited(succBlock);
    }
  }

  return {emittedDiagnostic, foundSingleBlockTakeDueToInitDown};
}

bool LivenessChecker::compute(
    SmallVectorImpl<std::pair<SILInstruction *, TypeTreeLeafTypeRange>>
        &takeUpInsts,
    SmallVectorImpl<std::pair<SILInstruction *, TypeTreeLeafTypeRange>>
        &takeDownInsts) {
  // Then revisit our takes, this time checking if we are within the boundary
  // and if we are, emit an error.
  LLVM_DEBUG(llvm::dbgs() << "Checking takes for errors!\n");
  bool emittedDiagnostic = false;
  bool foundSingleBlockTakeDueToInitDown = false;

  auto pair = testInstVectorLiveness(takeUpInsts);
  emittedDiagnostic |= pair.first;
  foundSingleBlockTakeDueToInitDown |= pair.second;

  pair = testInstVectorLiveness(takeDownInsts);
  emittedDiagnostic |= pair.first;
  foundSingleBlockTakeDueToInitDown |= pair.second;

  // If we emitted an error user, we should always emit at least one
  // diagnostic. If we didn't there is a bug in the implementation.
  assert(!hadAnyErrorUsers || emittedDiagnostic ||
         foundSingleBlockTakeDueToInitDown);
  return hadAnyErrorUsers;
}

//===----------------------------------------------------------------------===//
//                 MARK: Forward Declaration of Main Checker
//===----------------------------------------------------------------------===//

namespace {

struct MoveOnlyChecker {
  bool changed = false;

  SILFunction *fn;

  /// A set of mark_must_check that we are actually going to process.
  SmallSetVector<MarkMustCheckInst *, 32> moveIntroducersToProcess;

  /// A per mark must check, vector of uses that copy propagation says need a
  /// copy and thus are not final consuming uses.
  SmallVector<Operand *, 32> consumingUsesNeedingCopy;

  /// A per mark must check, vector of consuming uses that copy propagation says
  /// are actual last uses.
  SmallVector<Operand *, 32> finalConsumingUses;

  /// A set of mark must checks that we emitted diagnostics for. Used to
  /// reprocess mark_must_checks and clean up after the cases where we did not
  /// emit a diagnostic. We don't care about the cases where we emitted
  /// diagnostics since we are going to fail.
  SmallPtrSet<MarkMustCheckInst *, 4> valuesWithDiagnostics;

  /// The instruction deleter used by \p canonicalizer.
  InstructionDeleter deleter;

  /// The OSSA canonicalizer used to perform move checking for objects.
  CanonicalizeOSSALifetime canonicalizer;

  /// Per mark must check address use state.
  UseState addressUseState;

  /// Post order analysis used to lazily initialize post order function info
  /// only if we need it.
  PostOrderAnalysis *poa;

  /// Lazy function info, do not use directly. Use getPostOrderInfo() instead
  /// which will initialize this.
  PostOrderFunctionInfo *lazyPOI;

  /// Diagnostic emission routines wrapped around a consuming use cache. This
  /// ensures that we only emit a single error per use per marked value.
  DiagnosticEmitter diagnosticEmitter;

  MoveOnlyChecker(SILFunction *fn, DeadEndBlocks *deBlocks,
                  NonLocalAccessBlockAnalysis *accessBlockAnalysis,
                  DominanceInfo *domTree, PostOrderAnalysis *poa)
      : fn(fn),
        deleter(InstModCallbacks().onDelete([&](SILInstruction *instToDelete) {
          if (auto *mvi = dyn_cast<MarkMustCheckInst>(instToDelete))
            moveIntroducersToProcess.remove(mvi);
          instToDelete->eraseFromParent();
        })),
        canonicalizer(
            false /*pruneDebugMode*/, accessBlockAnalysis, domTree, deleter,
            [&](Operand *use) { consumingUsesNeedingCopy.push_back(use); },
            [&](Operand *use) { finalConsumingUses.push_back(use); }) {}

  /// Search through the current function for candidate mark_must_check
  /// [noimplicitcopy]. If we find one that does not fit a pattern that we
  /// understand, emit an error diagnostic telling the programmer that the move
  /// checker did not know how to recognize this code pattern.
  void searchForCandidateMarkMustChecks();

  /// Emits an error diagnostic for \p markedValue.
  void emitObjectDiagnostic(MarkMustCheckInst *markedValue,
                            bool originalValueGuaranteed);

  void performObjectCheck(MarkMustCheckInst *markedValue);

  bool performSingleCheck(MarkMustCheckInst *markedValue);

  bool check();

  PostOrderFunctionInfo *getPostOrderInfo() {
    if (!lazyPOI)
      lazyPOI = poa->get(fn);
    return lazyPOI;
  }
};

} // namespace

//===----------------------------------------------------------------------===//
//                   MARK: GatherLexicalLifetimeUseVisitor
//===----------------------------------------------------------------------===//

namespace {

/// Visit all of the uses of value in preparation for running our algorithm.
struct GatherUsesVisitor : public AccessUseVisitor {
  MoveOnlyChecker &moveChecker;
  UseState &useState;
  MarkMustCheckInst *markedValue;
  bool emittedEarlyDiagnostic = false;
  DiagnosticEmitter &diagnosticEmitter;

  GatherUsesVisitor(MoveOnlyChecker &moveChecker, UseState &useState,
                    MarkMustCheckInst *markedValue,
                    DiagnosticEmitter &diagnosticEmitter)
      : AccessUseVisitor(AccessUseType::Overlapping,
                         NestedAccessType::IgnoreAccessBegin),
        moveChecker(moveChecker), useState(useState), markedValue(markedValue),
        diagnosticEmitter(diagnosticEmitter) {}

  bool visitUse(Operand *op, AccessUseType useTy) override;
  void reset(SILValue address) { useState.address = address; }
  void clear() { useState.clear(); }

  /// For now always markedValue. If we start using this for move address
  /// checking, we need to check against the operand of the markedValue. This is
  /// because for move checking, our marker is placed along the variables
  /// initialization so we are always going to have all later uses from the
  /// marked value. For the move operator though we will want this to be the
  /// base address that we are checking which should be the operand of the mark
  /// must check value.
  SILValue getRootAddress() const { return markedValue; }
};

} // end anonymous namespace

// Filter out recognized uses that do not write to memory.
//
// TODO: Ensure that all of the conditional-write logic below is encapsulated in
// mayWriteToMemory and just call that instead. Possibly add additional
// verification that visitAccessPathUses recognizes all instructions that may
// propagate pointers (even though they don't write).
bool GatherUsesVisitor::visitUse(Operand *op, AccessUseType useTy) {
  // If this operand is for a dependent type, then it does not actually access
  // the operand's address value. It only uses the metatype defined by the
  // operation (e.g. open_existential).
  if (op->isTypeDependent()) {
    return true;
  }

  // We don't care about debug instructions.
  if (op->getUser()->isDebugInstruction())
    return true;

  // For convenience, grab the user of op.
  auto *user = op->getUser();

  // First check if we have init/reinit. These are quick/simple.
  if (::memInstMustInitialize(op)) {
    LLVM_DEBUG(llvm::dbgs() << "Found init: " << *user);

    // TODO: What about copy_addr of itself. We really should just pre-process
    // those maybe.
    auto leafRange = TypeTreeLeafTypeRange::get(op->get(), getRootAddress());
    if (!leafRange)
      return false;

    assert(!useState.initInsts.count(user));
    useState.initInsts.insert({user, *leafRange});
    return true;
  }

  if (::memInstMustReinitialize(op)) {
    LLVM_DEBUG(llvm::dbgs() << "Found reinit: " << *user);
    assert(!useState.reinitInsts.count(user));
    auto leafRange = TypeTreeLeafTypeRange::get(op->get(), getRootAddress());
    if (!leafRange)
      return false;
    useState.reinitInsts.insert({user, *leafRange});
    return true;
  }

  // Then handle destroy_addr specially. We want to as part of our dataflow to
  // ignore destroy_addr, so we need to track it separately from other uses.
  if (auto *dvi = dyn_cast<DestroyAddrInst>(user)) {
    // If we see a destroy_addr not on our base address, bail! Just error and
    // say that we do not understand the code.
    if (dvi->getOperand() != useState.address) {
      LLVM_DEBUG(llvm::dbgs()
                 << "!!! Error! Found destroy_addr no on base address: "
                 << *useState.address << "destroy: " << *dvi);
      return false;
    }
    LLVM_DEBUG(llvm::dbgs() << "Found destroy_addr: " << *dvi);
    useState.destroys.insert(dvi);
    return true;
  }

  // Ignore dealloc_stack.
  if (isa<DeallocStackInst>(user))
    return true;

  // Ignore end_access.
  if (isa<EndAccessInst>(user))
    return true;

  // At this point, we have handled all of the non-loadTakeOrCopy/consuming
  // uses.
  if (auto *copyAddr = dyn_cast<CopyAddrInst>(user)) {
    if (markedValue->getCheckKind() == MarkMustCheckInst::CheckKind::NoCopy) {
      LLVM_DEBUG(llvm::dbgs()
                 << "Found mark must check [nocopy] error: " << *user);
      diagnosticEmitter.emitAddressDiagnosticNoCopy(markedValue, copyAddr);
      moveChecker.valuesWithDiagnostics.insert(markedValue);
      emittedEarlyDiagnostic = true;
      return true;
    }

    auto leafRange = TypeTreeLeafTypeRange::get(op->get(), getRootAddress());
    if (!leafRange)
      return false;

    LLVM_DEBUG(llvm::dbgs() << "Found take: " << *user);
    useState.takeInsts.insert({user, *leafRange});
    return true;
  }

  // Then find load [copy], load [take] that are really takes since we need
  // copies for the loaded value. If we find that we need copies at that level
  // (due to e.x.: multiple consuming uses), we emit an error and bail. This
  // ensures that later on, we can assume that all of our load [take], load
  // [copy] actually follow move semantics at the object level and thus are
  // viewed as a consume requiring a copy. This is important since SILGen often
  // emits code of this form and we need to recognize it as a copy of the
  // underlying var.
  if (auto *li = dyn_cast<LoadInst>(user)) {
    if (li->getOwnershipQualifier() == LoadOwnershipQualifier::Copy ||
        li->getOwnershipQualifier() == LoadOwnershipQualifier::Take) {
      LLVM_DEBUG(llvm::dbgs() << "Found load: " << *li);
      SWIFT_DEFER {
        moveChecker.consumingUsesNeedingCopy.clear();
        moveChecker.finalConsumingUses.clear();
      };

      // Canonicalize the lifetime of the load [take], load [copy].
      moveChecker.changed |=
          moveChecker.canonicalizer.canonicalizeValueLifetime(li);

      // If we are asked to perform guaranteed checking, emit an error if we
      // have /any/ consuming uses. This is a case that can always be converted
      // to a load_borrow if we pass the check.
      if (markedValue->getCheckKind() == MarkMustCheckInst::CheckKind::NoCopy) {
        if (!moveChecker.consumingUsesNeedingCopy.empty() ||
            !moveChecker.finalConsumingUses.empty()) {
          LLVM_DEBUG(llvm::dbgs()
                     << "Found mark must check [nocopy] error: " << *user);
          moveChecker.emitObjectDiagnostic(markedValue,
                                           true /*original value guaranteed*/);
          emittedEarlyDiagnostic = true;
          moveChecker.valuesWithDiagnostics.insert(markedValue);
          return true;
        }

        // If set, this will tell the checker that we can change this load into
        // a load_borrow.
        auto leafRange =
            TypeTreeLeafTypeRange::get(op->get(), getRootAddress());
        if (!leafRange)
          return false;

        LLVM_DEBUG(llvm::dbgs() << "Found potential borrow: " << *user);
        useState.borrows.insert({user, *leafRange});
        return true;
      }

      // First check if we had any consuming uses that actually needed a
      // copy. This will always be an error and we allow the user to recompile
      // and eliminate the error. This just allows us to rely on invariants
      // later.
      if (!moveChecker.consumingUsesNeedingCopy.empty()) {
        LLVM_DEBUG(llvm::dbgs()
                   << "Found that load at object level requires copies!\n");
        // If we failed to understand how to perform the check or did not find
        // any targets... continue. In the former case we want to fail with a
        // checker did not understand diagnostic later and in the former, we
        // succeeded.
        // Otherwise, emit the diagnostic.
        moveChecker.emitObjectDiagnostic(markedValue,
                                         false /*original value guaranteed*/);
        emittedEarlyDiagnostic = true;
        moveChecker.valuesWithDiagnostics.insert(markedValue);
        LLVM_DEBUG(llvm::dbgs() << "Emitted early object level diagnostic.\n");
        return true;
      }

      // Then if we had any final consuming uses, mark that this liveness use is
      // a take and if not, mark this as a borrow.
      auto leafRange = TypeTreeLeafTypeRange::get(op->get(), getRootAddress());
      if (!leafRange)
        return false;

      if (moveChecker.finalConsumingUses.empty()) {
        LLVM_DEBUG(llvm::dbgs() << "Found borrow inst: " << *user);
        useState.borrows.insert({user, *leafRange});
      } else {
        LLVM_DEBUG(llvm::dbgs() << "Found take inst: " << *user);
        useState.takeInsts.insert({user, *leafRange});
      }
      return true;
    }
  }

  // Now that we have handled or loadTakeOrCopy, we need to now track our Takes.
  if (::memInstMustConsume(op)) {
    auto leafRange = TypeTreeLeafTypeRange::get(op->get(), getRootAddress());
    if (!leafRange)
      return false;
    LLVM_DEBUG(llvm::dbgs() << "Pure consuming use: " << *user);
    useState.takeInsts.insert({user, *leafRange});
    return true;
  }

  if (auto fas = FullApplySite::isa(op->getUser())) {
    switch (fas.getArgumentConvention(*op)) {
    case SILArgumentConvention::Indirect_In_Guaranteed: {
      auto leafRange = TypeTreeLeafTypeRange::get(op->get(), getRootAddress());
      if (!leafRange)
        return false;

      useState.livenessUses.insert({user, *leafRange});
      return true;
    }

    case SILArgumentConvention::Indirect_Inout:
    case SILArgumentConvention::Indirect_InoutAliasable:
    case SILArgumentConvention::Indirect_In:
    case SILArgumentConvention::Indirect_In_Constant:
    case SILArgumentConvention::Indirect_Out:
    case SILArgumentConvention::Direct_Unowned:
    case SILArgumentConvention::Direct_Owned:
    case SILArgumentConvention::Direct_Guaranteed:
      break;
    }
  }

  // If we don't fit into any of those categories, just track as a liveness
  // use. We assume all such uses must only be reads to the memory. So we assert
  // to be careful.
  auto leafRange = TypeTreeLeafTypeRange::get(op->get(), getRootAddress());
  if (!leafRange)
    return false;

  LLVM_DEBUG(llvm::dbgs() << "Found liveness use: " << *user);
#ifndef NDEBUG
  if (user->mayWriteToMemory()) {
    llvm::errs() << "Found a write classified as a liveness use?!\n";
    llvm::errs() << "Use: " << *user;
    llvm_unreachable("standard failure");
  }
#endif
  useState.livenessUses.insert({user, *leafRange});

  return true;
}

//===----------------------------------------------------------------------===//
//                       MARK: Main Pass Implementation
//===----------------------------------------------------------------------===//

void MoveOnlyChecker::searchForCandidateMarkMustChecks() {
  for (auto &block : *fn) {
    for (auto ii = block.begin(), ie = block.end(); ii != ie;) {
      auto *mmci = dyn_cast<MarkMustCheckInst>(&*ii);
      ++ii;

      if (!mmci || !mmci->hasMoveCheckerKind() || !mmci->getType().isAddress())
        continue;

      // Skip any alloc_box due to heap to stack failing on a box capture. This
      // will just cause an error.
      if (auto *pbi = dyn_cast<ProjectBoxInst>(mmci->getOperand())) {
        if (isa<AllocBoxInst>(pbi->getOperand())) {
          LLVM_DEBUG(
              llvm::dbgs()
              << "Early emitting diagnostic for unsupported alloc box!\n");
          if (mmci->getType().isMoveOnlyWrapped()) {
            diagnose(fn->getASTContext(), mmci->getLoc().getSourceLoc(),
                     diag::sil_moveonlychecker_not_understand_no_implicit_copy);
          } else {
            diagnose(fn->getASTContext(), mmci->getLoc().getSourceLoc(),
                     diag::sil_moveonlychecker_not_understand_moveonly);
          }
          valuesWithDiagnostics.insert(mmci);
          continue;
        }

        if (auto *bbi = dyn_cast<BeginBorrowInst>(pbi->getOperand())) {
          if (isa<AllocBoxInst>(bbi->getOperand())) {
            LLVM_DEBUG(
                llvm::dbgs()
                << "Early emitting diagnostic for unsupported alloc box!\n");
            if (mmci->getType().isMoveOnlyWrapped()) {
              diagnose(
                  fn->getASTContext(), mmci->getLoc().getSourceLoc(),
                  diag::sil_moveonlychecker_not_understand_no_implicit_copy);
            } else {
              diagnose(fn->getASTContext(), mmci->getLoc().getSourceLoc(),
                       diag::sil_moveonlychecker_not_understand_moveonly);
            }
            valuesWithDiagnostics.insert(mmci);
            continue;
          }
        }
      }

      moveIntroducersToProcess.insert(mmci);
    }
  }
}

void MoveOnlyChecker::emitObjectDiagnostic(MarkMustCheckInst *markedValue,
                                           bool originalValueGuaranteed) {
  auto &astContext = fn->getASTContext();
  StringRef varName = getVariableNameForValue(markedValue);
  LLVM_DEBUG(llvm::dbgs() << "Emitting error for: " << *markedValue);

  if (originalValueGuaranteed) {
    diagnose(astContext,
             markedValue->getDefiningInstruction()->getLoc().getSourceLoc(),
             diag::sil_moveonlychecker_guaranteed_value_consumed, varName);
  } else {
    diagnose(astContext,
             markedValue->getDefiningInstruction()->getLoc().getSourceLoc(),
             diag::sil_moveonlychecker_owned_value_consumed_more_than_once,
             varName);
  }

  while (consumingUsesNeedingCopy.size()) {
    auto *consumingUse = consumingUsesNeedingCopy.pop_back_val();

    LLVM_DEBUG(llvm::dbgs()
               << "Consuming use needing copy: " << *consumingUse->getUser());
    // See if the consuming use is an owned moveonly_to_copyable whose only
    // user is a return. In that case, use the return loc instead. We do this
    // b/c it is illegal to put a return value location on a non-return value
    // instruction... so we have to hack around this slightly.
    auto *user = consumingUse->getUser();
    auto loc = user->getLoc();
    if (auto *mtc = dyn_cast<MoveOnlyWrapperToCopyableValueInst>(user)) {
      if (auto *ri = mtc->getSingleUserOfType<ReturnInst>()) {
        loc = ri->getLoc();
      }
    }

    diagnose(astContext, loc.getSourceLoc(),
             diag::sil_moveonlychecker_consuming_use_here);
  }

  while (finalConsumingUses.size()) {
    auto *consumingUse = finalConsumingUses.pop_back_val();
    LLVM_DEBUG(llvm::dbgs()
               << "Final consuming use: " << *consumingUse->getUser());
    // See if the consuming use is an owned moveonly_to_copyable whose only
    // user is a return. In that case, use the return loc instead. We do this
    // b/c it is illegal to put a return value location on a non-return value
    // instruction... so we have to hack around this slightly.
    auto *user = consumingUse->getUser();
    auto loc = user->getLoc();
    if (auto *mtc = dyn_cast<MoveOnlyWrapperToCopyableValueInst>(user)) {
      if (auto *ri = mtc->getSingleUserOfType<ReturnInst>()) {
        loc = ri->getLoc();
      }
    }

    diagnose(astContext, loc.getSourceLoc(),
             diag::sil_moveonlychecker_consuming_use_here);
  }
}

bool MoveOnlyChecker::performSingleCheck(MarkMustCheckInst *markedAddress) {
  SWIFT_DEFER { diagnosticEmitter.clear(); };

  auto accessPathWithBase = AccessPathWithBase::compute(markedAddress);
  auto accessPath = accessPathWithBase.accessPath;
  if (!accessPath.isValid()) {
    LLVM_DEBUG(llvm::dbgs() << "Invalid access path: " << *markedAddress);
    return false;
  }

  // Then gather all uses of our address by walking from def->uses. We use this
  // to categorize the uses of this address into their ownership behavior (e.x.:
  // init, reinit, take, destroy, etc.).
  GatherUsesVisitor visitor(*this, addressUseState, markedAddress,
                            diagnosticEmitter);
  SWIFT_DEFER { visitor.clear(); };
  visitor.reset(markedAddress);
  if (!visitAccessPathUses(visitor, accessPath, fn)) {
    LLVM_DEBUG(llvm::dbgs() << "Failed access path visit: " << *markedAddress);
    return false;
  }

  // If we found a load [copy] or copy_addr that requires multiple copies, then
  // we emitted an early error. Bail now and allow the user to fix those errors
  // and recompile to get further errors.
  //
  // DISCUSSION: The reason why we do this is in the dataflow below we want to
  // be able to assume that the load [copy] or copy_addr/copy_addr [init] are
  // actual last uses, but the frontend that emitted the code for simplicity
  // emitted a copy from the base address + a destroy_addr of the use. By
  // bailing here, we can make that assumption since we would have errored
  // earlier otherwise.
  if (visitor.emittedEarlyDiagnostic)
    return true;

  SmallVector<SILBasicBlock *, 32> discoveredBlocks;
  FieldSensitiveAddressPrunedLiveness liveness(fn, markedAddress,
                                               &discoveredBlocks);

  // Now walk all of the blocks in the function... performing the single basic
  // block version of the algorithm and initializing our pruned liveness for our
  // interprocedural processing.
  using InstLeafTypePair = std::pair<SILInstruction *, TypeTreeLeafTypeRange>;
  using InstOptionalLeafTypePair =
      std::pair<SILInstruction *, Optional<TypeTreeLeafTypeRange>>;
  SmallVector<InstLeafTypePair, 32> initDownInsts;
  SmallVector<InstLeafTypePair, 32> takeDownInsts;
  SmallVector<InstLeafTypePair, 32> takeUpInsts;
  SmallVector<InstLeafTypePair, 32> livenessUpInsts;
  BlockState::Map blockToState;

  bool isInOut = false;
  if (auto *fArg = dyn_cast<SILFunctionArgument>(markedAddress->getOperand()))
    isInOut |= fArg->getArgumentConvention().isInoutConvention();

  LLVM_DEBUG(llvm::dbgs() << "Performing single basic block checks!\n");
  for (auto &block : *fn) {
    LLVM_DEBUG(llvm::dbgs()
               << "Visiting block: bb" << block.getDebugID() << "\n");
    // Then walk backwards from the bottom of the block to the top, initializing
    // its state, handling any completely in block diagnostics.
    InstOptionalLeafTypePair takeUp = {nullptr, {}};
    InstOptionalLeafTypePair firstTake = {nullptr, {}};
    InstOptionalLeafTypePair firstInit = {nullptr, {}};
    InstOptionalLeafTypePair livenessUp = {nullptr, {}};
    bool foundFirstNonLivenessUse = false;
    auto *term = block.getTerminator();
    bool isExitBlock = term->isFunctionExiting();
    for (auto &inst : llvm::reverse(block)) {
      if (isExitBlock && isInOut && &inst == term) {
        LLVM_DEBUG(llvm::dbgs()
                   << "    Found inout term liveness user: " << inst);
        livenessUp = {&inst, TypeTreeLeafTypeRange(markedAddress)};
        continue;
      }

      {
        auto iter = addressUseState.takeInsts.find(&inst);
        if (iter != addressUseState.takeInsts.end()) {
          SWIFT_DEFER { foundFirstNonLivenessUse = true; };

          // If we are not yet tracking a "take up" or a "liveness up", then we
          // can update our state. In those other two cases we emit an error
          // diagnostic below.
          if (!takeUp.first && !livenessUp.first) {
            LLVM_DEBUG(llvm::dbgs()
                       << "    Tracking new take up: " << *iter->first);
            if (!foundFirstNonLivenessUse) {
              firstTake = {iter->first, iter->second};
            }
            takeUp = {iter->first, iter->second};
            continue;
          }

          bool emittedSomeDiagnostic = false;
          if (takeUp.first) {
            LLVM_DEBUG(llvm::dbgs()
                       << "    Found two takes, emitting error!\n");
            LLVM_DEBUG(llvm::dbgs() << "    First take: " << *takeUp.first);
            LLVM_DEBUG(llvm::dbgs() << "    Second take: " << inst);
            diagnosticEmitter.emitAddressDiagnostic(
                markedAddress, takeUp.first, &inst, true /*is consuming*/);
            emittedSomeDiagnostic = true;
          }

          if (livenessUp.first) {
            if (livenessUp.first == term && isInOut) {
              LLVM_DEBUG(llvm::dbgs()
                         << "    Found liveness inout error: " << inst);
              // Even though we emit a diagnostic for inout here, we actually
              // want to no longer track the inout liveness use and instead want
              // to track the consuming use so that earlier errors are on the
              // take and not on the inout. This is to ensure that we only emit
              // a single inout not reinitialized before end of function error
              // if we have multiple consumes along that path.
              diagnosticEmitter.emitInOutEndOfFunctionDiagnostic(markedAddress,
                                                                 &inst);
              livenessUp = {nullptr, {}};
              takeUp = {iter->first, iter->second};
            } else {
              LLVM_DEBUG(llvm::dbgs() << "    Found liveness error: " << inst);
              diagnosticEmitter.emitAddressDiagnostic(
                  markedAddress, livenessUp.first, &inst,
                  false /*is not consuming*/);
            }
            emittedSomeDiagnostic = true;
          }

          (void)emittedSomeDiagnostic;
          assert(emittedSomeDiagnostic);
          valuesWithDiagnostics.insert(markedAddress);
          continue;
        }
      }

      {
        auto iter = addressUseState.livenessUses.find(&inst);
        if (iter != addressUseState.livenessUses.end()) {
          if (!livenessUp.first) {
            LLVM_DEBUG(llvm::dbgs()
                       << "    Found liveness use! Propagating liveness. Inst: "
                       << inst);
            livenessUp = {iter->first, iter->second};
          } else {
            LLVM_DEBUG(
                llvm::dbgs()
                << "    Found liveness use! Already tracking liveness. Inst: "
                << inst);
          }
          continue;
        }
      }

      // Just treat borrows at this point as liveness requiring.
      {
        auto iter = addressUseState.borrows.find(&inst);
        if (iter != addressUseState.borrows.end()) {
          if (!livenessUp.first) {
            LLVM_DEBUG(llvm::dbgs()
                       << "    Found borrowed use! Propagating liveness. Inst: "
                       << inst);
            livenessUp = {iter->first, iter->second};
          } else {
            LLVM_DEBUG(
                llvm::dbgs()
                << "    Found borrowed use! Already tracking liveness. Inst: "
                << inst);
          }
          continue;
        }
      }

      // If we have an init, then unset previous take up and liveness up.
      {
        auto iter = addressUseState.initInsts.find(&inst);
        if (iter != addressUseState.initInsts.end()) {
          takeUp = {nullptr, {}};
          livenessUp = {nullptr, {}};
          if (!foundFirstNonLivenessUse) {
            LLVM_DEBUG(llvm::dbgs() << "    Updated with new init: " << inst);
            firstInit = {iter->first, iter->second};
          } else {
            LLVM_DEBUG(
                llvm::dbgs()
                << "    Found init! Already have first non liveness use. Inst: "
                << inst);
          }
          foundFirstNonLivenessUse = true;
          continue;
        }
      }

      // We treat the reinit a reinit as only a kill and not an additional
      // take. This is because, we are treating their destroy as a last use like
      // destroy_addr. The hope is that as part of doing fixups, we can just
      // change this to a store [init] if there isn't an earlier reachable take
      // use, like we do with destroy_addr.
      {
        auto iter = addressUseState.reinitInsts.find(&inst);
        if (iter != addressUseState.reinitInsts.end()) {
          takeUp = {nullptr, {}};
          livenessUp = {nullptr, {}};
          if (!foundFirstNonLivenessUse) {
            LLVM_DEBUG(llvm::dbgs() << "    Updated with new reinit: " << inst);
            firstInit = {iter->first, iter->second};
          } else {
            LLVM_DEBUG(llvm::dbgs() << "    Found new reinit, but already "
                                       "tracking a liveness init. Inst: "
                                    << inst);
          }
          foundFirstNonLivenessUse = true;
          continue;
        }
      }
    }

    LLVM_DEBUG(llvm::dbgs() << "End of block. Dumping Results!\n");
    if (firstInit.first) {
      LLVM_DEBUG(llvm::dbgs() << "    First Init Down: " << *firstInit.first);
      initDownInsts.emplace_back(firstInit.first, *firstInit.second);
    } else {
      LLVM_DEBUG(llvm::dbgs() << "    No Init Down!\n");
    }

    // At this point we want to begin mapping blocks to "first" users.
    if (takeUp.first) {
      LLVM_DEBUG(llvm::dbgs() << "Take Up: " << *takeUp.first);
      blockToState.try_emplace(&block,
                               BlockState(takeUp.first, firstInit.first));
      takeUpInsts.emplace_back(takeUp.first, *takeUp.second);
    } else {
      LLVM_DEBUG(llvm::dbgs() << "    No Take Up!\n");
    }

    if (livenessUp.first) {
      // This try_emplace fail if we already above initialized blockToState
      // above in the previous if block.
      blockToState.try_emplace(&block,
                               BlockState(livenessUp.first, firstInit.first));
      LLVM_DEBUG(llvm::dbgs() << "Liveness Up: " << *livenessUp.first);
      livenessUpInsts.emplace_back(livenessUp.first, *livenessUp.second);
    } else {
      LLVM_DEBUG(llvm::dbgs() << "    No Liveness Up!\n");
    }

    if (firstTake.first) {
      LLVM_DEBUG(llvm::dbgs() << "    First Take Down: " << *firstTake.first);
      takeDownInsts.emplace_back(firstTake.first, *firstTake.second);
      // We only emplace if we didn't already have a takeUp above. In such a
      // case, the try_emplace fails.
      blockToState.try_emplace(&block, BlockState(nullptr, false));
    } else {
      LLVM_DEBUG(llvm::dbgs() << "    No Take Down!\n");
    }
  }

  // If we emitted a diagnostic while performing the single block check, just
  // bail early and let the user fix these issues in this functionand re-run the
  // compiler.
  //
  // DISCUSSION: This ensures that later when performing the global dataflow, we
  // can rely on the invariant that any potential single block cases are correct
  // already.
  if (valuesWithDiagnostics.size())
    return true;

  //---
  // Multi-Block Liveness Dataflow
  //

  // At this point, we have handled all of the single block cases and have
  // simplified the remaining cases to global cases that we compute using
  // liveness. We begin by using all of our init down blocks as def blocks.
  for (auto initInstAndValue : initDownInsts)
    liveness.initializeDefBlock(initInstAndValue.first->getParent(),
                                initInstAndValue.second);

  // Then add all of the takes that we saw propagated up to the top of our
  // block. Since we have done this for all of our defs
  for (auto takeInstAndValue : takeUpInsts)
    liveness.updateForUse(takeInstAndValue.first, takeInstAndValue.second,
                          true /*lifetime ending*/);
  // Do the same for our borrow and liveness insts.
  for (auto livenessInstAndValue : livenessUpInsts)
    liveness.updateForUse(livenessInstAndValue.first,
                          livenessInstAndValue.second,
                          false /*lifetime ending*/);

  // Finally, if we have an inout argument, add a liveness use of the entire
  // value on terminators in blocks that are exits from the function. This
  // ensures that along all paths, if our inout is not reinitialized before we
  // exit the function, we will get an error. We also stash these users into
  // inoutTermUser so we can quickly recognize them later and emit a better
  // error msg.
  SmallPtrSet<SILInstruction *, 8> inoutTermUser;
  if (auto *fArg = dyn_cast<SILFunctionArgument>(markedAddress->getOperand())) {
    if (fArg->getArgumentConvention() ==
        SILArgumentConvention::Indirect_Inout) {
      SmallVector<SILBasicBlock *, 8> exitBlocks;
      markedAddress->getFunction()->findExitingBlocks(exitBlocks);
      for (auto *block : exitBlocks) {
        inoutTermUser.insert(block->getTerminator());
        liveness.updateForUse(block->getTerminator(),
                              TypeTreeLeafTypeRange(markedAddress),
                              false /*lifetime ending*/);
      }
    }
  }

  // If we have multiple blocks in the function, now run the global pruned
  // liveness dataflow.
  if (std::next(fn->begin()) != fn->end()) {
    // Then compute the takes that are within the cumulative boundary of
    // liveness that we have computed. If we find any, they are the errors ones.
    LivenessChecker emitter(markedAddress, liveness, inoutTermUser,
                            blockToState, diagnosticEmitter);

    // If we had any errors, we do not want to modify the SIL... just bail.
    if (emitter.compute(takeUpInsts, takeDownInsts)) {
      // TODO: Remove next line.
      valuesWithDiagnostics.insert(markedAddress);
      return true;
    }
  }

  // Now before we do anything more, just exit and say we errored. This will not
  // actually make us fail and will cause code later in the checker to rewrite
  // all non-explicit copy instructions to their explicit variant. I did this
  // for testing purposes only.
  valuesWithDiagnostics.insert(markedAddress);
  return true;

  // Ok, we not have emitted our main errors. Now we begin the
  // transformation. We begin by processing borrows. We can also emit errors
  // here if we find that we can not expand the borrow scope of the load [copy]
  // to all of its uses.
  SmallVector<DestroyValueInst *, 8> destroys;
  SmallVector<EndAccessInst *, 8> endAccesses;
  for (auto pair : addressUseState.borrows) {
    if (auto *li = dyn_cast<LoadInst>(pair.first)) {
      if (li->getOwnershipQualifier() == LoadOwnershipQualifier::Copy) {
        // If we had a load [copy], borrow then we know that all of its destroys
        // must have been destroy_value. So we can just gather up those
        // destroy_value and use then to create a new load_borrow scope.
        SILBuilderWithScope builder(li);
        SILValue base = stripAccessMarkers(li->getOperand());
        bool foundAccess = base != li->getOperand();

        // Insert a new access scope.
        SILValue newBase = base;
        if (foundAccess) {
          newBase = builder.createBeginAccess(
              li->getLoc(), base, SILAccessKind::Read,
              SILAccessEnforcement::Static, false /*no nested conflict*/,
              false /*is from builtin*/);
        }
        auto *lbi = builder.createLoadBorrow(li->getLoc(), newBase);

        for (auto *consumeUse : li->getConsumingUses()) {
          auto *dvi = cast<DestroyValueInst>(consumeUse->getUser());
          SILBuilderWithScope destroyBuilder(dvi);
          destroyBuilder.createEndBorrow(dvi->getLoc(), lbi);
          if (foundAccess)
            destroyBuilder.createEndAccess(dvi->getLoc(), newBase,
                                           false /*aborted*/);
          destroys.push_back(dvi);
          changed = true;
        }

        // TODO: We need to check if this is the only use.
        if (auto *access = dyn_cast<BeginAccessInst>(li->getOperand())) {
          bool foundUnknownUse = false;
          for (auto *accUse : access->getUses()) {
            if (accUse->getUser() == li)
              continue;
            if (auto *ea = dyn_cast<EndAccessInst>(accUse->getUser())) {
              endAccesses.push_back(ea);
              continue;
            }
            foundUnknownUse = true;
          }
          if (!foundUnknownUse) {
            for (auto *end : endAccesses)
              end->eraseFromParent();
            access->replaceAllUsesWithUndef();
            access->eraseFromParent();
          }
        }

        for (auto *d : destroys)
          d->eraseFromParent();
        li->replaceAllUsesWith(lbi);
        li->eraseFromParent();
        continue;
      }
    }

    llvm_unreachable("Unhandled case?!");
  }

  // Now that we have rewritten all read only copies into borrows, we need to
  // handle merging of copyable takes/destroys. Begin by pairing destroy_addr
  // and copyableTake.
  for (auto *destroy : addressUseState.destroys) {
    // First if we aren't the first instruction in the block, see if we have a
    // copyableToTake that pairs with us. If we are the first instruction in the
    // block, then we just go straight to the global dataflow stage below.
    if (destroy->getIterator() != destroy->getParent()->begin()) {
      bool foundSingleBlockCase = false;
      for (auto ii = std::prev(destroy->getReverseIterator()),
                ie = destroy->getParent()->rend();
           ii != ie; ++ii) {
        if (addressUseState.initInsts.count(&*ii) ||
            addressUseState.reinitInsts.count(&*ii))
          break;

        if (addressUseState.takeInsts.count(&*ii)) {
          if (auto *li = dyn_cast<LoadInst>(&*ii)) {
            if (li->getOwnershipQualifier() == LoadOwnershipQualifier::Copy) {
              // If we have a copy in the single block case, erase the destroy
              // and visit the next one.
              destroy->eraseFromParent();
              changed = true;
              foundSingleBlockCase = true;
              break;
            }
            llvm_unreachable("Error?! This is a double destroy?!");
          }
        }

        if (addressUseState.livenessUses.count(&*ii)) {
          // If we have a liveness use, we break since this destroy_addr is
          // needed along this path.
          break;
        }
      }

      if (foundSingleBlockCase)
        continue;
    }
  }

  // Now that we have rewritten all borrows, convert any takes that are today
  // copies into their take form. If we couldn't do this, we would have had an
  // error.
  for (auto take : addressUseState.takeInsts) {
    if (auto *li = dyn_cast<LoadInst>(take.first)) {
      switch (li->getOwnershipQualifier()) {
      case LoadOwnershipQualifier::Unqualified:
      case LoadOwnershipQualifier::Trivial:
        llvm_unreachable("Should never hit this");
      case LoadOwnershipQualifier::Take:
        // This is already correct.
        continue;
      case LoadOwnershipQualifier::Copy:
        // Convert this to its take form.
        if (auto *access = dyn_cast<BeginAccessInst>(li->getOperand()))
          access->setAccessKind(SILAccessKind::Modify);
        li->setOwnershipQualifier(LoadOwnershipQualifier::Take);
        changed = true;
        continue;
      }
    }

    llvm_unreachable("Unhandled case?!");
  }

  return true;
}

bool MoveOnlyChecker::check() {
  // First search for candidates to process and emit diagnostics on any
  // mark_must_check [noimplicitcopy] we didn't recognize.
  searchForCandidateMarkMustChecks();

  // If we didn't find any introducers to check, just return changed.
  //
  // NOTE: changed /can/ be true here if we had any mark_must_check
  // [noimplicitcopy] that we didn't understand and emitting a diagnostic upon
  // and then deleting.
  if (moveIntroducersToProcess.empty())
    return changed;

  for (auto *markedValue : moveIntroducersToProcess) {
    LLVM_DEBUG(llvm::dbgs() << "Visiting: " << *markedValue);

    // Perform our address check.
    if (!performSingleCheck(markedValue)) {
      LLVM_DEBUG(llvm::dbgs()
                 << "Failed to perform single check! Emitting error!\n");
      // If we fail the address check in some way, set the diagnose!
      if (markedValue->getType().isMoveOnlyWrapped()) {
        diagnose(fn->getASTContext(), markedValue->getLoc().getSourceLoc(),
                 diag::sil_moveonlychecker_not_understand_no_implicit_copy);
      } else {
        diagnose(fn->getASTContext(), markedValue->getLoc().getSourceLoc(),
                 diag::sil_moveonlychecker_not_understand_moveonly);
      }
      valuesWithDiagnostics.insert(markedValue);
    }
  }
  bool emittedDiagnostic = valuesWithDiagnostics.size();

  // Ok, now that we have performed our checks, we need to eliminate all mark
  // must check inst since it is invalid for these to be in canonical SIL and
  // our work is done here.
  while (!moveIntroducersToProcess.empty()) {
    auto *markedInst = moveIntroducersToProcess.pop_back_val();
    markedInst->replaceAllUsesWith(markedInst->getOperand());
    markedInst->eraseFromParent();
    changed = true;
  }

  // Once we have finished processing, if we emitted any diagnostics, then we
  // may have copy_addr [init], load [copy] of @moveOnly typed values. This is
  // not valid in Canonical SIL, so we need to ensure that those copy_value
  // become explicit_copy_value. This is ok to do since we are already going to
  // fail the compilation and just are trying to maintain SIL invariants.
  //
  // It is also ok that we use a little more compile time and go over the
  // function again, since we are going to fail the compilation and not codegen.
  if (emittedDiagnostic) {
    for (auto &block : *fn) {
      for (auto ii = block.begin(), ie = block.end(); ii != ie;) {
        auto *inst = &*ii;
        ++ii;

        // Convert load [copy] -> load_borrow + explicit_copy_value.
        if (auto *li = dyn_cast<LoadInst>(inst)) {
          if (li->getOwnershipQualifier() == LoadOwnershipQualifier::Copy) {
            SILBuilderWithScope builder(li);
            auto *lbi =
                builder.createLoadBorrow(li->getLoc(), li->getOperand());
            auto *cvi = builder.createExplicitCopyValue(li->getLoc(), lbi);
            builder.createEndBorrow(li->getLoc(), lbi);
            li->replaceAllUsesWith(cvi);
            li->eraseFromParent();
            changed = true;
          }
        }

        // Convert copy_addr !take of src to its explicit value form so we don't
        // error.
        if (auto *copyAddr = dyn_cast<CopyAddrInst>(inst)) {
          if (!copyAddr->isTakeOfSrc()) {
            SILBuilderWithScope builder(copyAddr);
            builder.createExplicitCopyAddr(
                copyAddr->getLoc(), copyAddr->getSrc(), copyAddr->getDest(),
                IsTake_t(copyAddr->isTakeOfSrc()),
                IsInitialization_t(copyAddr->isInitializationOfDest()));
            copyAddr->eraseFromParent();
            changed = true;
          }
        }
      }
    }
  }

  return changed;
}

//===----------------------------------------------------------------------===//
//                            Top Level Entrypoint
//===----------------------------------------------------------------------===//

namespace {

class MoveOnlyCheckerPass : public SILFunctionTransform {
  void run() override {
    auto *fn = getFunction();

    // Don't rerun diagnostics on deserialized functions.
    if (getFunction()->wasDeserializedCanonical())
      return;

    assert(fn->getModule().getStage() == SILStage::Raw &&
           "Should only run on Raw SIL");
    LLVM_DEBUG(llvm::dbgs() << "===> MoveOnly Addr Checker. Visiting: "
                            << fn->getName() << '\n');
    auto *accessBlockAnalysis = getAnalysis<NonLocalAccessBlockAnalysis>();
    auto *dominanceAnalysis = getAnalysis<DominanceAnalysis>();
    auto *postOrderAnalysis = getAnalysis<PostOrderAnalysis>();
    DominanceInfo *domTree = dominanceAnalysis->get(fn);
    auto *deAnalysis = getAnalysis<DeadEndBlocksAnalysis>()->get(fn);

    if (MoveOnlyChecker(getFunction(), deAnalysis, accessBlockAnalysis, domTree,
                        postOrderAnalysis)
            .check()) {
      invalidateAnalysis(SILAnalysis::InvalidationKind::Instructions);
    }
  }
};

} // anonymous namespace

SILTransform *swift::createMoveOnlyAddressChecker() {
  return new MoveOnlyCheckerPass();
}
