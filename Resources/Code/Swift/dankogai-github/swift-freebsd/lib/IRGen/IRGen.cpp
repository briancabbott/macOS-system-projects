//===--- IRGen.cpp - Swift LLVM IR Generation -----------------------------===//
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
//  This file implements the entrypoints into IR generation.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "irgen"
#include "swift/Subsystems.h"
#include "swift/AST/AST.h"
#include "swift/AST/DiagnosticsIRGen.h"
#include "swift/AST/IRGenOptions.h"
#include "swift/AST/LinkLibrary.h"
#include "swift/SIL/SILModule.h"
#include "swift/Basic/Dwarf.h"
#include "swift/Basic/Platform.h"
#include "swift/ClangImporter/ClangImporter.h"
#include "swift/LLVMPasses/PassesFwd.h"
#include "swift/LLVMPasses/Passes.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/Bitcode/BitcodeWriterPass.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRPrintingPasses.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Linker/Linker.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Mutex.h"
#include "llvm/ADT/StringSet.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetSubtargetInfo.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/ObjCARC.h"
#include "IRGenModule.h"

#include <thread>

using namespace swift;
using namespace irgen;
using namespace llvm;

static void addSwiftARCOptPass(const PassManagerBuilder &Builder,
                               PassManagerBase &PM) {
  if (Builder.OptLevel > 0)
    PM.add(createSwiftARCOptPass());
}

static void addSwiftContractPass(const PassManagerBuilder &Builder,
                               PassManagerBase &PM) {
  if (Builder.OptLevel > 0)
    PM.add(createSwiftARCContractPass());
}

static void addSwiftStackPromotionPass(const PassManagerBuilder &Builder,
                                       PassManagerBase &PM) {
  if (Builder.OptLevel > 0)
    PM.add(createSwiftStackPromotionPass());
}

// FIXME: Copied from clang/lib/CodeGen/CGObjCMac.cpp. 
// These should be moved to a single definition shared by clang and swift.
enum ImageInfoFlags {
  eImageInfo_FixAndContinue      = (1 << 0),
  eImageInfo_GarbageCollected    = (1 << 1),
  eImageInfo_GCOnly              = (1 << 2),
  eImageInfo_OptimizedByDyld     = (1 << 3),
  eImageInfo_CorrectedSynthesize = (1 << 4),
  eImageInfo_ImageIsSimulated    = (1 << 5)
};

std::tuple<llvm::TargetOptions, std::string, std::vector<std::string>>
swift::getIRTargetOptions(IRGenOptions &Opts, ASTContext &Ctx) {
  // Things that maybe we should collect from the command line:
  //   - relocation model
  //   - code model
  // FIXME: We should do this entirely through Clang, for consistency.
  TargetOptions TargetOpts;

  auto *Clang = static_cast<ClangImporter *>(Ctx.getClangModuleLoader());
  clang::TargetOptions &ClangOpts = Clang->getTargetInfo().getTargetOpts();
  return std::make_tuple(TargetOpts, ClangOpts.CPU, ClangOpts.Features);
}

void setModuleFlags(IRGenModule &IGM) {

  auto *Module = IGM.getModule();

  // These module flags don't affect code generation; they just let us
  // error during LTO if the user tries to combine files across ABIs.
  Module->addModuleFlag(llvm::Module::Error, "Swift Version",
                        IRGenModule::swiftVersion);
}

