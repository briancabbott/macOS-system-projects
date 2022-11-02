//===--- TypeCheckGeneric.cpp - Generics ----------------------------------===//
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
// This file implements support for generics.
//
//===----------------------------------------------------------------------===//
#include "TypeChecker.h"
#include "GenericTypeResolver.h"
#include "swift/AST/ArchetypeBuilder.h"

using namespace swift;

Type DependentGenericTypeResolver::resolveGenericTypeParamType(
                                     GenericTypeParamType *gp) {
  // Don't resolve generic parameters.
  return gp;
}

Type DependentGenericTypeResolver::resolveDependentMemberType(
                                     Type baseTy,
                                     DeclContext *DC,
                                     SourceRange baseRange,
                                     ComponentIdentTypeRepr *ref) {
  return Builder.resolveArchetype(baseTy)->getRepresentative()
           ->getNestedType(ref->getIdentifier(), Builder)
           ->getDependentType(Builder, true);
}

Type DependentGenericTypeResolver::resolveSelfAssociatedType(
       Type selfTy,
       DeclContext *DC,
       AssociatedTypeDecl *assocType) {
  return Builder.resolveArchetype(selfTy)->getRepresentative()
           ->getNestedType(assocType->getName(), Builder)
           ->getDependentType(Builder, true);
}

Type DependentGenericTypeResolver::resolveTypeOfContext(DeclContext *dc) {
  if (auto nominal = dyn_cast<NominalTypeDecl>(dc))
    return nominal->getDeclaredInterfaceType();

  // FIXME: Should be the interface type of the extension.
  auto ext = dyn_cast<ExtensionDecl>(dc);
  return ext->getExtendedType()->getAnyNominal()->getDeclaredInterfaceType();
}

Type GenericTypeToArchetypeResolver::resolveGenericTypeParamType(
                                       GenericTypeParamType *gp) {
  auto gpDecl = gp->getDecl();
  assert(gpDecl && "Missing generic parameter declaration");

  auto archetype = gpDecl->getArchetype();
  if (!archetype)
    return ErrorType::get(gp->getASTContext());

  return archetype;
}

Type GenericTypeToArchetypeResolver::resolveDependentMemberType(
                                  Type baseTy,
                                  DeclContext *DC,
                                  SourceRange baseRange,
                                  ComponentIdentTypeRepr *ref) {
  llvm_unreachable("Dependent type after archetype substitution");
}

Type GenericTypeToArchetypeResolver::resolveSelfAssociatedType(
       Type selfTy,
       DeclContext *DC,
       AssociatedTypeDecl *assocType) {
  llvm_unreachable("Dependent type after archetype substitution");  
}

Type GenericTypeToArchetypeResolver::resolveTypeOfContext(DeclContext *dc) {
  return dc->getDeclaredTypeInContext();
}

Type PartialGenericTypeToArchetypeResolver::resolveGenericTypeParamType(
                                              GenericTypeParamType *gp) {
  auto gpDecl = gp->getDecl();
  if (!gpDecl)
    return Type(gp);


  auto archetype = gpDecl->getArchetype();
  if (!archetype)
    return Type(gp);

  return archetype;
}


Type PartialGenericTypeToArchetypeResolver::resolveDependentMemberType(
                                              Type baseTy,
                                              DeclContext *DC,
                                              SourceRange baseRange,
                                              ComponentIdentTypeRepr *ref) {
  // We don't have enough information to find the associated type.
  // FIXME: Nonsense, but we shouldn't need this code anyway.
  return DependentMemberType::get(baseTy, ref->getIdentifier(), TC.Context);
}

Type PartialGenericTypeToArchetypeResolver::resolveSelfAssociatedType(
       Type selfTy,
       DeclContext *DC,
       AssociatedTypeDecl *assocType) {
  // We don't have enough information to find the associated type.
  // FIXME: Nonsense, but we shouldn't need this code anyway.
  return DependentMemberType::get(selfTy, assocType->getName(), TC.Context);
}

Type
PartialGenericTypeToArchetypeResolver::resolveTypeOfContext(DeclContext *dc) {
  return dc->getDeclaredTypeInContext();
}

Type CompleteGenericTypeResolver::resolveGenericTypeParamType(
                                              GenericTypeParamType *gp) {
  // Retrieve the potential archetype corresponding to this generic type
  // parameter.
  // FIXME: When generic parameters can map down to specific types, do so
  // here.
  auto pa = Builder.resolveArchetype(gp);
  (void)pa;

  return gp;
}


