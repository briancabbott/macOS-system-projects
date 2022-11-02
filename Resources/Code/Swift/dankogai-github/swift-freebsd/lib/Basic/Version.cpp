//===--- Version.cpp - Swift Version Number -------------------------------===//
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
// This file defines several version-related utility functions for Swift.
//
//===----------------------------------------------------------------------===//

#include "clang/Basic/CharInfo.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/SmallString.h"
#include "swift/AST/DiagnosticsParse.h"
#include "swift/Basic/LLVM.h"
#include "swift/Basic/Version.h"

#include <vector>

#define TOSTR2(X) #X
#define TOSTR(X) TOSTR2(X)

#ifdef SWIFT_VERSION_PATCHLEVEL
/// Helper macro for SWIFT_VERSION_STRING.
#define SWIFT_MAKE_VERSION_STRING(X, Y, Z) TOSTR(X) "." TOSTR(Y) "." TOSTR(Z)

/// A string that describes the Swift version number, e.g., "1.0".
#define SWIFT_VERSION_STRING                                                   \
  SWIFT_MAKE_VERSION_STRING(SWIFT_VERSION_MAJOR, SWIFT_VERSION_MINOR,          \
                            SWIFT_VERSION_PATCHLEVEL)
#else
/// Helper macro for SWIFT_VERSION_STRING.
#define SWIFT_MAKE_VERSION_STRING(X, Y) TOSTR(X) "." TOSTR(Y)

/// A string that describes the Swift version number, e.g., "1.0".
#define SWIFT_VERSION_STRING                                                   \
  SWIFT_MAKE_VERSION_STRING(SWIFT_VERSION_MAJOR, SWIFT_VERSION_MINOR)
#endif

#if __has_include("LLVMRevision.inc")
# include "LLVMRevision.inc"
#endif
#if __has_include("ClangRevision.inc")
# include "ClangRevision.inc"
#endif
#if __has_include("SwiftRevision.inc")
# include "SwiftRevision.inc"
#endif

