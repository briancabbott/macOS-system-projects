//===--- Local.h - Local SIL transformations. -------------------*- C++ -*-===//
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

#ifndef SWIFT_SILPASSES_UTILS_LOCAL_H
#define SWIFT_SILPASSES_UTILS_LOCAL_H

#include "swift/Basic/ArrayRefView.h"
#include "swift/SILAnalysis/SimplifyInstruction.h"
#include "swift/SIL/SILInstruction.h"
#include "swift/SIL/SILBuilder.h"
#include "swift/SIL/SILCloner.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/Support/Allocator.h"
#include <functional>
#include <utility>

namespace swift {

class DominanceInfo;

/// Transform a Use Range (Operand*) into a User Range (SILInstruction*)
using UserTransform = std::function<SILInstruction *(Operand *)>;
using ValueBaseUserRange =
  TransformRange<IteratorRange<ValueBase::use_iterator>, UserTransform>;
using ValueUserRange =
  TransformRange<IteratorRange<SILValue::use_iterator>, UserTransform>;

inline ValueBaseUserRange makeUserRange(
    iterator_range<ValueBase::use_iterator> R) {
  auto toUser = [](Operand *O) { return O->getUser(); };
  return makeTransformRange(makeIteratorRange(R.begin(), R.end()),
                            UserTransform(toUser));
}
inline ValueUserRange makeUserRange(iterator_range<SILValue::use_iterator> R) {
  auto toUser = [](Operand *O) { return O->getUser(); };
  return makeTransformRange(makeIteratorRange(R.begin(), R.end()),
                            UserTransform(toUser));
}

/// \brief For each of the given instructions, if they are dead delete them
/// along with their dead operands.
///
/// \param I The instruction to be deleted.
/// \param Force If Force is set, don't check if the top level instructions
///        are considered dead - delete them regardless.
/// \param C a callback called whenever an instruction is deleted.
void
recursivelyDeleteTriviallyDeadInstructions(
  ArrayRef<SILInstruction*> I, bool Force = false,
  std::function<void(SILInstruction *)> C = [](SILInstruction *){});

/// \brief If the given instruction is dead, delete it along with its dead
/// operands.
///
/// \param I The instruction to be deleted.
/// \param Force If Force is set, don't check if the top level instruction is
///        considered dead - delete it regardless.
/// \param C a callback called whenever an instruction is deleted.
void
recursivelyDeleteTriviallyDeadInstructions(
  SILInstruction *I,
  bool Force = false,
  std::function<void(SILInstruction *)> C = [](SILInstruction *){});

/// \brief Perform a fast local check to see if the instruction is dead.
///
/// This routine only examines the state of the instruction at hand.
bool isInstructionTriviallyDead(SILInstruction *I);

/// \brief Recursively erase all of the uses of the instruction (but not the
/// instruction itself) and delete instructions that will become trivially
/// dead when this instruction is removed.
void eraseUsesOfInstruction(
    SILInstruction *Inst,
    std::function<void(SILInstruction *)> C = [](SILInstruction *){});

/// \brief Recursively erase all of the uses of the value (but not the
/// value itself)
void eraseUsesOfValue(SILValue V);

FullApplySite findApplyFromDevirtualizedResult(SILInstruction *I);

/// Cast a value into the expected, ABI compatible type if necessary.
/// This may happen e.g. when:
/// - a type of the return value is a subclass of the expected return type.
/// - actual return type and expected return type differ in optionality.
/// - both types are tuple-types and some of the elements need to be casted.
///
/// If CheckOnly flag is set, then this function only checks if the
/// required casting is possible. If it is not possible, then None
/// is returned.
/// If CheckOnly is not set, then a casting code is generated and the final
/// casted value is returned.
Optional<SILValue> castValueToABICompatibleType(SILBuilder *B, SILLocation Loc,
                                                SILValue Value,
                                                SILType SrcTy,
                                                SILType DestTy,
                                                bool CheckOnly = false);

/// Check if the optimizer can cast a value into the expected,
/// ABI compatible type if necessary.
bool canCastValueToABICompatibleType(SILModule &M,
                                     SILType SrcTy, SILType DestTy);

/// Replace an apply with an instruction that produces the same value,
/// then delete the apply and the instructions that produce its callee
/// if possible.
void replaceDeadApply(ApplySite Old, ValueBase *New);

/// \brief Return true if the substitution map contains a
/// substitution that is an unbound generic type.
bool hasUnboundGenericTypes(TypeSubstitutionMap &SubsMap);

/// Return true if the substitution list contains a substitution
/// that is an unbound generic.
bool hasUnboundGenericTypes(ArrayRef<Substitution> Subs);

/// \brief Return true if the substitution map contains a
/// substitution that refers to the dynamic Self type.
bool hasDynamicSelfTypes(TypeSubstitutionMap &SubsMap);

/// \brief Return true if the substitution list contains a
/// substitution that refers to the dynamic Self type.
bool hasDynamicSelfTypes(ArrayRef<Substitution> Subs);

/// \brief Return true if any call inside the given function may bind dynamic
/// 'Self' to a generic argument of the callee.
bool computeMayBindDynamicSelf(SILFunction *F);

/// \brief Move an ApplyInst's FuncRef so that it dominates the call site.
void placeFuncRef(ApplyInst *AI, DominanceInfo *DT);

/// \brief Add an argument, \p val, to the branch-edge that is pointing into
/// block \p Dest. Return a new instruction and do not erase the old
/// instruction.
TermInst *addArgumentToBranch(SILValue Val, SILBasicBlock *Dest,
                              TermInst *Branch);

/// Handle the mechanical aspects of removing an unreachable block.
void removeDeadBlock(SILBasicBlock *BB);

/// Remove all instructions in the body of \p BB in safe manner by using
/// undef.
void clearBlockBody(SILBasicBlock *BB);

/// \brief Get the linkage to be used for specializations of a function with
/// the given linkage.
SILLinkage getSpecializedLinkage(SILFunction *F, SILLinkage L);

/// Tries to optimize a given apply instruction if it is a concatenation of
/// string literals. Returns a new instruction if optimization was possible.
SILInstruction *tryToConcatenateStrings(ApplyInst *AI, SILBuilder &B);

/// Tries to perform jump-threading on a given checked_cast_br terminator.
bool tryCheckedCastBrJumpThreading(TermInst *Term, DominanceInfo *DT,
                                   SmallVectorImpl<SILBasicBlock *> &BBs);

/// Checks if a symbol with a given linkage can be referenced from fragile
/// functions.
bool isValidLinkageForFragileRef(SILLinkage linkage);

/// A structure containing callbacks that are called when an instruction is
/// removed or added.
struct InstModCallbacks {
  using CallbackTy = std::function<void (SILInstruction *)>;
  CallbackTy DeleteInst = [](SILInstruction *I) {
    I->eraseFromParent();
  };
  CallbackTy CreatedNewInst = [](SILInstruction *){};