Type CompleteGenericTypeResolver::resolveDependentMemberType(
                                    Type baseTy,
                                    DeclContext *DC,
                                    SourceRange baseRange,
                                    ComponentIdentTypeRepr *ref) {
  // Resolve the base to a potential archetype.
  auto basePA = Builder.resolveArchetype(baseTy);
  assert(basePA && "Missing potential archetype for base");
  basePA = basePA->getRepresentative();

  // Retrieve the potential archetype for the nested type.
  auto nestedPA = basePA->getNestedType(ref->getIdentifier(), Builder);

  // If this potential archetype was renamed due to typo correction,
  // complain and fix it.
  if (nestedPA->wasRenamed()) {
    auto newName = nestedPA->getName();
    TC.diagnose(ref->getIdLoc(), diag::invalid_member_type_suggest,
                baseTy, ref->getIdentifier(), newName)
      .fixItReplace(ref->getIdLoc(), newName.str());
    ref->overwriteIdentifier(newName);

    // Go get the actual nested type.
    nestedPA = basePA->getNestedType(newName, Builder);
    assert(!nestedPA->wasRenamed());
  }

  // If the nested type has been resolved to an associated type, use it.
  if (auto assocType = nestedPA->getResolvedAssociatedType()) {
    return DependentMemberType::get(baseTy, assocType, TC.Context);
  }

  Identifier name = ref->getIdentifier();
  SourceLoc nameLoc = ref->getIdLoc();

  // Check whether the name can be found in the superclass.
  // FIXME: The archetype builder should be doing this and mapping down to a
  // concrete type.
  if (auto superclassTy = basePA->getSuperclass()) {
    if (auto lookup = TC.lookupMemberType(DC, superclassTy, name)) {
      if (lookup.isAmbiguous()) {
        TC.diagnoseAmbiguousMemberType(baseTy, baseRange, name, nameLoc,
                                       lookup);
        return ErrorType::get(TC.Context);
      }

      // FIXME: Record (via type sugar) that this was referenced via baseTy.
      return lookup.front().second;
    }
  }

  // Complain that there is no suitable type.
  TC.diagnose(nameLoc, diag::invalid_member_type, name, baseTy)
    .highlight(baseRange);
  return ErrorType::get(TC.Context);
}

Type CompleteGenericTypeResolver::resolveSelfAssociatedType(Type selfTy,
       DeclContext *DC,
       AssociatedTypeDecl *assocType) {
  return Builder.resolveArchetype(selfTy)->getRepresentative()
           ->getNestedType(assocType->getName(), Builder)
           ->getDependentType(Builder, false);
}

Type CompleteGenericTypeResolver::resolveTypeOfContext(DeclContext *dc) {
  if (auto nominal = dyn_cast<NominalTypeDecl>(dc))
    return nominal->getDeclaredInterfaceType();

  // FIXME: Should be the interface type of the extension.
  auto ext = dyn_cast<ExtensionDecl>(dc);
  return ext->getExtendedType()->getAnyNominal()->getDeclaredInterfaceType();
}

/// Add the generic parameters and requirements from the parent context to the
/// archetype builder.
static void addContextParamsAndRequirements(ArchetypeBuilder &builder,
                                            DeclContext *dc,
                                            bool adoptArchetypes) {
  if (!dc->isTypeContext())
    return;

  if (auto sig = dc->getGenericSignatureOfContext()) {
    // Add generic signature from this context.
    builder.addGenericSignature(sig, adoptArchetypes);
  }
}

/// Check the generic parameters in the given generic parameter list (and its
/// parent generic parameter lists) according to the given resolver.
bool TypeChecker::checkGenericParamList(ArchetypeBuilder *builder,
                                        GenericParamList *genericParams,
                                        DeclContext *parentDC,
                                        bool adoptArchetypes,
                                        GenericTypeResolver *resolver) {
  bool invalid = false;

  // If there is a parent context, add the generic parameters and requirements
  // from that context.
  if (builder && parentDC)
    addContextParamsAndRequirements(*builder, parentDC, adoptArchetypes);

  // If there aren't any generic parameters at this level, we're done.
  if (!genericParams)
    return false;

  // Determine where and how to perform name lookup for the generic
  // parameter lists and where clause.
  TypeResolutionOptions options;
  DeclContext *lookupDC = genericParams->begin()[0]->getDeclContext();
  if (!lookupDC->isModuleScopeContext()) {
    assert(isa<NominalTypeDecl>(lookupDC) || isa<ExtensionDecl>(lookupDC) ||
           isa<AbstractFunctionDecl>(lookupDC) &&
           "not a proper generic parameter context?");
    options = TR_GenericSignature;
  }    

  // Visit each of the generic parameters.
  unsigned depth = genericParams->getDepth();
  for (auto param : *genericParams) {
    // Check the generic type parameter.
    // Set the depth of this type parameter.
    param->setDepth(depth);

    // Check the inheritance clause of this type parameter.
    checkInheritanceClause(param, resolver);

    if (builder) {
      // Add the generic parameter to the builder.
      if (builder->addGenericParameter(param))
        invalid = true;

      // Infer requirements from the inherited types.
      for (const auto &inherited : param->getInherited()) {
        if (builder->inferRequirements(inherited, genericParams))
          invalid = true;
      }
    }
  }

  // Visit each of the requirements, adding them to the builder.
  // Add the requirements clause to the builder, validating the types in
  // the requirements clause along the way.
  for (auto &req : genericParams->getRequirements()) {
    if (req.isInvalid())
      continue;

    switch (req.getKind()) {
    case RequirementKind::Conformance: {
      // Validate the types.
      if (validateType(req.getSubjectLoc(), lookupDC, options, resolver)) {
        invalid = true;
        req.setInvalid();
        continue;
      }

      if (validateType(req.getConstraintLoc(), lookupDC, options,
                       resolver)) {
        invalid = true;
        req.setInvalid();
        continue;
      }

      // FIXME: Feels too early to perform this check.
      if (!req.getConstraint()->isExistentialType() &&
          !req.getConstraint()->getClassOrBoundGenericClass()) {
        diagnose(genericParams->getWhereLoc(),
                 diag::requires_conformance_nonprotocol,
                 req.getSubjectLoc(), req.getConstraintLoc());
        req.getConstraintLoc().setInvalidType(Context);
        invalid = true;
        req.setInvalid();
        continue;
      }

      break;
    }

    case RequirementKind::SameType:
      if (validateType(req.getFirstTypeLoc(), lookupDC, options,
                       resolver)) {
        invalid = true;
        req.setInvalid();
        continue;
      }

      if (validateType(req.getSecondTypeLoc(), lookupDC, options,
                       resolver)) {
        invalid = true;
        req.setInvalid();
        continue;
      }
      
      break;

    case RequirementKind::WitnessMarker:
      llvm_unreachable("value witness markers in syntactic requirement?");
    }
    
    if (builder && builder->addRequirement(req)) {
      invalid = true;
      req.setInvalid();
    }
  }

  return invalid;
}

