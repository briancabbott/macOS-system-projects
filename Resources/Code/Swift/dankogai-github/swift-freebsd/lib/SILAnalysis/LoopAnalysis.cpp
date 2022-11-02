//===-------------- LoopAnalysis.cpp - SIL Loop Analysis -*- C++ -*--------===//
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

#include "swift/SIL/Dominance.h"
#include "swift/SILAnalysis/DominanceAnalysis.h"
#include "swift/SILAnalysis/LoopAnalysis.h"
#include "swift/SILPasses/PassManager.h"
#include "llvm/Support/Debug.h"

using namespace swift;

SILLoopInfo *SILLoopAnalysis::newFunctionAnalysis(SILFunction *F) {
  assert(DA != nullptr && "Expect a valid dominance analysis");
  DominanceInfo *DT = DA->get(F);
  assert(DT != nullptr && "Expect a valid dominance information");
  return new SILLoopInfo(F, DT);
}

void SILLoopAnalysis::initialize(SILPassManager *PM) {
  DA = PM->getAnalysis<DominanceAnalysis>();
}

SILAnalysis *swift::createLoopAnalysis(SILModule *M) {
  return new SILLoopAnalysis(M);
}
