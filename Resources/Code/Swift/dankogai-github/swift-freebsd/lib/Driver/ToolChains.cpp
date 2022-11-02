//===--- ToolChains.cpp - Job invocations (general and per-plaftorm) ------===//
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

#include "ToolChains.h"

#include "swift/Basic/Dwarf.h"
#include "swift/Basic/Fallthrough.h"
#include "swift/Basic/LLVM.h"
#include "swift/Basic/Platform.h"
#include "swift/Basic/Range.h"
#include "swift/Basic/TaskQueue.h"
#include "swift/Driver/Driver.h"
#include "swift/Driver/Job.h"
#include "swift/Frontend/Frontend.h"
#include "swift/Option/Options.h"
#include "swift/Config.h"
#include "clang/Basic/Version.h"
#include "clang/Driver/Util.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Option/Arg.h"
#include "llvm/Option/ArgList.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/Program.h"

using namespace swift;
using namespace swift::driver;
using namespace llvm::opt;

/// The name of the Swift migrator binary.
static const char * const SWIFT_UPDATE_NAME = "swift-update";

static void addInputsOfType(ArgStringList &Arguments,
                            ArrayRef<const Action *> Inputs,
                            types::ID InputType) {
  for (auto &Input : Inputs) {
    if (Input->getType() != InputType)
      continue;
    Arguments.push_back(cast<InputAction>(Input)->getInputArg().getValue());
  }
}

static void addInputsOfType(ArgStringList &Arguments,
                            ArrayRef<const Job *> Jobs,
                            types::ID InputType) {
  for (const Job *Cmd : Jobs) {
    auto &output = Cmd->getOutput().getAnyOutputForType(InputType);
    if (!output.empty())
      Arguments.push_back(output.c_str());
  }
}

static void addPrimaryInputsOfType(ArgStringList &Arguments,
                                   ArrayRef<const Job *> Jobs,
                                   types::ID InputType) {
  for (const Job *Cmd : Jobs) {
    auto &outputInfo = Cmd->getOutput();
    if (outputInfo.getPrimaryOutputType() == InputType) {
      for (const std::string &Output : outputInfo.getPrimaryOutputFilenames()) {
        Arguments.push_back(Output.c_str());
      }
    }
  }
}

/// Handle arguments common to all invocations of the frontend (compilation,
/// module-merging, LLDB's REPL, etc).
static void addCommonFrontendArgs(const ToolChain &TC,
                                  const OutputInfo &OI,
                                  const CommandOutput &output,
                                  const ArgList &inputArgs,
                                  ArgStringList &arguments) {
  arguments.push_back("-target");
  arguments.push_back(inputArgs.MakeArgString(TC.getTriple().str()));
  const llvm::Triple &Triple = TC.getTriple();

  // Enable address top-byte ignored in the ARM64 backend.
  if (Triple.getArch() == llvm::Triple::aarch64) {
    arguments.push_back("-Xllvm");
    arguments.push_back("-aarch64-use-tbi");
  }

  // Enable or disable ObjC interop appropriately for the platform
  if (Triple.isOSDarwin()) {
    arguments.push_back("-enable-objc-interop");
  } else {
    arguments.push_back("-disable-objc-interop");
  }

  // Handle the CPU and its preferences.
  if (auto arg = inputArgs.getLastArg(options::OPT_target_cpu))
    arg->render(inputArgs, arguments);

  if (!OI.SDKPath.empty()) {
    arguments.push_back("-sdk");
    arguments.push_back(inputArgs.MakeArgString(OI.SDKPath));
  }

  inputArgs.AddAllArgs(arguments, options::OPT_I);
  inputArgs.AddAllArgs(arguments, options::OPT_F);

  inputArgs.AddLastArg(arguments, options::OPT_AssertConfig);
  inputArgs.AddLastArg(arguments, options::OPT_autolink_force_load);
  inputArgs.AddLastArg(arguments, options::OPT_color_diagnostics);
  inputArgs.AddLastArg(arguments, options::OPT_fixit_all);
  inputArgs.AddLastArg(arguments, options::OPT_enable_app_extension);
  inputArgs.AddLastArg(arguments, options::OPT_enable_testing);
  inputArgs.AddLastArg(arguments, options::OPT_g_Group);
  inputArgs.AddLastArg(arguments, options::OPT_import_objc_header);
  inputArgs.AddLastArg(arguments, options::OPT_import_underlying_module);
  inputArgs.AddLastArg(arguments, options::OPT_module_cache_path);
  inputArgs.AddLastArg(arguments, options::OPT_module_link_name);
  inputArgs.AddLastArg(arguments, options::OPT_nostdimport);
  inputArgs.AddLastArg(arguments, options::OPT_parse_stdlib);
  inputArgs.AddLastArg(arguments, options::OPT_resource_dir);
  inputArgs.AddLastArg(arguments, options::OPT_solver_memory_threshold);
  inputArgs.AddLastArg(arguments, options::OPT_profile_generate);
  inputArgs.AddLastArg(arguments, options::OPT_profile_coverage_mapping);

  // Pass on any build config options
  inputArgs.AddAllArgs(arguments, options::OPT_D);

  // Pass through the values passed to -Xfrontend.
  inputArgs.AddAllArgValues(arguments, options::OPT_Xfrontend);

  // Pass through any subsystem flags.
  inputArgs.AddAllArgs(arguments, options::OPT_Xllvm);
  inputArgs.AddAllArgs(arguments, options::OPT_Xcc);

  const std::string &moduleDocOutputPath =
      output.getAdditionalOutputForType(types::TY_SwiftModuleDocFile);
  if (!moduleDocOutputPath.empty()) {
    arguments.push_back("-emit-module-doc-path");
    arguments.push_back(moduleDocOutputPath.c_str());
  }

  if (llvm::sys::Process::StandardErrHasColors())
    arguments.push_back("-color-diagnostics");
}