/// Collect all of the generic parameter types at every level in the generic
/// parameter list.
static void collectGenericParamTypes(
              GenericParamList *genericParams,
              DeclContext *parentDC,
              SmallVectorImpl<GenericTypeParamType *> &allParams) {
  // If the parent context is a generic type (or nested type thereof),
  // add its generic parameters.
  if (parentDC->isTypeContext()) {
    if (auto parentSig = parentDC->getGenericSignatureOfContext()) {
      allParams.append(parentSig->getGenericParams().begin(),
                       parentSig->getGenericParams().end());
    }
  }
    
  if (genericParams) {
    // Add our parameters.
    for (auto param : *genericParams) {
      allParams.push_back(param->getDeclaredType()
                            ->castTo<GenericTypeParamType>());
    }
  }
}

namespace {
  /// \brief Function object that orders potential archetypes by name.
  struct OrderPotentialArchetypeByName {
    using PotentialArchetype = ArchetypeBuilder::PotentialArchetype;

    bool operator()(std::pair<Identifier, PotentialArchetype *> X,
                    std::pair<Identifier, PotentialArchetype *> Y) const {
      return X.first.str() < Y.second->getName().str();
    }

    bool operator()(std::pair<Identifier, PotentialArchetype *> X,
                    Identifier Y) const {
      return X.first.str() < Y.str();
    }

    bool operator()(Identifier X,
                    std::pair<Identifier, PotentialArchetype *> Y) const {
      return X.str() < Y.first.str();
    }

    bool operator()(Identifier X, Identifier Y) const {
      return X.str() < Y.str();
    }
  };
}

/// Add the requirements for the given potential archetype and its nested
/// potential archetypes to the set of requirements.
static void
addRequirements(
    Module &mod, Type type,
    ArchetypeBuilder::PotentialArchetype *pa,
    llvm::SmallPtrSet<ArchetypeBuilder::PotentialArchetype *, 16> &knownPAs,
    SmallVectorImpl<Requirement> &requirements) {
  // If the potential archetype has been bound away to a concrete type,
  // it needs no requirements.
  if (pa->isConcreteType())
    return;
  
  // Add a value witness marker.
  requirements.push_back(Requirement(RequirementKind::WitnessMarker,
                                     type, Type()));

  // Add superclass requirement, if needed.
  if (auto superclass = pa->getSuperclass()) {
    // FIXME: Distinguish superclass from conformance?
    // FIXME: What if the superclass type involves a type parameter?
    requirements.push_back(Requirement(RequirementKind::Conformance,
                                       type, superclass));
  }

  // Add conformance requirements.
  SmallVector<ProtocolDecl *, 4> protocols;
  for (const auto &conforms : pa->getConformsTo()) {
    protocols.push_back(conforms.first);
  }

  ProtocolType::canonicalizeProtocols(protocols);
  for (auto proto : protocols) {
    requirements.push_back(Requirement(RequirementKind::Conformance,
                                       type, proto->getDeclaredType()));
  }
}

