//===--- DeclAndTypePrinter.h - Emit ObjC decls from Swift AST --*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_PRINTASCLANG_DECLANDTYPEPRINTER_H
#define SWIFT_PRINTASCLANG_DECLANDTYPEPRINTER_H

#include "OutputLanguageMode.h"

#include "swift/AST/Type.h"
// for OptionalTypeKind
#include "swift/ClangImporter/ClangImporter.h"

namespace clang {
  class NamedDecl;
} // end namespace clang

namespace swift {

class PrimitiveTypeMapping;
class ValueDecl;
class SwiftToClangInteropContext;

/// Responsible for printing a Swift Decl or Type in Objective-C, to be
/// included in a Swift module's ObjC compatibility header.
class DeclAndTypePrinter {
public:
  using DelayedMemberSet = llvm::SmallSetVector<const ValueDecl *, 32>;

private:
  class Implementation;
  friend class Implementation;

  ModuleDecl &M;
  raw_ostream &os;
  raw_ostream &prologueOS;
  raw_ostream &outOfLineDefinitionsOS;
  const DelayedMemberSet &delayedMembers;
  PrimitiveTypeMapping &typeMapping;
  SwiftToClangInteropContext &interopContext;
  AccessLevel minRequiredAccess;
  bool requiresExposedAttribute;
  OutputLanguageMode outputLang;

  /// The name 'CFTypeRef'.
  ///
  /// Cached for convenience.
  Identifier ID_CFTypeRef;

  Implementation getImpl();

public:
  DeclAndTypePrinter(ModuleDecl &mod, raw_ostream &out, raw_ostream &prologueOS,
                     raw_ostream &outOfLineDefinitionsOS,
                     DelayedMemberSet &delayed,
                     PrimitiveTypeMapping &typeMapping,
                     SwiftToClangInteropContext &interopContext,
                     AccessLevel access, bool requiresExposedAttribute,
                     OutputLanguageMode outputLang)
      : M(mod), os(out), prologueOS(prologueOS),
        outOfLineDefinitionsOS(outOfLineDefinitionsOS), delayedMembers(delayed),
        typeMapping(typeMapping), interopContext(interopContext),
        minRequiredAccess(access),
        requiresExposedAttribute(requiresExposedAttribute),
        outputLang(outputLang) {}

  SwiftToClangInteropContext &getInteropContext() { return interopContext; }

  /// Returns true if \p VD should be included in a compatibility header for
  /// the options the printer was constructed with.
  bool shouldInclude(const ValueDecl *VD);

  void print(const Decl *D);
  void print(Type ty);

  /// Is \p ED empty of members and protocol conformances to include?
  bool isEmptyExtensionDecl(const ExtensionDecl *ED);

  /// Returns the type that will be printed by PrintAsObjC for a parameter or
  /// result type resolved to this declaration.
  ///
  /// \warning This handles \c _ObjectiveCBridgeable types, but it doesn't
  /// currently know about other aspects of PrintAsObjC behavior, like known
  /// types.
  const TypeDecl *getObjCTypeDecl(const TypeDecl* TD);

  /// Prints a category declaring the given members.
  ///
  /// All members must have the same parent type. The list must not be empty.
  void
  printAdHocCategory(iterator_range<const ValueDecl * const *> members);

  /// Returns the name of an <os/object.h> type minus the leading "OS_",
  /// or an empty string if \p decl is not an <os/object.h> type.
  static StringRef maybeGetOSObjectBaseName(const clang::NamedDecl *decl);

  static std::pair<Type, OptionalTypeKind>
  getObjectTypeAndOptionality(const ValueDecl *D, Type ty);
};

} // end namespace swift

#endif