std::pair<const char *, llvm::opt::ArgStringList>
ToolChain::constructInvocation(const CompileJobAction &job,
                               const JobContext &context) const {
  ArgStringList Arguments;

  if (context.OI.CompilerMode != OutputInfo::Mode::UpdateCode)
    Arguments.push_back("-frontend");

  // Determine the frontend mode option.
  const char *FrontendModeOption = nullptr;
  switch (context.OI.CompilerMode) {
  case OutputInfo::Mode::StandardCompile:
  case OutputInfo::Mode::SingleCompile: {
    switch (context.Output.getPrimaryOutputType()) {
    case types::TY_Object:
      FrontendModeOption = "-c";
      break;
    case types::TY_RawSIL:
      FrontendModeOption = "-emit-silgen";
      break;
    case types::TY_SIL:
      FrontendModeOption = "-emit-sil";
      break;
    case types::TY_RawSIB:
      FrontendModeOption = "-emit-sibgen";
      break;
    case types::TY_SIB:
      FrontendModeOption = "-emit-sib";
      break;
    case types::TY_LLVM_IR:
      FrontendModeOption = "-emit-ir";
      break;
    case types::TY_LLVM_BC:
      FrontendModeOption = "-emit-bc";
      break;
    case types::TY_Assembly:
      FrontendModeOption = "-S";
      break;
    case types::TY_SwiftModuleFile:
      // Since this is our primary output, we need to specify the option here.
      FrontendModeOption = "-emit-module";
      break;
    case types::TY_Nothing:
      // We were told to output nothing, so get the last mode option and use that.
      if (const Arg *A = context.Args.getLastArg(options::OPT_modes_Group))
        FrontendModeOption = A->getSpelling().data();
      else
        llvm_unreachable("We were told to perform a standard compile, "
                         "but no mode option was passed to the driver.");
      break;
    case types::TY_Swift:
    case types::TY_dSYM:
    case types::TY_AutolinkFile:
    case types::TY_Dependencies:
    case types::TY_SwiftModuleDocFile:
    case types::TY_ClangModuleFile:
    case types::TY_SerializedDiagnostics:
    case types::TY_ObjCHeader:
    case types::TY_Image:
    case types::TY_SwiftDeps:
    case types::TY_Remapping:
      llvm_unreachable("Output type can never be primary output.");
    case types::TY_INVALID:
      llvm_unreachable("Invalid type ID");
    }
    break;
  }
  case OutputInfo::Mode::Immediate:
    FrontendModeOption = "-interpret";
    break;
  case OutputInfo::Mode::REPL:
    llvm_unreachable("REPL mode handled elsewhere");
  case OutputInfo::Mode::UpdateCode:
    // Make sure that adding '-update-code' will permit accepting all arguments
    // '-c' accepts.
    FrontendModeOption = "-c";
    break;
  }

  assert(FrontendModeOption != nullptr && "No frontend mode option specified!");
  
  Arguments.push_back(FrontendModeOption);

  assert(context.Inputs.empty() &&
         "The Swift frontend does not expect to be fed any input Jobs!");

  // Add input arguments.
  switch (context.OI.CompilerMode) {
  case OutputInfo::Mode::StandardCompile:
  case OutputInfo::Mode::UpdateCode: {
    assert(context.InputActions.size() == 1 &&
           "The Swift frontend expects exactly one input (the primary file)!");

    auto *IA = cast<InputAction>(context.InputActions[0]);
    const Arg &PrimaryInputArg = IA->getInputArg();
    bool FoundPrimaryInput = false;

    for (auto *A : make_range(context.Args.filtered_begin(options::OPT_INPUT),
                              context.Args.filtered_end())) {
      // See if this input should be passed with -primary-file.
      // FIXME: This will pick up non-source inputs too, like .o files.
      if (!FoundPrimaryInput && PrimaryInputArg.getIndex() == A->getIndex()) {
        Arguments.push_back("-primary-file");
        FoundPrimaryInput = true;
      }
      Arguments.push_back(A->getValue());
    }
    break;
  }
  case OutputInfo::Mode::SingleCompile:
  case OutputInfo::Mode::Immediate: {
    for (const Action *A : context.InputActions) {
      cast<InputAction>(A)->getInputArg().render(context.Args, Arguments);
    }
    break;
  }
  case OutputInfo::Mode::REPL: {
    llvm_unreachable("REPL mode handled elsewhere");
  }
  }

  if (context.Args.hasArg(options::OPT_parse_stdlib))
    Arguments.push_back("-disable-objc-attr-requires-foundation-module");

  addCommonFrontendArgs(*this, context.OI, context.Output, context.Args,
                        Arguments);

  // Pass the optimization level down to the frontend.
  context.Args.AddLastArg(Arguments, options::OPT_O_Group);

  if (context.Args.hasArg(options::OPT_parse_as_library) ||
      context.Args.hasArg(options::OPT_emit_library))
    Arguments.push_back("-parse-as-library");

  context.Args.AddLastArg(Arguments, options::OPT_parse_sil);

  Arguments.push_back("-module-name");
  Arguments.push_back(context.Args.MakeArgString(context.OI.ModuleName));

  // Mode-specific arguments.
  switch (context.OI.CompilerMode) {
  case OutputInfo::Mode::StandardCompile:
  case OutputInfo::Mode::SingleCompile:
  case OutputInfo::Mode::UpdateCode:
    break;
  case OutputInfo::Mode::Immediate:
  case OutputInfo::Mode::REPL:
    context.Args.AddAllArgs(Arguments, options::OPT_l, options::OPT_framework,
                            options::OPT_L);
    break;
  }

  const std::string &ModuleOutputPath =
    context.Output.getAdditionalOutputForType(types::ID::TY_SwiftModuleFile);
  if (!ModuleOutputPath.empty()) {
    Arguments.push_back("-emit-module-path");
    Arguments.push_back(ModuleOutputPath.c_str());
  }

  const std::string &ObjCHeaderOutputPath =
    context.Output.getAdditionalOutputForType(types::ID::TY_ObjCHeader);
  if (!ObjCHeaderOutputPath.empty()) {
    assert(context.OI.CompilerMode == OutputInfo::Mode::SingleCompile &&
           "The Swift tool should only emit an Obj-C header in single compile"
           "mode!");

    Arguments.push_back("-emit-objc-header-path");
    Arguments.push_back(ObjCHeaderOutputPath.c_str());
  }

  const std::string &SerializedDiagnosticsPath =
    context.Output.getAdditionalOutputForType(types::TY_SerializedDiagnostics);
  if (!SerializedDiagnosticsPath.empty()) {
    Arguments.push_back("-serialize-diagnostics-path");
    Arguments.push_back(SerializedDiagnosticsPath.c_str());
  }

  const std::string &DependenciesPath =
    context.Output.getAdditionalOutputForType(types::TY_Dependencies);
  if (!DependenciesPath.empty()) {
    Arguments.push_back("-emit-dependencies-path");
    Arguments.push_back(DependenciesPath.c_str());
  }

  const std::string &ReferenceDependenciesPath =
    context.Output.getAdditionalOutputForType(types::TY_SwiftDeps);
  if (!ReferenceDependenciesPath.empty()) {
    Arguments.push_back("-emit-reference-dependencies-path");
    Arguments.push_back(ReferenceDependenciesPath.c_str());
  }

  const std::string &FixitsPath =
    context.Output.getAdditionalOutputForType(types::TY_Remapping);
  if (!FixitsPath.empty()) {
    Arguments.push_back("-emit-fixits-path");
    Arguments.push_back(FixitsPath.c_str());
  }

  if (context.OI.numThreads > 0) {
    Arguments.push_back("-num-threads");
    Arguments.push_back(
        context.Args.MakeArgString(Twine(context.OI.numThreads)));
  }

  // Add the output file argument if necessary.
  if (context.Output.getPrimaryOutputType() != types::TY_Nothing) {
    for (auto &FileName : context.Output.getPrimaryOutputFilenames()) {
      Arguments.push_back("-o");
      Arguments.push_back(FileName.c_str());
    }
  }

  if (context.Args.hasArg(options::OPT_embed_bitcode_marker))
    Arguments.push_back("-embed-bitcode-marker");

  // The immediate arguments must be last.
  if (context.OI.CompilerMode == OutputInfo::Mode::Immediate)
    context.Args.AddLastArg(Arguments, options::OPT__DASH_DASH);

  auto program = SWIFT_EXECUTABLE_NAME;
  if (context.OI.CompilerMode == OutputInfo::Mode::UpdateCode)
    program = SWIFT_UPDATE_NAME;

  return std::make_pair(program, Arguments);
}