static void
addNestedRequirements(
    Module &mod, Type type,
    ArchetypeBuilder::PotentialArchetype *pa,
    llvm::SmallPtrSet<ArchetypeBuilder::PotentialArchetype *, 16> &knownPAs,
    SmallVectorImpl<Requirement> &requirements) {
  using PotentialArchetype = ArchetypeBuilder::PotentialArchetype;

  // Collect the nested types, sorted by name.
  // FIXME: Could collect these from the conformance requirements, above.
  SmallVector<std::pair<Identifier, PotentialArchetype*>, 16> nestedTypes;
  for (const auto &nested : pa->getNestedTypes()) {
    // FIXME: Dropping requirements among different associated types of the
    // same name.
    nestedTypes.push_back(std::make_pair(nested.first, nested.second.front()));
  }
  std::sort(nestedTypes.begin(), nestedTypes.end(),
            OrderPotentialArchetypeByName());

  // Add requirements for associated types.
  for (const auto &nested : nestedTypes) {
    auto rep = nested.second->getRepresentative();
    if (knownPAs.insert(rep).second) {
      // Form the dependent type that refers to this archetype.
      auto assocType = nested.second->getResolvedAssociatedType();
      if (!assocType)
        continue; // FIXME: If we do this late enough, there will be no failure.

      // Skip nested types bound to concrete types.
      if (rep->isConcreteType())
        continue;

      auto nestedType = DependentMemberType::get(type, assocType,
                                                 mod.getASTContext());

      addRequirements(mod, nestedType, rep, knownPAs, requirements);
      addNestedRequirements(mod, nestedType, rep, knownPAs, requirements);
    }
  }
}

/// Collect the set of requirements placed on the given generic parameters and
/// their associated types.
static void collectRequirements(ArchetypeBuilder &builder,
                                ArrayRef<GenericTypeParamType *> params,
                                SmallVectorImpl<Requirement> &requirements) {
  typedef ArchetypeBuilder::PotentialArchetype PotentialArchetype;

  // Find the "primary" potential archetypes, from which we'll collect all
  // of the requirements.
  llvm::SmallPtrSet<PotentialArchetype *, 16> knownPAs;
  llvm::SmallVector<GenericTypeParamType *, 8> primary;
  for (auto param : params) {
    auto pa = builder.resolveArchetype(param);
    assert(pa && "Missing potential archetype for generic parameter");

    // We only care about the representative.
    pa = pa->getRepresentative();

    if (knownPAs.insert(pa).second)
      primary.push_back(param);
  }

  // Add all of the conformance and superclass requirements placed on the given
  // generic parameters and their associated types.
  unsigned primaryIdx = 0, numPrimary = primary.size();
  while (primaryIdx < numPrimary) {
    unsigned depth = primary[primaryIdx]->getDepth();

    // For each of the primary potential archetypes, add the requirements.
    // Stop when we hit a parameter at a different depth.
    // FIXME: This algorithm falls out from the way the "all archetypes" lists
    // are structured. Once those lists no longer exist or are no longer
    // "the truth", we can simplify this algorithm considerably.
    unsigned lastPrimaryIdx = primaryIdx;
    for (unsigned idx = primaryIdx;
         idx < numPrimary && primary[idx]->getDepth() == depth;
         ++idx, ++lastPrimaryIdx) {
      auto param = primary[idx];
      auto pa = builder.resolveArchetype(param)->getRepresentative();

      // Add other requirements.
      addRequirements(builder.getModule(), param, pa, knownPAs,
                      requirements);
    }

    // For each of the primary potential archetypes, add the nested requirements.
    for (unsigned idx = primaryIdx; idx < lastPrimaryIdx; ++idx) {
      auto param = primary[idx];
      auto pa = builder.resolveArchetype(param)->getRepresentative();
      addNestedRequirements(builder.getModule(), param, pa, knownPAs,
                            requirements);
    }

    primaryIdx = lastPrimaryIdx;
  }


  // Add all of the same-type requirements.
  for (auto req : builder.getSameTypeRequirements()) {
    auto firstType = req.first->getDependentType(builder, false);
    Type secondType;
    if (auto concrete = req.second.dyn_cast<Type>())
      secondType = concrete;
    else if (auto secondPA = req.second.dyn_cast<PotentialArchetype*>())
      secondType = secondPA->getDependentType(builder, false);

    if (firstType->is<ErrorType>() || secondType->is<ErrorType>() ||
        firstType->isEqual(secondType))
      continue;

    requirements.push_back(Requirement(RequirementKind::SameType,
                                       firstType, secondType));
  }
}