void swift::performLLVMOptimizations(IRGenOptions &Opts, llvm::Module *Module,
                                     llvm::TargetMachine *TargetMachine) {
  // Set up a pipeline.
  PassManagerBuilder PMBuilder;

  if (Opts.Optimize && !Opts.DisableLLVMOptzns) {
    PMBuilder.OptLevel = 3;
    PMBuilder.Inliner = llvm::createFunctionInliningPass(200);
    PMBuilder.SLPVectorize = true;
    PMBuilder.LoopVectorize = true;
  } else {
    PMBuilder.OptLevel = 0;
    if (!Opts.DisableLLVMOptzns)
      PMBuilder.Inliner =
        llvm::createAlwaysInlinerPass(/*insertlifetime*/false);
  }

  PMBuilder.addExtension(PassManagerBuilder::EP_ModuleOptimizerEarly,
                         addSwiftStackPromotionPass);

  // If the optimizer is enabled, we run the ARCOpt pass in the scalar optimizer
  // and the Contract pass as late as possible.
  if (!Opts.DisableLLVMARCOpts) {
    PMBuilder.addExtension(PassManagerBuilder::EP_ScalarOptimizerLate,
                           addSwiftARCOptPass);
    PMBuilder.addExtension(PassManagerBuilder::EP_OptimizerLast,
                           addSwiftContractPass);
  }
  
  // Configure the function passes.
  legacy::FunctionPassManager FunctionPasses(Module);
  FunctionPasses.add(createTargetTransformInfoWrapperPass(
      TargetMachine->getTargetIRAnalysis()));
  if (Opts.Verify)
    FunctionPasses.add(createVerifierPass());
  PMBuilder.populateFunctionPassManager(FunctionPasses);

  // The PMBuilder only knows about LLVM AA passes.  We should explicitly add
  // the swift AA pass after the other ones.
  if (!Opts.DisableLLVMARCOpts) {
    FunctionPasses.add(createSwiftAAWrapperPass());
    FunctionPasses.add(createExternalAAWrapperPass([](Pass &P, Function &,
                                                      AAResults &AAR) {
      if (auto *WrapperPass = P.getAnalysisIfAvailable<SwiftAAWrapperPass>())
        AAR.addAAResult(WrapperPass->getResult());
    }));
  }

  // Run the function passes.
  FunctionPasses.doInitialization();
  for (auto I = Module->begin(), E = Module->end(); I != E; ++I)
    if (!I->isDeclaration())
      FunctionPasses.run(*I);
  FunctionPasses.doFinalization();

  // Configure the module passes.
  legacy::PassManager ModulePasses;
  ModulePasses.add(createTargetTransformInfoWrapperPass(
      TargetMachine->getTargetIRAnalysis()));
  PMBuilder.populateModulePassManager(ModulePasses);

  // The PMBuilder only knows about LLVM AA passes.  We should explicitly add
  // the swift AA pass after the other ones.
  if (!Opts.DisableLLVMARCOpts) {
    ModulePasses.add(createSwiftAAWrapperPass());
    ModulePasses.add(createExternalAAWrapperPass([](Pass &P, Function &,
                                                    AAResults &AAR) {
      if (auto *WrapperPass = P.getAnalysisIfAvailable<SwiftAAWrapperPass>())
        AAR.addAAResult(WrapperPass->getResult());
    }));
  }

  // If we're generating a profile, add the lowering pass now.
  if (Opts.GenerateProfile)
    ModulePasses.add(createInstrProfilingPass());

  if (Opts.Verify)
    ModulePasses.add(createVerifierPass());

  // Do it.
  ModulePasses.run(*Module);
}