std::pair<const char *, llvm::opt::ArgStringList>
ToolChain::constructInvocation(const BackendJobAction &job,
                               const JobContext &context) const {
  assert(context.Args.hasArg(options::OPT_embed_bitcode));
  ArgStringList Arguments;

  Arguments.push_back("-frontend");

  // Determine the frontend mode option.
  const char *FrontendModeOption = nullptr;
  switch (context.OI.CompilerMode) {
  case OutputInfo::Mode::StandardCompile:
  case OutputInfo::Mode::SingleCompile: {
    switch (context.Output.getPrimaryOutputType()) {
    case types::TY_Object:
      FrontendModeOption = "-c";
      break;
    case types::TY_LLVM_IR:
      FrontendModeOption = "-emit-ir";
      break;
    case types::TY_LLVM_BC:
      FrontendModeOption = "-emit-bc";
      break;
    case types::TY_Assembly:
      FrontendModeOption = "-S";
      break;
    case types::TY_Nothing:
      // We were told to output nothing, so get the last mode option and use that.
      if (const Arg *A = context.Args.getLastArg(options::OPT_modes_Group))
        FrontendModeOption = A->getSpelling().data();
      else
        llvm_unreachable("We were told to perform a standard compile, "
                         "but no mode option was passed to the driver.");
      break;

    case types::TY_SwiftModuleFile:
    case types::TY_RawSIL:
    case types::TY_RawSIB:
    case types::TY_SIL:
    case types::TY_SIB:
      llvm_unreachable("Cannot be output from backend job");
    case types::TY_Swift:
    case types::TY_dSYM:
    case types::TY_AutolinkFile:
    case types::TY_Dependencies:
    case types::TY_SwiftModuleDocFile:
    case types::TY_ClangModuleFile:
    case types::TY_SerializedDiagnostics:
    case types::TY_ObjCHeader:
    case types::TY_Image:
    case types::TY_SwiftDeps:
    case types::TY_Remapping:
      llvm_unreachable("Output type can never be primary output.");
    case types::TY_INVALID:
      llvm_unreachable("Invalid type ID");
    }
    break;
  }
  case OutputInfo::Mode::Immediate:
  case OutputInfo::Mode::REPL:
  case OutputInfo::Mode::UpdateCode:
    llvm_unreachable("invalid mode for backend job");
  }

  assert(FrontendModeOption != nullptr && "No frontend mode option specified!");
  
  Arguments.push_back(FrontendModeOption);

  // Add input arguments.
  switch (context.OI.CompilerMode) {
  case OutputInfo::Mode::StandardCompile: {
    assert(context.Inputs.size() == 1 && "The backend expects one input!");
    Arguments.push_back("-primary-file");
    const Job *Cmd = context.Inputs.front();
    Arguments.push_back(
      Cmd->getOutput().getPrimaryOutputFilename().c_str());
    break;
  }
  case OutputInfo::Mode::SingleCompile: {
    assert(context.Inputs.size() == 1 && "The backend expects one input!");
    Arguments.push_back("-primary-file");
    const Job *Cmd = context.Inputs.front();
    
    // In multi-threaded compilation, the backend job must select the correct
    // output file of the compilation job.
    auto OutNames = Cmd->getOutput().getPrimaryOutputFilenames();
    Arguments.push_back(OutNames[job.getInputIndex()].c_str());
    break;
  }
  case OutputInfo::Mode::Immediate:
  case OutputInfo::Mode::REPL:
  case OutputInfo::Mode::UpdateCode:
    llvm_unreachable("invalid mode for backend job");
  }

  // Only white-listed flags below are allowed to be embedded.
  Arguments.push_back("-target");
  Arguments.push_back(context.Args.MakeArgString(getTriple().str()));

  // Enable address top-byte ignored in the ARM64 backend.
  if (getTriple().getArch() == llvm::Triple::aarch64) {
    Arguments.push_back("-Xllvm");
    Arguments.push_back("-aarch64-use-tbi");
  }

  // Handle the CPU and its preferences.
  if (auto arg = context.Args.getLastArg(options::OPT_target_cpu))
    arg->render(context.Args, Arguments);

  context.Args.AddLastArg(Arguments, options::OPT_parse_stdlib);

  // Pass the optimization level down to the frontend.
  context.Args.AddLastArg(Arguments, options::OPT_O_Group);

  Arguments.push_back("-module-name");
  Arguments.push_back(context.Args.MakeArgString(context.OI.ModuleName));

  // Add the output file argument if necessary.
  if (context.Output.getPrimaryOutputType() != types::TY_Nothing) {
    for (auto &FileName : context.Output.getPrimaryOutputFilenames()) {
      Arguments.push_back("-o");
      Arguments.push_back(FileName.c_str());
    }
  }

  // Add flags implied by -embed-bitcode.
  Arguments.push_back("-embed-bitcode");
  // Disable all llvm IR level optimizations.
  Arguments.push_back("-disable-llvm-optzns");

  return std::make_pair(SWIFT_EXECUTABLE_NAME, Arguments);
}