/// Check the signature of a generic function.
static bool checkGenericFuncSignature(TypeChecker &tc,
                                      ArchetypeBuilder *builder,
                                      AbstractFunctionDecl *func,
                                      GenericTypeResolver &resolver) {
  bool badType = false;
  func->setIsBeingTypeChecked();

  // Check the generic parameter list.
  auto genericParams = func->getGenericParams();

  tc.checkGenericParamList(builder, genericParams,
                           func->getDeclContext(),
                           false, &resolver);

  // Check the parameter patterns.
  for (auto pattern : func->getBodyParamPatterns()) {
    // Check the pattern.
    if (tc.typeCheckPattern(pattern, func, TR_ImmediateFunctionInput,
                            &resolver))
      badType = true;

    // Infer requirements from the pattern.
    if (builder) {
      builder->inferRequirements(pattern, genericParams);
    }
  }

  // If there is a declared result type, check that as well.
  if (auto fn = dyn_cast<FuncDecl>(func)) {
    if (!fn->getBodyResultTypeLoc().isNull()) {
      // Check the result type of the function.
      TypeResolutionOptions options = TR_FunctionResult;
      if (fn->hasDynamicSelf())
        options |= TR_DynamicSelfResult;

      if (tc.validateType(fn->getBodyResultTypeLoc(), fn, options, &resolver)) {
        badType = true;
      }

      // Infer requirements from it.
      if (builder && fn->getBodyResultTypeLoc().getTypeRepr()) {
        builder->inferRequirements(fn->getBodyResultTypeLoc(), genericParams);
      }
    }
  }

  func->setIsBeingTypeChecked(false);
  return badType;
}

static Type getResultType(TypeChecker &TC, FuncDecl *fn, Type resultType) {
  // Look through optional types.
  OptionalTypeKind optKind;
  if (auto origValueType = resultType->getAnyOptionalObjectType(optKind)) {
    // Get the interface type of the result.
    Type ifaceValueType = getResultType(TC, fn, origValueType);

    // Preserve the optional type's original spelling if the interface
    // type is the same as the original.
    if (origValueType.getPointer() == ifaceValueType.getPointer()) {
      return resultType;
    }

    // Wrap the interface type in the right kind of optional.
    switch (optKind) {
    case OTK_None: llvm_unreachable("impossible");
    case OTK_Optional:
      return OptionalType::get(ifaceValueType);
    case OTK_ImplicitlyUnwrappedOptional:
      return ImplicitlyUnwrappedOptionalType::get(ifaceValueType);
    }
    llvm_unreachable("bad optional kind");
  }

  // Rewrite dynamic self to the appropriate interface type.
  if (resultType->is<DynamicSelfType>()) {
    return fn->getDynamicSelfInterface();
  }

  // Weird hacky special case.
  if (!fn->getBodyResultTypeLoc().hasLocation() &&
      fn->isGenericContext()) {
    // FIXME: This should not be rewritten.  This is only needed in cases where
    // we synthesize a function which returns a generic value.  In that case,
    // the return type is specified in terms of archetypes, but has no TypeLoc
    // in the TypeRepr.  Because of this, Sema isn't able to rebuild it in
    // terms of interface types.  When interface types prevail, this should be
    // removed.  Until then, we hack the mapping here.
    return TC.getInterfaceTypeFromInternalType(fn, resultType);
  }

  return resultType;
}