  InstModCallbacks(CallbackTy DeleteInst, CallbackTy CreatedNewInst)
    : DeleteInst(DeleteInst), CreatedNewInst(CreatedNewInst) {}
  InstModCallbacks() = default;
  ~InstModCallbacks() = default;
  InstModCallbacks(const InstModCallbacks &) = default;
  InstModCallbacks(InstModCallbacks &&) = default;
};

/// If Closure is a partial_apply or thin_to_thick_function with only local
/// ref count users and a set of post-dominating releases:
///
/// 1. Remove all ref count operations and the closure.
/// 2. Add each one of the last release locations insert releases for the
///    captured args if we have a partial_apply.
///
/// In the future this should be extended to be less conservative with users.
bool
tryDeleteDeadClosure(SILInstruction *Closure,
                     InstModCallbacks Callbacks = InstModCallbacks());

/// Given a SILValue argument to a partial apply \p Arg and the associated
/// parameter info for that argument, perform the necessary cleanups to Arg when
/// one is attempting to delete the partial apply.
void releasePartialApplyCapturedArg(
    SILBuilder &Builder, SILLocation Loc, SILValue Arg, SILParameterInfo PInfo,
    InstModCallbacks Callbacks = InstModCallbacks());

/// This represents the lifetime of a single SILValue.
struct ValueLifetime {
  llvm::SmallSetVector<SILInstruction *, 4> LastUsers;

