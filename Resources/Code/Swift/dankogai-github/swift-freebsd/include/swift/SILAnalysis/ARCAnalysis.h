//===--------------- ARCAnalysis.h - SIL ARC Analysis ----*- C++ -*--------===//
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

#ifndef SWIFT_SILANALYSIS_ARCANALYSIS_H
#define SWIFT_SILANALYSIS_ARCANALYSIS_H

#include "swift/SIL/SILValue.h"
#include "swift/SIL/SILBasicBlock.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/TinyPtrVector.h"

namespace swift {

class SILValue;
class SILInstruction;
class AliasAnalysis;
class PostOrderAnalysis;
class RCIdentityAnalysis;
class RCIdentityFunctionInfo;
class LoopRegionFunctionInfo;
class SILLoopInfo;
class SILFunction;

} // end namespace swift

namespace swift {

/// \returns True if the user \p User decrements the ref count of \p Ptr.
bool mayDecrementRefCount(SILInstruction *User, SILValue Ptr,
                          AliasAnalysis *AA);

/// \returns True if the user \p User checks the ref count of a pointer.
bool mayCheckRefCount(SILInstruction *User);

/// \returns True if the \p User might use the pointer \p Ptr in a manner that
/// requires \p Ptr to be alive before Inst.
bool mayUseValue(SILInstruction *User, SILValue Ptr, AliasAnalysis *AA);

/// \returns True if the \p User must use the pointer \p Ptr in a manner that
/// requires \p Ptr to be alive before Inst.
bool mustUseValue(SILInstruction *User, SILValue Ptr, AliasAnalysis *AA);

/// Returns true if User must use Ptr in a guaranteed way.
///
/// This means that assuming that everything is conservative, we can ignore the
/// ref count effects of User on Ptr since we will only remove things over
/// guaranteed parameters if we are known safe in both directions.
bool mustGuaranteedUseValue(SILInstruction *User, SILValue Ptr,
                            AliasAnalysis *AA);

/// Returns true if \p Inst can never conservatively decrement reference counts.
bool canNeverDecrementRefCounts(SILInstruction *Inst);

/// \returns True if \p User can never use a value in a way that requires the
/// value to be alive.
///
/// This is purposefully a negative query to contrast with canUseValue which is
/// about a specific value while this is about general values.
bool canNeverUseValues(SILInstruction *User);

/// \returns true if the user \p User may use \p Ptr in a manner that requires
/// Ptr's life to be guaranteed to exist at this point.
///
/// TODO: Better name.
bool mayGuaranteedUseValue(SILInstruction *User, SILValue Ptr,
                           AliasAnalysis *AA);

/// If \p Op has arc uses in the instruction range [Start, End), return the
/// first such instruction. Otherwise return None. We assume that
/// Start and End are both in the same basic block.
Optional<SILBasicBlock::iterator>
valueHasARCUsesInInstructionRange(SILValue Op,
                                  SILBasicBlock::iterator Start,
                                  SILBasicBlock::iterator End,
                                  AliasAnalysis *AA);

/// If \p Op has arc uses in the instruction range [Start, End), return the last
/// use of such instruction. Otherwise return None. We assume that Start and End
/// are both in the same basic block.
Optional<SILBasicBlock::iterator> valueHasARCUsesInReverseInstructionRange(
    SILValue Op, SILBasicBlock::iterator Start, SILBasicBlock::iterator End,
    AliasAnalysis *AA);

/// If \p Op has instructions in the instruction range (Start, End] which may
/// decrement it, return the first such instruction. Returns None
/// if no such instruction exists. We assume that Start and End are both in the
/// same basic block.
Optional<SILBasicBlock::iterator>
valueHasARCDecrementOrCheckInInstructionRange(SILValue Op,
                                              SILBasicBlock::iterator Start,
                                              SILBasicBlock::iterator End,
                                              AliasAnalysis *AA);

/// Match a call to a trap BB with no ARC relevant side effects.
bool isARCInertTrapBB(SILBasicBlock *BB);

/// A class that attempts to match owned arguments and corresponding epilogue
/// releases for a specific function.
///
/// TODO: This really needs a better name.
class ConsumedArgToEpilogueReleaseMatcher {
  llvm::SmallMapVector<SILArgument *, SILInstruction *, 8> ArgInstMap;

public:
  /// Default constructor: does not find matching releases, so
  /// findMatchingReleases should be called explicitly.
  ConsumedArgToEpilogueReleaseMatcher() { }