bool TypeChecker::validateGenericFuncSignature(AbstractFunctionDecl *func) {
  // Create the archetype builder.
  ArchetypeBuilder builder = createArchetypeBuilder(func->getParentModule());

  // Type check the function declaration, treating all generic type
  // parameters as dependent, unresolved.
  DependentGenericTypeResolver dependentResolver(builder);
  if (checkGenericFuncSignature(*this, &builder, func, dependentResolver)) {
    func->overwriteType(ErrorType::get(Context));
    return true;
  }

  // If this triggered a recursive validation, back out: we're done.
  // FIXME: This is an awful hack.
  if (func->hasType())
    return !func->isInvalid();

  // Finalize the generic requirements.
  (void)builder.finalize(func->getLoc());

  // The archetype builder now has all of the requirements, although there might
  // still be errors that have not yet been diagnosed. Revert the generic
  // function signature and type-check it again, completely.
  revertGenericFuncSignature(func);
  CompleteGenericTypeResolver completeResolver(*this, builder);
  if (checkGenericFuncSignature(*this, nullptr, func, completeResolver)) {
    func->overwriteType(ErrorType::get(Context));
    return true;
  }

  // The generic function signature is complete and well-formed. Determine
  // the type of the generic function.

  // Collect the complete set of generic parameter types.
  SmallVector<GenericTypeParamType *, 4> allGenericParams;
  collectGenericParamTypes(func->getGenericParams(),
                           func->getDeclContext(),
                           allGenericParams);

  // Collect the requirements placed on the generic parameter types.
  SmallVector<Requirement, 4> requirements;
  collectRequirements(builder, allGenericParams, requirements);

  // Compute the function type.
  Type funcTy;
  Type initFuncTy;
  if (auto fn = dyn_cast<FuncDecl>(func)) {
    funcTy = fn->getBodyResultTypeLoc().getType();
    
    if (!funcTy) {
      funcTy = TupleType::getEmpty(Context);
    } else {
      funcTy = getResultType(*this, fn, funcTy);
    }

  } else if (auto ctor = dyn_cast<ConstructorDecl>(func)) {
    // FIXME: shouldn't this just be
    // ctor->getDeclContext()->getDeclaredInterfaceType()?
    if (ctor->getDeclContext()->isProtocolOrProtocolExtensionContext()) {
      funcTy = ctor->getDeclContext()->getProtocolSelf()->getDeclaredType();
    } else {
      funcTy = ctor->getExtensionType()->getAnyNominal()
                 ->getDeclaredInterfaceType();
    }
    
    // Adjust result type for failability.
    if (ctor->getFailability() != OTK_None)
      funcTy = OptionalType::get(ctor->getFailability(), funcTy);

    initFuncTy = funcTy;
  } else {
    assert(isa<DestructorDecl>(func));
    funcTy = TupleType::getEmpty(Context);
  }

  auto patterns = func->getBodyParamPatterns();
  SmallVector<Pattern *, 4> storedPatterns;

  // FIXME: Destructors don't have the '()' pattern in their signature, so
  // paste it here.
  if (isa<DestructorDecl>(func)) {
    storedPatterns.append(patterns.begin(), patterns.end());

    Pattern *pattern = TuplePattern::create(Context, SourceLoc(), { },
                                            SourceLoc(), /*Implicit=*/true);
    pattern->setType(TupleType::getEmpty(Context));
    storedPatterns.push_back(pattern);
    patterns = storedPatterns;
  }

  auto sig = GenericSignature::get(allGenericParams, requirements);

  // Debugging of the archetype builder and generic signature generation.
  if (Context.LangOpts.DebugGenericSignatures) {
    func->dumpRef(llvm::errs());
    llvm::errs() << "\n";
    builder.dump(llvm::errs());
    llvm::errs() << "Generic signature: ";
    sig->print(llvm::errs());
    llvm::errs() << "\n";
    llvm::errs() << "Canonical generic signature: ";
    sig->getCanonicalSignature()->print(llvm::errs());
    llvm::errs() << "\n";
    llvm::errs() << "Canonical generic signature for mangling: ";
    sig->getCanonicalManglingSignature(*func->getParentModule())
      ->print(llvm::errs());
    llvm::errs() << "\n";    
  }

  bool hasSelf = func->getDeclContext()->isTypeContext();
  for (unsigned i = 0, e = patterns.size(); i != e; ++i) {
    Type argTy;
    Type initArgTy;

    Type selfTy;
    if (i == e-1 && hasSelf) {
      selfTy = func->computeInterfaceSelfType(/*isInitializingCtor=*/false);
      // Substitute in our own 'self' parameter.

      argTy = selfTy;
      if (initFuncTy) {
        initArgTy = func->computeInterfaceSelfType(/*isInitializingCtor=*/true);
      }
    } else {
      argTy = patterns[e - i - 1]->getType();

      // For an implicit declaration, our argument type will be in terms of
      // archetypes rather than dependent types. Replace the
      // archetypes with their corresponding dependent types.
      if (func->isImplicit()) {
        argTy = getInterfaceTypeFromInternalType(func, argTy); 
      }

      if (initFuncTy)
        initArgTy = argTy;
    }

    auto info = applyFunctionTypeAttributes(func, i);

    // FIXME: We shouldn't even get here if the function isn't locally generic
    // to begin with, but fixing that requires a lot of reengineering for local
    // definitions in generic contexts.
    if (sig && i == e-1) {
      if (func->getGenericParams()) {
        // Collect all generic params referenced in parameter types,
        // return type or requirements.
        SmallPtrSet<GenericTypeParamDecl *, 4> referencedGenericParams;
        argTy.visit([&referencedGenericParams](Type t) {
          if (isa<GenericTypeParamType>(t.getCanonicalTypeOrNull())) {
            referencedGenericParams.insert(
                t->castTo<GenericTypeParamType>()->getDecl());
          }
        });
        funcTy.visit([&referencedGenericParams](Type t) {
          if (isa<GenericTypeParamType>(t.getCanonicalTypeOrNull())) {
            referencedGenericParams.insert(
                t->castTo<GenericTypeParamType>()->getDecl());
          }
        });

        auto requirements = sig->getRequirements();
        for (auto req : requirements) {
          if (req.getKind() == RequirementKind::SameType) {
            // Same type requirements may allow for generic
            // inference, even if this generic parameter
            // is not mentioned in the function signature.
            // TODO: Make the test more precise.
            auto left = req.getFirstType();
            auto right = req.getSecondType();
            // For now consider any references inside requirements
            // as a possibility to infer the generic type.
            left.visit([&referencedGenericParams](Type t) {
              if (isa<GenericTypeParamType>(t.getCanonicalTypeOrNull())) {
                referencedGenericParams.insert(
                    t->castTo<GenericTypeParamType>()->getDecl());
              }
            });
            right.visit([&referencedGenericParams](Type t) {
              if (isa<GenericTypeParamType>(t.getCanonicalTypeOrNull())) {
                referencedGenericParams.insert(
                    t->castTo<GenericTypeParamType>()->getDecl());
              }
            });
          }
        }

        // Find the depth of the function's own generic parameters.
        unsigned fnGenericParamsDepth = func->getGenericParams()->getDepth();

        // Check that every generic parameter type from the signature is
        // among referencedArchetypes.
        for (auto *genParam : sig->getGenericParams()) {
          auto *paramDecl = genParam->getDecl();
          if (paramDecl->getDepth() != fnGenericParamsDepth)
            continue;
          if (!referencedGenericParams.count(paramDecl)) {
            // Produce an error that this generic parameter cannot be bound.
            diagnose(paramDecl->getLoc(), diag::unreferenced_generic_parameter,
                     paramDecl->getNameStr());
            func->setInvalid();
          }
        }
      }

      funcTy = GenericFunctionType::get(sig, argTy, funcTy, info);
      if (initFuncTy)
        initFuncTy = GenericFunctionType::get(sig, initArgTy, initFuncTy, info);
    } else {
      funcTy = FunctionType::get(argTy, funcTy, info);

      if (initFuncTy)
        initFuncTy = FunctionType::get(initArgTy, initFuncTy, info);
    }
  }

  // Record the interface type.
  func->setInterfaceType(funcTy);
  if (initFuncTy)
    cast<ConstructorDecl>(func)->setInitializerInterfaceType(initFuncTy);
  return false;
}