  ValueLifetime() {}
  ValueLifetime(ValueLifetime &&Ref) { LastUsers = std::move(Ref.LastUsers); }
  ValueLifetime &operator=(ValueLifetime &&Ref) {
    LastUsers = std::move(Ref.LastUsers);
    return *this;
  }

  ArrayRef<SILInstruction*> getLastUsers() const {
    return ArrayRef<SILInstruction*>(LastUsers.begin(), LastUsers.end());
  }
};

/// This computes the lifetime of a single SILValue.
///
/// This does not compute a set of jointly postdominating use points. Instead it
/// assumes that the value's existing uses already jointly postdominate the
/// definition. This makes sense for values that are returned +1 from an
/// instruction, like partial_apply, and therefore must be released on all paths
/// via strong_release or apply.
class ValueLifetimeAnalysis {
public:
  SILValue DefValue;
  llvm::SmallPtrSet<SILBasicBlock *, 16> LiveIn;
  llvm::SmallSetVector<SILBasicBlock *, 16> UseBlocks;
  // UserSet is nonempty if the client provides a user list. Otherwise we only
  // consider direct uses.
  llvm::SmallPtrSet<SILInstruction*, 16> UserSet;

  ValueLifetimeAnalysis(SILValue DefValue): DefValue(DefValue) {}

  template<typename UserList>
  ValueLifetime computeFromUserList(UserList Users) {
    return computeFromUserList(Users, std::true_type());
  }

  ValueLifetime computeFromDirectUses() {
    return computeFromUserList(makeUserRange(DefValue.getUses()),
                               std::false_type());
  }

  bool successorHasLiveIn(SILBasicBlock *BB);
  SILInstruction *findLastDirectUseInBlock(SILBasicBlock *BB);
  SILInstruction *findLastSpecifiedUseInBlock(SILBasicBlock *BB);

private:
  template<typename UserList, typename RecordUser>
  ValueLifetime computeFromUserList(UserList Users, RecordUser);
  void visitUser(SILInstruction *User);
  void propagateLiveness();
  ValueLifetime computeLastUsers();
};

template<typename UserList, typename RecordUser>
ValueLifetime ValueLifetimeAnalysis::
computeFromUserList(UserList Users, RecordUser RU) {
  for (SILInstruction *User : Users) {
    visitUser(User);
    if (RecordUser::value)
      UserSet.insert(User);
  }
  propagateLiveness();
  return computeLastUsers();
}

/// Base class for BB cloners.
class BaseThreadingCloner : public SILClonerWithScopes<BaseThreadingCloner> {
  friend class SILVisitor<BaseThreadingCloner>;
  friend class SILCloner<BaseThreadingCloner>;

  protected:
  SILBasicBlock *FromBB, *DestBB;

  public:
  // A map of old to new available values.
  SmallVector<std::pair<ValueBase *, SILValue>, 16> AvailVals;

  /// If WithinFunction is true, the debug scopes of the cloned
  /// instructions will not be updated.
  BaseThreadingCloner(SILFunction &To, bool WithinFunction)
      : SILClonerWithScopes(To, WithinFunction), FromBB(nullptr),
        DestBB(nullptr) {}

  BaseThreadingCloner(SILFunction &To, SILBasicBlock *From, SILBasicBlock *Dest)
      : SILClonerWithScopes(To, From->getParent() == &To), FromBB(From),
        DestBB(Dest) {}

  void process(SILInstruction *I) { visit(I); }

  SILBasicBlock *remapBasicBlock(SILBasicBlock *BB) { return BB; }

  SILValue remapValue(SILValue Value) {
    // If this is a use of an instruction in another block, then just use it.
    if (auto SI = dyn_cast<SILInstruction>(Value)) {
      if (SI->getParent() != FromBB)
        return Value;
    } else if (auto BBArg = dyn_cast<SILArgument>(Value)) {
      if (BBArg->getParent() != FromBB)
        return Value;
    } else {
      assert(isa<SILUndef>(Value) && "Unexpected Value kind");
      return Value;
    }

    return SILCloner<BaseThreadingCloner>::remapValue(Value);
  }