/// Run the LLVM passes. In multi-threaded compilation this will be done for
/// multiple LLVM modules in parallel.
static bool performLLVM(IRGenOptions &Opts, DiagnosticEngine &Diags,
                        llvm::sys::Mutex *DiagMutex,
                        llvm::Module *Module,
                        llvm::TargetMachine *TargetMachine,
                        StringRef OutputFilename) {
  llvm::SmallString<0> Buffer;
  std::unique_ptr<raw_pwrite_stream> RawOS;
  if (!OutputFilename.empty()) {
    // Try to open the output file.  Clobbering an existing file is fine.
    // Open in binary mode if we're doing binary output.
    llvm::sys::fs::OpenFlags OSFlags = llvm::sys::fs::F_None;
    std::error_code EC;
    auto *FDOS = new raw_fd_ostream(OutputFilename, EC, OSFlags);
    RawOS.reset(FDOS);
    if (FDOS->has_error() || EC) {
      if (DiagMutex)
        DiagMutex->lock();
      Diags.diagnose(SourceLoc(), diag::error_opening_output,
                     OutputFilename, EC.message());
      if (DiagMutex)
        DiagMutex->unlock();
      FDOS->clear_error();
      return true;
    }

    // Most output kinds want a formatted output stream.  It's not clear
    // why writing an object file does.
    //if (Opts.OutputKind != IRGenOutputKind::LLVMBitcode)
    //  FormattedOS.setStream(*RawOS, formatted_raw_ostream::PRESERVE_STREAM);
  } else {
    RawOS.reset(new raw_svector_ostream(Buffer));
  }

  performLLVMOptimizations(Opts, Module, TargetMachine);

  legacy::PassManager EmitPasses;

  // Set up the final emission passes.
  switch (Opts.OutputKind) {
  case IRGenOutputKind::Module:
    break;
  case IRGenOutputKind::LLVMAssembly:
    EmitPasses.add(createPrintModulePass(*RawOS));
    break;
  case IRGenOutputKind::LLVMBitcode:
    EmitPasses.add(createBitcodeWriterPass(*RawOS));
    break;
  case IRGenOutputKind::NativeAssembly:
  case IRGenOutputKind::ObjectFile: {
    llvm::TargetMachine::CodeGenFileType FileType;
    FileType = (Opts.OutputKind == IRGenOutputKind::NativeAssembly
                  ? llvm::TargetMachine::CGFT_AssemblyFile
                  : llvm::TargetMachine::CGFT_ObjectFile);

    EmitPasses.add(createTargetTransformInfoWrapperPass(
        TargetMachine->getTargetIRAnalysis()));

    // Make sure we do ARC contraction under optimization.  We don't
    // rely on any other LLVM ARC transformations, but we do need ARC
    // contraction to add the objc_retainAutoreleasedReturnValue
    // assembly markers.
    if (Opts.Optimize)
      EmitPasses.add(createObjCARCContractPass());

    bool fail = TargetMachine->addPassesToEmitFile(EmitPasses, *RawOS,
                                                   FileType, !Opts.Verify);
    if (fail) {
      if (DiagMutex)
        DiagMutex->lock();
      Diags.diagnose(SourceLoc(), diag::error_codegen_init_fail);
      if (DiagMutex)
        DiagMutex->unlock();
      return true;
    }
    break;
  }
  }

  EmitPasses.run(*Module);
  return false;
}

static llvm::TargetMachine *createTargetMachine(IRGenOptions &Opts,
                                                ASTContext &Ctx) {
  const llvm::Triple &Triple = Ctx.LangOpts.Target;
  std::string Error;
  const Target *Target = TargetRegistry::lookupTarget(Triple.str(), Error);
  if (!Target) {
    Ctx.Diags.diagnose(SourceLoc(), diag::no_llvm_target, Triple.str(), Error);
    return nullptr;
  }

  CodeGenOpt::Level OptLevel = Opts.Optimize ? CodeGenOpt::Aggressive
                                             : CodeGenOpt::None;

  // Set up TargetOptions and create the target features string.
  TargetOptions TargetOpts;
  std::string CPU;
  std::vector<std::string> targetFeaturesArray;
  std::tie(TargetOpts, CPU, targetFeaturesArray)
    = getIRTargetOptions(Opts, Ctx);
  std::string targetFeatures;
  if (!targetFeaturesArray.empty()) {
    llvm::SubtargetFeatures features;
    for (const std::string &feature : targetFeaturesArray)
      features.AddFeature(feature);
    targetFeatures = features.getString();
  }

  // Create a target machine.
  llvm::TargetMachine *TargetMachine
    = Target->createTargetMachine(Triple.str(), CPU,
                                  targetFeatures, TargetOpts, Reloc::PIC_,
                                  CodeModel::Default, OptLevel);
  if (!TargetMachine) {
    Ctx.Diags.diagnose(SourceLoc(), diag::no_llvm_target,
                       Triple.str(), "no LLVM target machine");
    return nullptr;
  }
  return TargetMachine;
}

