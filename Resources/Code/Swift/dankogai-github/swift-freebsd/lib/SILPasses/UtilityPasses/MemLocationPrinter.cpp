///===--- MemLocationPrinter.cpp - Dump all memory locations in program ---===//
///
/// This source file is part of the Swift.org open source project
///
/// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
/// Licensed under Apache License v2.0 with Runtime Library Exception
///
/// See http://swift.org/LICENSE.txt for license information
/// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
///
///===---------------------------------------------------------------------===//
///
/// This pass tests type expansion, memlocation expansion and memlocation
/// reduction.
///
///===---------------------------------------------------------------------===//

#define DEBUG_TYPE "sil-memlocation-dumper"
#include "swift/SILPasses/Passes.h"
#include "swift/SIL/Projection.h"
#include "swift/SIL/MemLocation.h"
#include "swift/SIL/SILFunction.h"
#include "swift/SIL/SILValue.h"
#include "swift/SILAnalysis/Analysis.h"
#include "swift/SILPasses/Transforms.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace swift;

//===----------------------------------------------------------------------===//
//                              Top Level Driver
//===----------------------------------------------------------------------===//

namespace {

enum class MLKind : unsigned {
  OnlyExpansion = 0,
  OnlyReduction = 1,
  OnlyTypeExpansion = 2,
  All = 3,
};

} // end anonymous namespace

static llvm::cl::opt<MLKind> MemLocationKinds(
    "ml", llvm::cl::desc("MemLocation Kinds:"), llvm::cl::init(MLKind::All),
    llvm::cl::values(
        clEnumValN(MLKind::OnlyExpansion, "only-expansion", "only-expansion"),
        clEnumValN(MLKind::OnlyReduction, "only-reduction", "only-reduction"),
        clEnumValN(MLKind::OnlyTypeExpansion, "only-type-expansion",
                   "only-type-expansion"),
        clEnumValN(MLKind::All, "all", "all"), clEnumValEnd));

namespace {

class MemLocationPrinter : public SILModuleTransform {

  /// Dumps the expansions of SILType accessed in the function.
  /// This tests the expandTypeIntoLeafProjectionPaths function, which is
  /// a function used extensively in expand and reduce functions.
  ///
  /// We test it to catch any suspicious things in the earliest point.
  ///
  void printTypeExpansion(SILFunction &Fn) {
    SILModule *M = &Fn.getModule();
    ProjectionPathList PPList;
    unsigned Counter = 0;
    for (auto &BB : Fn) {
      for (auto &II : BB) {
        if (auto *LI = dyn_cast<LoadInst>(&II)) {
          SILValue V = LI->getOperand();
          // This is an address type, take it object type.
          SILType Ty = V.getType().getObjectType();
          ProjectionPath::expandTypeIntoLeafProjectionPaths(Ty, M, PPList,
                                                            true);
        } else if (auto *SI = dyn_cast<StoreInst>(&II)) {
          SILValue V = SI->getDest();
          // This is an address type, take it object type.
          SILType Ty = V.getType().getObjectType();
          ProjectionPath::expandTypeIntoLeafProjectionPaths(Ty, M, PPList,
                                                            true);
        } else {
          // Not interested in these instructions yet.
          continue;
        }

        llvm::outs() << "#" << Counter++ << II;
        for (auto &T : PPList) {
          llvm::outs() << T.getValue();
        }
        PPList.clear();
      }
    }
    llvm::outs() << "\n";
  }

  /// Dumps the expansions of memory locations accessed in the function.
  /// This tests the expand function in MemLocation class.
  ///
  /// We test it to catch any suspicious things when memory location is
  /// expanded, i.e. base is traced back and aggregate is expanded
  /// properly.
  void printMemExpansion(SILFunction &Fn) {
    MemLocation L;
    TypeExpansionMap Types;
    MemLocationList Locs;
    unsigned Counter = 0;
    for (auto &BB : Fn) {
      for (auto &II : BB) {
        if (auto *LI = dyn_cast<LoadInst>(&II)) {
          L.initialize(LI->getOperand());
          if (!L.isValid())
            continue;
          MemLocation::expand(L, &Fn.getModule(), Locs, Types);
        } else if (auto *SI = dyn_cast<StoreInst>(&II)) {
          L.initialize(SI->getDest());
          if (!L.isValid())
            continue;
          MemLocation::expand(L, &Fn.getModule(), Locs, Types);
        } else {
          // Not interested in these instructions yet.
          continue;
        }

        llvm::outs() << "#" << Counter++ << II;
        for (auto &Loc : Locs) {
          Loc.print();
        }
        L.reset();
        Locs.clear();
      }
    }
    llvm::outs() << "\n";
  }

  /// Dumps the reductions of set of memory locations.
  ///
  /// This function first calls expand on a memory location. It then calls
  /// reduce, in hope to get the original memory location back.
  ///
  void printMemReduction(SILFunction &Fn) {
    MemLocation L;
    MemLocationList Locs;
    TypeExpansionMap Types;
    llvm::DenseSet<MemLocation> SLocs;
    unsigned Counter = 0;
    for (auto &BB : Fn) {
      for (auto &II : BB) {
   
        // Expand it first.
        //
        if (auto *LI = dyn_cast<LoadInst>(&II)) {
          L.initialize(LI->getOperand());
          if (!L.isValid())
            continue;
          MemLocation::expand(L, &Fn.getModule(), Locs, Types);
        } else if (auto *SI = dyn_cast<StoreInst>(&II)) {
          L.initialize(SI->getDest());
          if (!L.isValid())
            continue;
          MemLocation::expand(L, &Fn.getModule(), Locs, Types);
        } else {
          // Not interested in these instructions yet.
          continue;
        }

        // Try to reduce it.
        //
        // Add into the set in reverse order, Reduction should not care
        // about the order of the memory locations in the set.
        for (auto I = Locs.rbegin(); I != Locs.rend(); ++I) {
          SLocs.insert(*I);
        }
        // This should get the original (unexpanded) location back.
        MemLocation::reduce(L, &Fn.getModule(), SLocs);
        llvm::outs() << "#" << Counter++ << II;
        for (auto &Loc : SLocs) {
          Loc.print();
        }
        L.reset();
        Locs.clear();
        SLocs.clear();
      }
    }
    llvm::outs() << "\n";
  }

  void run() override {
    for (auto &Fn : *getModule()) {
      if (Fn.isExternalDeclaration()) continue;

      llvm::outs() << "@" << Fn.getName() << "\n";
      switch (MemLocationKinds) {
        case MLKind::OnlyTypeExpansion:
          printTypeExpansion(Fn);
          break;
        case MLKind::OnlyExpansion:
          printMemExpansion(Fn);
          break;
        case MLKind::OnlyReduction:
          printMemReduction(Fn);
          break;
        default:
          break;
      }
    }
  }

  StringRef getName() override { return "Mem Location Dumper"; }
};

} // end anonymous namespace

SILTransform *swift::createMemLocationPrinter() {
  return new MemLocationPrinter();
}