std::pair<const char *, llvm::opt::ArgStringList>
ToolChain::constructInvocation(const MergeModuleJobAction &job,
                               const JobContext &context) const {
  ArgStringList Arguments;

  if (context.OI.CompilerMode != OutputInfo::Mode::UpdateCode)
    Arguments.push_back("-frontend");

  // We just want to emit a module, so pass -emit-module without any other
  // mode options.
  Arguments.push_back("-emit-module");

  size_t origLen = Arguments.size();
  (void)origLen;
  addInputsOfType(Arguments, context.Inputs, types::TY_SwiftModuleFile);
  addInputsOfType(Arguments, context.InputActions, types::TY_SwiftModuleFile);
  assert(Arguments.size() - origLen >=
         context.Inputs.size() + context.InputActions.size());
  assert((Arguments.size() - origLen == context.Inputs.size() ||
          !context.InputActions.empty()) &&
         "every input to MergeModule must generate a swiftmodule");

  // Tell all files to parse as library, which is necessary to load them as
  // serialized ASTs.
  Arguments.push_back("-parse-as-library");

  addCommonFrontendArgs(*this, context.OI, context.Output, context.Args,
                        Arguments);

  Arguments.push_back("-module-name");
  Arguments.push_back(context.Args.MakeArgString(context.OI.ModuleName));

  assert(context.Output.getPrimaryOutputType() == types::TY_SwiftModuleFile &&
         "The MergeModule tool only produces swiftmodule files!");

  const std::string &ObjCHeaderOutputPath =
    context.Output.getAdditionalOutputForType(types::TY_ObjCHeader);
  if (!ObjCHeaderOutputPath.empty()) {
    Arguments.push_back("-emit-objc-header-path");
    Arguments.push_back(ObjCHeaderOutputPath.c_str());
  }

  Arguments.push_back("-o");
  Arguments.push_back(
      context.Args.MakeArgString(context.Output.getPrimaryOutputFilename()));

  auto program = SWIFT_EXECUTABLE_NAME;
  if (context.OI.CompilerMode == OutputInfo::Mode::UpdateCode)
    program = SWIFT_UPDATE_NAME;

  return std::make_pair(program, Arguments);
}

std::pair<const char *, llvm::opt::ArgStringList>
ToolChain::constructInvocation(const ModuleWrapJobAction &job,
                               const JobContext &context) const {
  ArgStringList Arguments;

  Arguments.push_back("-modulewrap");

  addInputsOfType(Arguments, context.Inputs, types::TY_SwiftModuleFile);
  addInputsOfType(Arguments, context.InputActions, types::TY_SwiftModuleFile);
  assert(Arguments.size() == 2 &&
         "ModuleWrap expects exactly one merged swiftmodule as input");

  assert(context.Output.getPrimaryOutputType() == types::TY_Object &&
         "The -modulewrap mode only produces object files");

  Arguments.push_back("-o");
  Arguments.push_back(
      context.Args.MakeArgString(context.Output.getPrimaryOutputFilename()));

  return std::make_pair(SWIFT_EXECUTABLE_NAME, Arguments);
}