// With -embed-bitcode, save a copy of the llvm IR as data in the
// __LLVM,__bitcode section and save the command-line options in the
// __LLVM,__swift_cmdline section.
static void embedBitcode(llvm::Module *M, const IRGenOptions &Opts)
{
  if (Opts.EmbedMode == IRGenEmbedMode::None)
    return;

  // Embed the bitcode for the llvm module.
  std::string Data;
  llvm::raw_string_ostream OS(Data);
  if (Opts.EmbedMode == IRGenEmbedMode::EmbedBitcode)
    llvm::WriteBitcodeToFile(M, OS);

  ArrayRef<uint8_t> ModuleData((uint8_t*)OS.str().data(), OS.str().size());
  llvm::Constant *ModuleConstant =
    llvm::ConstantDataArray::get(M->getContext(), ModuleData);
  // Use Appending linkage so it doesn't get optimized out.
  llvm::GlobalVariable *GV = new llvm::GlobalVariable(*M,
                                       ModuleConstant->getType(), true,
                                       llvm::GlobalValue::AppendingLinkage,
                                       ModuleConstant);
  GV->setSection("__LLVM,__bitcode");
  if (llvm::GlobalVariable *Old =
      M->getGlobalVariable("llvm.embedded.module")) {
    GV->takeName(Old);
    Old->replaceAllUsesWith(GV);
    delete Old;
  } else {
    GV->setName("llvm.embedded.module");
  }

  // Embed command-line options.
  ArrayRef<uint8_t> CmdData((uint8_t*)Opts.CmdArgs.data(),
                            Opts.CmdArgs.size());
  llvm::Constant *CmdConstant =
    llvm::ConstantDataArray::get(M->getContext(), CmdData);
  GV = new llvm::GlobalVariable(*M, CmdConstant->getType(), true,
                                llvm::GlobalValue::AppendingLinkage,
                                CmdConstant);
  GV->setSection("__LLVM,__swift_cmdline");
  if (llvm::GlobalVariable *Old = M->getGlobalVariable("llvm.cmdline")) {
    GV->takeName(Old);
    Old->replaceAllUsesWith(GV);
    delete Old;
  } else {
    GV->setName("llvm.cmdline");
  }
}

static void initLLVMModule(const IRGenModule &IGM) {
  auto *Module = IGM.getModule();
  assert(Module && "Expected llvm:Module for IR generation!");
  
  Module->setTargetTriple(IGM.Triple.str());

  // Set the module's string representation.
  Module->setDataLayout(IGM.DataLayout.getStringRepresentation());
}

/// Generates LLVM IR, runs the LLVM passes and produces the output file.
/// All this is done in a single thread.
static std::unique_ptr<llvm::Module> performIRGeneration(IRGenOptions &Opts,
                                                         swift::Module *M,
                                                         SILModule *SILMod,
                                                         StringRef ModuleName,
                                                 llvm::LLVMContext &LLVMContext,
                                                       SourceFile *SF = nullptr,
                                                       unsigned StartElem = 0) {
  auto &Ctx = M->getASTContext();
  assert(!Ctx.hadError());

  llvm::TargetMachine *TargetMachine = createTargetMachine(Opts, Ctx);
  if (!TargetMachine)
    return nullptr;

  const llvm::DataLayout DataLayout = TargetMachine->createDataLayout();

  // Create the IR emitter.
  IRGenModuleDispatcher dispatcher;
  const llvm::Triple &Triple = Ctx.LangOpts.Target;
  IRGenModule IGM(dispatcher, nullptr, Ctx, LLVMContext, Opts, ModuleName,
                  DataLayout, Triple,
                  TargetMachine, SILMod, Opts.getSingleOutputFilename());

  initLLVMModule(IGM);
  
  // Emit the module contents.
  dispatcher.emitGlobalTopLevel();

  if (SF) {
    IGM.emitSourceFile(*SF, StartElem);
  } else {
    assert(StartElem == 0 && "no explicit source file provided");
    for (auto *File : M->getFiles()) {
      if (auto *nextSF = dyn_cast<SourceFile>(File)) {
        if (nextSF->ASTStage >= SourceFile::TypeChecked)
          IGM.emitSourceFile(*nextSF, 0);
      } else {
        File->collectLinkLibraries([&IGM](LinkLibrary LinkLib) {
          IGM.addLinkLibrary(LinkLib);
        });
      }
    }
  }

  // Register our info with the runtime if needed.
  if (Opts.UseJIT) {
    IGM.emitRuntimeRegistration();
  } else {
    // Emit protocol conformances into a section we can recognize at runtime.
    // In JIT mode these are manually registered above.
    IGM.emitProtocolConformances();
  }

  // Okay, emit any definitions that we suddenly need.
  dispatcher.emitLazyDefinitions();

  // Emit symbols for eliminated dead methods.
  IGM.emitVTableStubs();

  // Verify type layout if we were asked to.
  if (!Opts.VerifyTypeLayoutNames.empty())
    IGM.emitTypeVerifier();

  std::for_each(Opts.LinkLibraries.begin(), Opts.LinkLibraries.end(),
                [&](LinkLibrary linkLib) {
    IGM.addLinkLibrary(linkLib);
  });

  // Hack to handle thunks eagerly synthesized by the Clang importer.
  swift::Module *prev = nullptr;
  for (auto external : Ctx.ExternalDefinitions) {
    swift::Module *next = external->getModuleContext();
    if (next == prev)
      continue;
    prev = next;

    if (next->getName() == M->getName())
      continue;

    next->collectLinkLibraries([&](LinkLibrary linkLib) {
      IGM.addLinkLibrary(linkLib);
    });
  }

  IGM.finalize();

  setModuleFlags(IGM);

  // Bail out if there are any errors.
  if (Ctx.hadError()) return nullptr;

  embedBitcode(IGM.getModule(), Opts);
  if (performLLVM(IGM.Opts, IGM.Context.Diags, nullptr, IGM.getModule(),
                  IGM.TargetMachine, IGM.OutputFilename))
    return nullptr;
  return std::unique_ptr<llvm::Module>(IGM.releaseModule());
}

