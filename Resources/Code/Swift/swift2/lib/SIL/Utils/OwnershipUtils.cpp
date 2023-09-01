//===--- OwnershipUtils.cpp -----------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/SIL/OwnershipUtils.h"
#include "swift/Basic/Defer.h"
#include "swift/Basic/GraphNodeWorklist.h"
#include "swift/Basic/SmallPtrSetVector.h"
#include "swift/SIL/InstructionUtils.h"
#include "swift/SIL/LinearLifetimeChecker.h"
#include "swift/SIL/MemAccessUtils.h"
#include "swift/SIL/Projection.h"
#include "swift/SIL/PrunedLiveness.h"
#include "swift/SIL/SILArgument.h"
#include "swift/SIL/SILBuilder.h"
#include "swift/SIL/SILInstruction.h"

using namespace swift;

bool swift::hasPointerEscape(BorrowedValue value) {
  assert(value.kind == BorrowedValueKind::BeginBorrow ||
         value.kind == BorrowedValueKind::LoadBorrow);
  GraphNodeWorklist<Operand *, 8> worklist;
  for (Operand *use : value->getUses()) {
    if (use->getOperandOwnership() != OperandOwnership::NonUse)
      worklist.insert(use);
  }

  while (Operand *op = worklist.pop()) {
    switch (op->getOperandOwnership()) {
    case OperandOwnership::NonUse:
    case OperandOwnership::TrivialUse:
    case OperandOwnership::ForwardingConsume:
    case OperandOwnership::DestroyingConsume:
      llvm_unreachable("this operand cannot handle an inner guaranteed use");

    case OperandOwnership::ForwardingUnowned:
    case OperandOwnership::PointerEscape:
      return true;

    case OperandOwnership::Borrow:
    case OperandOwnership::EndBorrow:
    case OperandOwnership::InstantaneousUse:
    case OperandOwnership::UnownedInstantaneousUse:
    case OperandOwnership::InteriorPointer:
    case OperandOwnership::BitwiseEscape:
      break;

    case OperandOwnership::GuaranteedForwardingPhi:
    case OperandOwnership::Reborrow: {
      SILArgument *phi = cast<BranchInst>(op->getUser())
                             ->getDestBB()
                             ->getArgument(op->getOperandNumber());
      for (auto *use : phi->getUses()) {
        if (use->getOperandOwnership() != OperandOwnership::NonUse)
          worklist.insert(use);
      }
      break;
    }
    case OperandOwnership::GuaranteedForwarding: {
      ForwardingOperand(op).visitForwardedValues([&](SILValue result) {
        // Do not include transitive uses with 'none' ownership
        if (result->getOwnershipKind() == OwnershipKind::None)
          return true;
        for (auto *resultUse : result->getUses()) {
          if (resultUse->getOperandOwnership() != OperandOwnership::NonUse) {
            worklist.insert(resultUse);
          }
        }
        return true;
      });
      break;
    }
    }
  }
  return false;
}

bool swift::canOpcodeForwardGuaranteedValues(SILValue value) {
  // If we have an argument from a transforming terminator, we can forward
  // guaranteed.
  if (auto *arg = dyn_cast<SILArgument>(value))
    if (auto *ti = arg->getSingleTerminator())
      if (ti->isTransformationTerminator())
        return OwnershipForwardingMixin::get(ti)->preservesOwnership();

  if (auto *inst = value->getDefiningInstruction())
    if (auto *mixin = OwnershipForwardingMixin::get(inst))
      return mixin->preservesOwnership() &&
             !isa<OwnedFirstArgForwardingSingleValueInst>(inst);

  return false;
}

bool swift::canOpcodeForwardGuaranteedValues(Operand *use) {
  if (auto *mixin = OwnershipForwardingMixin::get(use->getUser()))
    return mixin->preservesOwnership() &&
           !isa<OwnedFirstArgForwardingSingleValueInst>(use->getUser());
  return false;
}

bool swift::canOpcodeForwardOwnedValues(SILValue value) {
  // If we have a SILArgument and we are the successor block of a transforming
  // terminator, we are fine.
  if (auto *arg = dyn_cast<SILPhiArgument>(value))
    if (auto *predTerm = arg->getSingleTerminator())
      if (predTerm->isTransformationTerminator())
        return OwnershipForwardingMixin::get(predTerm)->preservesOwnership();

  if (auto *inst = value->getDefiningInstruction())
    if (auto *mixin = OwnershipForwardingMixin::get(inst))
      return mixin->preservesOwnership() &&
             !isa<GuaranteedFirstArgForwardingSingleValueInst>(inst);

  return false;
}

bool swift::canOpcodeForwardOwnedValues(Operand *use) {
  auto *user = use->getUser();
  if (auto *mixin = OwnershipForwardingMixin::get(user))
    return mixin->preservesOwnership() &&
           !isa<GuaranteedFirstArgForwardingSingleValueInst>(user);
  return false;
}

//===----------------------------------------------------------------------===//
//                 Guaranteed Use-Point (Lifetime) Discovery
//===----------------------------------------------------------------------===//

// Find all use points of \p guaranteedValue within its borrow scope. All uses
// are naturally dominated by \p guaranteedValue. If a PointerEscape is found,
// then no assumption can be made about \p guaranteedValue's lifetime. Therefore
// the use points are incomplete and this returns false.
//
// Accumulate results in \p usePoints, ignoring existing elements.
//
// Skip over nested borrow scopes. Their scope-ending instructions are their use
// points. Transitively find all nested scope-ending instructions by looking
// through nested reborrows. Nested reborrows are not use points.
//
// FIXME: handle inner reborrows, which aren't dominated by
// guaranteedValue. Audit all users to handle reborrows.
bool swift::findInnerTransitiveGuaranteedUses(
  SILValue guaranteedValue, SmallVectorImpl<Operand *> *usePoints) {

  bool foundPointerEscape = false;

  auto leafUse = [&](Operand *use) {
    if (usePoints && use->getOperandOwnership() != OperandOwnership::NonUse) {
      usePoints->push_back(use);
    }
    return true;
  };

  // Push the value's immediate uses.
  //
  // TODO: The worklist can be a simple vector without any a membership check if
  // destructures are changed to be represented as reborrows. Currently a
  // destructure forwards multiple results! This means that the worklist could
  // grow exponentially without the membership check. It's fine to do this
  // membership check locally in this function (within a borrow scope) because
  // it isn't needed for the immediate uses, only the transitive uses.
  GraphNodeWorklist<Operand *, 8> worklist;
  for (Operand *use : guaranteedValue->getUses()) {
    if (use->getOperandOwnership() != OperandOwnership::NonUse)
      worklist.insert(use);
  }

  // --- Transitively follow forwarded uses and look for escapes.

  // usePoints grows in this loop.
  while (Operand *use = worklist.pop()) {
    switch (use->getOperandOwnership()) {
    case OperandOwnership::NonUse:
    case OperandOwnership::TrivialUse:
    case OperandOwnership::ForwardingConsume:
    case OperandOwnership::DestroyingConsume:
      llvm_unreachable("this operand cannot handle an inner guaranteed use");

    case OperandOwnership::ForwardingUnowned:
    case OperandOwnership::PointerEscape:
      leafUse(use);
      foundPointerEscape = true;
      break;

    case OperandOwnership::InstantaneousUse:
    case OperandOwnership::UnownedInstantaneousUse:
    case OperandOwnership::BitwiseEscape:
    // Reborrow only happens when this is called on a value that creates a
    // borrow scope.
    case OperandOwnership::Reborrow:
    // EndBorrow either happens when this is called on a value that creates a
    // borrow scope, or when it is pushed as a use when processing a nested
    // borrow.
    case OperandOwnership::EndBorrow:
      leafUse(use);
      break;

    case OperandOwnership::InteriorPointer:
#if 0 // FIXME!!! Enable in a following commit that fixes RAUW
      // If our base guaranteed value does not have any consuming uses
      // (consider function arguments), we need to be sure to include interior
      // pointer operands since we may not get a use from a end_scope
      // instruction.
      if (InteriorPointerOperand(use).findTransitiveUses(usePoints)
          != AddressUseKind::NonEscaping) {
        foundPointerEscape = true;
      }
#endif
      leafUse(use);
      foundPointerEscape = true;
      break;

    case OperandOwnership::GuaranteedForwarding: {
      bool nonLeaf = false;
      ForwardingOperand(use).visitForwardedValues([&](SILValue result) {
        // Do not include transitive uses with 'none' ownership
        if (result->getOwnershipKind() == OwnershipKind::None)
          return true;
        for (auto *resultUse : result->getUses()) {
          if (resultUse->getOperandOwnership() != OperandOwnership::NonUse) {
            nonLeaf = true;
            worklist.insert(resultUse);
          }
        }
        return true;
      });
      // e.g. A dead forwarded value, e.g. a switch_enum with only trivial uses,
      // must itself be a leaf use.
      if (!nonLeaf) {
        leafUse(use);
      }
      break;
    }
    case OperandOwnership::GuaranteedForwardingPhi: {
      leafUse(use);
      foundPointerEscape = true;
      break;
    }
    case OperandOwnership::Borrow:
      // FIXME: Use visitExtendedScopeEndingUses and audit all clients to handle
      // reborrows.
      //
      // FIXME: visit[Extended]ScopeEndingUses can't return false here once dead
      // borrows are disallowed.
      if (!BorrowingOperand(use).visitScopeEndingUses([&](Operand *endUse) {
            if (endUse->getOperandOwnership() == OperandOwnership::Reborrow) {
              foundPointerEscape = true;
            }
            leafUse(endUse);
            return true;
          })) {
        // Special case for dead borrows. This is dangerous because clients
        // don't expect a begin_borrow to be in the use list.
        leafUse(use);
      }
      break;
    }
  }
  return !foundPointerEscape;
}

