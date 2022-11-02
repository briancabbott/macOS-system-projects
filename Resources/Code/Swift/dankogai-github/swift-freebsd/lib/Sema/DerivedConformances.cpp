//===--- DerivedConformances.cpp - Derived conformance utilities ----------===//
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

#include "TypeChecker.h"
#include "swift/AST/Decl.h"
#include "swift/AST/Stmt.h"
#include "swift/AST/Expr.h"
#include "swift/AST/Pattern.h"
#include "swift/AST/Types.h"
#include "DerivedConformances.h"

using namespace swift;
using namespace DerivedConformance;

ValueDecl *DerivedConformance::getDerivableRequirement(NominalTypeDecl *nominal,
                                                       ValueDecl *requirement) {
  // Note: whenever you update this function, also update
  // TypeChecker::deriveProtocolRequirement.
  ASTContext &ctx = nominal->getASTContext();
  auto name = requirement->getFullName();

  // Local function that retrieves the requirement with the same name as
  // the provided requirement, but within the given known protocol.
  auto getRequirement = [&](KnownProtocolKind kind) -> ValueDecl * {
    // Dig out the protocol.
    auto proto = ctx.getProtocol(kind);
    if (!proto) return nullptr;

    // Check whether this nominal type derives conformances to the
    if (!nominal->derivesProtocolConformance(proto)) return nullptr;

    // Retrieve the requirement.
    auto results = proto->lookupDirect(name);
    return  results.empty() ? nullptr : results.front();
  };

  // Properties.
  if (isa<VarDecl>(requirement)) {
    // RawRepresentable.rawValue
    if (name.isSimpleName(ctx.Id_rawValue))
      return getRequirement(KnownProtocolKind::RawRepresentable);

    // Hashable.hashValue
    if (name.isSimpleName(ctx.Id_hashValue))
      return getRequirement(KnownProtocolKind::Hashable);

    // ErrorType._code
    if (name.isSimpleName(ctx.Id_code_))
      return getRequirement(KnownProtocolKind::ErrorType);

    // _BridgedNSError._NSErrorDomain
    if (name.isSimpleName(ctx.Id_NSErrorDomain))
      return getRequirement(KnownProtocolKind::BridgedNSError);

    return nullptr;
  }

  // Functions.
  if (auto func = dyn_cast<FuncDecl>(requirement)) {
    if (func->isOperator() && name.getBaseName().str() == "==")
      return getRequirement(KnownProtocolKind::Equatable);

    return nullptr;
  }

  // Initializers.
  if (isa<ConstructorDecl>(requirement)) {
    auto argumentNames = name.getArgumentNames();
    if (argumentNames.size() == 1 && argumentNames[0] == ctx.Id_rawValue)
      return getRequirement(KnownProtocolKind::RawRepresentable);

    return nullptr;
  }

  // Associated types.
  if (isa<AssociatedTypeDecl>(requirement)) {
    // RawRepresentable.RawValue
    if (name.isSimpleName(ctx.Id_RawValue))
      return getRequirement(KnownProtocolKind::RawRepresentable);

    return nullptr;
  }

  return nullptr;
}

void DerivedConformance::_insertOperatorDecl(ASTContext &C,
                                             IterableDeclContext *scope,
                                             Decl *member) {
  // Find the module.
  auto mod = member->getModuleContext();

  // Add it to the module in a DerivedFileUnit.
  mod->getDerivedFileUnit().addDerivedDecl(cast<FuncDecl>(member));

  // Add it as a derived global decl to the nominal type.
  auto oldDerived = scope->getDerivedGlobalDecls();
  auto oldSize = std::distance(oldDerived.begin(), oldDerived.end());
  auto newDerived = C.Allocate<Decl*>(oldSize + 1);

  std::move(oldDerived.begin(), oldDerived.end(), newDerived.begin());
  newDerived[oldSize] = member;

  scope->setDerivedGlobalDecls(newDerived);
}

DeclRefExpr *
DerivedConformance::createSelfDeclRef(AbstractFunctionDecl *fn) {
  ASTContext &C = fn->getASTContext();

  Pattern *curriedArgs = fn->getBodyParamPatterns().front();
  auto selfPattern =
    cast<NamedPattern>(curriedArgs->getSemanticsProvidingPattern());
  auto selfDecl = selfPattern->getDecl();
  return new (C) DeclRefExpr(selfDecl, SourceLoc(), /*implicit*/true);
}