static void ThreadEntryPoint(IRGenModuleDispatcher *dispatcher,
                             llvm::sys::Mutex *DiagMutex, int ThreadIdx) {
  while (IRGenModule *IGM = dispatcher->fetchFromQueue()) {
    DEBUG(
      DiagMutex->lock();
      dbgs() << "thread " << ThreadIdx << ": fetched " << IGM->OutputFilename <<
          "\n";
      DiagMutex->unlock();
    );
    embedBitcode(IGM->getModule(), IGM->Opts);
    performLLVM(IGM->Opts, IGM->Context.Diags, DiagMutex, IGM->getModule(),
                IGM->TargetMachine, IGM->OutputFilename);
    if (IGM->Context.Diags.hadAnyError())
      return;
  }
  DEBUG(
    DiagMutex->lock();
    dbgs() << "thread " << ThreadIdx << ": done\n";
    DiagMutex->unlock();
  );
}

/// Generates LLVM IR, runs the LLVM passes and produces the output files.
/// All this is done in multiple threads.
static void performParallelIRGeneration(IRGenOptions &Opts,
                                        swift::Module *M,
                                        SILModule *SILMod,
                                        StringRef ModuleName, int numThreads) {

  IRGenModuleDispatcher dispatcher;
  
  auto OutputIter = Opts.OutputFilenames.begin();
  bool IGMcreated = false;
  
  auto &Ctx = M->getASTContext();
  // Create an IRGenModule for each source file.
  for (auto *File : M->getFiles()) {
    auto nextSF = dyn_cast<SourceFile>(File);
    if (!nextSF || nextSF->ASTStage < SourceFile::TypeChecked)
      continue;
    
    // Create a target machine.
    llvm::TargetMachine *TargetMachine = createTargetMachine(Opts, Ctx);
    
    const llvm::DataLayout DataLayout = TargetMachine->createDataLayout();
    
    LLVMContext *Context = new LLVMContext();
    const llvm::Triple &Triple = Ctx.LangOpts.Target;

    // There must be an output filename for each source file.
    // We ignore additional output filenames.
    if (OutputIter == Opts.OutputFilenames.end()) {
      // TODO: Check this already at argument parsing.
      Ctx.Diags.diagnose(SourceLoc(), diag::too_few_output_filenames);
      return;
    }
  
    // Create the IR emitter.
    IRGenModule *IGM = new IRGenModule(dispatcher, nextSF, Ctx, *Context,
                                       Opts, ModuleName, DataLayout, Triple,
                                       TargetMachine, SILMod, *OutputIter++);
    IGMcreated = true;

    initLLVMModule(*IGM);
  }
  
  if (!IGMcreated) {
    // TODO: Check this already at argument parsing.
    Ctx.Diags.diagnose(SourceLoc(), diag::no_input_files_for_mt);
    return;
  }

  // Emit the module contents.
  dispatcher.emitGlobalTopLevel();
  
  for (auto *File : M->getFiles()) {
    if (SourceFile *SF = dyn_cast<SourceFile>(File)) {
      IRGenModule *IGM = dispatcher.getGenModule(SF);
      IGM->emitSourceFile(*SF, 0);
    } else {
      File->collectLinkLibraries([&dispatcher](LinkLibrary LinkLib) {
        dispatcher.getPrimaryIGM()->addLinkLibrary(LinkLib);
      });
    }
  }
  
  IRGenModule *PrimaryGM = dispatcher.getPrimaryIGM();

  // Emit protocol conformances.
  dispatcher.emitProtocolConformances();

  // Okay, emit any definitions that we suddenly need.
  dispatcher.emitLazyDefinitions();
  
 // Emit symbols for eliminated dead methods.
  PrimaryGM->emitVTableStubs();
    
  // Verify type layout if we were asked to.
  if (!Opts.VerifyTypeLayoutNames.empty())
    PrimaryGM->emitTypeVerifier();
  
  std::for_each(Opts.LinkLibraries.begin(), Opts.LinkLibraries.end(),
                [&](LinkLibrary linkLib) {
                  PrimaryGM->addLinkLibrary(linkLib);
                });
  
  // Hack to handle thunks eagerly synthesized by the Clang importer.
  swift::Module *prev = nullptr;
  for (auto external : Ctx.ExternalDefinitions) {
    swift::Module *next = external->getModuleContext();
    if (next == prev)
      continue;
    prev = next;
    
    if (next->getName() == M->getName())
      continue;
    
    next->collectLinkLibraries([&](LinkLibrary linkLib) {
      PrimaryGM->addLinkLibrary(linkLib);
    });
  }
  
  llvm::StringSet<> referencedGlobals;

  for (auto it = dispatcher.begin(); it != dispatcher.end(); ++it) {
    IRGenModule *IGM = it->second;
    llvm::Module *M = IGM->getModule();
    auto collectReference = [&](llvm::GlobalObject &G) {
      if (G.isDeclaration()
          && G.getLinkage() == GlobalValue::LinkOnceODRLinkage) {
        referencedGlobals.insert(G.getName());
        G.setLinkage(GlobalValue::ExternalLinkage);
      }
    };
    for (llvm::GlobalVariable &G : M->getGlobalList()) {
      collectReference(G);
    }
    for (llvm::Function &F : M->getFunctionList()) {
      collectReference(F);
    }
  }

  for (auto it = dispatcher.begin(); it != dispatcher.end(); ++it) {
    IRGenModule *IGM = it->second;
    llvm::Module *M = IGM->getModule();
    
    // Update the linkage of shared functions/globals.
    // If a shared function/global is referenced from another file it must have
    // weak instead of linkonce linkage. Otherwise LLVM would remove the
    // definition (if it's not referenced in the same file).
    auto updateLinkage = [&](llvm::GlobalObject &G) {
      if (!G.isDeclaration()
          && G.getLinkage() == GlobalValue::LinkOnceODRLinkage
          && referencedGlobals.count(G.getName()) != 0) {
        G.setLinkage(GlobalValue::WeakODRLinkage);
      }
    };
    for (llvm::GlobalVariable &G : M->getGlobalList()) {
      updateLinkage(G);
    }
    for (llvm::Function &F : M->getFunctionList()) {
      updateLinkage(F);
    }

    IGM->finalize();
    setModuleFlags(*IGM);
  }

  // Bail out if there are any errors.
  if (Ctx.hadError()) return;

  std::vector<std::thread> Threads;
  llvm::sys::Mutex DiagMutex;

  // Start all the threads an do the LLVM compilation.
  for (int ThreadIdx = 1; ThreadIdx < numThreads; ++ThreadIdx) {
    Threads.push_back(std::thread(ThreadEntryPoint, &dispatcher, &DiagMutex,
                                  ThreadIdx));
  }

  ThreadEntryPoint(&dispatcher, &DiagMutex, 0);

  // Wait for all threads.
  for (std::thread &Thread : Threads) {
    Thread.join();
  }

  // Cleanup.
  for (auto it = dispatcher.begin(); it != dispatcher.end(); ++it) {
    IRGenModule *IGM = it->second;
    LLVMContext *Context = &IGM->LLVMContext;
    delete IGM;
    delete Context;
  }
}


