//===-- Internalize.cpp - Mark functions internal -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass loops over all of the functions and variables in the input module.
// If the function or variable is not in the list of external names given to
// the pass it is marked as internal.
//
// This transformation would not be legal in a regular compilation, but it gets
// extra information from the linker about what is safe.
//
// For example: Internalizing a function with external linkage. Only if we are
// told it is only used from within this module, it is safe to do it.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/IPO/Internalize.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/GlobalStatus.h"
#include <fstream>
#include <set>
using namespace llvm;

#define DEBUG_TYPE "internalize"

STATISTIC(NumAliases, "Number of aliases internalized");
STATISTIC(NumFunctions, "Number of functions internalized");
STATISTIC(NumGlobals, "Number of global vars internalized");

// APIFile - A file which contains a list of symbols that should not be marked
// external.
static cl::opt<std::string>
    APIFile("internalize-public-api-file", cl::value_desc("filename"),
            cl::desc("A file containing list of symbol names to preserve"));

// APIList - A list of symbols that should not be marked internal.
static cl::list<std::string>
    APIList("internalize-public-api-list", cl::value_desc("list"),
            cl::desc("A list of symbol names to preserve"), cl::CommaSeparated);

namespace {

// Helper to load an API list to preserve from file and expose it as a functor
// for internalization.
class PreserveAPIList {
public:
  PreserveAPIList() {
    if (!APIFile.empty())
      LoadFile(APIFile);
    ExternalNames.insert(APIList.begin(), APIList.end());
  }

  bool operator()(const GlobalValue &GV) {
    return ExternalNames.count(GV.getName());
  }

private:
  // Contains the set of symbols loaded from file
  StringSet<> ExternalNames;

  void LoadFile(StringRef Filename) {
    // Load the APIFile...
    std::ifstream In(Filename.data());
    if (!In.good()) {
      errs() << "WARNING: Internalize couldn't load file '" << Filename
             << "'! Continuing as if it's empty.\n";
      return; // Just continue as if the file were empty
    }
    while (In) {
      std::string Symbol;
      In >> Symbol;
      if (!Symbol.empty())
        ExternalNames.insert(Symbol);
    }
  }
};

// Internalization exposed as a pass
class InternalizePass : public ModulePass {
  // Client supplied callback to control wheter a symbol must be preserved.
  std::function<bool(const GlobalValue &)> MustPreserveGV;

public:
  static char ID; // Pass identification, replacement for typeid

  InternalizePass() : ModulePass(ID), MustPreserveGV(PreserveAPIList()) {}

  InternalizePass(std::function<bool(const GlobalValue &)> MustPreserveGV)
      : ModulePass(ID), MustPreserveGV(std::move(MustPreserveGV)) {
    initializeInternalizePassPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    CallGraphWrapperPass *CGPass =
        getAnalysisIfAvailable<CallGraphWrapperPass>();
    CallGraph *CG = CGPass ? &CGPass->getCallGraph() : nullptr;
    return internalizeModule(M, MustPreserveGV, CG);
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesCFG();
    AU.addPreserved<CallGraphWrapperPass>();
  }
};

// Helper class to perform internalization.
class Internalizer {
  // Client supplied callback to control wheter a symbol must be preserved.
  const std::function<bool(const GlobalValue &)> &MustPreserveGV;

  // Set of symbols private to the compiler that this pass should not touch.
  StringSet<> AlwaysPreserved;

  // Return false if we're allowed to internalize this GV.
  bool ShouldPreserveGV(const GlobalValue &GV) {
    // Function must be defined here
    if (GV.isDeclaration())
      return true;

    // Available externally is really just a "declaration with a body".
    if (GV.hasAvailableExternallyLinkage())
      return true;

    // Assume that dllexported symbols are referenced elsewhere
    if (GV.hasDLLExportStorageClass())
      return true;

    // Already local, has nothing to do.
    if (GV.hasLocalLinkage())
      return false;

    // Check some special cases
    if (AlwaysPreserved.count(GV.getName()))
      return true;

    return MustPreserveGV(GV);
  }

  bool maybeInternalize(GlobalValue &GV,
                        const std::set<const Comdat *> &ExternalComdats);
  void checkComdatVisibility(GlobalValue &GV,
                             std::set<const Comdat *> &ExternalComdats);

public:
  Internalizer(const std::function<bool(const GlobalValue &)> &MustPreserveGV)
      : MustPreserveGV(MustPreserveGV) {}