/// Find all uses in the extended lifetime (i.e. including copies) of a simple
/// (i.e. not reborrowed) borrow scope and its transitive uses.
bool swift::findExtendedUsesOfSimpleBorrowedValue(
    BorrowedValue borrowedValue, SmallVectorImpl<Operand *> *usePoints) {

  auto recordUse = [&](Operand *use) {
    if (usePoints && use->getOperandOwnership() != OperandOwnership::NonUse) {
      usePoints->push_back(use);
    }
  };

  // Push the value's immediate uses.
  //
  // TODO: The worklist can be a simple vector without any a membership check if
  // destructures are changed to be represented as reborrows. Currently a
  // destructure forwards multiple results! This means that the worklist could
  // grow exponentially without the membership check. It's fine to do this
  // membership check locally in this function (within a borrow scope) because
  // it isn't needed for the immediate uses, only the transitive uses.
  GraphNodeWorklist<Operand *, 8> worklist;
  auto addUsesToWorklist = [&worklist](SILValue value) {
    for (Operand *use : value->getUses()) {
      if (use->getOperandOwnership() != OperandOwnership::NonUse)
        worklist.insert(use);
    }
  };

  addUsesToWorklist(borrowedValue.value);

  // --- Transitively follow forwarded uses and look for escapes.

  // usePoints grows in this loop.
  while (Operand *use = worklist.pop()) {
    if (auto *cvi = dyn_cast<CopyValueInst>(use->getUser())) {
      addUsesToWorklist(cvi);
    }
    switch (use->getOperandOwnership()) {
    case OperandOwnership::NonUse:
      break;

    case OperandOwnership::TrivialUse:
    case OperandOwnership::ForwardingConsume:
    case OperandOwnership::DestroyingConsume:
      recordUse(use);
      break;

    case OperandOwnership::ForwardingUnowned:
    case OperandOwnership::PointerEscape:
    case OperandOwnership::Reborrow:
      return false;

    case OperandOwnership::InstantaneousUse:
    case OperandOwnership::UnownedInstantaneousUse:
    case OperandOwnership::BitwiseEscape:
    // EndBorrow either happens when this is called on a value that creates a
    // borrow scope, or when it is pushed as a use when processing a nested
    // borrow.
    case OperandOwnership::EndBorrow:
      recordUse(use);
      break;

    case OperandOwnership::InteriorPointer:
      if (InteriorPointerOperandKind::get(use) ==
          InteriorPointerOperandKind::Invalid)
        return false;
      // If our base guaranteed value does not have any consuming uses (consider
      // function arguments), we need to be sure to include interior pointer
      // operands since we may not get a use from a end_scope instruction.
      if (InteriorPointerOperand(use).findTransitiveUses(usePoints) !=
          AddressUseKind::NonEscaping) {
        return false;
      }
      recordUse(use);
      break;
    // \p borrowedValue will dominate this GuaranteedForwardingPhi, because we
    // return false in the case of Reborrow.
    case OperandOwnership::GuaranteedForwardingPhi: {
      SILArgument *phi = PhiOperand(use).getValue();
      for (auto *use : phi->getUses()) {
        if (use->getOperandOwnership() != OperandOwnership::NonUse)
          worklist.insert(use);
      }
      recordUse(use);
      break;
    }
    case OperandOwnership::GuaranteedForwarding: {
      ForwardingOperand(use).visitForwardedValues([&](SILValue result) {
        // Do not include transitive uses with 'none' ownership
        if (result->getOwnershipKind() == OwnershipKind::None)
          return true;
        for (auto *resultUse : result->getUses()) {
          if (resultUse->getOperandOwnership() != OperandOwnership::NonUse) {
            worklist.insert(resultUse);
          }
        }
        return true;
      });
      recordUse(use);
      break;
    }
    case OperandOwnership::Borrow:
      // FIXME: visitExtendedScopeEndingUses can't return false here once dead
      // borrows are disallowed.
      if (!BorrowingOperand(use).visitExtendedScopeEndingUses(
            [&](Operand *endUse) {
              recordUse(endUse);
              return true;
            })) {
        // Special case for dead borrows. This is dangerous because clients
        // don't expect a begin_borrow to be in the use list.
        recordUse(use);
      }
      break;
    }
  }
  return true;
}

// TODO: refactor this with SSAPrunedLiveness::computeLiveness.
bool swift::findUsesOfSimpleValue(SILValue value,
                                  SmallVectorImpl<Operand *> *usePoints) {
  for (auto *use : value->getUses()) {
    switch (use->getOperandOwnership()) {
    case OperandOwnership::PointerEscape:
      return false;
    case OperandOwnership::Borrow:
      if (!BorrowingOperand(use).visitScopeEndingUses([&](Operand *end) {
        if (end->getOperandOwnership() == OperandOwnership::Reborrow) {
          return false;
        }
        usePoints->push_back(end);
        return true;
      })) {
        return false;
      }
      break;
    default:
      break;
    }
    usePoints->push_back(use);
  }
  return true;
}

bool swift::visitGuaranteedForwardingPhisForSSAValue(
    SILValue value, function_ref<bool(Operand *)> visitor) {
  assert(isa<BeginBorrowInst>(value) || isa<LoadBorrowInst>(value) ||
         (isa<SILPhiArgument>(value) &&
          value->getOwnershipKind() == OwnershipKind::Guaranteed));
  // guaranteedForwardingOps is a collection of all transitive
  // GuaranteedForwarding uses of \p value. It is a set, to avoid repeated
  // processing of structs and tuples which are GuaranteedForwarding.
  SmallSetVector<Operand *, 4> guaranteedForwardingOps;
  // Collect first-level GuaranteedForwarding uses, and call the visitor on any
  // GuaranteedForwardingPhi uses.
  for (auto *use : value->getUses()) {
    if (use->getOperandOwnership() == OperandOwnership::GuaranteedForwarding) {
      guaranteedForwardingOps.insert(use);
      continue;
    }
    if (use->getOperandOwnership() ==
        OperandOwnership::GuaranteedForwardingPhi) {
      if (!visitor(use)) {
        return false;
      }
      continue;
    }
  }

  // Transitively, collect GuaranteedForwarding uses.
  for (unsigned i = 0; i < guaranteedForwardingOps.size(); i++) {
    for (auto val : guaranteedForwardingOps[i]->getUser()->getResults()) {
      for (auto *valUse : val->getUses()) {
        if (valUse->getOperandOwnership() ==
            OperandOwnership::GuaranteedForwarding) {
          guaranteedForwardingOps.insert(valUse);
          continue;
        }
        if (valUse->getOperandOwnership() ==
            OperandOwnership::GuaranteedForwardingPhi) {
          if (!visitor(valUse)) {
            return false;
          }
          continue;
        }
      }
    }
  }
  return true;
}

// Find all use points of \p guaranteedValue within its borrow scope. All use
// points will be dominated by \p guaranteedValue.
//
// Record (non-nested) reborrows as uses.
//
// BorrowedValues (which introduce a borrow scope) are fundamentally different
// than "inner" guaranteed values. Their only use points are their scope-ending
// uses. There is no need to transitively process uses. However, unlike inner
// guaranteed values, they can have reborrows. To transitively process
// reborrows, use findExtendedTransitiveBorrowedUses.
bool swift::findTransitiveGuaranteedUses(
    SILValue guaranteedValue, SmallVectorImpl<Operand *> &usePoints,
    function_ref<void(Operand *)> visitReborrow) {

  // Handle local borrow introducers without following uses.
  // SILFunctionArguments are *not* borrow introducers in this context--we're
  // trying to find lifetime of values within a function.
  if (auto borrowedValue = BorrowedValue(guaranteedValue)) {
    if (borrowedValue.isLocalScope()) {
      borrowedValue.visitLocalScopeEndingUses([&](Operand *scopeEnd) {
        // Initially push the reborrow as a use point. visitReborrow may pop it
        // if it only wants to compute the extended lifetime's use points.
        usePoints.push_back(scopeEnd);
        if (scopeEnd->getOperandOwnership() == OperandOwnership::Reborrow)
          visitReborrow(scopeEnd);
        return true;
      });
    }
    return true;
  }
  return findInnerTransitiveGuaranteedUses(guaranteedValue, &usePoints);
}

// Find all use points of \p guaranteedValue within its borrow scope. If the
// guaranteed value introduces a borrow scope, then this includes the extended
// borrow scope by following reborrows.
bool swift::
findExtendedTransitiveGuaranteedUses(SILValue guaranteedValue,
                                     SmallVectorImpl<Operand *> &usePoints) {
  // Multiple paths may reach the same reborrows, and reborrow may even be
  // recursive, so the working set requires a membership check.
  SmallPtrSetVector<SILValue, 4> reborrows;
  auto visitReborrow = [&](Operand *reborrow) {
    // Pop the reborrow. It should not appear in the use points of the
    // extend lifetime.
    assert(reborrow == usePoints.back());
    usePoints.pop_back();
    auto borrowedPhi =
      BorrowingOperand(reborrow).getBorrowIntroducingUserResult();
    reborrows.insert(borrowedPhi.value);
  };
  if (!findTransitiveGuaranteedUses(guaranteedValue, usePoints, visitReborrow))
    return false;

  // For guaranteed values that do not introduce a borrow scope, reborrows will
  // be empty at this point.
  for (unsigned idx = 0; idx < reborrows.size(); ++idx) {
    bool result =
      findTransitiveGuaranteedUses(reborrows[idx], usePoints, visitReborrow);
    // It is impossible to find a Pointer escape while traversing reborrows.
    assert(result && "visiting reborrows always succeeds");
    (void)result;
  }
  return true;
}