  void postProcess(SILInstruction *Orig, SILInstruction *Cloned) {
    DestBB->push_back(Cloned);
    SILCloner<BaseThreadingCloner>::postProcess(Orig, Cloned);
    // A terminator defines no values. Keeping terminators in the AvailVals list
    // is problematic because terminators get replaced during SSA update.
    if (!isa<TermInst>(Orig))
      AvailVals.push_back(std::make_pair(Orig, SILValue(Cloned, 0)));
  }
};

/// Clone a basic block to edge \p BI.
class EdgeThreadingCloner : public BaseThreadingCloner {
public:
  EdgeThreadingCloner(BranchInst *BI)
      : BaseThreadingCloner(*BI->getFunction(),
                            BI->getDestBB(), nullptr) {
    DestBB = createEdgeBlockAndRedirectBranch(BI);
  }

  SILBasicBlock *createEdgeBlockAndRedirectBranch(BranchInst *BI) {
    auto *Fn = BI->getFunction();
    auto *SrcBB = BI->getParent();
    auto *DestBB = BI->getDestBB();
    auto *EdgeBB = new (Fn->getModule()) SILBasicBlock(Fn, SrcBB);

    // Create block arguments.
    unsigned ArgIdx = 0;
    for (auto Arg : BI->getArgs()) {
      assert(Arg.getType() == DestBB->getBBArg(ArgIdx)->getType() &&
             "Types must match");
      auto *BlockArg = EdgeBB->createBBArg(Arg.getType());
      ValueMap[DestBB->getBBArg(ArgIdx)] = SILValue(BlockArg);
      AvailVals.push_back(std::make_pair(DestBB->getBBArg(ArgIdx), BlockArg));
      ++ArgIdx;
    }

    // Redirect the branch.
    SILBuilderWithScope(BI).createBranch(BI->getLoc(), EdgeBB, BI->getArgs());
    BI->eraseFromParent();
    return EdgeBB;
  }

  SILBasicBlock *getEdgeBB() {
    // DestBB really is the edge basic block we created to clone instructions
    // to.
    return DestBB;
  }
};

/// Helper class for cloning of basic blocks.
class BasicBlockCloner : public BaseThreadingCloner {
  public:
    BasicBlockCloner(SILBasicBlock *From, SILBasicBlock *To = nullptr,
                     bool WithinFunction = true)
        : BaseThreadingCloner(To ? *To->getParent() : *From->getParent(),
                              WithinFunction) {
      FromBB = From;
      if (To == nullptr) {
        // Create a new BB that is to be used as a target
        // for cloning.
        To = From->getParent()->createBasicBlock();
        for (auto *Arg : FromBB->getBBArgs()) {
          To->createBBArg(Arg->getType(), Arg->getDecl());
        }
      }
      DestBB = To;

      // Populate the value map so that uses of the BBArgs in the SrcBB are
      // replaced with the BBArgs of the DestBB.
      for (unsigned i = 0, e = FromBB->bbarg_size(); i != e; ++i) {
        ValueMap[FromBB->getBBArg(i)] = DestBB->getBBArg(i);
        AvailVals.push_back(
            std::make_pair(FromBB->getBBArg(i), DestBB->getBBArg(i)));
      }
    }

    // Clone all instructions of the FromBB into DestBB
    void clone() {
      for (auto &I : *FromBB)
        process(&I);
    }

    SILBasicBlock *getDestBB() { return DestBB; }
};

/// Helper function to perform SSA updates in case of jump threading. Set
/// 'NeedToSplitCriticalEdges' to false if all critical edges are split,
/// otherwise this call will try to split all critical edges.
void updateSSAAfterCloning(BaseThreadingCloner &Cloner, SILBasicBlock *SrcBB,
                           SILBasicBlock *DestBB,
                           bool NeedToSplitCriticalEdges = true);

/// \brief This is a helper class used to optimize casts.
class CastOptimizer {
  // Callback to be called when uses of an instruction should be replaced.
  std::function<void (SILInstruction *I, ValueBase *V)> ReplaceInstUsesAction;

  // Callback to call when an instruction needs to be erased.
  std::function<void (SILInstruction *)> EraseInstAction;

  // Callback to call after an optimization was performed based on the fact
  // that a cast will succeed.
  std::function<void ()> WillSucceedAction;

  // Callback to call after an optimization was performed based on the fact
  // that a cast will fail.
  std::function<void ()> WillFailAction;