std::pair<const char *, llvm::opt::ArgStringList>
ToolChain::constructInvocation(const REPLJobAction &job,
                               const JobContext &context) const {
  assert(context.Inputs.empty());
  assert(context.InputActions.empty());

  bool useLLDB;

  switch (job.getRequestedMode()) {
  case REPLJobAction::Mode::Integrated:
    useLLDB = false;
    break;
  case REPLJobAction::Mode::RequireLLDB:
    useLLDB = true;
    break;
  case REPLJobAction::Mode::PreferLLDB:
    useLLDB = !findProgramRelativeToSwift("lldb").empty();
    break;
  }

  ArgStringList FrontendArgs;
  addCommonFrontendArgs(*this, context.OI, context.Output, context.Args,
                        FrontendArgs);
  context.Args.AddAllArgs(FrontendArgs, options::OPT_l, options::OPT_framework,
                          options::OPT_L);

  if (!useLLDB) {
    FrontendArgs.insert(FrontendArgs.begin(), {"-frontend", "-repl"});
    FrontendArgs.push_back("-module-name");
    FrontendArgs.push_back(context.Args.MakeArgString(context.OI.ModuleName));
    return std::make_pair(SWIFT_EXECUTABLE_NAME, FrontendArgs);
  }

  // Squash important frontend options into a single argument for LLDB.
  std::string SingleArg = "--repl=";
  {
    llvm::raw_string_ostream os(SingleArg);
    Job::printArguments(os, FrontendArgs);
  }

  ArgStringList Arguments;
  Arguments.push_back(context.Args.MakeArgString(std::move(SingleArg)));

  return std::make_pair("lldb", Arguments);
}


std::pair<const char *, llvm::opt::ArgStringList>
ToolChain::constructInvocation(const GenerateDSYMJobAction &job,
                               const JobContext &context) const {
  assert(context.Inputs.size() == 1);
  assert(context.InputActions.empty());
  assert(context.Output.getPrimaryOutputType() == types::TY_dSYM);

  ArgStringList Arguments;

  StringRef inputPath =
      context.Inputs.front()->getOutput().getPrimaryOutputFilename();
  Arguments.push_back(context.Args.MakeArgString(inputPath));

  Arguments.push_back("-o");
  Arguments.push_back(
      context.Args.MakeArgString(context.Output.getPrimaryOutputFilename()));

  return std::make_pair("dsymutil", Arguments);
}

std::pair<const char *, llvm::opt::ArgStringList>
ToolChain::constructInvocation(const AutolinkExtractJobAction &job,
                               const JobContext &context) const {
  llvm_unreachable("autolink extraction not implemented for this toolchain");
}

std::pair<const char *, llvm::opt::ArgStringList>
ToolChain::constructInvocation(const LinkJobAction &job,
                               const JobContext &context) const {
  llvm_unreachable("linking not implemented for this toolchain");
}

std::string
toolchains::Darwin::findProgramRelativeToSwiftImpl(StringRef name) const {
  StringRef swiftPath = getDriver().getSwiftProgramPath();
  StringRef swiftBinDir = llvm::sys::path::parent_path(swiftPath);

  // See if we're in an Xcode toolchain.
  bool hasToolchain = false;
  llvm::SmallString<128> path{swiftBinDir};
  llvm::sys::path::remove_filename(path); // bin
  llvm::sys::path::remove_filename(path); // usr
  if (llvm::sys::path::extension(path) == ".xctoolchain") {
    hasToolchain = true;
    llvm::sys::path::remove_filename(path); // *.xctoolchain
    llvm::sys::path::remove_filename(path); // Toolchains
    llvm::sys::path::append(path, "usr", "bin");
  }

  StringRef paths[] = { swiftBinDir, path };
  auto pathsRef = llvm::makeArrayRef(paths);
  if (!hasToolchain)
    pathsRef = pathsRef.drop_back();

  auto result = llvm::sys::findProgramByName(name, pathsRef);
  if (result)
    return result.get();
  return {};
}

static void addVersionString(const ArgList &inputArgs, ArgStringList &arguments,
                             unsigned major, unsigned minor, unsigned micro) {
  llvm::SmallString<8> buf;
  llvm::raw_svector_ostream os{buf};
  os << major << '.' << minor << '.' << micro;
  arguments.push_back(inputArgs.MakeArgString(os.str()));
}

/// Runs <code>xcrun -f clang</code> in order to find the location of Clang for
/// the currently active Xcode.
///
/// We get the "currently active" part by passing through the DEVELOPER_DIR
/// environment variable (along with the rest of the environment).
static bool findXcodeClangPath(llvm::SmallVectorImpl<char> &path) {
  assert(path.empty());

  auto xcrunPath = llvm::sys::findProgramByName("xcrun");
  if (!xcrunPath.getError()) {
    const char *args[] = { "-f", "clang", nullptr };
    sys::TaskQueue queue;
    queue.addTask(xcrunPath->c_str(), args);
    queue.execute(nullptr,
                  [&path](sys::ProcessId PID,
                          int returnCode,
                          StringRef output,
                          void *unused) -> sys::TaskFinishedResponse {
      if (returnCode == 0) {
        output = output.rtrim();
        path.append(output.begin(), output.end());
      }
      return sys::TaskFinishedResponse::ContinueExecution;
    });
  }

  return !path.empty();
}