  /// Finds matching releases in the return block of the function \p F.
  ConsumedArgToEpilogueReleaseMatcher(RCIdentityFunctionInfo *RCIA,
                                      SILFunction *F);

  /// Finds matching releases in the provided block \p BB.
  void findMatchingReleases(RCIdentityFunctionInfo *RCIA, SILBasicBlock *BB);

  bool argumentHasRelease(SILArgument *Arg) const {
    return ArgInstMap.find(Arg) != ArgInstMap.end();
  }

  bool argumentHasRelease(SILValue V) const {
    auto *Arg = dyn_cast<SILArgument>(V);
    if (!Arg)
      return false;
    return argumentHasRelease(Arg);
  }

  SILInstruction *releaseForArgument(SILArgument *Arg) const {
    auto I = ArgInstMap.find(Arg);
    if (I == ArgInstMap.end())
      return nullptr;
    return I->second;
  }

  bool isReleaseMatchedToArgument(SILInstruction *Inst) const {
    auto Pred = [&Inst](const std::pair<SILArgument *,
                                        SILInstruction *> &P) -> bool {
      return P.second == Inst;
    };
    return std::count_if(ArgInstMap.begin(), ArgInstMap.end(), Pred);
  }

  using iterator = decltype(ArgInstMap)::iterator;
  using const_iterator = decltype(ArgInstMap)::const_iterator;
  iterator begin() { return ArgInstMap.begin(); }
  iterator end() { return ArgInstMap.end(); }
  const_iterator begin() const { return ArgInstMap.begin(); }
  const_iterator end() const { return ArgInstMap.end(); }

  using reverse_iterator = decltype(ArgInstMap)::reverse_iterator;
  using const_reverse_iterator = decltype(ArgInstMap)::const_reverse_iterator;
  reverse_iterator rbegin() { return ArgInstMap.rbegin(); }
  reverse_iterator rend() { return ArgInstMap.rend(); }
  const_reverse_iterator rbegin() const { return ArgInstMap.rbegin(); }
  const_reverse_iterator rend() const { return ArgInstMap.rend(); }

  unsigned size() const { return ArgInstMap.size(); }

  iterator_range<iterator> getRange() { return swift::make_range(begin(), end()); }
};

class ReleaseTracker {
  llvm::SmallSetVector<SILInstruction *, 4> TrackedUsers;
  llvm::SmallSetVector<SILInstruction *, 4> FinalReleases;
  std::function<bool(SILInstruction *)> AcceptableUserQuery;

public:
  ReleaseTracker(std::function<bool(SILInstruction *)> AcceptableUserQuery)
      : TrackedUsers(), FinalReleases(),
        AcceptableUserQuery(AcceptableUserQuery) {}

  void trackLastRelease(SILInstruction *Inst) { FinalReleases.insert(Inst); }

  bool isUserAcceptable(SILInstruction *User) const {
    return AcceptableUserQuery(User);
  }

  void trackUser(SILInstruction *User) { TrackedUsers.insert(User); }

  using range = iterator_range<llvm::SmallSetVector<SILInstruction *, 4>::iterator>;

  range getTrackedUsers() { return {TrackedUsers.begin(), TrackedUsers.end()}; }

  range getFinalReleases() {
    return {FinalReleases.begin(), FinalReleases.end()};
  }
};

/// Return true if we can find a set of post-dominating final releases. Returns
/// false otherwise. The FinalRelease set is placed in the out parameter
/// FinalRelease.
bool getFinalReleasesForValue(SILValue Value, ReleaseTracker &Tracker);

} // end namespace swift

#endif
