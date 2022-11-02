//===-------- Passes.cpp - Swift Compiler SIL Pass Entrypoints ------------===//
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
///  \file
///  \brief This file provides implementations of a few helper functions
///  which provide abstracted entrypoints to the SILPasses stage.
///
///  \note The actual SIL passes should be implemented in per-pass source files,
///  not in this file.
///
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "sil-optimizer"

#include "swift/SILPasses/Passes.h"
#include "swift/SILPasses/PassManager.h"
#include "swift/SILPasses/Transforms.h"
#include "swift/SILPasses/Utils/Local.h"
#include "swift/SILAnalysis/Analysis.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/Module.h"
#include "swift/SIL/SILModule.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/YAMLParser.h"

llvm::cl::opt<bool>
    SILViewCFG("sil-view-cfg", llvm::cl::init(false),
               llvm::cl::desc("Enable the sil cfg viewer pass"));

llvm::cl::opt<bool>
    SILViewGuaranteedCFG("sil-view-guaranteed-cfg", llvm::cl::init(false),
               llvm::cl::desc("Enable the sil cfg viewer pass after diagnostics"));

llvm::cl::opt<bool>
    SILViewSILGenCFG("sil-view-silgen-cfg", llvm::cl::init(false),
               llvm::cl::desc("Enable the sil cfg viewer pass before diagnostics"));

using namespace swift;

// Enumerates the optimization kinds that we do in SIL.
enum OptimizationLevelKind {
  LowLevel,
  MidLevel,
  HighLevel,
};

bool swift::runSILDiagnosticPasses(SILModule &Module) {
  // Verify the module, if required.
  if (Module.getOptions().VerifyAll)
    Module.verify();

  // If we parsed a .sil file that is already in canonical form, don't rerun
  // the diagnostic passes.
  if (Module.getStage() == SILStage::Canonical)
    return false;

  auto &Ctx = Module.getASTContext();

  SILPassManager PM(&Module);

  if (SILViewSILGenCFG) {
    PM.resetAndRemoveTransformations();
    PM.addCFGPrinter();
    PM.runOneIteration();
  }

  // If we are asked do debug serialization, instead of running all diagnostic
  // passes, just run mandatory inlining with dead transparent function cleanup
  // disabled.
  if (Module.getOptions().DebugSerialization) {
    PM.addMandatoryInlining();
    PM.run();
    return Ctx.hadError();
  }

  // Otherwise run the rest of diagnostics.
  PM.addCapturePromotion();
  PM.addAllocBoxToStack();
  PM.addInOutDeshadowing();
  PM.addNoReturnFolding();
  PM.addDefiniteInitialization();

  PM.addMandatoryInlining();
  PM.addPredictableMemoryOptimizations();
  PM.addDiagnosticConstantPropagation();
  PM.addDiagnoseUnreachable();
  PM.addEmitDFDiagnostics();
  // Canonical swift requires all non cond_br critical edges to be split.
  PM.addSplitNonCondBrCriticalEdges();
  PM.run();

  // Generate diagnostics.
  Module.setStage(SILStage::Canonical);

  if (SILViewGuaranteedCFG) {
    PM.resetAndRemoveTransformations();
    PM.addCFGPrinter();
    PM.runOneIteration();
  }

  // Verify the module, if required.
  if (Module.getOptions().VerifyAll)
    Module.verify();
  else {
    DEBUG(Module.verify());
  }

  // If errors were produced during SIL analysis, return true.
  return Ctx.hadError();
}

void AddSimplifyCFGSILCombine(SILPassManager &PM) {
  PM.addSimplifyCFG();
  // Jump threading can expose opportunity for silcombine (enum -> is_enum_tag->
  // cond_br).
  PM.addSILCombine();
  // Which can expose opportunity for simplifcfg.
  PM.addSimplifyCFG();
}