//===----------------------------------------------------------------------===//
//                           Borrowing Operand
//===----------------------------------------------------------------------===//

void BorrowingOperandKind::print(llvm::raw_ostream &os) const {
  switch (value) {
  case Kind::Invalid:
    llvm_unreachable("Using an unreachable?!");
  case Kind::BeginBorrow:
    os << "BeginBorrow";
    return;
  case Kind::BeginApply:
    os << "BeginApply";
    return;
  case Kind::Branch:
    os << "Branch";
    return;
  case Kind::Apply:
    os << "Apply";
    return;
  case Kind::TryApply:
    os << "TryApply";
    return;
  case Kind::Yield:
    os << "Yield";
    return;
  }
  llvm_unreachable("Covered switch isn't covered?!");
}

llvm::raw_ostream &swift::operator<<(llvm::raw_ostream &os,
                                     BorrowingOperandKind kind) {
  kind.print(os);
  return os;
}

void BorrowingOperand::print(llvm::raw_ostream &os) const {
  os << "BorrowScopeOperand:\n"
        "Kind: " << kind << "\n"
        "Value: " << op->get()
     << "User: " << *op->getUser();
}

llvm::raw_ostream &swift::operator<<(llvm::raw_ostream &os,
                                     const BorrowingOperand &operand) {
  operand.print(os);
  return os;
}

bool BorrowingOperand::hasEmptyRequiredEndingUses() const {
  switch (kind) {
  case BorrowingOperandKind::Invalid:
    llvm_unreachable("Using invalid case");
  case BorrowingOperandKind::BeginBorrow:
  case BorrowingOperandKind::BeginApply: {
    return op->getUser()->hasUsesOfAnyResult();
  }
  case BorrowingOperandKind::Branch: {
    auto *br = cast<BranchInst>(op->getUser());
    return br->getArgForOperand(op)->use_empty();
  }
  // These are instantaneous borrow scopes so there aren't any special end
  // scope instructions.
  case BorrowingOperandKind::Apply:
  case BorrowingOperandKind::TryApply:
  case BorrowingOperandKind::Yield:
    return false;
  }
  llvm_unreachable("Covered switch isn't covered");
}

bool BorrowingOperand::visitScopeEndingUses(
    function_ref<bool(Operand *)> func) const {
  switch (kind) {
  case BorrowingOperandKind::Invalid:
    llvm_unreachable("Using invalid case");
  case BorrowingOperandKind::BeginBorrow: {
    bool deadBorrow = true;
    for (auto *use : cast<BeginBorrowInst>(op->getUser())->getUses()) {
      if (use->isLifetimeEnding()) {
        deadBorrow = false;
        if (!func(use))
          return false;
      }
    }
    // FIXME: special case for dead borrows. This is dangerous because clients
    // only expect visitScopeEndingUses to return false if the visitor returned
    // false.
    return !deadBorrow;
  }
  case BorrowingOperandKind::BeginApply: {
    bool deadApply = true;
    auto *user = cast<BeginApplyInst>(op->getUser());
    for (auto *use : user->getTokenResult()->getUses()) {
      deadApply = false;
      if (!func(use))
        return false;
    }
    return !deadApply;
  }
  // These are instantaneous borrow scopes so there aren't any special end
  // scope instructions.
  case BorrowingOperandKind::Apply:
  case BorrowingOperandKind::TryApply:
  case BorrowingOperandKind::Yield:
    return true;
  case BorrowingOperandKind::Branch: {
    bool deadBranch = true;
    auto *br = cast<BranchInst>(op->getUser());
    for (auto *use : br->getArgForOperand(op)->getUses()) {
      if (use->isLifetimeEnding()) {
        deadBranch = false;
        if (!func(use))
          return false;
      }
    }
    return !deadBranch;
  }
  }
  llvm_unreachable("Covered switch isn't covered");
}

bool BorrowingOperand::visitExtendedScopeEndingUses(
    function_ref<bool(Operand *)> visitor) const {

  if (hasBorrowIntroducingUser()) {
    return visitBorrowIntroducingUserResults(
        [visitor](BorrowedValue borrowedValue) {
          return borrowedValue.visitExtendedScopeEndingUses(visitor);
        });
  }
  return visitScopeEndingUses(visitor);
}

bool BorrowingOperand::visitBorrowIntroducingUserResults(
    function_ref<bool(BorrowedValue)> visitor) const {
  switch (kind) {
  case BorrowingOperandKind::Invalid:
    llvm_unreachable("Using invalid case");
  case BorrowingOperandKind::Apply:
  case BorrowingOperandKind::TryApply:
  case BorrowingOperandKind::BeginApply:
  case BorrowingOperandKind::Yield:
    llvm_unreachable("Never has borrow introducer results!");
  case BorrowingOperandKind::BeginBorrow: {
    auto value = BorrowedValue(cast<BeginBorrowInst>(op->getUser()));
    assert(value);
    return visitor(value);
  }
  case BorrowingOperandKind::Branch: {
    auto *bi = cast<BranchInst>(op->getUser());
    auto value = BorrowedValue(
        bi->getDestBB()->getArgument(op->getOperandNumber()));
    assert(value && "guaranteed-to-unowned conversion not allowed on branches");
    return visitor(value);
  }
  }
  llvm_unreachable("Covered switch isn't covered?!");
}

BorrowedValue BorrowingOperand::getBorrowIntroducingUserResult() {
  switch (kind) {
  case BorrowingOperandKind::Invalid:
  case BorrowingOperandKind::Apply:
  case BorrowingOperandKind::TryApply:
  case BorrowingOperandKind::BeginApply:
  case BorrowingOperandKind::Yield:
    return BorrowedValue();

  case BorrowingOperandKind::BeginBorrow:
    return BorrowedValue(cast<BeginBorrowInst>(op->getUser()));

  case BorrowingOperandKind::Branch: {
    auto *bi = cast<BranchInst>(op->getUser());
    return BorrowedValue(bi->getDestBB()->getArgument(op->getOperandNumber()));
  }
  }
  llvm_unreachable("covered switch");
}

void BorrowingOperand::getImplicitUses(
    SmallVectorImpl<Operand *> &foundUses) const {
  // FIXME: this visitScopeEndingUses should never return false once dead
  // borrows are disallowed.
  if (!visitScopeEndingUses([&](Operand *endOp) {
    foundUses.push_back(endOp);
    return true;
  })) {
    // Special-case for dead borrows.
    foundUses.push_back(op);
  }
}

//===----------------------------------------------------------------------===//
//                             Borrow Introducers
//===----------------------------------------------------------------------===//

void BorrowedValueKind::print(llvm::raw_ostream &os) const {
  switch (value) {
  case BorrowedValueKind::Invalid:
    llvm_unreachable("Using invalid case?!");
  case BorrowedValueKind::SILFunctionArgument:
    os << "SILFunctionArgument";
    return;
  case BorrowedValueKind::BeginBorrow:
    os << "BeginBorrowInst";
    return;
  case BorrowedValueKind::LoadBorrow:
    os << "LoadBorrowInst";
    return;
  case BorrowedValueKind::Phi:
    os << "Phi";
    return;
  }
  llvm_unreachable("Covered switch isn't covered?!");
}

void BorrowedValue::print(llvm::raw_ostream &os) const {
  os << "BorrowScopeIntroducingValue:\n"
    "Kind: " << kind << "\n"
    "Value: " << value;
}

void BorrowedValue::getLocalScopeEndingInstructions(
    SmallVectorImpl<SILInstruction *> &scopeEndingInsts) const {
  assert(isLocalScope() && "Should only call this given a local scope");

  switch (kind) {
  case BorrowedValueKind::Invalid:
    llvm_unreachable("Using invalid case?!");
  case BorrowedValueKind::SILFunctionArgument:
    llvm_unreachable("Should only call this with a local scope");
  case BorrowedValueKind::BeginBorrow:
  case BorrowedValueKind::LoadBorrow:
  case BorrowedValueKind::Phi:
    for (auto *use : value->getUses()) {
      if (use->isLifetimeEnding()) {
        scopeEndingInsts.push_back(use->getUser());
      }
    }
    return;
  }
  llvm_unreachable("Covered switch isn't covered?!");
}

// Note: BorrowedLifetimeExtender assumes no intermediate values between a
// borrow introducer and its reborrow. The borrowed value must be an operand of
// the reborrow.
bool BorrowedValue::visitLocalScopeEndingUses(
    function_ref<bool(Operand *)> visitor) const {
  assert(isLocalScope() && "Should only call this given a local scope");
  switch (kind) {
  case BorrowedValueKind::Invalid:
    llvm_unreachable("Using invalid case?!");
  case BorrowedValueKind::SILFunctionArgument:
    llvm_unreachable("Should only call this with a local scope");
  case BorrowedValueKind::LoadBorrow:
  case BorrowedValueKind::BeginBorrow:
  case BorrowedValueKind::Phi:
    for (auto *use : value->getUses()) {
      if (use->isLifetimeEnding()) {
        if (!visitor(use))
          return false;
      }
    }
    return true;
  }
  llvm_unreachable("Covered switch isn't covered?!");
}