GenericSignature *TypeChecker::validateGenericSignature(
                    GenericParamList *genericParams,
                    DeclContext *dc,
                    GenericSignature *outerSignature,
                    std::function<bool(ArchetypeBuilder &)> inferRequirements,
                    bool &invalid) {
  assert(genericParams && "Missing generic parameters?");

  // Create the archetype builder.
  Module *module = dc->getParentModule();
  ArchetypeBuilder builder = createArchetypeBuilder(module);
  if (outerSignature)
    builder.addGenericSignature(outerSignature, true);

  // Type check the generic parameters, treating all generic type
  // parameters as dependent, unresolved.
  DependentGenericTypeResolver dependentResolver(builder);  
  if (checkGenericParamList(&builder, genericParams, dc,
                            false, &dependentResolver)) {
    invalid = true;
  }

  /// Perform any necessary requirement inference.
  if (inferRequirements && inferRequirements(builder)) {
    invalid = true;
  }

  // Finalize the generic requirements.
  (void)builder.finalize(genericParams->getSourceRange().Start);

  // The archetype builder now has all of the requirements, although there might
  // still be errors that have not yet been diagnosed. Revert the signature
  // and type-check it again, completely.
  revertGenericParamList(genericParams);
  CompleteGenericTypeResolver completeResolver(*this, builder);
  if (checkGenericParamList(nullptr, genericParams, dc,
                            false, &completeResolver)) {
    invalid = true;
  }

  // The generic signature is complete and well-formed. Gather the
  // generic parameter types at all levels.
  SmallVector<GenericTypeParamType *, 4> allGenericParams;
  collectGenericParamTypes(genericParams, dc, allGenericParams);

  // Collect the requirements placed on the generic parameter types.
  // FIXME: This ends up copying all of the requirements from outer scopes,
  // which is mostly harmless (but quite annoying).
  SmallVector<Requirement, 4> requirements;
  collectRequirements(builder, allGenericParams, requirements);

  // Record the generic type parameter types and the requirements.
  auto sig = GenericSignature::get(allGenericParams, requirements);

  // Debugging of the archetype builder and generic signature generation.
  if (Context.LangOpts.DebugGenericSignatures) {
    dc->printContext(llvm::errs());
    llvm::errs() << "\n";
    builder.dump(llvm::errs());
    llvm::errs() << "Generic signature: ";
    sig->print(llvm::errs());
    llvm::errs() << "\n";
    llvm::errs() << "Canonical generic signature: ";
    sig->getCanonicalSignature()->print(llvm::errs());
    llvm::errs() << "\n";
    llvm::errs() << "Canonical generic signature for mangling: ";
    sig->getCanonicalManglingSignature(*dc->getParentModule())
      ->print(llvm::errs());
    llvm::errs() << "\n";
  }

  return sig;
}

bool TypeChecker::validateGenericTypeSignature(NominalTypeDecl *nominal) {
  bool invalid = false;
  if (!nominal->IsValidatingGenericSignature()) {
    nominal->setIsValidatingGenericSignature();
    auto sig = validateGenericSignature(nominal->getGenericParams(),
                                        nominal->getDeclContext(),
                                        nullptr, nullptr, invalid);
    assert(sig->getInnermostGenericParams().size()
             == nominal->getGenericParams()->size());
    nominal->setGenericSignature(sig);
    nominal->setIsValidatingGenericSignature(false);
  }
  return invalid;
}