/// Perform semantic annotation/loop base optimizations.
void AddHighLevelLoopOptPasses(SILPassManager &PM) {
  // Perform classsic SSA optimizations for cleanup.
  PM.addLowerAggregateInstrs();
  PM.addSILCombine();
  PM.addSROA();
  PM.addMem2Reg();
  PM.addDCE();
  PM.addSILCombine();
  AddSimplifyCFGSILCombine(PM);

  // Run high-level loop opts.
  PM.addLoopRotate();

  // Cleanup.
  PM.addDCE();
  // Also CSE semantic calls.
  PM.addHighLevelCSE();
  PM.addSILCombine();
  PM.addSimplifyCFG();
  PM.addArrayCountPropagation();
  PM.addHighLevelLICM();
  PM.addRemovePins();
  PM.addABCOpt();
  // Cleanup.
  PM.addDCE();
  PM.addCOWArrayOpts();
  // Cleanup.
  PM.addDCE();
  PM.addSwiftArrayOpts();
}

void AddSSAPasses(SILPassManager &PM, OptimizationLevelKind OpLevel) {
  AddSimplifyCFGSILCombine(PM);
  PM.addAllocBoxToStack();
  PM.addCopyForwarding();
  PM.addLowerAggregateInstrs();
  PM.addSILCombine();
  PM.addSROA();
  PM.addMem2Reg();

  // Perform classsic SSA optimizations.
  PM.addGlobalOpt();
  PM.addLetPropertiesOpt();
  PM.addPerformanceConstantPropagation();
  PM.addDCE();
  PM.addCSE();
  PM.addSILCombine();
  PM.addJumpThreadSimplifyCFG();
  // Jump threading can expose opportunity for silcombine (enum -> is_enum_tag->
  // cond_br).
  PM.addSILCombine();
  // Which can expose opportunity for simplifcfg.
  PM.addSimplifyCFG();

  // Perform retain/release code motion and run the first ARC optimizer.
  PM.addRedundantLoadElimination();
  PM.addDeadStoreElimination();
  PM.addCSE();
  PM.addEarlyCodeMotion();
  PM.addARCSequenceOpts();

  PM.addSILLinker();

  switch (OpLevel) {
    case OptimizationLevelKind::HighLevel:
      // Does not inline functions with defined semantics.
      PM.addEarlyInliner();
      break;
    case OptimizationLevelKind::MidLevel:
      // Does inline semantics-functions (except "availability"), but not
      // global-init functions.
      PM.addPerfInliner();
      break;
    case OptimizationLevelKind::LowLevel:
      // Inlines everything
      PM.addLateInliner();
      break;
  }
  PM.addSimplifyCFG();
  // Only hoist releases very late.
  if (OpLevel == OptimizationLevelKind::LowLevel)
    PM.addLateCodeMotion();
  else
    PM.addEarlyCodeMotion();
  PM.addARCSequenceOpts();
  PM.addRemovePins();
  PM.addUpdateSideEffects();
}