namespace swift {
namespace version {

/// Print a string of the form "LLVM xxxxx, Clang yyyyy, Swift zzzzz",
/// where each placeholder is the revision for the associated repository.
static void printFullRevisionString(raw_ostream &out) {
  // Abitrarily truncate to 10 characters. This should be enough to unique
  // Git hashes for the time being, and certainly enough for SVN revisions,
  // while keeping the version string from being ridiculously long.
#if defined(LLVM_REVISION)
  out << "LLVM " << StringRef(LLVM_REVISION).slice(0, 10);
# if defined(CLANG_REVISION) || defined(SWIFT_REVISION)
  out << ", ";
# endif
#endif

#if defined(CLANG_REVISION)
  out << "Clang " << StringRef(CLANG_REVISION).slice(0, 10);
# if defined(SWIFT_REVISION)
  out << ", ";
# endif
#endif

#if defined(SWIFT_REVISION)
  out << "Swift " << StringRef(SWIFT_REVISION).slice(0, 10);
#endif
}

void splitVersionComponents(
  SmallVectorImpl<std::pair<StringRef, SourceRange>> &SplitComponents,
                            StringRef &VersionString, SourceLoc Loc,
                            DiagnosticEngine *Diags) {
  SourceLoc Start = Loc.isValid() ? Loc.getAdvancedLoc(1) : Loc;
  SourceLoc End = Start;

  // Split the version string into tokens separated by the '.' character.
  while (!VersionString.empty()) {
    StringRef SplitComponent, Rest;
    std::tie(SplitComponent, Rest) = VersionString.split('.');

    if (Loc.isValid()) {
      End = End.getAdvancedLoc(SplitComponent.size());
    }
    auto Range = Loc.isValid() ? SourceRange(Start, End) : SourceRange();
    if (Loc.isValid())
      End = End.getAdvancedLoc(1);
    Start = End;
    SplitComponents.push_back({SplitComponent, Range});
    VersionString = Rest;
  }
}

Version Version::parseCompilerVersionString(
  StringRef VersionString, SourceLoc Loc, DiagnosticEngine *Diags) {

  Version CV;
  SmallString<16> digits;
  llvm::raw_svector_ostream OS(digits);
  SmallVector<std::pair<StringRef, SourceRange>, 5> SplitComponents;

  auto checkVersionComponent = [&](unsigned Component, SourceRange Range) {
    unsigned limit = CV.Components.size() == 0 ? 9223371 : 999;

    if (Component > limit) {
      if (Diags)
        Diags->diagnose(Range.Start,
                        diag::compiler_version_component_out_of_range, limit);
      else
        llvm_unreachable("Compiler version component out of range");
    }
  };

  splitVersionComponents(SplitComponents, VersionString, Loc, Diags);

  uint64_t ComponentNumber;

  for (size_t i = 0; i < SplitComponents.size(); ++i) {
    StringRef SplitComponent;
    SourceRange Range;
    std::tie(SplitComponent, Range) = SplitComponents[i];

    // Version components can't be empty.
    if (SplitComponent.empty()) {
      if (Diags)
        Diags->diagnose(Range.Start, diag::empty_compiler_version_component);
      else
        llvm_unreachable("Found empty compiler version component");
      continue;
    }

    // The second version component isn't used for comparison.
    if (i == 1) {
      if (!SplitComponent.equals("*")) {
        if (Diags) {
          Diags->diagnose(Range.Start, diag::unused_compiler_version_component)
          .fixItReplaceChars(Range.Start, Range.End, "*");
        } else {
          llvm_unreachable("Expected * for second compiler version component");
        }
      }

      CV.Components.push_back(0);
      continue;
    }

    // All other version components must be numbers.
    if (!SplitComponent.getAsInteger(10, ComponentNumber)) {
      checkVersionComponent(ComponentNumber, Range);
      CV.Components.push_back(ComponentNumber);
      continue;
    } else if (Diags) {
      Diags->diagnose(Range.Start,
                      diag::compiler_version_component_not_number);
    } else {
      llvm_unreachable("Invalid character in _compiler_version build configuration");
    }
  }

  if (CV.Components.size() > 5) {
    if (Diags) {
      Diags->diagnose(Loc, diag::compiler_version_too_many_components);
    } else {
      llvm_unreachable("Compiler version must not have more than 5 components");
    }
  }

  return CV;
}

Version Version::parseVersionString(StringRef VersionString,
                                                    SourceLoc Loc,
                                                    DiagnosticEngine *Diags) {
  Version CV;
  SmallString<16> digits;
  llvm::raw_svector_ostream OS(digits);
  SmallVector<std::pair<StringRef, SourceRange>, 5> SplitComponents;
  // Skip over quote character in string literal

  splitVersionComponents(SplitComponents, VersionString, Loc, Diags);

  uint64_t ComponentNumber;

  for (size_t i = 0; i < SplitComponents.size(); ++i) {
    StringRef SplitComponent;
    SourceRange Range;
    std::tie(SplitComponent, Range) = SplitComponents[i];

    // Version components can't be empty.
    if (SplitComponent.empty()) {
      if (Diags)
        Diags->diagnose(Range.Start, diag::empty_compiler_version_component);
      else
        llvm_unreachable("Found empty compiler version component");
      continue;
    }

    // All other version components must be numbers.
    if (!SplitComponent.getAsInteger(10, ComponentNumber)) {
      CV.Components.push_back(ComponentNumber);
      continue;
    } else if (Diags) {
      Diags->diagnose(Range.Start,
                      diag::compiler_version_component_not_number);
    } else {
      llvm_unreachable("Invalid character in _compiler_version build configuration");
    }
  }

  return CV;
}


Version Version::getCurrentCompilerVersion() {
#ifdef SWIFT_COMPILER_VERSION
  return Version::parseVersionString(
    SWIFT_COMPILER_VERSION, SourceLoc(), nullptr);
#else
  return Version();
#endif
}

std::string Version::str() const {
  std::string VersionString;
  llvm::raw_string_ostream OS(VersionString);
  for (auto i = Components.begin(); i != Components.end(); i++) {
    OS << *i;
    if (i != Components.end() - 1)
      OS << '.';
  }
  return OS.str();
}

std::string Version::preprocessorDefinition() const {
  SmallString<64> define("-D__SWIFT_COMPILER_VERSION=");
  llvm::raw_svector_ostream OS(define);
  uint64_t versionConstant = 0;

  auto NumComponents = Components.size();

  if (NumComponents > 0)
    versionConstant += Components[0] * 1000 * 1000 * 1000;
  // Component 2 is not used.
  if (NumComponents > 2)
    versionConstant += Components[2] * 1000 * 1000;
  if (NumComponents > 3)
    versionConstant += Components[3] * 1000;
  if (NumComponents > 4)
    versionConstant += Components[4];

  OS << versionConstant;
  return OS.str().str();
}

bool operator>=(const class Version &lhs,
                const class Version &rhs) {

  // The empty compiler version represents the latest possible version,
  // usually built from the source repository.
  if (lhs.empty())
    return true;

  auto n = std::min(lhs.size(), rhs.size());

  for (size_t i = 0; i < n; ++i) {
    if (lhs[i] < rhs[i])
      return false;
    else if (lhs[i] > rhs[i])
      return true;
  }
  return lhs.size() >= rhs.size();
}

std::pair<unsigned, unsigned> getSwiftNumericVersion() {
  return { SWIFT_VERSION_MAJOR, SWIFT_VERSION_MINOR };
}

std::string getSwiftFullVersion() {
  std::string buf;
  llvm::raw_string_ostream OS(buf);

#ifdef SWIFT_VENDOR
  OS << SWIFT_VENDOR " ";
#endif

  OS << "Swift version " SWIFT_VERSION_STRING;
#ifndef SWIFT_COMPILER_VERSION
  OS << "-dev";
#endif

#if defined(SWIFT_COMPILER_VERSION)
  OS << " (swiftlang-" SWIFT_COMPILER_VERSION;
#if defined(CLANG_COMPILER_VERSION)
  OS << " clang-" CLANG_COMPILER_VERSION;
#endif
  OS << ")";
#elif defined(LLVM_REVISION) || defined(CLANG_REVISION) || \
      defined(SWIFT_REVISION)
  OS << " (";
  printFullRevisionString(OS);
  OS << ")";
#endif
  return OS.str();
}

} // end namespace version
} // end namespace swift