  /// Optimize a cast from a bridged ObjC type into
  /// a corresponding Swift type implementing _ObjectiveCBridgeable.
  SILInstruction *
  optimizeBridgedObjCToSwiftCast(SILInstruction *Inst,
      bool isConditional,
      SILValue Src,
      SILValue Dest,
      CanType Source,
      CanType Target,
      Type BridgedSourceTy,
      Type BridgedTargetTy,
      SILBasicBlock *SuccessBB,
      SILBasicBlock *FailureBB);

  /// Optimize a cast from   a Swift type implementing _ObjectiveCBridgeable
  /// into a bridged ObjC type.
  SILInstruction *
  optimizeBridgedSwiftToObjCCast(SILInstruction *Inst,
      bool isConditional,
      SILValue Src,
      SILValue Dest,
      CanType Source,
      CanType Target,
      Type BridgedSourceTy,
      Type BridgedTargetTy,
      SILBasicBlock *SuccessBB,
      SILBasicBlock *FailureBB);

public:
  CastOptimizer(std::function<void (SILInstruction *I, ValueBase *V)> ReplaceInstUsesAction,
                std::function<void (SILInstruction *)> EraseAction = [](SILInstruction*){},
                std::function<void ()> WillSucceedAction = [](){},
                std::function<void ()> WillFailAction = [](){})
    : ReplaceInstUsesAction(ReplaceInstUsesAction),
      EraseInstAction(EraseAction),
      WillSucceedAction(WillSucceedAction),
      WillFailAction(WillFailAction) {}

  /// Simplify checked_cast_br. It may change the control flow.
  SILInstruction *
  simplifyCheckedCastBranchInst(CheckedCastBranchInst *Inst);

  /// Simplify checked_cast_addr_br. It may change the control flow.
  SILInstruction *
  simplifyCheckedCastAddrBranchInst(CheckedCastAddrBranchInst *Inst);

  /// Optimize checked_cast_br. This cannot change the control flow.
  SILInstruction *
  optimizeCheckedCastBranchInst(CheckedCastBranchInst *Inst);

  /// Optimize checked_cast_addr_br. This cannot change the control flow.
  SILInstruction *
  optimizeCheckedCastAddrBranchInst(CheckedCastAddrBranchInst *Inst);

  /// Optimize unconditional_checked_cast. This cannot change the control flow.
  ValueBase *
  optimizeUnconditionalCheckedCastInst(UnconditionalCheckedCastInst *Inst);

  /// Optimize unconditional_checked_cast_addr. This cannot change the control
  /// flow.
  SILInstruction *
  optimizeUnconditionalCheckedCastAddrInst(UnconditionalCheckedCastAddrInst *Inst);

  /// Check if is is a bridged cast and optimize it.
  /// May change the control flow.
  SILInstruction *
  optimizeBridgedCasts(SILInstruction *Inst,
      bool isConditional,
      SILValue Src,
      SILValue Dest,
      CanType Source,
      CanType Target,
      SILBasicBlock *SuccessBB,
      SILBasicBlock *FailureBB);

};

// Helper class that provides a callback that can be used in
// inliners/cloners for collecting new call sites.
class CloneCollector {
public:
  typedef std::pair<SILInstruction *, SILInstruction *> value_type;
  typedef std::function<void(SILInstruction *, SILInstruction *)> CallbackType;
  typedef std::function<bool (SILInstruction *)> FilterType;

private:
  FilterType Filter;

  // Pairs of collected instructions; (new, old)
  llvm::SmallVector<value_type, 4> InstructionPairs;

  void collect(SILInstruction *Old, SILInstruction *New) {
    if (Filter(New))
      InstructionPairs.push_back(std::make_pair(New, Old));
  }

public:
  CloneCollector(FilterType Filter) : Filter(Filter) {}

  CallbackType getCallback() {
    return std::bind(&CloneCollector::collect, this, std::placeholders::_1,
                     std::placeholders::_2);
  }