std::pair<const char *, llvm::opt::ArgStringList>
toolchains::Darwin::constructInvocation(const LinkJobAction &job,
                                        const JobContext &context) const {
  assert(context.Output.getPrimaryOutputType() == types::TY_Image &&
         "Invalid linker output type.");

  const Driver &D = getDriver();
  const llvm::Triple &Triple = getTriple();

  ArgStringList Arguments;
  addPrimaryInputsOfType(Arguments, context.Inputs, types::TY_Object);
  addInputsOfType(Arguments, context.InputActions, types::TY_Object);

  if (context.OI.DebugInfoKind == IRGenDebugInfoKind::Normal) {
    size_t argCount = Arguments.size();
    if (context.OI.CompilerMode == OutputInfo::Mode::SingleCompile)
      addInputsOfType(Arguments, context.Inputs, types::TY_SwiftModuleFile);
    else
      addPrimaryInputsOfType(Arguments, context.Inputs,
                             types::TY_SwiftModuleFile);

    if (Arguments.size() > argCount) {
      assert(argCount + 1 == Arguments.size() &&
             "multiple swiftmodules found for -g");
      Arguments.insert(Arguments.end() - 1, "-add_ast_path");
    }
  }

  switch (job.getKind()) {
  case LinkKind::None:
    llvm_unreachable("invalid link kind");
  case LinkKind::Executable:
    // The default for ld; no extra flags necessary.
    break;
  case LinkKind::DynamicLibrary:
    Arguments.push_back("-dylib");
    break;
  }

  assert(Triple.isOSDarwin());

  // FIXME: If we used Clang as a linker instead of going straight to ld,
  // we wouldn't have to replicate Clang's logic here.
  bool wantsObjCRuntime = false;
  if (Triple.isiOS())
    wantsObjCRuntime = Triple.isOSVersionLT(8);
  else if (Triple.isWatchOS())
    wantsObjCRuntime = Triple.isOSVersionLT(2);
  else if (Triple.isMacOSX())
    wantsObjCRuntime = Triple.isMacOSXVersionLT(10, 10);

  if (context.Args.hasFlag(options::OPT_link_objc_runtime,
                           options::OPT_no_link_objc_runtime,
                           /*default=*/wantsObjCRuntime)) {
    llvm::SmallString<128> ARCLiteLib(D.getSwiftProgramPath());
    llvm::sys::path::remove_filename(ARCLiteLib); // 'swift'
    llvm::sys::path::remove_filename(ARCLiteLib); // 'bin'
    llvm::sys::path::append(ARCLiteLib, "lib", "arc");

    if (!llvm::sys::fs::is_directory(ARCLiteLib)) {
      // If we don't have a 'lib/arc/' directory, find the "arclite" library
      // relative to the Clang in the active Xcode.
      ARCLiteLib.clear();
      if (findXcodeClangPath(ARCLiteLib)) {
        llvm::sys::path::remove_filename(ARCLiteLib); // 'clang'
        llvm::sys::path::remove_filename(ARCLiteLib); // 'bin'
        llvm::sys::path::append(ARCLiteLib, "lib", "arc");
      }
    }

    if (!ARCLiteLib.empty()) {
      llvm::sys::path::append(ARCLiteLib, "libarclite_");
      ARCLiteLib += getPlatformNameForTriple(Triple);
      ARCLiteLib += ".a";

      Arguments.push_back("-force_load");
      Arguments.push_back(context.Args.MakeArgString(ARCLiteLib));

      // Arclite depends on CoreFoundation.
      Arguments.push_back("-framework");
      Arguments.push_back("CoreFoundation");
    } else {
      // FIXME: We should probably diagnose this, but this is not a place where
      // we can emit diagnostics. Silently ignore it for now.
    }
  }

  context.Args.AddAllArgValues(Arguments, options::OPT_Xlinker);
  context.Args.AddAllArgs(Arguments, options::OPT_linker_option_Group);
  context.Args.AddAllArgs(Arguments, options::OPT_F);

  if (context.Args.hasArg(options::OPT_enable_app_extension)) {
    // Keep this string fixed in case the option used by the
    // compiler itself changes.
    Arguments.push_back("-application_extension");
  }

  if (context.Args.hasArg(options::OPT_embed_bitcode,
                          options::OPT_embed_bitcode_marker)) {
    Arguments.push_back("-bitcode_bundle");
  }

  if (!context.OI.SDKPath.empty()) {
    Arguments.push_back("-syslibroot");
    Arguments.push_back(context.Args.MakeArgString(context.OI.SDKPath));
  }

  Arguments.push_back("-lobjc");
  Arguments.push_back("-lSystem");

  Arguments.push_back("-arch");
  Arguments.push_back(context.Args.MakeArgString(getTriple().getArchName()));

  // Add the runtime library link path, which is platform-specific and found
  // relative to the compiler.
  // FIXME: Duplicated from CompilerInvocation, but in theory the runtime
  // library link path and the standard library module import path don't
  // need to be the same.
  llvm::SmallString<128> RuntimeLibPath;

  if (const Arg *A = context.Args.getLastArg(options::OPT_resource_dir)) {
    RuntimeLibPath = A->getValue();
  } else {
    RuntimeLibPath = D.getSwiftProgramPath();
    llvm::sys::path::remove_filename(RuntimeLibPath); // remove /swift
    llvm::sys::path::remove_filename(RuntimeLibPath); // remove /bin
    llvm::sys::path::append(RuntimeLibPath, "lib", "swift");
  }
  llvm::sys::path::append(RuntimeLibPath,
                          getPlatformNameForTriple(getTriple()));
  Arguments.push_back("-L");
  Arguments.push_back(context.Args.MakeArgString(RuntimeLibPath));

  if (context.Args.hasArg(options::OPT_profile_generate)) {
    SmallString<128> LibProfile(RuntimeLibPath);
    llvm::sys::path::remove_filename(LibProfile); // remove platform name
    llvm::sys::path::append(LibProfile, "clang", CLANG_VERSION_STRING);

    StringRef RT;
    if (Triple.isiOS()) {
      if (Triple.isTvOS())
        RT = "tvos";
      else
        RT = "ios";
    }
    else if (Triple.isWatchOS())
      RT = "watchos";
    else
      RT = "osx";
    llvm::sys::path::append(LibProfile, "lib", "darwin",
                            "libclang_rt.profile_" + RT + ".a");
    Arguments.push_back(context.Args.MakeArgString(LibProfile));
  }

  // FIXME: We probably shouldn't be adding an rpath here unless we know ahead
  // of time the standard library won't be copied.
  Arguments.push_back("-rpath");
  Arguments.push_back(context.Args.MakeArgString(RuntimeLibPath));

  // FIXME: Properly handle deployment targets.
  assert(Triple.isiOS() || Triple.isWatchOS() || Triple.isMacOSX());
  if (Triple.isiOS()) {
    bool isiOSSimulator = tripleIsiOSSimulator(Triple);
    if (Triple.isTvOS()) {
      if (isiOSSimulator)
        Arguments.push_back("-tvos_simulator_version_min");
      else
        Arguments.push_back("-tvos_version_min");
    } else {
      if (isiOSSimulator)
        Arguments.push_back("-ios_simulator_version_min");
      else
        Arguments.push_back("-iphoneos_version_min");
    }
    unsigned major, minor, micro;
    Triple.getiOSVersion(major, minor, micro);
    addVersionString(context.Args, Arguments, major, minor, micro);
  } else if (Triple.isWatchOS()) {
    if (tripleIsWatchSimulator(Triple))
      Arguments.push_back("-watchos_simulator_version_min");
    else
      Arguments.push_back("-watchos_version_min");
    unsigned major, minor, micro;
    Triple.getOSVersion(major, minor, micro);
    addVersionString(context.Args, Arguments, major, minor, micro);
  } else {
    Arguments.push_back("-macosx_version_min");
    unsigned major, minor, micro;
    Triple.getMacOSXVersion(major, minor, micro);
    addVersionString(context.Args, Arguments, major, minor, micro);
  }

  Arguments.push_back("-no_objc_category_merging");

  // This should be the last option, for convenience in checking output.
  Arguments.push_back("-o");
  Arguments.push_back(context.Output.getPrimaryOutputFilename().c_str());

  return std::make_pair("ld", Arguments);
}