llvm::raw_ostream &swift::operator<<(llvm::raw_ostream &os,
                                     BorrowedValueKind kind) {
  kind.print(os);
  return os;
}

llvm::raw_ostream &swift::operator<<(llvm::raw_ostream &os,
                                     const BorrowedValue &value) {
  value.print(os);
  return os;
}

/// Add this scopes live blocks into the PrunedLiveness result.
void BorrowedValue::
computeTransitiveLiveness(MultiDefPrunedLiveness &liveness) const {
  liveness.initializeDef(value);
  visitTransitiveLifetimeEndingUses([&](Operand *endOp) {
    if (endOp->getOperandOwnership() == OperandOwnership::EndBorrow) {
      liveness.updateForUse(endOp->getUser(), /*lifetimeEnding*/ true);
      return true;
    }
    assert(endOp->getOperandOwnership() == OperandOwnership::Reborrow);
    PhiOperand phiOper(endOp);
    liveness.initializeDef(phiOper.getValue());
    liveness.updateForUse(endOp->getUser(), /*lifetimeEnding*/ false);
    return true;
  });
}

bool BorrowedValue::areUsesWithinExtendedScope(
    ArrayRef<Operand *> uses, DeadEndBlocks *deadEndBlocks) const {
  // First make sure that we actually have a local scope. If we have a non-local
  // scope, then we have something (like a SILFunctionArgument) where a larger
  // semantic construct (in the case of SILFunctionArgument, the function
  // itself) acts as the scope. So we already know that our passed in
  // instructions must be in the same scope.
  if (!isLocalScope())
    return true;

  // Compute the local scope's liveness.
  MultiDefPrunedLiveness liveness(value->getFunction());
  computeTransitiveLiveness(liveness);
  return liveness.areUsesWithinBoundary(uses, deadEndBlocks);
}

// The visitor \p func is only called on final scope-ending uses, not reborrows.
bool BorrowedValue::visitExtendedScopeEndingUses(
    function_ref<bool(Operand *)> visitor) const {
  assert(isLocalScope());

  SmallPtrSetVector<SILValue, 4> reborrows;

  auto visitEnd = [&](Operand *scopeEndingUse) {
    if (scopeEndingUse->getOperandOwnership() == OperandOwnership::Reborrow) {
      BorrowingOperand(scopeEndingUse).visitBorrowIntroducingUserResults(
        [&](BorrowedValue borrowedValue) {
          reborrows.insert(borrowedValue.value);
          return true;
        });
      return true;
    }
    return visitor(scopeEndingUse);
  };

  if (!visitLocalScopeEndingUses(visitEnd))
    return false;

  // reborrows grows in this loop.
  for (unsigned idx = 0; idx < reborrows.size(); ++idx) {
    if (!BorrowedValue(reborrows[idx]).visitLocalScopeEndingUses(visitEnd))
      return false;
  }
  return true;
}

bool BorrowedValue::visitTransitiveLifetimeEndingUses(
    function_ref<bool(Operand *)> visitor) const {
  assert(isLocalScope());

  SmallPtrSetVector<SILValue, 4> reborrows;

  auto visitEnd = [&](Operand *scopeEndingUse) {
    if (scopeEndingUse->getOperandOwnership() == OperandOwnership::Reborrow) {
      BorrowingOperand(scopeEndingUse)
          .visitBorrowIntroducingUserResults([&](BorrowedValue borrowedValue) {
            reborrows.insert(borrowedValue.value);
            return true;
          });
      // visitor on the reborrow
      return visitor(scopeEndingUse);
    }
    // visitor on the end_borrow
    return visitor(scopeEndingUse);
  };

  if (!visitLocalScopeEndingUses(visitEnd))
    return false;

  // reborrows grows in this loop.
  for (unsigned idx = 0; idx < reborrows.size(); ++idx) {
    if (!BorrowedValue(reborrows[idx]).visitLocalScopeEndingUses(visitEnd))
      return false;
  }

  return true;
}

bool BorrowedValue::visitInteriorPointerOperandHelper(
    function_ref<void(InteriorPointerOperand)> func,
    BorrowedValue::InteriorPointerOperandVisitorKind kind) const {
  using Kind = BorrowedValue::InteriorPointerOperandVisitorKind;

  SmallVector<Operand *, 32> worklist(value->getUses());
  while (!worklist.empty()) {
    auto *op = worklist.pop_back_val();

    if (auto interiorPointer = InteriorPointerOperand(op)) {
      func(interiorPointer);
      continue;
    }

    if (auto borrowingOperand = BorrowingOperand(op)) {
      switch (kind) {
      case Kind::NoNestedNoReborrows:
        // We do not look through nested things and or reborrows, so just
        // continue.
        continue;
      case Kind::YesNestedNoReborrows:
        // We only look through nested borrowing operands, we never look through
        // reborrows though.
        if (borrowingOperand.isReborrow())
          continue;
        break;
      case Kind::YesNestedYesReborrows:
        // Look through everything!
        break;
      }

      borrowingOperand.visitBorrowIntroducingUserResults([&](auto bv) {
        for (auto *use : bv->getUses()) {
          if (auto intPtrOperand = InteriorPointerOperand(use)) {
            func(intPtrOperand);
            continue;
          }
          worklist.push_back(use);
        }
        return true;
      });
      continue;
    }

    auto *user = op->getUser();
    if (isa<DebugValueInst>(user) || isa<SuperMethodInst>(user) ||
        isa<ClassMethodInst>(user) || isa<CopyValueInst>(user) ||
        isa<EndBorrowInst>(user) || isa<ApplyInst>(user) ||
        isa<StoreInst>(user) || isa<PartialApplyInst>(user) ||
        isa<UnmanagedRetainValueInst>(user) ||
        isa<UnmanagedReleaseValueInst>(user) ||
        isa<UnmanagedAutoreleaseValueInst>(user)) {
      continue;
    }

    // These are interior pointers that have not had support yet added for them.
    if (isa<ProjectExistentialBoxInst>(user)) {
      continue;
    }

    // Look through object.
    if (auto *svi = dyn_cast<SingleValueInstruction>(user)) {
      if (Projection::isObjectProjection(svi)) {
        for (SILValue result : user->getResults()) {
          llvm::copy(result->getUses(), std::back_inserter(worklist));
        }
        continue;
      }
    }

    return false;
  }

  return true;
}

// FIXME: This does not yet assume complete lifetimes. Therefore, it currently
// recursively looks through scoped uses, such as load_borrow. We should
// separate the logic for lifetime completion from the logic that can assume
// complete lifetimes.
AddressUseKind
swift::findTransitiveUsesForAddress(SILValue projectedAddress,
                                    SmallVectorImpl<Operand *> *foundUses,
                                    std::function<void(Operand *)> *onError) {
  // If the projectedAddress is dead, it is itself a leaf use. Since we don't
  // have an operand for it, simply bail. Dead projectedAddress is unexpected.
  //
  // TODO: store_borrow is currently an InteriorPointer with no uses, so we end
  // up bailing. It should be in a dependence scope instead. It's not clear why
  // it produces an address at all.
  if (projectedAddress->use_empty())
    return AddressUseKind::PointerEscape;

  SmallVector<Operand *, 8> worklist(projectedAddress->getUses());

  AddressUseKind result = AddressUseKind::NonEscaping;

  auto leafUse = [foundUses](Operand *use) {
    if (foundUses)
      foundUses->push_back(use);
  };
  auto transitiveResultUses = [&](Operand *use) {
    auto *svi = cast<SingleValueInstruction>(use->getUser());
    if (svi->use_empty()) {
      leafUse(use);
    } else {
      worklist.append(svi->use_begin(), svi->use_end());
    }
  };

  while (!worklist.empty()) {
    auto *op = worklist.pop_back_val();

    // Skip type dependent operands.
    if (op->isTypeDependent())
      continue;

    // Then update the worklist with new things to find if we recognize this
    // inst and then continue. If we fail, we emit an error at the bottom of the
    // loop that we didn't recognize the user.
    auto *user = op->getUser();

    // TODO: Partial apply should be NonEscaping, but then we need to consider
    // the apply to be a use point.
    if (isa<PartialApplyInst>(user) || isa<AddressToPointerInst>(user)) {
      result = meet(result, AddressUseKind::PointerEscape);
      continue;
    }
    // First, eliminate "end point uses" that we just need to check liveness at
    // and do not need to check transitive uses of.
    if (isa<LoadInst>(user) || isa<CopyAddrInst>(user) ||
        isa<MarkUnresolvedMoveAddrInst>(user) || isIncidentalUse(user) ||
        isa<StoreInst>(user) || isa<DestroyAddrInst>(user) ||
        isa<AssignInst>(user) || isa<YieldInst>(user) ||
        isa<LoadUnownedInst>(user) || isa<StoreUnownedInst>(user) ||
        isa<EndApplyInst>(user) || isa<LoadWeakInst>(user) ||
        isa<StoreWeakInst>(user) || isa<AssignByWrapperInst>(user) ||
        isa<BeginUnpairedAccessInst>(user) ||
        isa<EndUnpairedAccessInst>(user) || isa<WitnessMethodInst>(user) ||
        isa<SwitchEnumAddrInst>(user) || isa<CheckedCastAddrBranchInst>(user) ||
        isa<SelectEnumAddrInst>(user) || isa<InjectEnumAddrInst>(user) ||
        isa<IsUniqueInst>(user) || isa<ValueMetatypeInst>(user)) {
      leafUse(op);
      continue;
    }

    if (isa<UnconditionalCheckedCastAddrInst>(user)
        || isa<MarkFunctionEscapeInst>(user)) {
      assert(!user->hasResults());
      continue;
    }

    // Then handle users that we need to look at transitive uses of.
    if (Projection::isAddressProjection(user) ||
        isa<ProjectBlockStorageInst>(user) ||
        isa<OpenExistentialAddrInst>(user) ||
        isa<InitExistentialAddrInst>(user) || isa<InitEnumDataAddrInst>(user) ||
        isa<BeginAccessInst>(user) || isa<TailAddrInst>(user) ||
        isa<IndexAddrInst>(user) || isa<StoreBorrowInst>(user) ||
        isa<UncheckedAddrCastInst>(user) || isa<MarkMustCheckInst>(user)) {
      transitiveResultUses(op);
      continue;
    }

    if (auto *builtin = dyn_cast<BuiltinInst>(user)) {
      if (auto kind = builtin->getBuiltinKind()) {
        if (*kind == BuiltinValueKind::TSanInoutAccess) {
          leafUse(op);
          continue;
        }
      }
    }

    // If we have a load_borrow, add it's end scope to the liveness requirement.
    if (auto *lbi = dyn_cast<LoadBorrowInst>(user)) {
      if (foundUses) {
        // FIXME: if we can assume complete lifetimes, then this should be
        // as simple as:
        //   for (Operand *use : lbi->getUses()) {
        //     if (use->endsLocalBorrowScope()) {
        if (!findInnerTransitiveGuaranteedUses(lbi, foundUses)) {
          result = meet(result, AddressUseKind::PointerEscape);
        }
      }
      continue;
    }

    // TODO: Merge this into the full apply site code below.
    if (auto *beginApply = dyn_cast<BeginApplyInst>(user)) {
      if (foundUses) {
        // TODO: the empty check should not be needed when dead begin_apply is
        // disallowed.
        if (beginApply->getTokenResult()->use_empty()) {
          leafUse(op);
        } else {
          llvm::copy(beginApply->getTokenResult()->getUses(),
                     std::back_inserter(*foundUses));
        }
      }
      continue;
    }

    if (auto fas = FullApplySite::isa(user)) {
      leafUse(op);
      continue;
    }

    if (auto *mdi = dyn_cast<MarkDependenceInst>(user)) {
      // If this is the base, just treat it as a liveness use.
      if (op->get() == mdi->getBase()) {
        leafUse(op);
        continue;
      }

      // If we are the value use, look through it.
      transitiveResultUses(op);
      continue;
    }

    // We were unable to recognize this user, so return true that we failed.
    if (onError) {
      (*onError)(op);
    }
    result = meet(result, AddressUseKind::Unknown);
  }
  return result;
}