  /// Run the internalizer on \p TheModule, returns true if any changes was
  /// made.
  ///
  /// If the CallGraph \p CG is supplied, it will be updated when
  /// internalizing a function (by removing any edge from the "external node")
  bool internalizeModule(Module &TheModule, CallGraph *CG = nullptr);
};

// Internalize GV if it is possible to do so, i.e. it is not externally visible
// and is not a member of an externally visible comdat.
bool Internalizer::maybeInternalize(
    GlobalValue &GV, const std::set<const Comdat *> &ExternalComdats) {
  if (Comdat *C = GV.getComdat()) {
    if (ExternalComdats.count(C))
      return false;

    // If a comdat is not externally visible we can drop it.
    if (auto GO = dyn_cast<GlobalObject>(&GV))
      GO->setComdat(nullptr);

    if (GV.hasLocalLinkage())
      return false;
  } else {
    if (GV.hasLocalLinkage())
      return false;

    if (ShouldPreserveGV(GV))
      return false;
  }

  GV.setVisibility(GlobalValue::DefaultVisibility);
  GV.setLinkage(GlobalValue::InternalLinkage);
  return true;
}

// If GV is part of a comdat and is externally visible, keep track of its
// comdat so that we don't internalize any of its members.
void Internalizer::checkComdatVisibility(
    GlobalValue &GV, std::set<const Comdat *> &ExternalComdats) {
  Comdat *C = GV.getComdat();
  if (!C)
    return;

  if (ShouldPreserveGV(GV))
    ExternalComdats.insert(C);
}

bool Internalizer::internalizeModule(Module &M, CallGraph *CG) {
  bool Changed = false;
  CallGraphNode *ExternalNode = CG ? CG->getExternalCallingNode() : nullptr;

  SmallPtrSet<GlobalValue *, 8> Used;
  collectUsedGlobalVariables(M, Used, false);

  // Collect comdat visiblity information for the module.
  std::set<const Comdat *> ExternalComdats;
  if (!M.getComdatSymbolTable().empty()) {
    for (Function &F : M)
      checkComdatVisibility(F, ExternalComdats);
    for (GlobalVariable &GV : M.globals())
      checkComdatVisibility(GV, ExternalComdats);
    for (GlobalAlias &GA : M.aliases())
      checkComdatVisibility(GA, ExternalComdats);
  }

  // We must assume that globals in llvm.used have a reference that not even
  // the linker can see, so we don't internalize them.
  // For llvm.compiler.used the situation is a bit fuzzy. The assembler and
  // linker can drop those symbols. If this pass is running as part of LTO,
  // one might think that it could just drop llvm.compiler.used. The problem
  // is that even in LTO llvm doesn't see every reference. For example,
  // we don't see references from function local inline assembly. To be
  // conservative, we internalize symbols in llvm.compiler.used, but we
  // keep llvm.compiler.used so that the symbol is not deleted by llvm.
  for (GlobalValue *V : Used) {
    AlwaysPreserved.insert(V->getName());
  }

  // Mark all functions not in the api as internal.
  for (Function &I : M) {
    if (!maybeInternalize(I, ExternalComdats))
      continue;
    Changed = true;

    if (ExternalNode)
      // Remove a callgraph edge from the external node to this function.
      ExternalNode->removeOneAbstractEdgeTo((*CG)[&I]);

    ++NumFunctions;
    DEBUG(dbgs() << "Internalizing func " << I.getName() << "\n");
  }

  // Never internalize the llvm.used symbol.  It is used to implement
  // attribute((used)).
  // FIXME: Shouldn't this just filter on llvm.metadata section??
  AlwaysPreserved.insert("llvm.used");
  AlwaysPreserved.insert("llvm.compiler.used");

  // Never internalize anchors used by the machine module info, else the info
  // won't find them.  (see MachineModuleInfo.)
  AlwaysPreserved.insert("llvm.global_ctors");
  AlwaysPreserved.insert("llvm.global_dtors");
  AlwaysPreserved.insert("llvm.global.annotations");

  // Never internalize symbols code-gen inserts.
  // FIXME: We should probably add this (and the __stack_chk_guard) via some
  // type of call-back in CodeGen.
  AlwaysPreserved.insert("__stack_chk_fail");
  AlwaysPreserved.insert("__stack_chk_guard");

  // Mark all global variables with initializers that are not in the api as
  // internal as well.
  for (auto &GV : M.globals()) {
    if (!maybeInternalize(GV, ExternalComdats))
      continue;
    Changed = true;

    ++NumGlobals;
    DEBUG(dbgs() << "Internalized gvar " << GV.getName() << "\n");
  }

  // Mark all aliases that are not in the api as internal as well.
  for (auto &GA : M.aliases()) {
    if (!maybeInternalize(GA, ExternalComdats))
      continue;
    Changed = true;

    ++NumAliases;
    DEBUG(dbgs() << "Internalized alias " << GA.getName() << "\n");
  }

  return Changed;
}

} // end anonymous namespace

char InternalizePass::ID = 0;
INITIALIZE_PASS(InternalizePass, "internalize", "Internalize Global Symbols",
                false, false)

/// Public API below

bool llvm::internalizeModule(
    Module &TheModule,
    const std::function<bool(const GlobalValue &)> &MustPreserveGV,
    CallGraph *CG) {
  return Internalizer(MustPreserveGV).internalizeModule(TheModule, CG);
}

ModulePass *llvm::createInternalizePass() { return new InternalizePass(); }

ModulePass *llvm::createInternalizePass(
    std::function<bool(const GlobalValue &)> MustPreserveGV) {
  return new InternalizePass(std::move(MustPreserveGV));
}