#if defined(SWIFT_ENABLE_TARGET_LINUX)

std::pair<const char *, llvm::opt::ArgStringList>
toolchains::Linux::constructInvocation(const AutolinkExtractJobAction &job,
                                       const JobContext &context) const {
  assert(context.Output.getPrimaryOutputType() == types::TY_AutolinkFile);

  ArgStringList Arguments;
  addPrimaryInputsOfType(Arguments, context.Inputs, types::TY_Object);
  addInputsOfType(Arguments, context.InputActions, types::TY_Object);

  Arguments.push_back("-o");
  Arguments.push_back(
      context.Args.MakeArgString(context.Output.getPrimaryOutputFilename()));

  return std::make_pair("swift-autolink-extract", Arguments);
}

std::pair<const char *, llvm::opt::ArgStringList>
toolchains::Linux::constructInvocation(const LinkJobAction &job,
                                       const JobContext &context) const {
  const Driver &D = getDriver();

  assert(context.Output.getPrimaryOutputType() == types::TY_Image &&
         "Invalid linker output type.");

  ArgStringList Arguments;

  switch (job.getKind()) {
  case LinkKind::None:
    llvm_unreachable("invalid link kind");
  case LinkKind::Executable:
    // Default case, nothing extra needed
    break;
  case LinkKind::DynamicLibrary:
    Arguments.push_back("-shared");
    break;
  }

  addPrimaryInputsOfType(Arguments, context.Inputs, types::TY_Object);
  addInputsOfType(Arguments, context.InputActions, types::TY_Object);

  context.Args.AddAllArgs(Arguments, options::OPT_Xlinker);
  context.Args.AddAllArgs(Arguments, options::OPT_linker_option_Group);
  context.Args.AddAllArgs(Arguments, options::OPT_F);

  if (!context.OI.SDKPath.empty()) {
    Arguments.push_back("--sysroot");
    Arguments.push_back(context.Args.MakeArgString(context.OI.SDKPath));
  }

  // Add the runtime library link path, which is platform-specific and found
  // relative to the compiler.
  // FIXME: Duplicated from CompilerInvocation, but in theory the runtime
  // library link path and the standard library module import path don't
  // need to be the same.
  llvm::SmallString<128> RuntimeLibPath;

  if (const Arg *A = context.Args.getLastArg(options::OPT_resource_dir)) {
    RuntimeLibPath = A->getValue();
  } else {
    RuntimeLibPath = D.getSwiftProgramPath();
    llvm::sys::path::remove_filename(RuntimeLibPath); // remove /swift
    llvm::sys::path::remove_filename(RuntimeLibPath); // remove /bin
    llvm::sys::path::append(RuntimeLibPath, "lib", "swift");
  }
  llvm::sys::path::append(RuntimeLibPath,
                          getPlatformNameForTriple(getTriple()));
  Arguments.push_back("-L");
  Arguments.push_back(context.Args.MakeArgString(RuntimeLibPath));

  if (context.Args.hasArg(options::OPT_profile_generate)) {
    SmallString<128> LibProfile(RuntimeLibPath);
    llvm::sys::path::remove_filename(LibProfile); // remove platform name
    llvm::sys::path::append(LibProfile, "clang", CLANG_VERSION_STRING);

    llvm::sys::path::append(LibProfile, "lib", getTriple().getOSName(),
                            Twine("libclang_rt.profile-") +
                              getTriple().getArchName() +
                              ".a");
    Arguments.push_back(context.Args.MakeArgString(LibProfile));
  }

  // FIXME: We probably shouldn't be adding an rpath here unless we know ahead
  // of time the standard library won't be copied.
  Arguments.push_back("-Xlinker");
  Arguments.push_back("-rpath");
  Arguments.push_back("-Xlinker");
  Arguments.push_back(context.Args.MakeArgString(RuntimeLibPath));

  // Always add the stdlib
  Arguments.push_back("-lswiftCore");

  // Add any autolinking scripts to the arguments
  for (const Job *Cmd : context.Inputs) {
    auto &OutputInfo = Cmd->getOutput();
    if (OutputInfo.getPrimaryOutputType() == types::TY_AutolinkFile)
      Arguments.push_back(context.Args.MakeArgString(
        Twine("@") + OutputInfo.getPrimaryOutputFilename()));
  }

  // Add the linker script that coalesces protocol conformance sections.
  Arguments.push_back("-Xlinker");
  Arguments.push_back("-T");
  Arguments.push_back(
      context.Args.MakeArgString(Twine(RuntimeLibPath) + "/x86_64/swift.ld"));

  // This should be the last option, for convenience in checking output.
  Arguments.push_back("-o");
  Arguments.push_back(context.Output.getPrimaryOutputFilename().c_str());

  return std::make_pair("clang++", Arguments);
}