//===----------------------------------------------------------------------===//
//                              AddressOwnership
//===----------------------------------------------------------------------===//

bool AddressOwnership::areUsesWithinLifetime(
    ArrayRef<Operand *> uses, DeadEndBlocks &deadEndBlocks) const {
  if (!base.hasLocalOwnershipLifetime())
    return true;

  SILValue root = base.getOwnershipReferenceRoot();
  BorrowedValue borrow(root);
  if (borrow)
    return borrow.areUsesWithinExtendedScope(uses, &deadEndBlocks);

  // --- A reference with no borrow scope! Currently happens for project_box.

  // Compute the reference value's liveness.
  SSAPrunedLiveness liveness;
  liveness.initializeDef(root);
  SimpleLiveRangeSummary summary = liveness.computeSimple();
  // Conservatively ignore InnerBorrowKind::Reborrowed and
  // AddressUseKind::PointerEscape and Reborrowed. The resulting liveness at
  // least covers the known uses.
  (void)summary;

  // FIXME (implicit borrow): handle reborrows transitively just like above so
  // we don't bail out if a uses is within the reborrowed scope.
  return liveness.areUsesWithinBoundary(uses, &deadEndBlocks);
}

//===----------------------------------------------------------------------===//
//                          Owned Value Introducers
//===----------------------------------------------------------------------===//

void OwnedValueIntroducerKind::print(llvm::raw_ostream &os) const {
  switch (value) {
  case OwnedValueIntroducerKind::Invalid:
    llvm_unreachable("Using invalid case?!");
  case OwnedValueIntroducerKind::Apply:
    os << "Apply";
    return;
  case OwnedValueIntroducerKind::BeginApply:
    os << "BeginApply";
    return;
  case OwnedValueIntroducerKind::TryApply:
    os << "TryApply";
    return;
  case OwnedValueIntroducerKind::Copy:
    os << "Copy";
    return;
  case OwnedValueIntroducerKind::LoadCopy:
    os << "LoadCopy";
    return;
  case OwnedValueIntroducerKind::LoadTake:
    os << "LoadTake";
    return;
  case OwnedValueIntroducerKind::Phi:
    os << "Phi";
    return;
  case OwnedValueIntroducerKind::Struct:
    os << "Struct";
    return;
  case OwnedValueIntroducerKind::Tuple:
    os << "Tuple";
    return;
  case OwnedValueIntroducerKind::FunctionArgument:
    os << "FunctionArgument";
    return;
  case OwnedValueIntroducerKind::PartialApplyInit:
    os << "PartialApplyInit";
    return;
  case OwnedValueIntroducerKind::AllocBoxInit:
    os << "AllocBoxInit";
    return;
  case OwnedValueIntroducerKind::AllocRefInit:
    os << "AllocRefInit";
    return;
  }
  llvm_unreachable("Covered switch isn't covered");
}

//===----------------------------------------------------------------------===//
//                       Introducer Searching Routines
//===----------------------------------------------------------------------===//

bool swift::getAllBorrowIntroducingValues(SILValue inputValue,
                                          SmallVectorImpl<BorrowedValue> &out) {
  if (inputValue->getOwnershipKind() != OwnershipKind::Guaranteed)
    return false;

  SmallVector<SILValue, 32> worklist;
  worklist.emplace_back(inputValue);

  while (!worklist.empty()) {
    SILValue value = worklist.pop_back_val();

    // First check if v is an introducer. If so, stash it and continue.
    if (auto scopeIntroducer = BorrowedValue(value)) {
      out.push_back(scopeIntroducer);
      continue;
    }

    // If v produces .none ownership, then we can ignore it. It is important
    // that we put this before checking for guaranteed forwarding instructions,
    // since we want to ignore guaranteed forwarding instructions that in this
    // specific case produce a .none value.
    if (value->getOwnershipKind() == OwnershipKind::None)
      continue;

    // Otherwise if v is an ownership forwarding value, add its defining
    // instruction
    if (isGuaranteedForwarding(value)) {
      if (auto *i = value->getDefiningInstruction()) {
        llvm::copy(i->getNonTypeDependentOperandValues(),
                   std::back_inserter(worklist));
        continue;
      }

      // Otherwise, we should have a block argument that is defined by a single
      // predecessor terminator.
      auto *arg = cast<SILPhiArgument>(value);
      auto *termInst = arg->getSingleTerminator();
      assert(termInst && termInst->isTransformationTerminator() &&
             OwnershipForwardingMixin::get(termInst)->preservesOwnership());
      assert(termInst->getNumOperands() == 1 &&
             "Transforming terminators should always have a single operand");
      worklist.push_back(termInst->getAllOperands()[0].get());
      continue;
    }

    // Otherwise, this is an introducer we do not understand. Bail and return
    // false.
    return false;
  }

  return true;
}

// FIXME: replace this logic with AccessBase::findOwnershipReferenceRoot.
BorrowedValue swift::getSingleBorrowIntroducingValue(SILValue inputValue) {
  if (inputValue->getOwnershipKind() != OwnershipKind::Guaranteed)
    return {};

  SILValue currentValue = inputValue;
  while (true) {
    // First check if our initial value is an introducer. If we have one, just
    // return it.
    if (auto scopeIntroducer = BorrowedValue(currentValue)) {
      return scopeIntroducer;
    }

    if (currentValue->getOwnershipKind() == OwnershipKind::None)
      return {};

    // Otherwise if v is an ownership forwarding value, add its defining
    // instruction
    if (isGuaranteedForwarding(currentValue)) {
      if (auto *i = currentValue->getDefiningInstruction()) {
        auto instOps = i->getNonTypeDependentOperandValues();
        // If we have multiple incoming values, return .None. We can't handle
        // this.
        auto begin = instOps.begin();
        if (std::next(begin) != instOps.end()) {
          return {};
        }
        // Otherwise, set currentOp to the single operand and continue.
        currentValue = *begin;
        continue;
      }

      // Otherwise, we should have a block argument that is defined by a single
      // predecessor terminator.
      auto *arg = cast<SILPhiArgument>(currentValue);
      auto *termInst = arg->getSingleTerminator();
      assert(termInst && termInst->isTransformationTerminator() &&
             OwnershipForwardingMixin::get(termInst)->preservesOwnership());
      assert(termInst->getNumOperands() == 1 &&
             "Transformation terminators should only have single operands");
      currentValue = termInst->getAllOperands()[0].get();
      continue;
    }

    // Otherwise, this is an introducer we do not understand. Bail and return
    // None.
    return {};
  }

  llvm_unreachable("Should never hit this");
}

