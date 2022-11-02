//===--- LangOptions.h - Language & configuration options -------*- C++ -*-===//
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
//  This file defines the LangOptions class, which provides various
//  language and configuration flags.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_LANGOPTIONS_H
#define SWIFT_LANGOPTIONS_H

#include "swift/Basic/LLVM.h"
#include "clang/Basic/VersionTuple.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include <string>

namespace swift {
  /// \brief A collection of options that affect the language dialect and
  /// provide compiler debugging facilities.
  class LangOptions {
  public:

    /// \brief The target we are building for.
    ///
    /// This represents the minimum deployment target.
    llvm::Triple Target;

    ///
    /// Language features
    ///

    /// \brief If true, all types are treated as resilient unless declared
    /// @_fixed_layout.
    bool EnableResilience = false;

    /// \brief Disable API availability checking.
    bool DisableAvailabilityChecking = false;
    
    /// Whether to warn about "needless" words in declarations.
    bool WarnOmitNeedlessWords = false;

    /// Should access control be respected?
    bool EnableAccessControl = true;

    /// Enable 'availability' restrictions for App Extensions.
    bool EnableAppExtensionRestrictions = false;

    ///
    /// Support for alternate usage modes
    ///

    /// \brief Enable features useful for running in the debugger.
    bool DebuggerSupport = false;

    /// Allows using identifiers with a leading dollar.
    bool EnableDollarIdentifiers = false;

    /// \brief Allow throwing call expressions without annotation with 'try'.
    bool EnableThrowWithoutTry = false;

    /// \brief Enable features useful for running playgrounds.
    // FIXME: This should probably be limited to the particular SourceFile.
    bool Playground = false;

    /// Whether to delay adding enum protocol conformances during code
    /// completion. This isn't completely correct with multiple files but is
    /// currently necessary to get reasonable performance.
    // FIXME: remove this when rdar://20047340 is fixed.
    bool EnableCodeCompletionDelayedEnumConformanceHack = false;

    /// \brief Keep comments during lexing and attach them to declarations.
    bool AttachCommentsToDecls = false;

    /// Whether to include initializers when code-completing a postfix
    /// expression.
    bool CodeCompleteInitsInPostfixExpr = false;

    ///
    /// Flags for use by tests
    ///

    /// Enable Objective-C Runtime interop code generation and build
    /// configuration options.
    bool EnableObjCInterop = true;

    /// Enables checking that uses of @objc require importing
    /// the Foundation module.
    /// This is enabled by default because SILGen can crash in such a case, but
    /// it gets disabled when compiling the Swift core stdlib.
    bool EnableObjCAttrRequiresFoundation = true;

    /// If true, <code>@testable import Foo</code> produces an error if \c Foo
    /// was not compiled with -enable-testing.
    bool EnableTestableAttrRequiresTestableModule = true;

    ///
    /// Flags for developers
    ///

    /// \brief Whether we are debugging the constraint solver.
    ///
    /// This option enables verbose debugging output from the constraint
    /// solver.
    bool DebugConstraintSolver = false;

    /// \brief Specific solution attempt for which the constraint
    /// solver should be debugged.
    unsigned DebugConstraintSolverAttempt = 0;

    /// \brief Enable the iterative type checker.
    bool IterativeTypeChecker = false;

    /// Debug the generic signatures computed by the archetype builder.
    bool DebugGenericSignatures = false;

    /// Triggers llvm fatal_error if typechecker tries to typecheck a decl or an
    /// identifier reference with the provided prefix name.
    /// This is for testing purposes.
    std::string DebugForbidTypecheckPrefix;

    /// Number of paralellel processes performing AST verification.
    unsigned ASTVerifierProcessCount = 1U;

    /// ID of the current process for the purposes of AST verification.
    unsigned ASTVerifierProcessId = 1U;

    /// \brief The upper bound, in bytes, of temporary data that can be
    /// allocated by the constraint solver.
    unsigned SolverMemoryThreshold = 15000000;

    /// \brief Perform all dynamic allocations using malloc/free instead of
    /// optimized custom allocator, so that memory debugging tools can be used.
    bool UseMalloc = false;
    
    /// \brief Enable experimental "switch" pattern-matching features.
    bool EnableExperimentalPatterns = false;

    /// Should we check the target OSs of serialized modules to see that they're
    /// new enough?
    bool EnableTargetOSChecking = true;
    
    /// Don't mangle the Self type as part of declaration manglings.
    bool DisableSelfTypeMangling = true;
    
    /// Sets the target we are building for and updates configuration options
    /// to match.
    ///
    /// \returns A pair - the first element is true if the OS was invalid.
    /// The second element is true if the Arch was invalid.
    std::pair<bool, bool> setTarget(llvm::Triple triple);

    /// Returns the minimum platform version to which code will be deployed.
    ///
    /// This is only implemented on certain OSs. If no target has been
    /// configured, returns v0.0.0.
    clang::VersionTuple getMinPlatformVersion() const {
      unsigned major, minor, revision;
      if (Target.isMacOSX()) {
        Target.getMacOSXVersion(major, minor, revision);
      } else if (Target.isiOS()) {
        Target.getiOSVersion(major, minor, revision);
      } else if (Target.isWatchOS()) {
        Target.getOSVersion(major, minor, revision);
      } else if (Target.isOSLinux() || Target.isOSFreeBSD() ||
                 Target.getTriple().empty())
      {
        major = minor = revision = 0;
      } else {
        llvm_unreachable("Unsupported target OS");
      }
      return clang::VersionTuple(major, minor, revision);
    }

    /// Implicit target configuration options.  There are currently three
    ///   supported target configuration values:
    ///     os - The active os target (OSX or IOS)
    ///     arch - The active arch target (X64, I386, ARM, ARM64)
    ///     _runtime - Runtime support (_ObjC or _Native)
    void addTargetConfigOption(StringRef Name, StringRef Value) {
      assert(!Name.empty() && !Value.empty());
      TargetConfigOptions.push_back(std::make_pair(Name, Value));
    }

    /// Removes all configuration options added with addTargetConfigOption.
    void clearAllTargetConfigOptions() {
      TargetConfigOptions.clear();
    }
    
    /// Returns the value for the given target configuration or an empty string.
    StringRef getTargetConfigOption(StringRef Name) const;
    
    /// Explicit build configuration options, initialized via the '-D'
    /// compiler flag.
    void addBuildConfigOption(StringRef Name) {
      assert(!Name.empty());
      BuildConfigOptions.push_back(Name);
    }

    /// Determines if a given build configuration has been defined.
    bool hasBuildConfigOption(StringRef Name) const;

    ArrayRef<std::pair<std::string, std::string>>
        getTargetConfigOptions() const {
      return TargetConfigOptions;
    }

    ArrayRef<std::string> getBuildConfigOptions() const {
      return BuildConfigOptions;
    }

    /// The constant list of supported os build configuration arguments.
    static const std::vector<std::string> SupportedOSBuildConfigArguments;

    /// Returns true if the os build configuration argument represents
    /// a supported target operating system.
    static bool isOSBuildConfigSupported(StringRef OSName);

    /// The constant list of supported arch build configuration arguments.
    static const std::vector<std::string> SupportedArchBuildConfigArguments;

    /// Returns true if the arch build configuration argument represents
    /// a supported target architecture.
    static bool isArchBuildConfigSupported(StringRef ArchName);

  private:
    llvm::SmallVector<std::pair<std::string, std::string>, 2>
        TargetConfigOptions; 
    llvm::SmallVector<std::string, 2> BuildConfigOptions;
  };
}

#endif // LLVM_SWIFT_LANGOPTIONS_H

