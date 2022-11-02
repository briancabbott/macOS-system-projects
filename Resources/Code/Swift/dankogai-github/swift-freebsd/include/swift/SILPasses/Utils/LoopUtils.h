//===--- LoopUtils.h ------------------------------------------------------===//
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
///
/// This header file declares utility functions for simplifying and
/// canonicalizing loops.
///
//===----------------------------------------------------------------------===//

#ifndef SWIFT_SILPASSES_UTILS_LOOPUTILS_H
#define SWIFT_SILPASSES_UTILS_LOOPUTILS_H

namespace swift {

class SILFunction;
class SILBasicBlock;
class SILLoop;
class DominanceInfo;
class SILLoopInfo;

/// Canonicalize the loop for rotation and downstream passes.
///
/// Create a single preheader and single latch block.
bool canonicalizeLoop(SILLoop *L, DominanceInfo *DT, SILLoopInfo *LI);

/// Canonicalize all loops in the function F for which \p LI contains loop
/// information. We update loop info and dominance info while we do this.
bool canonicalizeAllLoops(DominanceInfo *DT, SILLoopInfo *LI);

/// A visitor that visits loops in a function in a bottom up order. It only
/// performs the visit.
class SILLoopVisitor {
  SILFunction *F;
  SILLoopInfo *LI;

public:
  SILLoopVisitor(SILFunction *Func, SILLoopInfo *LInfo) : F(Func), LI(LInfo) {}
  virtual ~SILLoopVisitor() {}

  void run();

  SILFunction *getFunction() const { return F; }

  virtual void runOnLoop(SILLoop *L) = 0;
  virtual void runOnFunction(SILFunction *F) = 0;
};

} // end swift namespace

#endif