/// Create a text string that describes the bindings of generic parameters that
/// are relevant to the given set of types, e.g., "[with T = Bar, U = Wibble]".
///
/// \param types The types that will be scanned for generic type parameters,
/// which will be used in the resulting type.
///
/// \param genericParams The actual generic parameters, whose names will be used
/// in the resulting text.
///
/// \param substitutions The generic parameter -> generic argument substitutions
/// that will have been applied to these types. These are used to produce the
/// "parameter = argument" bindings in the test.
static std::string gatherGenericParamBindingsText(
                     ArrayRef<Type> types,
                     ArrayRef<GenericTypeParamType *> genericParams,
                     TypeSubstitutionMap &substitutions) {
  llvm::SmallPtrSet<GenericTypeParamType *, 2> knownGenericParams;
  for (auto type : types) {
    type.findIf([&](Type type) -> bool {
      if (auto gp = type->getAs<GenericTypeParamType>()) {
        knownGenericParams.insert(gp->getCanonicalType()
                                    ->castTo<GenericTypeParamType>());
      }
      return false;
    });
  }

  if (knownGenericParams.empty())
    return "";

  SmallString<128> result;
  for (auto gp : genericParams) {
    auto canonGP = gp->getCanonicalType()->castTo<GenericTypeParamType>();
    if (!knownGenericParams.count(canonGP))
      continue;

    if (result.empty())
      result += " [with ";
    else
      result += ", ";
    result += gp->getName().str();
    result += " = ";
    result += substitutions[canonGP].getString();
  }

  result += "]";
  return result.str().str();
}

bool TypeChecker::checkGenericArguments(DeclContext *dc, SourceLoc loc,
                                        SourceLoc noteLoc,
                                        Type owner,
                                        GenericSignature *genericSig,
                                        ArrayRef<Type> genericArgs) {
  // Form the set of generic substitutions required
  TypeSubstitutionMap substitutions;
  auto genericParams = genericSig->getGenericParams();
  assert(genericParams.size() == genericArgs.size());
  for (unsigned i = 0, n = genericParams.size(); i != n; ++i) {
    auto gp
      = genericParams[i]->getCanonicalType()->castTo<GenericTypeParamType>();
    substitutions[gp] = genericArgs[i];
  }

  // Check each of the requirements.
  Module *module = dc->getParentModule();
  for (const auto &req : genericSig->getRequirements()) {
    Type firstType = req.getFirstType().subst(module, substitutions,
                                              SubstFlags::IgnoreMissing);
    if (firstType.isNull()) {
      // Another requirement will fail later; just continue.
      continue;
    }

    Type secondType = req.getSecondType();
    if (secondType) {
      secondType = secondType.subst(module, substitutions,
                                    SubstFlags::IgnoreMissing);
      if (secondType.isNull()) {
        // Another requirement will fail later; just continue.
        continue;
      }
    }

    switch (req.getKind()) {
    case RequirementKind::Conformance: {
      // Protocol conformance requirements.
      if (auto proto = secondType->getAs<ProtocolType>()) {
        // FIXME: This should track whether this should result in a private
        // or non-private dependency.
        // FIXME: Do we really need "used" at this point?
        // FIXME: Poor location information. How much better can we do here?
        if (!conformsToProtocol(firstType, proto->getDecl(), dc,
                                ConformanceCheckFlags::Used, nullptr, loc))
          return true;

        continue;
      }

      // Superclass requirements.
      if (!isSubtypeOf(firstType, secondType, dc)) {
        // FIXME: Poor source-location information.
        diagnose(loc, diag::type_does_not_inherit, owner, firstType,
                 secondType);

        diagnose(noteLoc, diag::type_does_not_inherit_requirement,
                 req.getFirstType(), req.getSecondType(),
                 gatherGenericParamBindingsText(
                   {req.getFirstType(), req.getSecondType()},
                   genericParams, substitutions));
        return true;
      }

      continue;
    }

    case RequirementKind::SameType:
      if (!firstType->isEqual(secondType)) {
        // FIXME: Better location info for both diagnostics.
        diagnose(loc, diag::types_not_equal, owner, firstType, secondType);

        diagnose(noteLoc, diag::types_not_equal_requirement,
                 req.getFirstType(), req.getSecondType(),
                 gatherGenericParamBindingsText(
                   {req.getFirstType(), req.getSecondType()},
                   genericParams, substitutions));
        return true;
      }
      continue;
      
    case RequirementKind::WitnessMarker:
      continue;
    }
  }

  return false;
}

Type TypeChecker::getInterfaceTypeFromInternalType(DeclContext *dc, Type type) {
  assert(dc->isGenericContext() && "Not a generic context?");

  // Capture the archetype -> generic parameter type mapping.
  TypeSubstitutionMap substitutions;
  for (auto params = dc->getGenericParamsOfContext(); params;
       params = params->getOuterParameters()) {
    for (auto param : *params) {
      substitutions[param->getArchetype()] = param->getDeclaredType();
    }
  }

  return type.subst(dc->getParentModule(), substitutions, None);
}

Type TypeChecker::getWitnessType(Type type, ProtocolDecl *protocol,
                                 ProtocolConformance *conformance,
                                 Identifier name,
                                 Diag<> brokenProtocolDiag) {
  Type ty = ProtocolConformance::getTypeWitnessByName(type, conformance,
                                                      name, this);
  if (!ty && !conformance->isInvalid())
    diagnose(protocol->getLoc(), brokenProtocolDiag);

  return ty;
}