FuncDecl *DerivedConformance::declareDerivedPropertyGetter(TypeChecker &tc,
                                                 Decl *parentDecl,
                                                 NominalTypeDecl *typeDecl,
                                                 Type contextType,
                                                 Type propertyInterfaceType,
                                                 Type propertyContextType,
                                                 bool isStatic) {
  auto &C = tc.Context;

  Type selfType = contextType;
  if (isStatic)
    selfType = MetatypeType::get(selfType);

  auto parentDC = cast<DeclContext>(parentDecl);

  VarDecl *selfDecl = new (C) ParamDecl(/*IsLet*/true,
                                        SourceLoc(),
                                        Identifier(),
                                        SourceLoc(),
                                        C.Id_self,
                                        selfType,
                                        parentDC);
  selfDecl->setImplicit();
  Pattern *selfParam = new (C) NamedPattern(selfDecl, /*implicit*/ true);
  selfParam->setType(selfType);
  selfParam = new (C) TypedPattern(selfParam,
                                   TypeLoc::withoutLoc(selfType));
  selfParam->setType(selfType);
  Pattern *methodParam = TuplePattern::create(C, SourceLoc(),{},SourceLoc());
  methodParam->setType(TupleType::getEmpty(C));
  Pattern *params[] = {selfParam, methodParam};

  FuncDecl *getterDecl =
    FuncDecl::create(C, SourceLoc(), StaticSpellingKind::None, SourceLoc(),
                     DeclName(), SourceLoc(), SourceLoc(), SourceLoc(),
                     nullptr, Type(), params,
                     TypeLoc::withoutLoc(propertyContextType), parentDC);
  getterDecl->setImplicit();
  getterDecl->setStatic(isStatic);

  // Compute the type of the getter.
  GenericParamList *genericParams = nullptr;
  Type type = FunctionType::get(TupleType::getEmpty(C),
                                propertyContextType);
  selfType = getterDecl->computeSelfType(&genericParams);
  if (genericParams)
    type = PolymorphicFunctionType::get(selfType, type, genericParams);
  else
    type = FunctionType::get(selfType, type);
  getterDecl->setType(type);
  getterDecl->setBodyResultType(propertyContextType);

  // Compute the interface type of the getter.
  Type interfaceType = FunctionType::get(TupleType::getEmpty(C),
                                         propertyInterfaceType);
  Type selfInterfaceType = getterDecl->computeInterfaceSelfType(false);
  if (auto sig = parentDC->getGenericSignatureOfContext())
    interfaceType = GenericFunctionType::get(sig, selfInterfaceType,
                                             interfaceType,
                                             FunctionType::ExtInfo());
  else
    interfaceType = type;
  getterDecl->setInterfaceType(interfaceType);
  getterDecl->setAccessibility(typeDecl->getFormalAccess());

  if (typeDecl->hasClangNode())
    tc.implicitlyDefinedFunctions.push_back(getterDecl);

  return getterDecl;
}

std::pair<VarDecl *, PatternBindingDecl *>
DerivedConformance::declareDerivedReadOnlyProperty(TypeChecker &tc,
                                                   Decl *parentDecl,
                                                   NominalTypeDecl *typeDecl,
                                                   Identifier name,
                                                   Type propertyInterfaceType,
                                                   Type propertyContextType,
                                                   FuncDecl *getterDecl,
                                                   bool isStatic) {
  auto &C = tc.Context;
  auto parentDC = cast<DeclContext>(parentDecl);

  VarDecl *propDecl = new (C) VarDecl(isStatic,
                                      /*let*/ false,
                                      SourceLoc(), name,
                                      propertyContextType,
                                      parentDC);
  propDecl->setImplicit();
  propDecl->makeComputed(SourceLoc(), getterDecl, nullptr, nullptr,
                         SourceLoc());
  propDecl->setAccessibility(typeDecl->getFormalAccess());
  propDecl->setInterfaceType(propertyInterfaceType);

  Pattern *propPat = new (C) NamedPattern(propDecl, /*implicit*/ true);
  propPat->setType(propertyContextType);
  propPat = new (C) TypedPattern(propPat,
                                 TypeLoc::withoutLoc(propertyContextType),
                                 /*implicit*/ true);

  auto pbDecl = PatternBindingDecl::create(C, SourceLoc(),
                                           StaticSpellingKind::None,
                                           SourceLoc(), propPat, nullptr,
                                           parentDC);
  pbDecl->setImplicit();

  return {propDecl, pbDecl};
}