std::unique_ptr<llvm::Module> swift::
performIRGeneration(IRGenOptions &Opts, swift::Module *M, SILModule *SILMod,
                    StringRef ModuleName, llvm::LLVMContext &LLVMContext) {
  int numThreads = SILMod->getOptions().NumThreads;
  if (numThreads != 0) {
    ::performParallelIRGeneration(Opts, M, SILMod, ModuleName, numThreads);
    // TODO: Parallel LLVM compilation cannot be used if a (single) module is
    // needed as return value.
    return nullptr;
  }
  return ::performIRGeneration(Opts, M, SILMod, ModuleName, LLVMContext);
}

std::unique_ptr<llvm::Module> swift::
performIRGeneration(IRGenOptions &Opts, SourceFile &SF, SILModule *SILMod,
                    StringRef ModuleName, llvm::LLVMContext &LLVMContext,
                    unsigned StartElem) {
  return ::performIRGeneration(Opts, SF.getParentModule(), SILMod, ModuleName,
                               LLVMContext, &SF, StartElem);
}

void
swift::createSwiftModuleObjectFile(SILModule &SILMod, StringRef Buffer,
                                   StringRef OutputPath) {
  LLVMContext *VMContext = new LLVMContext();

  auto &Ctx = SILMod.getASTContext();
  assert(!Ctx.hadError());

  IRGenOptions Opts;
  Opts.OutputKind = IRGenOutputKind::ObjectFile;
  llvm::TargetMachine *TargetMachine = createTargetMachine(Opts, Ctx);
  if (!TargetMachine)
    return;

  const auto DataLayout = TargetMachine->createDataLayout();

  const llvm::Triple &Triple = Ctx.LangOpts.Target;
  IRGenModuleDispatcher dispatcher;
  IRGenModule IGM(dispatcher, nullptr, Ctx, *VMContext, Opts, OutputPath,
                  DataLayout, Triple,
                  TargetMachine, &SILMod, Opts.getSingleOutputFilename());
  initLLVMModule(IGM);
  auto *Ty = llvm::ArrayType::get(IGM.Int8Ty, Buffer.size());
  auto *Data =
      llvm::ConstantDataArray::getString(*VMContext, Buffer, /*AddNull=*/false);
  auto &M = *IGM.getModule();
  auto *ASTSym = new llvm::GlobalVariable(M, Ty, /*constant*/ true,
                                          llvm::GlobalVariable::InternalLinkage,
                                          Data, "__Swift_AST");
  std::string Section;
  if (Triple.isOSBinFormatMachO())
    Section = std::string(MachOASTSegmentName) + "," + MachOASTSectionName;
  else if (Triple.isOSBinFormatCOFF())
    Section = COFFASTSectionName;
  else
    Section = ELFASTSectionName;

  ASTSym->setSection(Section);
  ASTSym->setAlignment(8);
  ::performLLVM(Opts, Ctx.Diags, nullptr, IGM.getModule(), TargetMachine,
                OutputPath);
}

bool swift::performLLVM(IRGenOptions &Opts, ASTContext &Ctx,
                        llvm::Module *Module) {
  // Build TargetMachine.
  llvm::TargetMachine *TargetMachine = createTargetMachine(Opts, Ctx);
  if (!TargetMachine)
    return true;

  embedBitcode(Module, Opts);
  if (::performLLVM(Opts, Ctx.Diags, nullptr, Module, TargetMachine,
                    Opts.getSingleOutputFilename()))
    return true;
  return false;
}