bool swift::getAllOwnedValueIntroducers(
    SILValue inputValue, SmallVectorImpl<OwnedValueIntroducer> &out) {
  if (inputValue->getOwnershipKind() != OwnershipKind::Owned)
    return false;

  SmallVector<SILValue, 32> worklist;
  worklist.emplace_back(inputValue);

  while (!worklist.empty()) {
    SILValue value = worklist.pop_back_val();

    // First check if v is an introducer. If so, stash it and continue.
    if (auto introducer = OwnedValueIntroducer::get(value)) {
      out.push_back(introducer);
      continue;
    }

    // If v produces .none ownership, then we can ignore it. It is important
    // that we put this before checking for guaranteed forwarding instructions,
    // since we want to ignore guaranteed forwarding instructions that in this
    // specific case produce a .none value.
    if (value->getOwnershipKind() == OwnershipKind::None)
      continue;

    // Otherwise if v is an ownership forwarding value, add its defining
    // instruction
    if (isForwardingConsume(value)) {
      if (auto *i = value->getDefiningInstruction()) {
        llvm::copy(i->getNonTypeDependentOperandValues(),
                   std::back_inserter(worklist));
        continue;
      }

      // Otherwise, we should have a block argument that is defined by a single
      // predecessor terminator.
      auto *arg = cast<SILPhiArgument>(value);
      auto *termInst = arg->getSingleTerminator();
      assert(termInst && termInst->isTransformationTerminator() &&
             OwnershipForwardingMixin::get(termInst)->preservesOwnership());
      assert(termInst->getNumOperands() == 1 &&
             "Transforming terminators should always have a single operand");
      worklist.push_back(termInst->getAllOperands()[0].get());
      continue;
    }

    // Otherwise, this is an introducer we do not understand. Bail and return
    // false.
    return false;
  }

  return true;
}

OwnedValueIntroducer swift::getSingleOwnedValueIntroducer(SILValue inputValue) {
  if (inputValue->getOwnershipKind() != OwnershipKind::Owned)
    return {};

  SILValue currentValue = inputValue;
  while (true) {
    // First check if our initial value is an introducer. If we have one, just
    // return it.
    if (auto introducer = OwnedValueIntroducer::get(currentValue)) {
      return introducer;
    }

    // Otherwise if v is an ownership forwarding value, add its defining
    // instruction
    if (isForwardingConsume(currentValue)) {
      if (auto *i = currentValue->getDefiningInstruction()) {
        auto instOps = i->getNonTypeDependentOperandValues();
        // If we have multiple incoming values, return .None. We can't handle
        // this.
        auto begin = instOps.begin();
        if (std::next(begin) != instOps.end()) {
          return {};
        }
        // Otherwise, set currentOp to the single operand and continue.
        currentValue = *begin;
        continue;
      }

      // Otherwise, we should have a block argument that is defined by a single
      // predecessor terminator and is directly forwarding.
      auto *arg = cast<SILPhiArgument>(currentValue);
      auto *termInst = arg->getSingleTerminator();
      assert(termInst && termInst->isTransformationTerminator() &&
             OwnershipForwardingMixin::get(termInst)->preservesOwnership());
      assert(termInst->getNumOperands()
             - termInst->getNumTypeDependentOperands() == 1 &&
             "Transformation terminators should only have single operands");
      currentValue = termInst->getAllOperands()[0].get();
      continue;
    }

    // Otherwise, this is an introducer we do not understand. Bail and return
    // None.
    return {};
  }

  llvm_unreachable("Should never hit this");
}

//===----------------------------------------------------------------------===//
//                             Forwarding Operand
//===----------------------------------------------------------------------===//

ForwardingOperand::ForwardingOperand(Operand *use) {
  if (use->isTypeDependent())
    return;

  if (!OwnershipForwardingMixin::isa(use->getUser())) {
    return;
  }
#ifndef NDEBUG
  switch (use->getOperandOwnership()) {
  case OperandOwnership::ForwardingUnowned:
  case OperandOwnership::ForwardingConsume:
  case OperandOwnership::GuaranteedForwarding:
    break;
  case OperandOwnership::NonUse:
  case OperandOwnership::TrivialUse:
  case OperandOwnership::InstantaneousUse:
  case OperandOwnership::UnownedInstantaneousUse:
  case OperandOwnership::PointerEscape:
  case OperandOwnership::BitwiseEscape:
  case OperandOwnership::Borrow:
  case OperandOwnership::DestroyingConsume:
  case OperandOwnership::InteriorPointer:
  case OperandOwnership::GuaranteedForwardingPhi:
  case OperandOwnership::EndBorrow:
  case OperandOwnership::Reborrow:
    llvm_unreachable("this isn't the operand being forwarding!");
  }
#endif
  this->use = use;
}

ValueOwnershipKind ForwardingOperand::getForwardingOwnershipKind() const {
  auto *user = use->getUser();

  // NOTE: This if chain is meant to be a covered switch, so make sure to return
  // in each if itself since we have an unreachable at the bottom to ensure if a
  // new subclass of OwnershipForwardingInst is added
  if (auto *ofsvi = dyn_cast<AllArgOwnershipForwardingSingleValueInst>(user))
    return ofsvi->getForwardingOwnershipKind();

  if (auto *ofsvi = dyn_cast<FirstArgOwnershipForwardingSingleValueInst>(user))
    return ofsvi->getForwardingOwnershipKind();

  if (auto *ofci = dyn_cast<OwnershipForwardingConversionInst>(user))
    return ofci->getForwardingOwnershipKind();

  if (auto *ofseib = dyn_cast<OwnershipForwardingSelectEnumInstBase>(user))
    return ofseib->getForwardingOwnershipKind();

  if (auto *ofmvi =
          dyn_cast<OwnershipForwardingMultipleValueInstruction>(user)) {
    assert(ofmvi->getNumOperands() == 1);
    return ofmvi->getForwardingOwnershipKind();
  }

  if (auto *ofti = dyn_cast<OwnershipForwardingTermInst>(user)) {
    assert(ofti->getNumOperands() == 1);
    return ofti->getForwardingOwnershipKind();
  }

  if (auto *move = dyn_cast<MoveOnlyWrapperToCopyableValueInst>(user)) {
    return move->getForwardingOwnershipKind();
  }

  llvm_unreachable("Unhandled forwarding inst?!");
}

void ForwardingOperand::setForwardingOwnershipKind(
    ValueOwnershipKind newKind) const {
  auto *user = use->getUser();
  // NOTE: This if chain is meant to be a covered switch, so make sure to return
  // in each if itself since we have an unreachable at the bottom to ensure if a
  // new subclass of OwnershipForwardingInst is added
  if (auto *ofsvi = dyn_cast<AllArgOwnershipForwardingSingleValueInst>(user))
    return ofsvi->setForwardingOwnershipKind(newKind);
  if (auto *ofsvi = dyn_cast<FirstArgOwnershipForwardingSingleValueInst>(user))
    return ofsvi->setForwardingOwnershipKind(newKind);
  if (auto *ofci = dyn_cast<OwnershipForwardingConversionInst>(user))
    return ofci->setForwardingOwnershipKind(newKind);
  if (auto *ofseib = dyn_cast<OwnershipForwardingSelectEnumInstBase>(user))
    return ofseib->setForwardingOwnershipKind(newKind);
  if (auto *ofmvi = dyn_cast<OwnershipForwardingMultipleValueInstruction>(user)) {
    assert(ofmvi->getNumOperands() == 1);
    if (!ofmvi->getOperand(0)->getType().isTrivial(*ofmvi->getFunction())) {
      ofmvi->setForwardingOwnershipKind(newKind);
      // TODO: Refactor this better.
      if (auto *dsi = dyn_cast<DestructureStructInst>(ofmvi)) {
        for (auto &result : dsi->getAllResultsBuffer()) {
          if (result.getType().isTrivial(*dsi->getFunction()))
            continue;
          result.setOwnershipKind(newKind);
        }
      } else {
        auto *dti = cast<DestructureTupleInst>(ofmvi);
        for (auto &result : dti->getAllResultsBuffer()) {
          if (result.getType().isTrivial(*dti->getFunction()))
            continue;
          result.setOwnershipKind(newKind);
        }
      }
    }
    return;
  }

  if (auto *ofti = dyn_cast<OwnershipForwardingTermInst>(user)) {
    assert(ofti->getNumOperands() == 1);
    if (!ofti->getOperand()->getType().isTrivial(*ofti->getFunction())) {
      ofti->setForwardingOwnershipKind(newKind);

      // Then convert all of its incoming values that are owned to be guaranteed.
      for (auto &succ : ofti->getSuccessors()) {
        auto *succBlock = succ.getBB();

        // If we do not have any arguments, then continue.
        if (succBlock->args_empty())
          continue;

        for (auto *succArg : succBlock->getSILPhiArguments()) {
          // If we have an any value, just continue.
          if (!succArg->getType().isTrivial(*ofti->getFunction()))
            continue;
          succArg->setOwnershipKind(newKind);
        }
      }
    }
    return;
  }

  assert(
      !isa<MoveOnlyWrapperToCopyableValueInst>(user) &&
      "MoveOnlyWrapperToCopyableValueInst can not have its ownership changed");

  llvm_unreachable("Out of sync with OperandOwnership");
}

