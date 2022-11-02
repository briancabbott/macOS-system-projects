//===--- ToolChain.h - Collections of tools for one platform ----*- C++ -*-===//
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

#ifndef SWIFT_DRIVER_TOOLCHAIN_H
#define SWIFT_DRIVER_TOOLCHAIN_H

#include "swift/Driver/Action.h"
#include "swift/Driver/Types.h"
#include "swift/Basic/LLVM.h"
#include "llvm/Option/Option.h"
#include "llvm/ADT/Triple.h"

#include <memory>

namespace swift {
namespace driver {
  class CommandOutput;
  class Compilation;
  class Driver;
  class Job;
  class OutputInfo;

/// A ToolChain is responsible for turning abstract Actions into concrete,
/// runnable Jobs.
///
/// The primary purpose of a ToolChain is built around the
/// \c constructInvocation family of methods. This is a set of callbacks
/// following the Visitor pattern for the various JobAction subclasses, which
/// returns an executable name and arguments for the Job to be run. The base
/// ToolChain knows how to perform most operations, but some (like linking)
/// require platform-specific knowledge, provided in subclasses.
class ToolChain {
  const Driver &D;
  const llvm::Triple Triple;
  mutable llvm::StringMap<std::string> ProgramLookupCache;

protected:
  ToolChain(const Driver &D, const llvm::Triple &T) : D(D), Triple(T) {}

  /// A special name used to identify the Swift executable itself.
  constexpr static const char * const SWIFT_EXECUTABLE_NAME = "swift";

  /// Packs together the supplementary information about the job being created.
  struct JobContext {
    ArrayRef<const Job *> Inputs;
    const CommandOutput &Output;
    ArrayRef<const Action *> InputActions;
    const llvm::opt::ArgList &Args;
    const OutputInfo &OI;
  };

  virtual std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const CompileJobAction &job,
                      const JobContext &context) const;
  virtual std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const BackendJobAction &job,
                      const JobContext &context) const;
  virtual std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const MergeModuleJobAction &job,
                      const JobContext &context) const;
  virtual std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const ModuleWrapJobAction &job,
                      const JobContext &context) const;

  virtual std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const REPLJobAction &job,
                      const JobContext &context) const;

  virtual std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const GenerateDSYMJobAction &job,
                      const JobContext &context) const;
  virtual std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const AutolinkExtractJobAction &job,
                      const JobContext &context) const;
  virtual std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const LinkJobAction &job,
                      const JobContext &context) const;

  /// Searches for the given executable in appropriate paths relative to the
  /// Swift binary.
  ///
  /// This method caches its results.
  ///
  /// \sa findProgramRelativeToSwiftImpl
  std::string findProgramRelativeToSwift(StringRef name) const;

  /// An override point for platform-specific subclasses to customize how to
  /// do relative searches for programs.
  ///
  /// This method is invoked by findProgramRelativeToSwift().
  virtual std::string findProgramRelativeToSwiftImpl(StringRef name) const;

public:
  virtual ~ToolChain() = default;

  const Driver &getDriver() const { return D; }
  const llvm::Triple &getTriple() const { return Triple; }

  /// Construct a Job for the action \p JA, taking the given information into
  /// account.
  ///
  /// This method dispatches to the various \c constructInvocation methods,
  /// which may be overridden by platform-specific subclasses.
  std::unique_ptr<Job> constructJob(const JobAction &JA,
                                    SmallVectorImpl<const Job *> &&inputs,
                                    std::unique_ptr<CommandOutput> output,
                                    const ActionList &inputActions,
                                    const llvm::opt::ArgList &args,
                                    const OutputInfo &OI) const;

  /// Return the default langauge type to use for the given extension.
  virtual types::ID lookupTypeForExtension(StringRef Ext) const;
};
} // end namespace driver
} // end namespace swift

#endif
