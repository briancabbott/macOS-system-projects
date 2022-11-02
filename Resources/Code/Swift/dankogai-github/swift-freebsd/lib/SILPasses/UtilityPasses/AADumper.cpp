//===-------- AADumper.cpp - Compare all values in Function with AA -------===//
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
//
// This pass collects all values in a function and applies alias analysis to
// them. The purpose of this is to enable unit tests for SIL Alias Analysis
// implementations independent of any other passes.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "sil-aa-evaluator"
#include "swift/SILPasses/Passes.h"
#include "swift/SIL/SILArgument.h"
#include "swift/SIL/SILFunction.h"
#include "swift/SIL/SILValue.h"
#include "swift/SILAnalysis/AliasAnalysis.h"
#include "swift/SILAnalysis/SideEffectAnalysis.h"
#include "swift/SILAnalysis/Analysis.h"
#include "swift/SILPasses/Transforms.h"
#include "llvm/Support/Debug.h"

using namespace swift;

//===----------------------------------------------------------------------===//
//                               Value Gatherer
//===----------------------------------------------------------------------===//

// Return a list of all instruction values in Fn. Returns true if we have at
// least two values to compare.
static bool gatherValues(SILFunction &Fn, std::vector<SILValue> &Values) {
  for (auto &BB : Fn) {
    for (auto *Arg : BB.getBBArgs())
      Values.push_back(SILValue(Arg));
    for (auto &II : BB)
      for (unsigned i = 0, e = II.getNumTypes(); i != e; ++i)
        Values.push_back(SILValue(&II, i));
  }
  return Values.size() > 1;
}

//===----------------------------------------------------------------------===//
//                              Top Level Driver
//===----------------------------------------------------------------------===//

namespace {

/// Dumps the alias relations between all instructions of a function.
class SILAADumper : public SILModuleTransform {

  void run() override {
    for (auto &Fn: *getModule()) {
      llvm::outs() << "@" << Fn.getName() << "\n";
      // Gather up all Values in Fn.
      std::vector<SILValue> Values;
      if (!gatherValues(Fn, Values))
        continue;

      AliasAnalysis *AA = PM->getAnalysis<AliasAnalysis>();

      // A cache
      llvm::DenseMap<uint64_t, AliasAnalysis::AliasResult> Results;

      // Emit the N^2 alias evaluation of the values.
      unsigned PairCount = 0;
      for (unsigned i1 = 0, e1 = Values.size(); i1 != e1; ++i1) {
        for (unsigned i2 = 0, e2 = Values.size(); i2 != e2; ++i2) {
          auto V1 = Values[i1];
          auto V2 = Values[i2];

          auto Result =
              AA->alias(V1, V2, computeTBAAType(V1), computeTBAAType(V2));

          // Results should always be the same. But if they are different print
          // it out so we find the error. This should make our test results less
          // verbose.
          uint64_t Key = uint64_t(i1) | (uint64_t(i2) << 32);
          uint64_t OpKey = uint64_t(i2) | (uint64_t(i1) << 32);
          auto R = Results.find(OpKey);
          if (R != Results.end() && R->second == Result)
            continue;

          Results[Key] = Result;
          llvm::outs() << "PAIR #" << PairCount++ << ".\n" << V1 << V2 << Result
                      << "\n";
        }
      }
          llvm::outs() << "\n";
    }
  }

  StringRef getName() override { return "AA Dumper"; }
};

/// Dumps the memory behavior of instructions in a function.
class MemBehaviorDumper : public SILModuleTransform {
  
  // To reduce the amount of output, we only dump the memory behavior of
  // selected types of instructions.
  static bool shouldTestInstruction(SILInstruction *I) {
    // Only consider function calls.
    if (FullApplySite::isa(I))
      return true;
    
    return false;
  }
  
  void run() override {
    for (auto &Fn: *getModule()) {
      llvm::outs() << "@" << Fn.getName() << "\n";
      // Gather up all Values in Fn.
      std::vector<SILValue> Values;
      if (!gatherValues(Fn, Values))
        continue;

      AliasAnalysis *AA = PM->getAnalysis<AliasAnalysis>();
      SideEffectAnalysis *SEA = PM->getAnalysis<SideEffectAnalysis>();
      SEA->recompute();

      unsigned PairCount = 0;
      for (auto &BB : Fn) {
        for (auto &I : BB) {
          if (shouldTestInstruction(&I)) {

            // Print the memory behavior in relation to all other values in the
            // function.
            for (auto &V : Values) {
              bool Read = AA->mayReadFromMemory(&I, V);
              bool Write = AA->mayWriteToMemory(&I, V);
              bool SideEffects = AA->mayHaveSideEffects(&I, V);
              llvm::outs() <<
              "PAIR #" << PairCount++ << ".\n" <<
              "  " << SILValue(&I) <<
              "  " << V <<
              "  r=" << Read << ",w=" << Write << ",se=" << SideEffects << "\n";
            }
          }
        }
      }
      llvm::outs() << "\n";
    }
  }
  
  StringRef getName() override { return "Memory Behavior Dumper"; }
};
        
} // end anonymous namespace

SILTransform *swift::createAADumper() { return new SILAADumper(); }

SILTransform *swift::createMemBehaviorDumper() {
  return new MemBehaviorDumper();
}