  llvm::SmallVectorImpl<value_type> &getInstructionPairs() {
    return InstructionPairs;
  }
};

/// This iterator 'looks through' one level of builtin expect users exposing all
/// users of the looked through builtin expect instruction i.e it presents a
/// view that shows all users as if there were no builtin expect instructions
/// interposed.
class IgnoreExpectUseIterator
    : public std::iterator<std::forward_iterator_tag, Operand *, ptrdiff_t> {
  ValueBaseUseIterator OrigUseChain;
  ValueBaseUseIterator CurrentIter;

  static bool isExpect(Operand *Use) {
    if (auto *BI = dyn_cast<BuiltinInst>(Use->getUser()))
      if (BI->getIntrinsicInfo().ID == llvm::Intrinsic::expect)
        return true;
    return false;
  }

  // Advance through expect users to their users until we encounter a user that
  // is not an expect.
  void advanceThroughExpects() {
    while (CurrentIter == OrigUseChain &&
           CurrentIter != ValueBaseUseIterator(nullptr) &&
           isExpect(*CurrentIter)) {
      auto *Expect = CurrentIter->getUser();
      CurrentIter = Expect->use_begin();
      // Expect with no users advance to next item in original use chain.
      if (CurrentIter == Expect->use_end())
        CurrentIter = ++OrigUseChain;
    }
  }

public:
  IgnoreExpectUseIterator(ValueBase *V)
      : OrigUseChain(V->use_begin()), CurrentIter(V->use_begin()) {
    advanceThroughExpects();
  }

  IgnoreExpectUseIterator() = default;

  Operand *operator*() const { return *CurrentIter; }
  Operand *operator->() const { return *CurrentIter; }
  SILInstruction *getUser() const { return this->getUser(); }

  IgnoreExpectUseIterator &operator++() {
    assert(**this && "increment past end()!");
    if (OrigUseChain == CurrentIter) {
      // Use chain of the original value.
      ++OrigUseChain;
      ++CurrentIter;
      // Ignore expects.
      advanceThroughExpects();
      } else {
      // Use chain of an expect.
      ++CurrentIter;
      if (CurrentIter == ValueBaseUseIterator(nullptr)) {
        // At the end of the use chain of an expect.
        CurrentIter = ++OrigUseChain;
        advanceThroughExpects();
      }
    }
    return *this;
  }

  IgnoreExpectUseIterator operator++(int unused) {
    IgnoreExpectUseIterator Copy = *this;
    ++*this;
    return Copy;
  }
  friend bool operator==(IgnoreExpectUseIterator lhs,
                         IgnoreExpectUseIterator rhs) {
    return lhs.CurrentIter == rhs.CurrentIter;
  }
  friend bool operator!=(IgnoreExpectUseIterator lhs,
                         IgnoreExpectUseIterator rhs) {
    return !(lhs == rhs);
  }
};

inline iterator_range<IgnoreExpectUseIterator>
ignore_expect_uses(ValueBase *V) {
  return make_range(IgnoreExpectUseIterator(V),
                    IgnoreExpectUseIterator());
}

/// Run simplifyInstruction() on all of the instruction I's users if they only
/// have one result (since simplifyInstruction assumes that). Replace all uses
/// of the user with its simplification of we succeed. Returns true if we
/// succeed and false otherwise.
///
/// An example of how this is useful is in cases where one is splitting up an
/// aggregate and reforming it, the reformed aggregate may have extract
/// operations from it. These can be simplified and removed.
bool simplifyUsers(SILInstruction *I);

/// Check if a given type is a simple type, i.e. a builtin
/// integer or floating point type or a struct/tuple whose members
/// are of simple types.
bool isSimpleType(SILType SILTy, SILModule& Module);

/// Check if the value of V is computed by means of a simple initialization.
/// Store the actual SILValue into \p Val and the reversed list of instructions
/// initializing it in \p Insns.
/// The check is performed by recursively walking the computation of the
/// SIL value being analyzed.
bool analyzeStaticInitializer(SILValue V,
                              SmallVectorImpl<SILInstruction *> &Insns);

/// Replace load sequence which may contain
/// a chain of struct_element_addr followed by a load.
/// The sequence is traversed inside out, i.e.
/// starting with the innermost struct_element_addr
void replaceLoadSequence(SILInstruction *I,
                         SILInstruction *Value,
                         SILBuilder &B);


/// Do we have enough information to determine all callees that could
/// be reached by calling the function represented by Decl?
bool calleesAreStaticallyKnowable(SILModule &M, SILDeclRef Decl);

} // end namespace swift

#endif