#endif // SWIFT_ENABLE_TARGET_LINUX

#if defined(SWIFT_ENABLE_TARGET_FREEBSD)

std::pair<const char *, llvm::opt::ArgStringList>
toolchains::FreeBSD::constructInvocation(const AutolinkExtractJobAction &job,
                                       const JobContext &context) const {
  assert(context.Output.getPrimaryOutputType() == types::TY_AutolinkFile);

  ArgStringList Arguments;
  addPrimaryInputsOfType(Arguments, context.Inputs, types::TY_Object);
  addInputsOfType(Arguments, context.InputActions, types::TY_Object);

  Arguments.push_back("-o");
  Arguments.push_back(
      context.Args.MakeArgString(context.Output.getPrimaryOutputFilename()));

  return std::make_pair("swift-autolink-extract", Arguments);
}

std::pair<const char *, llvm::opt::ArgStringList>
toolchains::FreeBSD::constructInvocation(const LinkJobAction &job,
                                       const JobContext &context) const {
  const Driver &D = getDriver();

  assert(context.Output.getPrimaryOutputType() == types::TY_Image &&
         "Invalid linker output type.");

  ArgStringList Arguments;

  switch (job.getKind()) {
  case LinkKind::None:
    llvm_unreachable("invalid link kind");
  case LinkKind::Executable:
    // Default case, nothing extra needed
    break;
  case LinkKind::DynamicLibrary:
    Arguments.push_back("-shared");
    break;
  }

  addPrimaryInputsOfType(Arguments, context.Inputs, types::TY_Object);
  addInputsOfType(Arguments, context.InputActions, types::TY_Object);

  context.Args.AddAllArgs(Arguments, options::OPT_Xlinker);
  context.Args.AddAllArgs(Arguments, options::OPT_linker_option_Group);
  context.Args.AddAllArgs(Arguments, options::OPT_F);

  if (!context.OI.SDKPath.empty()) {
    Arguments.push_back("--sysroot");
    Arguments.push_back(context.Args.MakeArgString(context.OI.SDKPath));
  }

  // Add the runtime library link path, which is platform-specific and found
  // relative to the compiler.
  // FIXME: Duplicated from CompilerInvocation, but in theory the runtime
  // library link path and the standard library module import path don't
  // need to be the same.
  llvm::SmallString<128> RuntimeLibPath;

  if (const Arg *A = context.Args.getLastArg(options::OPT_resource_dir)) {
    RuntimeLibPath = A->getValue();
  } else {
    RuntimeLibPath = D.getSwiftProgramPath();
    llvm::sys::path::remove_filename(RuntimeLibPath); // remove /swift
    llvm::sys::path::remove_filename(RuntimeLibPath); // remove /bin
    llvm::sys::path::append(RuntimeLibPath, "lib", "swift");
  }
  llvm::sys::path::append(RuntimeLibPath,
                          getPlatformNameForTriple(getTriple()));
  Arguments.push_back("-L");
  Arguments.push_back(context.Args.MakeArgString(RuntimeLibPath));

  if (context.Args.hasArg(options::OPT_profile_generate)) {
    SmallString<128> LibProfile(RuntimeLibPath);
    llvm::sys::path::remove_filename(LibProfile); // remove platform name
    llvm::sys::path::append(LibProfile, "clang", CLANG_VERSION_STRING);

    llvm::sys::path::append(LibProfile, "lib", getTriple().getOSName(),
                            Twine("libclang_rt.profile-") +
                              getTriple().getArchName() +
                              ".a");
    Arguments.push_back(context.Args.MakeArgString(LibProfile));
  }

  // FIXME: We probably shouldn't be adding an rpath here unless we know ahead
  // of time the standard library won't be copied.
  Arguments.push_back("-Xlinker");
  Arguments.push_back("-rpath");
  Arguments.push_back("-Xlinker");
  Arguments.push_back(context.Args.MakeArgString(RuntimeLibPath));

  // Always add the stdlib
  Arguments.push_back("-lswiftCore");

  // Add any autolinking scripts to the arguments
  for (const Job *Cmd : context.Inputs) {
    auto &OutputInfo = Cmd->getOutput();
    if (OutputInfo.getPrimaryOutputType() == types::TY_AutolinkFile)
      Arguments.push_back(context.Args.MakeArgString(
        Twine("@") + OutputInfo.getPrimaryOutputFilename()));
  }

  // Add the linker script that coalesces protocol conformance sections.
  Arguments.push_back("-Xlinker");
  Arguments.push_back("-T");
  Arguments.push_back(
      context.Args.MakeArgString(Twine(RuntimeLibPath) + "/x86_64/swift.ld"));

  // This should be the last option, for convenience in checking output.
  Arguments.push_back("-o");
  Arguments.push_back(context.Output.getPrimaryOutputFilename().c_str());

  return std::make_pair("clang++", Arguments);
}

#endif // SWIFT_ENABLE_TARGET_FREEBSD