void ForwardingOperand::replaceOwnershipKind(ValueOwnershipKind oldKind,
                                             ValueOwnershipKind newKind) const {
  auto *user = use->getUser();

  if (auto *fInst = dyn_cast<AllArgOwnershipForwardingSingleValueInst>(user))
    if (fInst->getForwardingOwnershipKind() == oldKind)
      return fInst->setForwardingOwnershipKind(newKind);

  if (auto *fInst = dyn_cast<FirstArgOwnershipForwardingSingleValueInst>(user))
    if (fInst->getForwardingOwnershipKind() == oldKind)
      return fInst->setForwardingOwnershipKind(newKind);

  if (auto *ofci = dyn_cast<OwnershipForwardingConversionInst>(user))
    if (ofci->getForwardingOwnershipKind() == oldKind)
      return ofci->setForwardingOwnershipKind(newKind);

  if (auto *ofseib = dyn_cast<OwnershipForwardingSelectEnumInstBase>(user))
    if (ofseib->getForwardingOwnershipKind() == oldKind)
      return ofseib->setForwardingOwnershipKind(newKind);

  if (auto *ofmvi = dyn_cast<OwnershipForwardingMultipleValueInstruction>(user)) {
    if (ofmvi->getForwardingOwnershipKind() == oldKind) {
      ofmvi->setForwardingOwnershipKind(newKind);
    }
    // TODO: Refactor this better.
    if (auto *dsi = dyn_cast<DestructureStructInst>(ofmvi)) {
      for (auto &result : dsi->getAllResultsBuffer()) {
        if (result.getOwnershipKind() != oldKind)
          continue;
        result.setOwnershipKind(newKind);
      }
    } else {
      auto *dti = cast<DestructureTupleInst>(ofmvi);
      for (auto &result : dti->getAllResultsBuffer()) {
        if (result.getOwnershipKind() != oldKind)
          continue;
        result.setOwnershipKind(newKind);
      }
    }
    return;
  }

  if (auto *ofti = dyn_cast<OwnershipForwardingTermInst>(user)) {
    if (ofti->getForwardingOwnershipKind() == oldKind) {
      ofti->setForwardingOwnershipKind(newKind);
      // Then convert all of its incoming values that are owned to be guaranteed.
      for (auto &succ : ofti->getSuccessors()) {
        auto *succBlock = succ.getBB();

        // If we do not have any arguments, then continue.
        if (succBlock->args_empty())
          continue;

        for (auto *succArg : succBlock->getSILPhiArguments()) {
          // If we have an any value, just continue.
          if (succArg->getOwnershipKind() == oldKind) {
            succArg->setOwnershipKind(newKind);
          }
        }
      }
    }
    return;
  }

  assert(
      !isa<MoveOnlyWrapperToCopyableValueInst>(user) &&
      "MoveOnlyWrapperToCopyableValueInst can not have its ownership changed");

  llvm_unreachable("Missing Case! Out of sync with OperandOwnership");
}

SILValue ForwardingOperand::getSingleForwardedValue() const {
  if (auto *svi = dyn_cast<SingleValueInstruction>(use->getUser()))
    return svi;
  return SILValue();
}

bool ForwardingOperand::visitForwardedValues(
    function_ref<bool(SILValue)> visitor) {
  auto *user = use->getUser();

  // See if we have a single value instruction... if we do that is always the
  // transitive result.
  if (auto *svi = dyn_cast<SingleValueInstruction>(user)) {
    return visitor(svi);
  }

  if (auto *mvri = dyn_cast<MultipleValueInstruction>(user)) {
    return llvm::all_of(mvri->getResults(), [&](SILValue value) {
      if (value->getOwnershipKind() == OwnershipKind::None)
        return true;
      return visitor(value);
    });
  }

  // This is an instruction like switch_enum and checked_cast_br that are
  // "transforming terminators"... We know that this means that we should at
  // most have a single phi argument.
  auto *ti = cast<TermInst>(user);
  return llvm::all_of(ti->getSuccessorBlocks(), [&](SILBasicBlock *succBlock) {
    // If we do not have any arguments, then continue.
    if (succBlock->args_empty())
      return true;

    auto args = succBlock->getSILPhiArguments();
    assert(args.size() == 1 && "Transforming terminator with multiple args?!");
    return visitor(args[0]);
  });
}

void swift::visitExtendedReborrowPhiBaseValuePairs(
    BeginBorrowInst *borrowInst, function_ref<void(SILPhiArgument *, SILValue)>
                                     visitReborrowPhiBaseValuePair) {
  // A Reborrow can have different base values on different control flow
  // paths.
  // For that reason, worklist stores (reborrow, base value) pairs.
  // We need a SetVector to make sure we don't revisit the same pair again.
  SmallSetVector<std::tuple<PhiOperand, SILValue>, 4> worklist;

  // Find all reborrows of value and insert the (reborrow, base value) pair into
  // the worklist.
  auto collectReborrows = [&](SILValue value, SILValue baseValue) {
    BorrowedValue(value).visitLocalScopeEndingUses([&](Operand *op) {
      if (op->getOperandOwnership() == OperandOwnership::Reborrow) {
        worklist.insert(std::make_tuple(PhiOperand(op), baseValue));
      }
      return true;
    });
  };

  // Initialize the worklist.
  collectReborrows(borrowInst, borrowInst->getOperand());

  // For every (reborrow, base value) pair in the worklist:
  // - Find phi value and new base value
  // - Call the visitor on the phi value and new base value pair
  // - Populate the worklist with pairs of reborrows of phi value and the new
  // base.
  for (unsigned idx = 0; idx < worklist.size(); idx++) {
    PhiOperand phiOp;
    SILValue currentBaseValue;
    std::tie(phiOp, currentBaseValue) = worklist[idx];

    auto *phiValue = phiOp.getValue();
    SILValue newBaseValue = currentBaseValue;

    // If the previous base value was also passed as a phi operand along with
    // the reborrow, its phi value will be the new base value.
    for (auto &op : phiOp.getBranch()->getAllOperands()) {
      PhiOperand otherPhiOp(&op);
      if (otherPhiOp.getSource() != currentBaseValue) {
        continue;
      }
      newBaseValue = otherPhiOp.getValue();
    }

    // Call the visitor function
    visitReborrowPhiBaseValuePair(phiValue, newBaseValue);

    collectReborrows(phiValue, newBaseValue);
  }
}

void swift::visitExtendedGuaranteedForwardingPhiBaseValuePairs(
    BorrowedValue borrow, function_ref<void(SILPhiArgument *, SILValue)>
                              visitGuaranteedForwardingPhiBaseValuePair) {
  assert(borrow.kind == BorrowedValueKind::BeginBorrow ||
         borrow.kind == BorrowedValueKind::LoadBorrow);
  // A GuaranteedForwardingPhi can have different base values on different
  // control flow paths.
  // For that reason, worklist stores (GuaranteedForwardingPhi operand, base
  // value) pairs. We need a SetVector to make sure we don't revisit the same
  // pair again.
  SmallSetVector<std::tuple<PhiOperand, SILValue>, 4> worklist;

  auto collectGuaranteedForwardingPhis = [&](SILValue value,
                                             SILValue baseValue) {
    visitGuaranteedForwardingPhisForSSAValue(value, [&](Operand *op) {
      worklist.insert(std::make_tuple(PhiOperand(op), baseValue));
      return true;
    });
  };

  // Collect all GuaranteedForwardingPhis
  collectGuaranteedForwardingPhis(borrow.value, borrow.value);
  borrow.visitTransitiveLifetimeEndingUses([&](Operand *endUse) {
    if (endUse->getOperandOwnership() == OperandOwnership::Reborrow) {
      auto *phiValue = PhiOperand(endUse).getValue();
      collectGuaranteedForwardingPhis(phiValue, phiValue);
    }
    return true;
  });
  // For every (GuaranteedForwardingPhi operand, base value) pair in the
  // worklist:
  // - Find phi value and new base value
  // - Call the visitor on the phi value and new base value pair
  // - Populate the worklist with pairs of GuaranteedForwardingPhi ops of phi
  // value and the new base.
  for (unsigned idx = 0; idx < worklist.size(); idx++) {
    PhiOperand phiOp;
    SILValue currentBaseValue;
    std::tie(phiOp, currentBaseValue) = worklist[idx];

    auto *phiValue = phiOp.getValue();
    SILValue newBaseValue = currentBaseValue;

    // If an adjacent reborrow is found in the same block as the guaranteed phi,
    // then set newBaseValue to the reborrow.
    for (auto &op : phiOp.getBranch()->getAllOperands()) {
      PhiOperand otherPhiOp(&op);
      if (otherPhiOp.getSource() != currentBaseValue) {
        continue;
      }
      newBaseValue = otherPhiOp.getValue();
    }

    // Call the visitor function
    visitGuaranteedForwardingPhiBaseValuePair(phiValue, newBaseValue);

    collectGuaranteedForwardingPhis(phiValue, newBaseValue);
  }
}