void swift::runSILOptimizationPasses(SILModule &Module) {
  // Verify the module, if required.
  if (Module.getOptions().VerifyAll)
    Module.verify();

  if (Module.getOptions().DebugSerialization) {
    SILPassManager PM(&Module);
    PM.addSILLinker();
    PM.run();
    return;
  }

  SILPassManager PM(&Module, "PreSpecialize");

  // Start by cloning functions from stdlib.
  PM.addSILLinker();
  PM.run();
  PM.resetAndRemoveTransformations();

  // Run two iterations of the high-level SSA passes.
  PM.setStageName("HighLevel");
  AddSSAPasses(PM, OptimizationLevelKind::HighLevel);
  PM.runOneIteration();
  PM.runOneIteration();
  PM.resetAndRemoveTransformations();

  PM.setStageName("EarlyLoopOpt");
  AddHighLevelLoopOptPasses(PM);

  PM.addDeadFunctionElimination();
  PM.addDeadObjectElimination();
  PM.addGlobalPropertyOpt();
  PM.addUpdateEscapeAnalysis();

  // Do the first stack promotion on high-level SIL.
  PM.addStackPromotion();

  PM.runOneIteration();
  PM.resetAndRemoveTransformations();


  // Run two iterations of the mid-level SSA passes.
  PM.setStageName("MidLevel");
  AddSSAPasses(PM, OptimizationLevelKind::MidLevel);
  PM.runOneIteration();
  PM.runOneIteration();
  PM.resetAndRemoveTransformations();

  // Perform lowering optimizations.
  PM.setStageName("Lower");
  PM.addDeadFunctionElimination();
  PM.addDeadObjectElimination();

  // Hoist globals out of loops.
  // Global-init functions should not be inlined GlobalOpt is done.
  PM.addGlobalOpt();
  PM.addLetPropertiesOpt();

  // Propagate constants into closures and convert to static dispatch.  This
  // should run after specialization and inlining because we don't want to
  // specialize a call that can be inlined. It should run before
  // ClosureSpecialization, because constant propagation is more effective.  At
  // least one round of SSA optimization and inlining should run after this to
  // take advantage of static dispatch.
  PM.addCapturePropagation();

  // Specialize closure.
  PM.addClosureSpecializer();

  PM.addUpdateEscapeAnalysis();

  // Do the second stack promotion on low-level SIL.
  PM.addStackPromotion();

  // Speculate virtual call targets.
  PM.addSpeculativeDevirtualization();

  // We do this late since it is a pass like the inline caches that we only want
  // to run once very late. Make sure to run at least one round of the ARC
  // optimizer after this.
  PM.addFunctionSignatureOpts();

  PM.runOneIteration();
  PM.resetAndRemoveTransformations();

  // Run another iteration of the SSA optimizations to optimize the
  // devirtualized inline caches and constants propagated into closures
  // (CapturePropagation).

  PM.setStageName("LowLevel");

  // Should be after FunctionSignatureOpts and before the last inliner.
  PM.addReleaseDevirtualizer();

  PM.addLateInliner();
  AddSimplifyCFGSILCombine(PM);
  PM.addAllocBoxToStack();

  // The ReleaseDevirtualizer + specialization (in the inliner) can produce
  // aggregates in specialized deinit functions, which can be lowered.
  PM.addLowerAggregateInstrs();

  PM.addSROA();
  PM.addMem2Reg();
  PM.addCSE();
  PM.addSILCombine();
  PM.addJumpThreadSimplifyCFG();
  PM.addRedundantLoadElimination();
  PM.addDeadStoreElimination();
  PM.addCSE();
  PM.addLateCodeMotion();
  PM.addARCSequenceOpts();

  PM.runOneIteration();
  PM.resetAndRemoveTransformations();
  PM.addCodeSinking();

  PM.setStageName("LateLoopOpt");
  PM.addLICM();

  // Perform the final lowering transformations.
  PM.addExternalFunctionDefinitionsElimination();
  PM.addDeadFunctionElimination();

  // Optimize overflow checks:
  PM.addRedundantOverflowCheckRemoval();
  PM.addMergeCondFails();

  // Remove dead code.
  PM.addDCE();
  // Clean-up after DCE.
  PM.addSimplifyCFG();
  PM.runOneIteration();

  // Call the CFG viewer.
  if (SILViewCFG) {
    PM.resetAndRemoveTransformations();
    PM.addCFGPrinter();
    PM.runOneIteration();
  }

  // Verify the module, if required.
  if (Module.getOptions().VerifyAll)
    Module.verify();
  else {
    DEBUG(Module.verify());
  }
}

void swift::runSILPassesForOnone(SILModule &Module) {
  // Verify the module, if required.
  if (Module.getOptions().VerifyAll)
    Module.verify();

  SILPassManager PM(&Module, "Onone");

  // First specialize user-code.
  PM.addUsePrespecialized();
  PM.run();
  PM.resetAndRemoveTransformations();

  // Don't keep external functions from stdlib and other modules.
  // We don't want that our unoptimized version will be linked instead
  // of the optimized version from the stdlib.
  // Here we just convert external definitions to declarations. LLVM will
  // eventually remove unused declarations.
  PM.addExternalDefsToDecls();

  PM.runOneIteration();

  // Verify the module, if required.
  if (Module.getOptions().VerifyAll)
    Module.verify();
  else {
    DEBUG(Module.verify());
  }
}

#ifndef NDEBUG

namespace {

struct PMDescriptor {
  llvm::SmallString<32> Id;
  llvm::SmallString<32> ActionName;
  Optional<unsigned> ActionCount;
  std::vector<llvm::SmallString<32>> Passes;

  explicit PMDescriptor(llvm::yaml::SequenceNode *Descriptor);
  ~PMDescriptor() = default;
  PMDescriptor(const PMDescriptor &) = delete;
  PMDescriptor(PMDescriptor &&) = default;

