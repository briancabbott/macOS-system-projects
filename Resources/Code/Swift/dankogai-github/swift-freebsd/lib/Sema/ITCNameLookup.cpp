//===--- ITCNameLookup.cpp - Iterative Type Checker Name Lookup -----------===//
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
//  This file implements the portions of the IterativeTypeChecker
//  class that involve name lookup.
//
//===----------------------------------------------------------------------===//
#include "TypeChecker.h"
#include "swift/Sema/IterativeTypeChecker.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/Decl.h"
#include <tuple>
using namespace swift;

//----------------------------------------------------------------------------//
// Qualified name lookup handling
//----------------------------------------------------------------------------//
bool IterativeTypeChecker::isQualifiedLookupInDeclContextSatisfied(
       TypeCheckRequest::DeclContextLookupPayloadType payload) {
  auto dc = payload.DC;

  NominalTypeDecl *nominal = nullptr;
  switch (dc->getContextKind()) {
  case DeclContextKind::AbstractClosureExpr:
  case DeclContextKind::AbstractFunctionDecl:
  case DeclContextKind::Initializer:
  case DeclContextKind::TopLevelCodeDecl:
  case DeclContextKind::SerializedLocal:
    llvm_unreachable("not a DeclContext that supports name lookup");

  case DeclContextKind::Module:
  case DeclContextKind::FileUnit:
    // Modules and file units can always handle name lookup.
    return true;

  case DeclContextKind::NominalTypeDecl:
    // Get the nominal type.
    nominal = cast<NominalTypeDecl>(dc);
    break;

  case DeclContextKind::ExtensionDecl: {
    auto ext = cast<ExtensionDecl>(dc);
    // FIXME: bind the extension. We currently assume this is done.
    nominal = ext->isNominalTypeOrNominalTypeExtensionContext();
    if (!nominal) return true;
    break;
  }
  }

  assert(nominal && "Only nominal types are handled here");
  // FIXME: Cache a bit indicating when qualified lookup is possible.

  // If we needed them for this query, did we already add implicit
  // initializers?
  auto name = payload.Name;
  if ((!name || name.matchesRef(getASTContext().Id_init)) &&
      !nominal->addedImplicitInitializers())
    return false;

  // For classes, check the superclass.
  if (auto classDecl = dyn_cast<ClassDecl>(nominal)) {
    if (!isSatisfied(requestTypeCheckSuperclass(classDecl)))
      return false;

    if (auto superclass = classDecl->getSuperclass()) {
      if (auto superclassDecl = superclass->getAnyNominal()) {
        if (!isSatisfied(requestQualifiedLookupInDeclContext({ superclassDecl,
                                                               payload.Name,
                                                               payload.Loc })))
          return false;
      }
    }
  }

  return true;
}

void IterativeTypeChecker::processQualifiedLookupInDeclContext(
       TypeCheckRequest::DeclContextLookupPayloadType payload,
       UnsatisfiedDependency unsatisfiedDependency) {
  auto nominal = payload.DC->isNominalTypeOrNominalTypeExtensionContext();
  assert(nominal && "Only nominal types are handled here");

  // For classes, we need the superclass (if any) to support qualified lookup.
  if (auto classDecl = dyn_cast<ClassDecl>(nominal)) {
    if (unsatisfiedDependency(requestTypeCheckSuperclass(classDecl)))
      return;

    if (auto superclass = classDecl->getSuperclass()) {
      if (auto superclassDecl = superclass->getAnyNominal()) {
        if (unsatisfiedDependency(
              requestQualifiedLookupInDeclContext({ superclassDecl,
                                                    payload.Name,
                                                    payload.Loc })))
          return;
      }
    }
  }

  // FIXME: we need to resolve the set of protocol conformances to do
  // unqualified lookup.

  // If we're looking for all names or for an initializer, resolve
  // implicitly-declared initializers.
  // FIXME: Recursion into old type checker.
  auto name = payload.Name;
  if (!name || name.matchesRef(getASTContext().Id_init))
    TC.resolveImplicitConstructors(nominal);
}

bool IterativeTypeChecker::breakCycleForQualifiedLookupInDeclContext(
       TypeCheckRequest::DeclContextLookupPayloadType payload) {
  return false;
}

//----------------------------------------------------------------------------//
// Qualified name lookup handling
//----------------------------------------------------------------------------//
bool IterativeTypeChecker::isUnqualifiedLookupInDeclContextSatisfied(
       TypeCheckRequest::DeclContextLookupPayloadType payload) {
  auto dc = payload.DC;
  switch (dc->getContextKind()) {
  case DeclContextKind::AbstractClosureExpr:
  case DeclContextKind::AbstractFunctionDecl:
  case DeclContextKind::Initializer:
  case DeclContextKind::TopLevelCodeDecl:
  case DeclContextKind::SerializedLocal:
    // FIXME: Actually do the lookup in these contexts, because if we find
    // something, we don't need to continue outward.
    return isUnqualifiedLookupInDeclContextSatisfied({dc->getParent(),
                                                      payload.Name,
                                                      payload.Loc});

  case DeclContextKind::Module:
  case DeclContextKind::FileUnit:
    // Modules and file units can always handle name lookup.
    return true;

  case DeclContextKind::NominalTypeDecl:
  case DeclContextKind::ExtensionDecl:
    // Check whether we can perform qualified lookup into this
    // declaration context.
    if (!isSatisfied(
          requestQualifiedLookupInDeclContext({dc, payload.Name, payload.Loc})))
      return false;
      
    // FIXME: If there is a name, actually perform qualified lookup
    // into this DeclContext. If it succeeds, there's nothing more to
    // do.
    return isUnqualifiedLookupInDeclContextSatisfied({dc->getParent(),
                                                      payload.Name,
                                                      payload.Loc});
  }
}

void IterativeTypeChecker::processUnqualifiedLookupInDeclContext(
       TypeCheckRequest::DeclContextLookupPayloadType payload,
       UnsatisfiedDependency unsatisfiedDependency) {
  // Everything is handled by the dependencies.
}

bool IterativeTypeChecker::breakCycleForUnqualifiedLookupInDeclContext(
       TypeCheckRequest::DeclContextLookupPayloadType payload) {
  return false;
}