/// Visit the phis in the same block as \p phi which are reborrows of a borrow
/// of one of the values reaching \p phi.
///
/// If the visitor returns false, stops visiting and returns false.  Otherwise,
/// returns true.
///
///
/// When an owned value is passed as a phi argument, it is consumed.  So any
/// open scope borrowing that owned value must be ended no later than in that
/// branch instruction.  Either such a borrow scope is ended beforehand
///     %lifetime = begin_borrow %value
///     ...
///     end_borrow %lifetime <-- borrow scope ended here
///     br block(%value)        <-- before consume
/// or the borrow scope is ended in the same instruction as the owned value is
/// consumed
///     %lifetime = begin_borrow %value
///     ...
///     end_borrow %lifetime
///     br block(%value, %lifetime)         <-- borrow scope ended here
///                                         <-- in same instruction as consume
/// In particular, the following is invalid
///         %lifetime = begin_borrow %value
///         ...
///         br block(%value)
///     block(%value_2 : @owned):
///         end_borrow %lifetime
///         destroy_value %value_2
/// because %lifetime was guaranteed by %value but value is consumed at
/// `br two`.
///
/// Similarly, when a guaranteed value is passed as a phi argument, its borrow
/// scope ends and a new borrow scope is begun.  And so any open nested borrow
/// of the original outer borrow must be ended no later than in that branch
/// instruction.
///
///
/// Given an phi argument
///     block(..., %value : @owned, ...)
/// this function finds the adjacent reborrow phis
///     block(..., %lifetime : @guaranteed, ..., %value : @owned, ...)
///                ^^^^^^^^^^^^^^^^^^^^^^^
/// one of whose reaching values is a borrow of a reaching value of %value.
///
/// Finding these is more complicated than merely looking for guaranteed
/// operands adjacent to the incoming operands to phi and which are borrows of
/// the value consumed there.  The reason is that they might not be borrows of
/// that incoming value _directly_ but rather reborrows of some other reborrow
/// if the incoming value is itself a phi argument:
///         %lifetime = begin_borrow %value
///         br one(%value, %lifetime)
///     one(%value_1 : @owned, %lifetime_1 : @guaranteed)
///         br two(%value_1, %lifetime_1)
///     two(%value_2 : @owned, %lifetime_2 : @guaranteed)
///         end_borrow %lifetime_2
///         destroy_value %value_2
///
/// When called with %value_2, \p visitor is invoked with both:
///     two(%value_2 : @owned, %lifetime_2 : @guaranteed)
///                            ^^^^^^^^^^^^^^^^^^^^^^^^^
bool swift::visitAdjacentReborrowsOfPhi(
    SILPhiArgument *phi, function_ref<bool(SILPhiArgument *)> visitor) {
  assert(phi->isPhi());

  // First, collect all the values that reach \p phi, that is:
  // - operands to the phi
  // - operands to phis which are operands to the phi
  // - and so forth.
  SmallPtrSet<SILValue, 8> reachingValues;
  // At the same time, record all the phis in \p phi's phi web: the phis which
  // are transitively operands to \p phi.  This is the subset of \p
  // reachingValues that are phis.
  SmallVector<SILPhiArgument *, 4> phis;
  phi->visitTransitiveIncomingPhiOperands(
      [&](auto *phi, auto *operand) -> bool {
        phis.push_back(phi);
        reachingValues.insert(phi);
        reachingValues.insert(operand->get());
        return true;
      });

  // Second, find all the guaranteed phis one of whose operands _could_ (by
  // dint of being adjacent to a phi in the phi web with the appropriate
  // ownership and type) be a reborrow of a reaching value of \p phi.
  SmallVector<SILPhiArgument *, 4> candidates;
  for (auto *phi : phis) {
    SILBasicBlock *block = phi->getParentBlock();
    for (auto *uncastAdjacent : block->getArguments()) {
      auto *adjacent = cast<SILPhiArgument>(uncastAdjacent);
      if (adjacent == phi)
        continue;
      if (adjacent->getType() != phi->getType())
        continue;
      if (adjacent->getOwnershipKind() != OwnershipKind::Guaranteed)
        continue;
      candidates.push_back(adjacent);
    }
  }

  // Finally, look through \p candidates to find those one of whose incoming
  // operands either
  // (1) borrow one of reaching values of \p phi
  // or (2) is itself a guaranteed phi which does so.
  // Because we may discover a reborrow R1 of type (1) after visiting another
  // R2 of type (2) which reborrows R1, we need to iterate to a fixed point.
  //
  // Record all the phis that we see which are borrows or reborrows of a
  // reaching value \p so that we can check for case (2) above.
  //
  // Visit those phis which are both reborrows of a reaching value AND are in
  // the same block as \phi.
  //
  // For example, given
  //
  //         %lifetime = begin_borrow %value
  //         br one(%value, %lifetime)
  //     one(%value_1 : @owned, %lifetime_1 : @guaranteed)
  //         br two(%value_1, %lifetime_1)
  //     two(%value_2 : @owned, %lifetime_2 : @guaranteed)
  //         end_borrow %lifetime_2
  //         destroy_value %value_2
  //
  // when visiting the reborrow phis adjacent to %value_2, The following steps
  // would be taken:
  //
  // (1) Look at the first candidate:
  //   two(%value_2 : @owned, %lifetime_2 : @guaranteed)
  //                          ^^^^^^^^^^^^^^^^^^^^^^^^^
  // but see that its one incoming value
  //   br two(%value_1, %lifetime_1)
  //                    ^^^^^^^^^^^
  // although a phi argument itself, is not known (yet!) to be a reborrow phi.
  // So the first candidate is NOT (yet!) added to reborrowPhis.
  //
  // (2) Look at the second candidate:
  //     one(%value_1 : @owned, %lifetime_1 : @guaranteed)
  //                            ^^^^^^^^^^^^^^^^^^^^^^^^^
  // and see that one of its incoming values
  //   br one(%value, %lifetime)
  //                  ^^^^^^^^^
  // is a borrow
  //   %lifetime = begin_borrow %value
  // of %value, one of the values reaching %value_2.
  // So the second candidate IS added to reborrowPhis.
  // AND changed is set to true, so we will repeat the outer loop.
  // But this candidate is not adjacent to our phi %value_2, so it is not
  // visited.
  //
  // (4.5) Changed is true: repeat the outer loop.  Set changed to false.
  //
  // (3) Look at the first candidate:
  //   two(%value_2 : @owned, %lifetime_2 : @guaranteed)
  //                          ^^^^^^^^^^^^^^^^^^^^^^^^^
  // and see that one of its incoming values
  //   br two(%value_1, %lifetime_1)
  //                    ^^^^^^^^^^^
  // is itself a phi
  //   one(%value_1 : @owned, %lifetime_1 : @guaranteed)
  //                          ^^^^^^^^^^^^^^^^^^^^^^^^^
  // which was added to reborrowPhis in (2).
  // So the first candidate IS added to reborrowPhis.
  // AND changed is set to true.
  // ALSO, see that the first candidate IS adjacent to our phi %value_2, so our
  // visitor is invoked with the first candidate.
  //
  // (4) Look at the second candidate.
  // See that it is already a member of reborrowPhis.
  //
  // (4.5) Changed is true: repeat the outer loop.  Set changed to false.
  //
  // (5) Look at the first candidate.
  // See that it is already a member of reborrowPhis.
  //
  // (6) Look at the second candidate.
  // See that it is already a member of reborrowPhis.
  //
  // (6.5) Changed is false: exit the outer loop.
  bool changed = false;
  SmallSetVector<SILPhiArgument *, 4> reborrowPhis;
  do {
    changed = false;
    for (auto *candidate : candidates) {
      if (reborrowPhis.contains(candidate))
        continue;
      auto success = candidate->visitIncomingPhiOperands([&](auto *operand) {
        // If the value being reborrowed is itself a reborrow of a value
        // reaching \p phi, then visit it.
        SILPhiArgument *forwarded;
        if ((forwarded = dyn_cast<SILPhiArgument>(operand->get()))) {
          if (!reborrowPhis.contains(forwarded))
            return true;
          changed = true;
          reborrowPhis.insert(candidate);
          if (candidate->getParentBlock() == phi->getParentBlock())
            return visitor(candidate);
          return true;
        }
        BeginBorrowInst *bbi;
        if (!(bbi = dyn_cast<BeginBorrowInst>(operand->get())))
          return true;
        auto borrowee = bbi->getOperand();
        if (!reachingValues.contains(borrowee))
          return true;
        changed = true;
        reborrowPhis.insert(candidate);
        if (candidate->getParentBlock() == phi->getParentBlock())
          return visitor(candidate);
        return true;
      });
      if (!success)
        return false;
    }
  } while (changed);

  return true;
}

void swift::visitTransitiveEndBorrows(
    SILValue value,
    function_ref<void(EndBorrowInst *)> visitEndBorrow) {
  GraphNodeWorklist<SILValue, 4> worklist;
  worklist.insert(value);

  while (!worklist.empty()) {
    auto val = worklist.pop();
    for (auto *consumingUse : val->getConsumingUses()) {
      auto *consumingUser = consumingUse->getUser();
      if (auto *branch = dyn_cast<BranchInst>(consumingUser)) {
        auto *succBlock = branch->getSingleSuccessorBlock();
        auto *phiArg = cast<SILPhiArgument>(
            succBlock->getArgument(consumingUse->getOperandNumber()));
        worklist.insert(phiArg);
      } else {
        visitEndBorrow(cast<EndBorrowInst>(consumingUser));
      }
    }
  }
}

/// Whether the specified lexical begin_borrow instruction is nested.
///
/// A begin_borrow [lexical] is nested if the borrowed value's lifetime is
/// guaranteed by another lexical scope.  That happens if:
/// - the value is a guaranteed argument to the function
/// - the value is itself a begin_borrow [lexical]
bool swift::isNestedLexicalBeginBorrow(BeginBorrowInst *bbi) {
  assert(bbi->isLexical());
  auto value = bbi->getOperand();
  if (auto *outerBBI = dyn_cast<BeginBorrowInst>(value)) {
    return outerBBI->isLexical();
  }
  if (auto *arg = dyn_cast<SILFunctionArgument>(value)) {
    return arg->getOwnershipKind() == OwnershipKind::Guaranteed;
  }
  return false;
}