  static void
  descriptorsForFile(StringRef Filename,
                     llvm::SmallVectorImpl<PMDescriptor> &Descriptors);
};

} // end anonymous namespace

void
PMDescriptor::
descriptorsForFile(StringRef Filename,
                   llvm::SmallVectorImpl<PMDescriptor> &Descriptors) {
  namespace yaml = llvm::yaml;

  // Load the input file.
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> FileBufOrErr =
    llvm::MemoryBuffer::getFileOrSTDIN(Filename);
  if (!FileBufOrErr) {
    llvm_unreachable("Failed to read yaml file");
  }

  StringRef Buffer = FileBufOrErr->get()->getBuffer();
  llvm::SourceMgr SM;
  yaml::Stream Stream(Buffer, SM);
  yaml::document_iterator DI = Stream.begin();
  assert(DI != Stream.end() && "Failed to read a document");
  yaml::Node *N = DI->getRoot();
  assert(N && "Failed to find a root");

  auto *RootList = cast<yaml::SequenceNode>(N);

  for (auto &PMDescriptorIter : make_range(RootList->begin(), RootList->end())) {
    PMDescriptor PM(cast<yaml::SequenceNode>(&PMDescriptorIter));
    Descriptors.push_back(std::move(PM));
  }
}

/// Our general format is as follows:
///
///   [
///     [
///       "PASS_MANAGER_ID",
///       "run_n_times"|"run_to_fixed_point",
///       count,
///       "PASS1", "PASS2", ...
///     ],
///     ...
///   ]
///
/// Where "id" is printed out when we process the action, "action" can be one of
/// "run_n_times", "run_to_fixed_point" and "passes" is a list of passes to
/// run. The names to use are the stringified versions of pass kinds.
PMDescriptor::PMDescriptor(llvm::yaml::SequenceNode *Desc) {
  namespace yaml = llvm::yaml;

  yaml::SequenceNode::iterator DescIter = Desc->begin();
  Id = cast<yaml::ScalarNode>(&*DescIter)->getRawValue();
  ++DescIter;

  ActionName = cast<yaml::ScalarNode>(&*DescIter)->getRawValue();
  unsigned ActionSize = ActionName.size()-2;
  ActionName = ActionName.substr(1, ActionSize);
  ++DescIter;

  auto *ActionCountValue = cast<yaml::ScalarNode>(&*DescIter);
  APInt APCount(64, ActionCountValue->getRawValue(), 10);
  ActionCount = APCount.getLimitedValue(UINT_MAX);
  ++DescIter;

  for (auto DescEnd = Desc->end(); DescIter != DescEnd; ++DescIter) {
    StringRef PassName = cast<yaml::ScalarNode>(&*DescIter)->getRawValue();
    unsigned Size = PassName.size()-2;
    Passes.push_back(PassName.substr(1, Size));
  }
}

#endif

void swift::runSILOptimizationPassesWithFileSpecification(SILModule &Module,
                                                          StringRef FileName) {
#ifndef NDEBUG
  llvm::SmallVector<PMDescriptor, 4> Descriptors;
  PMDescriptor::descriptorsForFile(FileName, Descriptors);

  for (auto &Desc : Descriptors) {
    DEBUG(llvm::dbgs() << "Creating PM: " << Desc.Id << "\n");
    SILPassManager PM(&Module, Desc.Id);

    for (auto &P : Desc.Passes) {
      DEBUG(llvm::dbgs() << "  Adding Pass: " << P << "\n");
      PM.addPassForName(P);
    }

    if (Desc.ActionName.equals("run_n_times")) {
      unsigned Count = Desc.ActionCount.getValue();
      DEBUG(llvm::dbgs() << "    Running " << Count << " iterations...\n");
      for (unsigned i = 0, e = Count; i < e; ++i) {
        PM.runOneIteration();
      }
    } else if (Desc.ActionName.equals("run_to_fixed_point")) {
      DEBUG(llvm::dbgs() << "    Running until fixed point...\n");
      PM.run();
    } else {
      llvm_unreachable("unknown action");
    }
  }
#endif
}
