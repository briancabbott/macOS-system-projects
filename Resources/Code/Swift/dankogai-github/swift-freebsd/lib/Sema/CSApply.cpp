//===--- CSApply.cpp - Constraint Application -----------------------------===//
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
// This file implements application of a solution to a constraint
// system to a particular expression, resulting in a
// fully-type-checked expression.
//
//===----------------------------------------------------------------------===//

#include "ConstraintSystem.h"
#include "swift/AST/ArchetypeBuilder.h"
#include "swift/AST/ASTVisitor.h"
#include "swift/AST/ASTWalker.h"
#include "swift/AST/Attr.h"
#include "llvm/ADT/APFloat.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/SaveAndRestore.h"

using namespace swift;
using namespace constraints;

/// \brief  Get a substitution corresponding to the type witness.
/// Inspired by ProtocolConformance::getTypeWitnessByName.
const Substitution *
getTypeWitnessByName(ProtocolConformance *conformance,
                     Identifier name,
                     LazyResolver *resolver) {
  // Find the named requirement.
  AssociatedTypeDecl *assocType = nullptr;
  auto members = conformance->getProtocol()->lookupDirect(name);
  for (auto member : members) {
    assocType = dyn_cast<AssociatedTypeDecl>(member);
    if (assocType)
      break;
  }

  if (!assocType)
    return nullptr;

  assert(conformance && "Missing conformance information");
  return &conformance->getTypeWitness(assocType, resolver);
}


/// \brief Retrieve the fixed type for the given type variable.
Type Solution::getFixedType(TypeVariableType *typeVar) const {
  auto knownBinding = typeBindings.find(typeVar);
  assert(knownBinding != typeBindings.end());
  return knownBinding->second;
}

/// Determine whether the given type is an opened AnyObject.
static bool isOpenedAnyObject(Type type) {
  auto archetype = type->getAs<ArchetypeType>();
  if (!archetype)
    return false;

  auto existential = archetype->getOpenedExistentialType();
  if (!existential)
    return false;

  SmallVector<ProtocolDecl *, 2> protocols;
  existential->isExistentialType(protocols);
  return protocols.size() == 1 &&
         protocols[0]->isSpecificProtocol(KnownProtocolKind::AnyObject);
}

Type Solution::computeSubstitutions(
       Type origType, DeclContext *dc,
       Type openedType,
       ConstraintLocator *locator,
       SmallVectorImpl<Substitution> &substitutions) const {
  auto &tc = getConstraintSystem().getTypeChecker();
  auto &ctx = tc.Context;

  // Gather the substitutions from dependent types to concrete types.
  auto openedTypes = OpenedTypes.find(locator);
  assert(openedTypes != OpenedTypes.end() && "Missing opened type information");
  TypeSubstitutionMap typeSubstitutions;
  for (const auto &opened : openedTypes->second) {
    typeSubstitutions[opened.first.getPointer()] = getFixedType(opened.second);
  }

  // Produce the concrete form of the opened type.
  auto type = openedType.transform([&](Type type) -> Type {
                if (auto tv = dyn_cast<TypeVariableType>(type.getPointer())) {
                  auto archetype = tv->getImpl().getArchetype();
                  auto simplified = getFixedType(tv);
                  return SubstitutedType::get(archetype, simplified,
                                              tc.Context);
                }

                return type;
              });

  auto currentModule = getConstraintSystem().DC->getParentModule();
  ArchetypeType *currentArchetype = nullptr;
  Type currentReplacement;
  SmallVector<ProtocolConformance *, 4> currentConformances;

  ArrayRef<Requirement> requirements;
  if (auto genericFn = origType->getAs<GenericFunctionType>()) {
    requirements = genericFn->getRequirements();
  } else {
    requirements = dc->getGenericSignatureOfContext()->getRequirements();
  }

  for (const auto &req : requirements) {
    // Drop requirements for parameters that have been constrained away to
    // concrete types.
    auto firstArchetype
      = ArchetypeBuilder::mapTypeIntoContext(dc, req.getFirstType(), &tc)
        ->getAs<ArchetypeType>();
    if (!firstArchetype)
      continue;
    
    switch (req.getKind()) {
    case RequirementKind::Conformance:
      // If this is a protocol conformance requirement, get the conformance
      // and record it.
      if (auto protoType = req.getSecondType()->getAs<ProtocolType>()) {
        assert(firstArchetype == currentArchetype
               && "Archetype out-of-sync");
        ProtocolConformance *conformance = nullptr;
        Type replacement = currentReplacement;
        bool conforms = tc.conformsToProtocol(
                          replacement,
                          protoType->getDecl(),
                          getConstraintSystem().DC,
                          (ConformanceCheckFlags::InExpression|
                           ConformanceCheckFlags::Used),
                          &conformance);
        (void)isOpenedAnyObject;
        assert((conforms ||
                firstArchetype->getIsRecursive() ||
                isOpenedAnyObject(replacement) ||
                replacement->is<GenericTypeParamType>()) &&
               "Constraint system missed a conformance?");
        (void)conforms;

        assert(conformance || replacement->hasDependentProtocolConformances());
        currentConformances.push_back(conformance);

        break;
      }
      break;

    case RequirementKind::SameType:
      // Same-type requirements aren't recorded in substitutions.
      break;

    case RequirementKind::WitnessMarker:
      // Flush the current conformances.
      if (currentArchetype) {
        substitutions.push_back({
          currentArchetype,
          currentReplacement,
          ctx.AllocateCopy(currentConformances)
        });
        currentConformances.clear();
      }

      // Each witness marker starts a new substitution.
      currentArchetype = firstArchetype;
      currentReplacement = req.getFirstType().subst(currentModule,
                                                    typeSubstitutions,
                                                    None);
      break;
    }
  }
  
  // Flush the final conformances.
  if (currentArchetype) {
    substitutions.push_back({
      currentArchetype,
      currentReplacement,
      ctx.AllocateCopy(currentConformances),
    });
    currentConformances.clear();
  }

  return type;
}

/// \brief Find a particular named function witness for a type that conforms to
/// the given protocol.
///
/// \param tc The type check we're using.
///
/// \param dc The context in which we need a witness.
///
/// \param type The type whose witness to find.
///
/// \param proto The protocol to which the type conforms.
///
/// \param name The name of the requirement.
///
/// \param diag The diagnostic to emit if the protocol definition doesn't
/// have a requirement with the given name.
///
/// \returns The named witness, or nullptr if no witness could be found.
template <typename DeclTy>
static DeclTy *findNamedWitnessImpl(TypeChecker &tc, DeclContext *dc, Type type,
                                    ProtocolDecl *proto, DeclName name,
                                    Diag<> diag) {
  // Find the named requirement.
  DeclTy *requirement = nullptr;
  for (auto member : proto->getMembers()) {
    auto d = dyn_cast<DeclTy>(member);
    if (!d || !d->hasName())
      continue;

    if (d->getFullName().matchesRef(name)) {
      requirement = d;
      break;
    }
  }

  if (!requirement || requirement->isInvalid()) {
    tc.diagnose(proto->getLoc(), diag);
    return nullptr;
  }

  // Find the member used to satisfy the named requirement.
  ProtocolConformance *conformance = 0;
  bool conforms = tc.conformsToProtocol(type, proto, dc,
                                        ConformanceCheckFlags::InExpression,
                                        &conformance);
  if (!conforms)
    return nullptr;

  // For an type with dependent conformance, just return the requirement from
  // the protocol. There are no protocol conformance tables.
  if (type->hasDependentProtocolConformances()) {
    return requirement;
  }

  assert(conformance && "Missing conformance information");
  // FIXME: Dropping substitutions here.
  return cast_or_null<DeclTy>(
    conformance->getWitness(requirement, &tc).getDecl());
}

static VarDecl *findNamedPropertyWitness(TypeChecker &tc, DeclContext *dc,
                                         Type type, ProtocolDecl *proto,
                                         Identifier name, Diag<> diag) {
  return findNamedWitnessImpl<VarDecl>(tc, dc, type, proto, name, diag);
}

/// Return the implicit access kind for a MemberRefExpr with the
/// specified base and member in the specified DeclContext.
static AccessSemantics
getImplicitMemberReferenceAccessSemantics(Expr *base, VarDecl *member,
                                          DeclContext *DC) {
  // Properties that have storage and accessors are frequently accessed through
  // accessors.  However, in the init and destructor methods for the type
  // immediately containing the property, accesses are done direct.
  if (auto *AFD_DC = dyn_cast<AbstractFunctionDecl>(DC))
    if (member->hasStorage() &&
        // In a ctor or dtor.
        (isa<ConstructorDecl>(AFD_DC) || isa<DestructorDecl>(AFD_DC)) &&

        // Ctor or dtor are for immediate class, not a derived class.
        AFD_DC->getParent()->getDeclaredTypeOfContext()->getCanonicalType() ==
          member->getDeclContext()->getDeclaredTypeOfContext()->getCanonicalType() &&

        // Is a "self.property" reference.
        isa<DeclRefExpr>(base) &&
        AFD_DC->getImplicitSelfDecl() == cast<DeclRefExpr>(base)->getDecl()) {
      // Access this directly instead of going through (e.g.) observing or
      // trivial accessors.
      return AccessSemantics::DirectToStorage;
    }

  // If the value is always directly accessed from this context, do it.
  return member->getAccessSemanticsFromContext(DC);
}

namespace {
  /// \brief Rewrites an expression by applying the solution of a constraint
  /// system to that expression.
  class ExprRewriter : public ExprVisitor<ExprRewriter, Expr *> {
  public:
    ConstraintSystem &cs;
    DeclContext *dc;
    const Solution &solution;
    bool SuppressDiagnostics;

  private:
    /// \brief Coerce the given tuple to another tuple type.
    ///
    /// \param expr The expression we're converting.
    ///
    /// \param fromTuple The tuple type we're converting from, which is the same
    /// as \c expr->getType().
    ///
    /// \param toTuple The tuple type we're converting to.
    ///
    /// \param locator Locator describing where this tuple conversion occurs.
    ///
    /// \param sources The sources of each of the elements to be used in the
    /// resulting tuple, as provided by \c computeTupleShuffle.
    ///
    /// \param variadicArgs The source indices that are mapped to the variadic
    /// parameter of the resulting tuple, as provided by \c computeTupleShuffle.
    Expr *coerceTupleToTuple(Expr *expr, TupleType *fromTuple,
                             TupleType *toTuple,
                             ConstraintLocatorBuilder locator,
                             SmallVectorImpl<int> &sources,
                             SmallVectorImpl<unsigned> &variadicArgs);

    /// \brief Coerce the given scalar value to the given tuple type.
    ///
    /// \param expr The expression to be coerced.
    /// \param toTuple The tuple type to which the expression will be coerced.
    /// \param toScalarIdx The index of the scalar field within the tuple type
    /// \c toType.
    /// \param locator Locator describing where this conversion occurs.
    ///
    /// \returns The coerced expression, whose type will be equivalent to
    /// \c toTuple.
    Expr *coerceScalarToTuple(Expr *expr, TupleType *toTuple,
                              int toScalarIdx,
                              ConstraintLocatorBuilder locator);

    /// \brief Coerce the given value to existential type.
    ///
    /// The following conversions are supported:
    /// - concrete to existential
    /// - existential to existential
    /// - concrete metatype to existential metatype
    /// - existential metatype to existential metatype
    ///
    /// \param expr The expression to be coerced.
    /// \param toType The type to which the expression will be coerced.
    /// \param locator Locator describing where this conversion occurs.
    ///
    /// \return The coerced expression, whose type will be equivalent to
    /// \c toType.
    Expr *coerceExistential(Expr *expr, Type toType,
                            ConstraintLocatorBuilder locator);

    /// \brief Coerce an expression of (possibly unchecked) optional
    /// type to have a different (possibly unchecked) optional type.
    Expr *coerceOptionalToOptional(Expr *expr, Type toType,
                                   ConstraintLocatorBuilder locator);

    /// \brief Coerce an expression of implicitly unwrapped optional type to its
    /// underlying value type, in the correct way for an implicit
    /// look-through.
    Expr *coerceImplicitlyUnwrappedOptionalToValue(Expr *expr, Type objTy,
                                         ConstraintLocatorBuilder locator);

  public:
    /// \brief Build a reference to the given declaration.
    Expr *buildDeclRef(ValueDecl *decl, SourceLoc loc, Type openedType,
                       ConstraintLocatorBuilder locator,
                       bool specialized, bool implicit,
                       AccessSemantics semantics) {
      // Determine the declaration selected for this overloaded reference.
      auto &ctx = cs.getASTContext();

      // If this is a member of a nominal type, build a reference to the
      // member with an implied base type.
      if (decl->getDeclContext()->isTypeContext() && isa<FuncDecl>(decl)) {
        assert(cast<FuncDecl>(decl)->isOperator() && "Must be an operator");
        auto openedFnType = openedType->castTo<FunctionType>();
        auto baseTy = simplifyType(openedFnType->getInput())
                        ->getRValueInstanceType();
        Expr *base = TypeExpr::createImplicitHack(loc, baseTy, ctx);
        auto result = buildMemberRef(base, openedType, SourceLoc(), decl,
                                     loc, openedFnType->getResult(),
                                     locator, locator, implicit, semantics,
                                     /*isDynamic=*/false);
        if (!result)
          return nullptr;

        return result;
      }

      // If this is a declaration with generic function type, build a
      // specialized reference to it.
      if (auto genericFn
            = decl->getInterfaceType()->getAs<GenericFunctionType>()) {
        auto dc = decl->getPotentialGenericDeclContext();

        SmallVector<Substitution, 4> substitutions;
        auto type = solution.computeSubstitutions(
                      genericFn, dc, openedType,
                      getConstraintSystem().getConstraintLocator(locator),
                      substitutions);
        return new (ctx) DeclRefExpr(ConcreteDeclRef(ctx, decl, substitutions),
                                     loc, implicit, semantics, type);
      }

      auto type = simplifyType(openedType);
      return new (ctx) DeclRefExpr(decl, loc, implicit, semantics, type);
    }

    /// Describes an opened existential that has not yet been closed.
    struct OpenedExistential {
      /// The archetype describing this opened existential.
      ArchetypeType *Archetype;

      /// The existential value being opened.
      Expr *ExistentialValue;

      /// The opaque value (of archetype type) stored within the
      /// existential.
      OpaqueValueExpr *OpaqueValue;

      /// The depth of this currently-opened existential. Once the
      /// depth of the expression stack is equal to this value, the
      /// existential can be closed.
      unsigned Depth;
    };

    /// A stack of opened existentials that have not yet been closed.
    /// Ordered by decreasing depth.
    llvm::SmallVector<OpenedExistential, 2> OpenedExistentials;

    /// A stack of expressions being walked, used to compute existential depth.
    llvm::SmallVector<Expr *, 8> ExprStack;

    /// Members which are AbstractFunctionDecls but not FuncDecls cannot
    /// mutate self.
    bool isNonMutatingMember(ValueDecl *member) {
      if (!isa<AbstractFunctionDecl>(member))
        return false;
      return !isa<FuncDecl>(member) || !cast<FuncDecl>(member)->isMutating();
    }

    unsigned getNaturalArgumentCount(ValueDecl *member) {
      if (auto func = dyn_cast<AbstractFunctionDecl>(member)) {
        // For functions, close the existential once the function
        // has been fully applied.
        return func->getNaturalArgumentCount();
      } else {
        // For storage, close the existential either when it's
        // accessed (if it's an rvalue only) or when it is loaded or
        // stored (if it's an lvalue).
        assert(isa<AbstractStorageDecl>(member) &&
              "unknown member when opening existential");
        return 1;
      }
    }

    /// If the expression might be a dynamic method call, return the base
    /// value for the call.
    Expr *getBaseExpr(Expr *expr) {
      // Keep going up as long as this expression is the parent's base.
      if (auto unresolvedDot = dyn_cast<UnresolvedDotExpr>(expr)) {
        return unresolvedDot->getBase();
      // Remaining cases should only come up when we're re-typechecking.
      // FIXME: really it would be much better if Sema had stricter phase
      // separation.
      } else if (auto dotSyntax = dyn_cast<DotSyntaxCallExpr>(expr)) {
        return dotSyntax->getArg();
      } else if (auto ctorRef = dyn_cast<ConstructorRefCallExpr>(expr)) {
        return ctorRef->getArg();
      } else if (auto apply = dyn_cast<ApplyExpr>(expr)) {
        return apply->getFn();
      } else if (auto memberRef = dyn_cast<MemberRefExpr>(expr)) {
        return memberRef->getBase();
      } else if (auto dynMemberRef = dyn_cast<DynamicMemberRefExpr>(expr)) {
        return dynMemberRef->getBase();
      } else if (auto subscriptRef = dyn_cast<SubscriptExpr>(expr)) {
        return subscriptRef->getBase();
      } else if (auto dynSubscriptRef = dyn_cast<DynamicSubscriptExpr>(expr)) {
        return dynSubscriptRef->getBase();
      } else if (auto load = dyn_cast<LoadExpr>(expr)) {
        return load->getSubExpr();
      } else if (auto inout = dyn_cast<InOutExpr>(expr)) {
        return inout->getSubExpr();
      } else if (auto force = dyn_cast<ForceValueExpr>(expr)) {
        return force->getSubExpr();
      } else {
        return nullptr;
      }
    }

    /// Calculates the nesting depth of the current application.
    unsigned getArgCount(unsigned maxArgCount) {
      unsigned e = ExprStack.size();
      unsigned argCount;

      // Starting from the current expression, count up if the expression is
      // equal to its parent expression's base.
      Expr *prev = ExprStack.back();

      for (argCount = 1; argCount < maxArgCount && argCount < e; argCount++) {
        Expr *result = ExprStack[e - argCount - 1];
        Expr *base = getBaseExpr(result);
        if (base != prev)
          break;
        prev = result;
      }

      return argCount;
    }

    /// Open an existential value into a new, opaque value of
    /// archetype type.
    ///
    /// \param base An expression of existential type whose value will
    /// be opened.
    ///
    /// \param archetype The archetype that describes the opened existential
    /// type.
    ///
    /// \param member The member that is being referenced on the existential
    /// type.
    ///
    /// \returns An OpaqueValueExpr that provides a reference to the value
    /// stored within the expression or its metatype (if the base was a
    /// metatype).
    Expr *openExistentialReference(Expr *base, ArchetypeType *archetype,
                                   ValueDecl *member) {
      assert(archetype && "archetype not already opened?");

      auto &tc = cs.getTypeChecker();

      // Dig out the base type.
      auto baseTy = base->getType();

      // Look through lvalues.
      bool isLValue = false;
      if (auto lvalueTy = baseTy->getAs<LValueType>()) {
        isLValue = true;
        baseTy = lvalueTy->getObjectType();
      }

      // Look through metatypes.
      bool isMetatype = false;
      if (auto metaTy = baseTy->getAs<AnyMetatypeType>()) {
        isMetatype = true;
        baseTy = metaTy->getInstanceType();
      }

      assert(baseTy->isAnyExistentialType() && "Type must be existential");

      // If the base was an lvalue but it will only be treated as an
      // rvalue, turn the base into an rvalue now. This results in
      // better SILGen.
      if (isLValue &&
          (isNonMutatingMember(member) ||
           isMetatype || baseTy->isClassExistentialType())) {
        base = tc.coerceToRValue(base);
        isLValue = false;
      }

      // Determine the number of applications that need to occur before
      // we can close this existential, and record it.
      unsigned maxArgCount = getNaturalArgumentCount(member);
      unsigned depth = ExprStack.size() - getArgCount(maxArgCount);

      // Create the opaque opened value. If we started with a
      // metatype, it's a metatype.
      Type opaqueType = archetype;
      if (isMetatype)
        opaqueType = MetatypeType::get(opaqueType);
      if (isLValue)
        opaqueType = LValueType::get(opaqueType);

      ASTContext &ctx = tc.Context;
      auto archetypeVal = new (ctx) OpaqueValueExpr(base->getLoc(), opaqueType);

      // Record the opened existential.
      OpenedExistentials.push_back({archetype, base, archetypeVal, depth});

      return archetypeVal;
    }

    /// Trying to close the active existential, if there is one.
    bool closeExistential(Expr *&result, bool force=false) {
      if (OpenedExistentials.empty())
        return false;

      auto &record = OpenedExistentials.back();
      assert(record.Depth <= ExprStack.size() - 1);

      if (!force && record.Depth < ExprStack.size() - 1)
        return false;

      // If we had a return type of 'Self', erase it.
      ConstraintSystem &cs = solution.getConstraintSystem();
      auto &tc = cs.getTypeChecker();
      auto resultTy = result->getType();
      if (resultTy->hasOpenedExistential(record.Archetype)) {
        // Erase the opened existential.

        // Remove the optional, if present.
        OptionalTypeKind optKind;
        if (auto optValueTy = resultTy->getAnyOptionalObjectType(optKind)) {
          resultTy = optValueTy;
        }

        //   - Drill down to the optional value (if necessary).
        if (optKind) {
          result = new (tc.Context) BindOptionalExpr(result,
                                                     result->getEndLoc(),
                                                     0,
                                                     resultTy);
          result->setImplicit(true);
        }

        Type erasedTy;
        if (resultTy->isEqual(record.Archetype)) {
          //   - Coerce to an existential value.
          erasedTy = record.Archetype->getOpenedExistentialType();

          result = coerceToType(result, erasedTy, nullptr);
          // FIXME: can this really ever fail? We'll leave behind rogue
          // OpaqueValueExprs if that is the case.
          assert(result);
        } else {
          //   - Perform a covariant function coercion.
          erasedTy = resultTy->eraseOpenedExistential(
                       cs.DC->getParentModule(),
                       record.Archetype);
          result = new (tc.Context) CovariantFunctionConversionExpr(
                                      result,
                                      erasedTy);
        }

        //   - Bind up the result back up as an optional (if necessary).
        if (optKind) {
          Type optErasedTy = OptionalType::get(optKind, erasedTy);
          result = new (tc.Context) InjectIntoOptionalExpr(result,
                                                           optErasedTy);
          result = new (tc.Context) OptionalEvaluationExpr(result,
                                                           optErasedTy);
        }
      }

      // If the opaque value has an l-value access kind, then
      // the OpenExistentialExpr isn't making a derived l-value, which
      // means this is our only chance to propagate the l-value access kind
      // down to the original existential value.  Otherwise, propagateLVAK
      // will handle this.
      if (record.OpaqueValue->hasLValueAccessKind())
        record.ExistentialValue->propagateLValueAccessKind(
                                    record.OpaqueValue->getLValueAccessKind());

      // Form the open-existential expression.
      result = new (tc.Context) OpenExistentialExpr(
                                  record.ExistentialValue,
                                  record.OpaqueValue,
                                  result);

      OpenedExistentials.pop_back();
      return true;
    }

    /// Is the given function a constructor of a class or protocol?
    /// Such functions are subject to DynamicSelf manipulations.
    ///
    /// We want to avoid taking the DynamicSelf paths for other
    /// constructors for two reasons:
    ///   - it's an unnecessary cost
    ///   - optionality preservation has a problem with constructors on
    ///     optional types
    static bool isPolymorphicConstructor(AbstractFunctionDecl *fn) {
      if (!isa<ConstructorDecl>(fn))
        return false;
      DeclContext *parent = fn->getParent();
      if (auto extension = dyn_cast<ExtensionDecl>(parent))
        parent = extension->getExtendedType()->getAnyNominal();
      return (isa<ClassDecl>(parent) || isa<ProtocolDecl>(parent));
    }

    /// \brief Build a new member reference with the given base and member.
    Expr *buildMemberRef(Expr *base, Type openedFullType, SourceLoc dotLoc,
                         ValueDecl *member, SourceLoc memberLoc,
                         Type openedType, ConstraintLocatorBuilder locator,
                         ConstraintLocatorBuilder memberLocator,
                         bool Implicit, AccessSemantics semantics,
                         bool isDynamic) {
      auto &tc = cs.getTypeChecker();
      auto &context = tc.Context;

      bool isSuper = base->isSuperExpr();

      Type baseTy = base->getType()->getRValueType();

      // Explicit member accesses are permitted to implicitly look
      // through ImplicitlyUnwrappedOptional<T>.
      if (!Implicit) {
        if (auto objTy = cs.lookThroughImplicitlyUnwrappedOptionalType(baseTy)) {
          base = coerceImplicitlyUnwrappedOptionalToValue(base, objTy, locator);
          baseTy = objTy;
        }
      }

      // Figure out the actual base type, and whether we have an instance of
      // that type or its metatype.
      bool baseIsInstance = true;
      if (auto baseMeta = baseTy->getAs<AnyMetatypeType>()) {
        baseIsInstance = false;
        baseTy = baseMeta->getInstanceType();
        // If the member is a constructor, verify that it can be legally
        // referenced from this base.
        if (auto ctor = dyn_cast<ConstructorDecl>(member)) {
          if (!tc.diagnoseInvalidDynamicConstructorReferences(base, memberLoc,
                                           baseMeta, ctor, SuppressDiagnostics))
            return nullptr;
        }
      }

      // Produce a reference to the member, the type of the container it
      // resides in, and the type produced by the reference itself.
      Type containerTy;
      ConcreteDeclRef memberRef;
      Type refTy;
      Type dynamicSelfFnType;
      if (openedFullType->hasTypeVariable()) {
        // We require substitutions. Figure out what they are.

        // Figure out the declaration context where we'll get the generic
        // parameters.
        auto dc = member->getPotentialGenericDeclContext();

        // Build a reference to the generic member.
        SmallVector<Substitution, 4> substitutions;
        refTy = solution.computeSubstitutions(
                  member->getInterfaceType(),
                  dc,
                  openedFullType,
                  getConstraintSystem().getConstraintLocator(memberLocator),
                  substitutions);

        memberRef = ConcreteDeclRef(context, member, substitutions);

        if (auto openedFullFnType = openedFullType->getAs<FunctionType>()) {
          auto openedBaseType = openedFullFnType->getInput()
                                  ->getRValueInstanceType();
          containerTy = solution.simplifyType(tc, openedBaseType);
        }
      } else {
        // No substitutions required; the declaration reference is simple.
        containerTy = member->getDeclContext()->getDeclaredTypeOfContext();
        memberRef = member;
        auto baseDeclRefExpr = dyn_cast<DeclRefExpr>(base);
        refTy = tc.getUnopenedTypeOfReference(member, Type(), dc,
                                              baseDeclRefExpr,
                                              /*wantInterfaceType=*/true);
      }

      // If we opened up an existential when referencing this member, update
      // the base accordingly.
      auto knownOpened = solution.OpenedExistentialTypes.find(
                           getConstraintSystem().getConstraintLocator(
                             memberLocator));
      bool openedExistential = false;
      if (knownOpened != solution.OpenedExistentialTypes.end()) {
        base = openExistentialReference(base, knownOpened->second, member);
        baseTy = knownOpened->second;
        containerTy = baseTy;
        openedExistential = true;
      }

      // If this is a method whose result type is dynamic Self, or a
      // construction, replace the result type with the actual object type.
      if (auto func = dyn_cast<AbstractFunctionDecl>(member)) {
        if ((isa<FuncDecl>(func) &&
             (cast<FuncDecl>(func)->hasDynamicSelf() ||
              (openedExistential && cast<FuncDecl>(func)->hasArchetypeSelf()))) ||
            isPolymorphicConstructor(func)) {
          refTy = refTy->replaceCovariantResultType(
                    containerTy,
                    func->getNumParamPatterns());
          dynamicSelfFnType = refTy->replaceCovariantResultType(
                                baseTy,
                                func->getNumParamPatterns());

          if (openedExistential) {
            // Replace the covariant result type in the opened type. We need to
            // handle dynamic member references, which wrap the function type
            // in an optional.
            OptionalTypeKind optKind;
            if (auto optObject = openedType->getAnyOptionalObjectType(optKind))
              openedType = optObject;
            openedType = openedType->replaceCovariantResultType(
                           baseTy,
                           func->getNumParamPatterns()-1);
            if (optKind != OptionalTypeKind::OTK_None)
              openedType = OptionalType::get(optKind, openedType);
          }

          // If the type after replacing DynamicSelf with the provided base
          // type is no different, we don't need to perform a conversion here.
          if (refTy->isEqual(dynamicSelfFnType))
            dynamicSelfFnType = nullptr;
        }
      }

      // If we're referring to the member of a module, it's just a simple
      // reference.
      if (baseTy->is<ModuleType>()) {
        assert(semantics == AccessSemantics::Ordinary &&
               "Direct property access doesn't make sense for this");
        assert(!dynamicSelfFnType && "No reference type to convert to");
        Expr *ref = new (context) DeclRefExpr(memberRef, memberLoc, Implicit);
        ref->setType(refTy);
        return new (context) DotSyntaxBaseIgnoredExpr(base, dotLoc, ref);
      }

      // Otherwise, we're referring to a member of a type.

      // Is it an archetype member?
      bool isDependentConformingRef
            = isa<ProtocolDecl>(member->getDeclContext()) &&
              baseTy->hasDependentProtocolConformances();

      // References to properties with accessors and storage usually go
      // through the accessors, but sometimes are direct.
      if (auto *VD = dyn_cast<VarDecl>(member)) {
        if (semantics == AccessSemantics::Ordinary)
          semantics = getImplicitMemberReferenceAccessSemantics(base, VD, dc);
      }

      if (baseIsInstance) {
        // Convert the base to the appropriate container type, turning it
        // into an lvalue if required.
        Type selfTy;
        if (isDependentConformingRef)
          selfTy = baseTy;
        else
          selfTy = containerTy;
        
        // If the base is already an lvalue with the right base type, we can
        // pass it as an inout qualified type.
        if (selfTy->isEqual(baseTy))
          if (base->getType()->is<LValueType>())
            selfTy = InOutType::get(selfTy);
        base = coerceObjectArgumentToType(
                 base,  selfTy, member, semantics,
                 locator.withPathElement(ConstraintLocator::MemberRefBase));
      } else {
        // Convert the base to an rvalue of the appropriate metatype.
        base = coerceToType(base,
                            MetatypeType::get(isDependentConformingRef
                                                ? baseTy
                                                : containerTy),
                            locator.withPathElement(
                              ConstraintLocator::MemberRefBase));
        if (!base)
          return nullptr;

        base = tc.coerceToRValue(base);
      }
      assert(base && "Unable to convert base?");

      // Handle dynamic references.
      if (isDynamic || member->getAttrs().hasAttribute<OptionalAttr>()) {
        base = tc.coerceToRValue(base);
        if (!base) return nullptr;
        Expr *ref = new (context) DynamicMemberRefExpr(base, dotLoc, memberRef,
                                                       memberLoc);
        ref->setImplicit(Implicit);

        // Compute the type of the reference.
        Type refType = simplifyType(openedType);

        // If the base was an opened existential, erase the opened
        // existential.
        if (openedExistential &&
            refType->hasOpenedExistential(knownOpened->second)) {
          refType = refType->eraseOpenedExistential(
                      cs.DC->getParentModule(),
                      knownOpened->second);
        }

        ref->setType(refType);

        closeExistential(ref, /*force=*/openedExistential);

        return ref;
      }

      // For types and properties, build member references.
      if (isa<TypeDecl>(member) || isa<VarDecl>(member)) {
        assert(!dynamicSelfFnType && "Converted type doesn't make sense here");
        assert(baseIsInstance || !member->isInstanceMember());

        auto memberRefExpr
          = new (context) MemberRefExpr(base, dotLoc, memberRef,
                                        memberLoc, Implicit, semantics);
        memberRefExpr->setIsSuper(isSuper);

        // Skip the synthesized 'self' input type of the opened type.
        memberRefExpr->setType(simplifyType(openedType));
        Expr *result = memberRefExpr;
        closeExistential(result);
        return result;
      }
      
      // Handle all other references.
      Expr *ref = new (context) DeclRefExpr(memberRef, memberLoc, Implicit,
                                            semantics);
      ref->setType(refTy);

      // If the reference needs to be converted, do so now.
      if (dynamicSelfFnType) {
        ref = new (context) CovariantFunctionConversionExpr(ref,
                                                            dynamicSelfFnType);
      }

      ApplyExpr *apply;
      if (isa<ConstructorDecl>(member)) {
        // FIXME: Provide type annotation.
        apply = new (context) ConstructorRefCallExpr(ref, base);
      } else if (!baseIsInstance && member->isInstanceMember()) {
        // Reference to an unbound instance method.
        Expr *result = new (context) DotSyntaxBaseIgnoredExpr(base, dotLoc,
                                                              ref);
        closeExistential(result, /*force=*/openedExistential);
        return result;
      } else {
        assert((!baseIsInstance || member->isInstanceMember()) &&
               "can't call a static method on an instance");
        apply = new (context) DotSyntaxCallExpr(ref, dotLoc, base);
      }
      return finishApply(apply, openedType, nullptr);
    }
    
    /// \brief Describes either a type or the name of a type to be resolved.
    typedef llvm::PointerUnion<Identifier, Type> TypeOrName;

    /// \brief Convert the given literal expression via a protocol pair.
    ///
    /// This routine handles the two-step literal conversion process used
    /// by integer, float, character, extended grapheme cluster, and string
    /// literals. The first step uses \c builtinProtocol while the second
    /// step uses \c protocol.
    ///
    /// \param literal The literal expression.
    ///
    /// \param type The literal type. This type conforms to \c protocol,
    /// and may also conform to \c builtinProtocol.
    ///
    /// \param openedType The literal type as it was opened in the type system.
    ///
    /// \param protocol The protocol that describes the literal requirement.
    ///
    /// \param literalType Either the name of the associated type in
    /// \c protocol that describes the argument type of the conversion function
    /// (\c literalFuncName) or the argument type itself.
    ///
    /// \param literalFuncName The name of the conversion function requirement
    /// in \c protocol.
    ///
    /// \param builtinProtocol The "builtin" form of the protocol, which
    /// always takes builtin types and can only be properly implemented
    /// by standard library types. If \c type does not conform to this
    /// protocol, it's literal type will.
    ///
    /// \param builtinLiteralType Either the name of the associated type in
    /// \c builtinProtocol that describes the argument type of the builtin
    /// conversion function (\c builtinLiteralFuncName) or the argument type
    /// itself.
    ///
    /// \param builtinLiteralFuncName The name of the conversion function
    /// requirement in \c builtinProtocol.
    ///
    /// \param isBuiltinArgType Function that determines whether the given
    /// type is acceptable as the argument type for the builtin conversion.
    ///
    /// \param brokenProtocolDiag The diagnostic to emit if the protocol
    /// is broken.
    ///
    /// \param brokenBuiltinProtocolDiag The diagnostic to emit if the builtin
    /// protocol is broken.
    ///
    /// \returns the converted literal expression.
    Expr *convertLiteral(Expr *literal,
                         Type type,
                         Type openedType,
                         ProtocolDecl *protocol,
                         TypeOrName literalType,
                         DeclName literalFuncName,
                         ProtocolDecl *builtinProtocol,
                         TypeOrName builtinLiteralType,
                         DeclName builtinLiteralFuncName,
                         bool (*isBuiltinArgType)(Type),
                         Diag<> brokenProtocolDiag,
                         Diag<> brokenBuiltinProtocolDiag);

    /// \brief Finish a function application by performing the appropriate
    /// conversions on the function and argument expressions and setting
    /// the resulting type.
    ///
    /// \param apply The function application to finish type-checking, which
    /// may be a newly-built expression.
    ///
    /// \param openedType The "opened" type this expression had during
    /// type checking, which will be used to specialize the resulting,
    /// type-checked expression appropriately.
    ///
    /// \param locator The locator for the original expression.
    Expr *finishApply(ApplyExpr *apply, Type openedType,
                      ConstraintLocatorBuilder locator);

  private:
    /// \brief Retrieve the overload choice associated with the given
    /// locator.
    SelectedOverload getOverloadChoice(ConstraintLocator *locator) {
      return *getOverloadChoiceIfAvailable(locator);
    }

    /// \brief Retrieve the overload choice associated with the given
    /// locator.
    Optional<SelectedOverload>
    getOverloadChoiceIfAvailable(ConstraintLocator *locator) {
      auto known = solution.overloadChoices.find(locator);
      if (known != solution.overloadChoices.end())
        return known->second;

      return None;
    }

    /// \brief Simplify the given type by substituting all occurrences of
    /// type variables for their fixed types.
    Type simplifyType(Type type) {
      return solution.simplifyType(cs.getTypeChecker(), type);
    }

  public:
    
    
    /// \brief Coerce a closure expression with a non-void return type to a
    /// contextual function type with a void return type.
    ///
    /// This operation cannot fail.
    ///
    /// \param expr The closure expression to coerce.
    ///
    /// \returns The coerced closure expression.
    ///
    ClosureExpr *coerceClosureExprToVoid(ClosureExpr *expr);
    
    /// \brief Coerce the given expression to the given type.
    ///
    /// This operation cannot fail.
    ///
    /// \param expr The expression to coerce.
    /// \param toType The type to coerce the expression to.
    /// \param locator Locator used to describe where in this expression we are.
    ///
    /// \returns the coerced expression, which will have type \c ToType.
    Expr *coerceToType(Expr *expr, Type toType,
                       ConstraintLocatorBuilder locator);

    /// \brief Coerce the given expression (which is the argument to a call) to
    /// the given parameter type.
    ///
    /// This operation cannot fail.
    ///
    /// \param arg The argument expression.
    /// \param paramType The parameter type.
    /// \param locator Locator used to describe where in this expression we are.
    ///
    /// \returns the coerced expression, which will have type \c ToType.
    Expr *coerceCallArguments(Expr *arg, Type paramType,
                              ConstraintLocatorBuilder locator);

    /// \brief Coerce the given object argument (e.g., for the base of a
    /// member expression) to the given type.
    ///
    /// \param expr The expression to coerce.
    ///
    /// \param baseTy The base type
    ///
    /// \param member The member being accessed.
    ///
    /// \param semantics The kind of access we've been asked to perform.
    ///
    /// \param locator Locator used to describe where in this expression we are.
    Expr *coerceObjectArgumentToType(Expr *expr,
                                     Type baseTy, ValueDecl *member,
                                     AccessSemantics semantics,
                                     ConstraintLocatorBuilder locator);

  private:
    /// \brief Build a new subscript.
    ///
    /// \param base The base of the subscript.
    /// \param index The index of the subscript.
    /// \param locator The locator used to refer to the subscript.
    /// \param isImplicit Whether this is an implicit subscript.
    Expr *buildSubscript(Expr *base, Expr *index,
                         ConstraintLocatorBuilder locator,
                         bool isImplicit, AccessSemantics semantics) {
      // Determine the declaration selected for this subscript operation.
      auto selected = getOverloadChoice(
                        cs.getConstraintLocator(
                          locator.withPathElement(
                            ConstraintLocator::SubscriptMember)));
      auto choice = selected.choice;
      auto subscript = cast<SubscriptDecl>(choice.getDecl());

      auto &tc = cs.getTypeChecker();
      auto baseTy = base->getType()->getRValueType();

      // Check whether the base is 'super'.
      bool isSuper = base->isSuperExpr();

      // Handle accesses that implicitly look through ImplicitlyUnwrappedOptional<T>.
      if (auto objTy = cs.lookThroughImplicitlyUnwrappedOptionalType(baseTy)) {
        base = coerceImplicitlyUnwrappedOptionalToValue(base, objTy, locator);
        baseTy = base->getType();
      }

      // Figure out the index and result types.
      auto containerTy
        = subscript->getDeclContext()->getDeclaredTypeOfContext();
      auto subscriptTy = simplifyType(selected.openedType);
      auto indexTy = subscriptTy->castTo<AnyFunctionType>()->getInput();
      auto resultTy = subscriptTy->castTo<AnyFunctionType>()->getResult();

      // If we opened up an existential when performing the subscript, open
      // the base accordingly.
      auto knownOpened = solution.OpenedExistentialTypes.find(
                           getConstraintSystem().getConstraintLocator(
                             locator.withPathElement(
                               ConstraintLocator::SubscriptMember)));
      if (knownOpened != solution.OpenedExistentialTypes.end()) {
        base = openExistentialReference(base, knownOpened->second, subscript);
        baseTy = knownOpened->second;
        containerTy = baseTy;
      }

      // Coerce the index argument.
      index = coerceCallArguments(index, indexTy,
                                  locator.withPathElement(
                                    ConstraintLocator::SubscriptIndex));
      if (!index)
        return nullptr;

      // Form the subscript expression.

      // Handle dynamic lookup.
      if (selected.choice.getKind() == OverloadChoiceKind::DeclViaDynamic ||
          subscript->getAttrs().hasAttribute<OptionalAttr>()) {
        base = coerceObjectArgumentToType(base, baseTy, subscript,
                                          AccessSemantics::Ordinary, locator);
        if (!base)
          return nullptr;

        // TODO: diagnose if semantics != AccessSemantics::Ordinary?
        auto subscriptExpr = new (tc.Context) DynamicSubscriptExpr(base,
                                                                   index,
                                                                   subscript);
        subscriptExpr->setType(resultTy);
        subscriptExpr->setImplicit(isImplicit);
        Expr *result = subscriptExpr;
        closeExistential(result);
        return result;
      }

      // Handle subscripting of generics.
      if (subscript->getDeclContext()->isGenericContext()) {
        auto dc = subscript->getDeclContext();

        // Compute the substitutions used to reference the subscript.
        SmallVector<Substitution, 4> substitutions;
        solution.computeSubstitutions(
          subscript->getInterfaceType(),
          dc,
          selected.openedFullType,
          getConstraintSystem().getConstraintLocator(
            locator.withPathElement(ConstraintLocator::SubscriptMember)),
          substitutions);

        // Convert the base.
        auto openedFullFnType = selected.openedFullType->castTo<FunctionType>();
        auto openedBaseType = openedFullFnType->getInput();
        containerTy = solution.simplifyType(tc, openedBaseType);
        base = coerceObjectArgumentToType(base, containerTy, subscript,
                                          AccessSemantics::Ordinary, locator);
                 locator.withPathElement(ConstraintLocator::MemberRefBase);
        if (!base)
          return nullptr;

        // Form the generic subscript expression.
        auto subscriptExpr
          = new (tc.Context) SubscriptExpr(base, index,
                                           ConcreteDeclRef(tc.Context,
                                                           subscript,
                                                           substitutions),
                                           isImplicit,
                                           semantics);
        subscriptExpr->setType(resultTy);
        subscriptExpr->setIsSuper(isSuper);

        Expr *result = subscriptExpr;
        closeExistential(result);
        return result;
      }

      Type selfTy = containerTy;
      if (selfTy->isEqual(baseTy) && !selfTy->hasReferenceSemantics())
        if (base->getType()->is<LValueType>())
          selfTy = InOutType::get(selfTy);

      // Coerce the base to the container type.
      base = coerceObjectArgumentToType(base, selfTy, subscript,
                                        AccessSemantics::Ordinary, locator);
      if (!base)
        return nullptr;

      // Form a normal subscript.
      auto *subscriptExpr
        = new (tc.Context) SubscriptExpr(base, index, subscript,
                                         isImplicit, semantics);
      subscriptExpr->setType(resultTy);
      subscriptExpr->setIsSuper(isSuper);
      Expr *result = subscriptExpr;
      closeExistential(result);
      return result;
    }

    /// \brief Build a new reference to another constructor.
    Expr *buildOtherConstructorRef(Type openedFullType,
                                   ConstructorDecl *ctor, SourceLoc loc,
                                   ConstraintLocatorBuilder locator,
                                   bool implicit) {
      auto &tc = cs.getTypeChecker();
      auto &ctx = tc.Context;

      // Compute the concrete reference.
      ConcreteDeclRef ref;
      Type resultTy;
      if (ctor->getInterfaceType()->is<GenericFunctionType>()) {
        // Compute the reference to the generic constructor.
        SmallVector<Substitution, 4> substitutions;
        resultTy = solution.computeSubstitutions(
                     ctor->getInterfaceType(),
                     ctor,
                     openedFullType,
                     getConstraintSystem().getConstraintLocator(locator),
                     substitutions);

        ref = ConcreteDeclRef(ctx, ctor, substitutions);

        // The constructor was opened with the allocating type, not the
        // initializer type. Map the former into the latter.
        auto resultFnTy = resultTy->castTo<FunctionType>();
        auto selfTy = resultFnTy->getInput()->getRValueInstanceType();
        if (!selfTy->hasReferenceSemantics())
          selfTy = InOutType::get(selfTy);

        resultTy = FunctionType::get(selfTy, resultFnTy->getResult(),
                                     resultFnTy->getExtInfo());
      } else {
        ref = ConcreteDeclRef(ctor);
        resultTy = ctor->getInitializerType();
      }

      // Build the constructor reference.
      return new (ctx) OtherConstructorDeclRefExpr(ref, loc, implicit,
                                                   resultTy);
    }

    /// Bridge the given value to its corresponding Objective-C object
    /// type.
    ///
    /// This routine should only be used for bridging value types.
    ///
    /// \param value The value to be bridged.
    Expr *bridgeToObjectiveC(Expr *value) {
      auto &tc = cs.getTypeChecker();

      // Find the _BridgedToObjectiveC protocol.
      auto bridgedProto
        = tc.Context.getProtocol(KnownProtocolKind::ObjectiveCBridgeable);

      // Find the conformance of the value type to _BridgedToObjectiveC.
      Type valueType = value->getType()->getRValueType();
      ProtocolConformance *conformance = nullptr;
      bool conforms = tc.conformsToProtocol(valueType, bridgedProto, cs.DC,
                                            ConformanceCheckFlags::InExpression,
                                            &conformance);
      assert(conforms && "Should already have checked the conformance");
      (void)conforms;

      // Form the call.
      return tc.callWitness(value, cs.DC, bridgedProto,
                            conformance,
                            tc.Context.Id_bridgeToObjectiveC,
                            { }, diag::broken_bridged_to_objc_protocol);
    }

    /// Bridge the given object from Objective-C to its value type.
    ///
    /// This routine should only be used for bridging value types.
    ///
    /// \param object The object, whose type should already be of the type
    /// that the value type bridges through.
    ///
    /// \param valueType The value type to which we are bridging.
    ///
    /// \param conditional Whether the bridging should be conditional. If false,
    /// uses forced bridging.
    ///
    /// \returns a value of type \c valueType (optional if \c conditional) that
    /// stores the bridged result or (when \c conditional) an empty optional if
    /// conditional bridging fails.
    Expr *bridgeFromObjectiveC(Expr *object, Type valueType, bool conditional) {
      auto &tc = cs.getTypeChecker();

      // Find the _BridgedToObjectiveC protocol.
      auto bridgedProto
        = tc.Context.getProtocol(KnownProtocolKind::ObjectiveCBridgeable);

      // Try to find the conformance of the value type to _BridgedToObjectiveC.
      ProtocolConformance *conformance = nullptr;

      bool conformsToBridgedToObjectiveC
        = tc.conformsToProtocol(valueType,
                                bridgedProto,
                                cs.DC,
                                ConformanceCheckFlags::InExpression,
                                &conformance);

      FuncDecl *fn = nullptr;

      if (conformsToBridgedToObjectiveC) {
        // The conformance to _BridgedToObjectiveC is statically known.
        // Retrieve the  bridging operation to be used if a static conformance
        // to _BridgedToObjectiveC can be proven.
        fn = conditional
                 ? tc.Context.getConditionallyBridgeFromObjectiveCBridgeable(&tc)
                 : tc.Context.getForceBridgeFromObjectiveCBridgeable(&tc);
      } else {
        // Retrieve the  bridging operation to be used if a static conformance
        // to _BridgedToObjectiveC cannot be proven.
        fn = conditional ? tc.Context.getConditionallyBridgeFromObjectiveC(&tc)
                         : tc.Context.getForceBridgeFromObjectiveC(&tc);
      }

      if (!fn) {
        tc.diagnose(object->getLoc(), diag::missing_bridging_function,
                    conditional);
        return nullptr;
      }

      tc.validateDecl(fn);
      
      // Form a reference to the function. The bridging operations are generic,
      // so we need to form substitutions and compute the resulting type.
      auto Conformances =
        tc.Context.Allocate<ProtocolConformance *>(conformance ? 1 : 0);

      if (conformsToBridgedToObjectiveC)
        Conformances[0] = conformance;


      auto fnGenericParams
        = fn->getGenericSignatureOfContext()->getGenericParams();
      auto firstArchetype
        = ArchetypeBuilder::mapTypeIntoContext(fn, fnGenericParams[0])
            ->castTo<ArchetypeType>();

      SmallVector<Substitution, 2> Subs;
      Substitution sub(firstArchetype, valueType, Conformances);
      Subs.push_back(sub);

      // Add substitution for the dependent type T._ObjectiveCType.
      if (conformsToBridgedToObjectiveC) {
        auto objcTypeId = tc.Context.getIdentifier("_ObjectiveCType");
        auto objcAssocType = cast<AssociatedTypeDecl>(
                               conformance->getProtocol()->lookupDirect(
                                 objcTypeId).front());
        auto objcDepType = DependentMemberType::get(fnGenericParams[0],
                                                    objcAssocType,
                                                    tc.Context);
        auto objcArchetype
          = ArchetypeBuilder::mapTypeIntoContext(fn, objcDepType)
            ->castTo<ArchetypeType>();

        const Substitution &objcSubst = conformance->getTypeWitness(
                                          objcAssocType, &tc);

        // Create a substitution for the dependent type.
        Substitution newDepTypeSubst(
                       objcArchetype,
                       objcSubst.getReplacement(),
                       objcSubst.getConformances());

        Subs.push_back(newDepTypeSubst);
      }

      ConcreteDeclRef fnSpecRef(tc.Context, fn, Subs);
      Expr *fnRef = new (tc.Context) DeclRefExpr(fnSpecRef, object->getLoc(),
                                                 /*Implicit=*/true);
      TypeSubstitutionMap subMap;
      auto genericParam = fnGenericParams[0];
      subMap[genericParam->getCanonicalType()->castTo<SubstitutableType>()]
        = valueType;
      fnRef->setType(fn->getInterfaceType().subst(dc->getParentModule(), subMap,
                                                  None));

      // Form the arguments.
      Expr *args[2] = {
        object,
        new (tc.Context) DotSelfExpr(
                           TypeExpr::createImplicitHack(object->getLoc(),
                                                        valueType,
                                                        tc.Context),
                           object->getLoc(), object->getLoc(),
                           MetatypeType::get(valueType))
      };

      // Form the argument tuple.
      Expr *argTuple = TupleExpr::createImplicit(tc.Context, args, {});
      argTuple->setImplicit();
      TupleTypeElt tupleTypeFields[2] = {
        args[0]->getType(),
        args[1]->getType()
      };
      argTuple->setType(TupleType::get(tupleTypeFields, tc.Context));

      // Form the call and type-check it.
      Expr *call = new (tc.Context) CallExpr(fnRef, argTuple, /*Implicit=*/true);
      if (tc.typeCheckExpressionShallow(call, dc))
        return nullptr;

      return call;
    }

    /// Bridge the given object from Objective-C to its value type.
    ///
    /// This routine should only be used for bridging value types.
    ///
    /// \param object The object, whose type should already be of the type
    /// that the value type bridges through.
    ///
    /// \param valueType The value type to which we are bridging.
    ///
    /// \returns a value of type \c valueType that stores the bridged result.
    Expr *forceBridgeFromObjectiveC(Expr *object, Type valueType) {
      return bridgeFromObjectiveC(object, valueType, false);
    }

    TypeAliasDecl *MaxIntegerTypeDecl = nullptr;
    TypeAliasDecl *MaxFloatTypeDecl = nullptr;
    
  public:
    ExprRewriter(ConstraintSystem &cs, const Solution &solution,
                 bool suppressDiagnostics)
      : cs(cs), dc(cs.DC), solution(solution), 
        SuppressDiagnostics(suppressDiagnostics) { }

    ConstraintSystem &getConstraintSystem() const { return cs; }

    /// \brief Simplify the expression type and return the expression.
    ///
    /// This routine is used for 'simple' expressions that only need their
    /// types simplified, with no further computation.
    Expr *simplifyExprType(Expr *expr) {
      auto toType = simplifyType(expr->getType());
      expr->setType(toType);
      return expr;
    }

    Expr *visitErrorExpr(ErrorExpr *expr) {
      // Do nothing with error expressions.
      return expr;
    }

    Expr *visitCodeCompletionExpr(CodeCompletionExpr *expr) {
      // Do nothing with code completion expressions.
      return expr;
    }

    Expr *handleIntegerLiteralExpr(LiteralExpr *expr) {
      // If the literal has been assigned a builtin integer type,
      // don't mess with it.
      if (expr->getType()->is<BuiltinIntegerType>())
        return expr;

      auto &tc = cs.getTypeChecker();
      ProtocolDecl *protocol
        = tc.getProtocol(expr->getLoc(),
                         KnownProtocolKind::IntegerLiteralConvertible);
      ProtocolDecl *builtinProtocol
        = tc.getProtocol(expr->getLoc(),
                         KnownProtocolKind::BuiltinIntegerLiteralConvertible);

      // For type-sugar reasons, prefer the spelling of the default literal
      // type.
      auto type = simplifyType(expr->getType());
      if (auto defaultType = tc.getDefaultType(protocol, dc)) {
        if (defaultType->isEqual(type))
          type = defaultType;
      }
      if (auto floatProtocol
            = tc.getProtocol(expr->getLoc(),
                             KnownProtocolKind::FloatLiteralConvertible)) {
        if (auto defaultFloatType = tc.getDefaultType(floatProtocol, dc)) {
          if (defaultFloatType->isEqual(type))
            type = defaultFloatType;
        }
      }

      // Find the maximum-sized builtin integer type.
      
      if(!MaxIntegerTypeDecl) {
        SmallVector<ValueDecl *, 1> lookupResults;
        tc.getStdlibModule(dc)->lookupValue(/*filter=*/{},
                                            tc.Context.Id_MaxBuiltinIntegerType,
                                            NLKind::QualifiedLookup,
                                            lookupResults);
        if (lookupResults.size() == 1) {
          MaxIntegerTypeDecl = dyn_cast<TypeAliasDecl>(lookupResults.front());
          tc.validateDecl(MaxIntegerTypeDecl);
        }
      }
      if (!MaxIntegerTypeDecl ||
          !MaxIntegerTypeDecl->hasUnderlyingType() ||
          !MaxIntegerTypeDecl->getUnderlyingType()->is<BuiltinIntegerType>()) {
        tc.diagnose(expr->getLoc(), diag::no_MaxBuiltinIntegerType_found);
        return nullptr;
      }
      tc.validateDecl(MaxIntegerTypeDecl);
      auto maxType = MaxIntegerTypeDecl->getUnderlyingType();

      DeclName initName(tc.Context, tc.Context.Id_init,
                        { tc.Context.Id_integerLiteral });
      DeclName builtinInitName(tc.Context, tc.Context.Id_init,
                               { tc.Context.Id_builtinIntegerLiteral });

      return convertLiteral(
               expr,
               type,
               expr->getType(),
               protocol,
               tc.Context.Id_IntegerLiteralType,
               initName,
               builtinProtocol,
               maxType,
               builtinInitName,
               nullptr,
               diag::integer_literal_broken_proto,
               diag::builtin_integer_literal_broken_proto);
    }
    
    Expr *visitNilLiteralExpr(NilLiteralExpr *expr) {
      auto &tc = cs.getTypeChecker();
      auto *protocol = tc.getProtocol(expr->getLoc(),
                                      KnownProtocolKind::NilLiteralConvertible);

      // For type-sugar reasons, prefer the spelling of the default literal
      // type.
      auto type = simplifyType(expr->getType());
      if (auto defaultType = tc.getDefaultType(protocol, dc)) {
        if (defaultType->isEqual(type))
          type = defaultType;
      }

      DeclName initName(tc.Context, tc.Context.Id_init,
                        { tc.Context.Id_nilLiteral });
      return convertLiteral(expr, type, expr->getType(), protocol,
                            Identifier(), initName,
                            nullptr, Identifier(),
                            Identifier(),
                            [] (Type type) -> bool {
                              return false;
                            },
                            diag::nil_literal_broken_proto,
                            diag::nil_literal_broken_proto);
    }

    
    Expr *visitIntegerLiteralExpr(IntegerLiteralExpr *expr) {
      return handleIntegerLiteralExpr(expr);
    }

    Expr *visitFloatLiteralExpr(FloatLiteralExpr *expr) {
      // If the literal has been assigned a builtin float type,
      // don't mess with it.
      if (expr->getType()->is<BuiltinFloatType>())
        return expr;

      auto &tc = cs.getTypeChecker();
      ProtocolDecl *protocol
        = tc.getProtocol(expr->getLoc(),
                         KnownProtocolKind::FloatLiteralConvertible);
      ProtocolDecl *builtinProtocol
        = tc.getProtocol(expr->getLoc(),
                         KnownProtocolKind::BuiltinFloatLiteralConvertible);

      // For type-sugar reasons, prefer the spelling of the default literal
      // type.
      auto type = simplifyType(expr->getType());
      if (auto defaultType = tc.getDefaultType(protocol, dc)) {
        if (defaultType->isEqual(type))
          type = defaultType;
      }

      // Find the maximum-sized builtin float type.
      // FIXME: Cache name lookup.
      if (!MaxFloatTypeDecl) {
        SmallVector<ValueDecl *, 1> lookupResults;
        tc.getStdlibModule(dc)->lookupValue(/*filter=*/{},
                                            tc.Context.Id_MaxBuiltinFloatType,
                                            NLKind::QualifiedLookup,
                                            lookupResults);
        if (lookupResults.size() == 1)
          MaxFloatTypeDecl = dyn_cast<TypeAliasDecl>(lookupResults.front());
      }
      if (!MaxFloatTypeDecl ||
          !MaxFloatTypeDecl->hasUnderlyingType() ||
          !MaxFloatTypeDecl->getUnderlyingType()->is<BuiltinFloatType>()) {
        tc.diagnose(expr->getLoc(), diag::no_MaxBuiltinFloatType_found);
        return nullptr;
      }
      tc.validateDecl(MaxFloatTypeDecl);
      auto maxType = MaxFloatTypeDecl->getUnderlyingType();

      DeclName initName(tc.Context, tc.Context.Id_init,
                        { tc.Context.Id_floatLiteral });
      DeclName builtinInitName(tc.Context, tc.Context.Id_init,
                               { tc.Context.Id_builtinFloatLiteral });

      return convertLiteral(
               expr,
               type,
               expr->getType(),
               protocol,
               tc.Context.Id_FloatLiteralType,
               initName,
               builtinProtocol,
               maxType,
               builtinInitName,
               nullptr,
               diag::float_literal_broken_proto,
               diag::builtin_float_literal_broken_proto);
    }

    Expr *visitBooleanLiteralExpr(BooleanLiteralExpr *expr) {
      if (expr->getType() && expr->getType()->is<BuiltinIntegerType>())
        return expr;

      auto &tc = cs.getTypeChecker();
      ProtocolDecl *protocol
        = tc.getProtocol(expr->getLoc(),
                         KnownProtocolKind::BooleanLiteralConvertible);
      ProtocolDecl *builtinProtocol
        = tc.getProtocol(expr->getLoc(),
                         KnownProtocolKind::BuiltinBooleanLiteralConvertible);
      if (!protocol || !builtinProtocol)
        return nullptr;

      auto type = simplifyType(expr->getType());
      DeclName initName(tc.Context, tc.Context.Id_init,
                        { tc.Context.Id_booleanLiteral });
      DeclName builtinInitName(tc.Context, tc.Context.Id_init,
                               { tc.Context.Id_builtinBooleanLiteral });
      return convertLiteral(
               expr,
               type,
               expr->getType(),
               protocol,
               tc.Context.Id_BooleanLiteralType,
               initName,
               builtinProtocol,
               Type(BuiltinIntegerType::get(BuiltinIntegerWidth::fixed(1), 
                                            tc.Context)),
               builtinInitName,
               nullptr,
               diag::boolean_literal_broken_proto,
               diag::builtin_boolean_literal_broken_proto);
    }

    Expr *handleStringLiteralExpr(LiteralExpr *expr) {
      if (expr->getType() && !expr->getType()->hasTypeVariable())
        return expr;
      
      auto stringLiteral = dyn_cast<StringLiteralExpr>(expr);
      auto magicLiteral = dyn_cast<MagicIdentifierLiteralExpr>(expr);
      assert(bool(stringLiteral) != bool(magicLiteral) &&
             "literal must be either a string literal or a magic literal");

      auto type = simplifyType(expr->getType());
      auto &tc = cs.getTypeChecker();

      bool isStringLiteral = true;
      bool isGraphemeClusterLiteral = false;
      ProtocolDecl *protocol = tc.getProtocol(
          expr->getLoc(), KnownProtocolKind::StringLiteralConvertible);

      if (!tc.conformsToProtocol(type, protocol, cs.DC,
                                 ConformanceCheckFlags::InExpression)) {
        // If the type does not conform to StringLiteralConvertible, it should
        // be ExtendedGraphemeClusterLiteralConvertible.
        protocol = tc.getProtocol(
            expr->getLoc(),
            KnownProtocolKind::ExtendedGraphemeClusterLiteralConvertible);
        isStringLiteral = false;
        isGraphemeClusterLiteral = true;
      }
      if (!tc.conformsToProtocol(type, protocol, cs.DC,
                                 ConformanceCheckFlags::InExpression)) {
        // ... or it should be UnicodeScalarLiteralConvertible.
        protocol = tc.getProtocol(
            expr->getLoc(),
            KnownProtocolKind::UnicodeScalarLiteralConvertible);
        isStringLiteral = false;
        isGraphemeClusterLiteral = false;
      }

      assert(tc.conformsToProtocol(type, protocol, cs.DC,
                                   ConformanceCheckFlags::InExpression));

      // For type-sugar reasons, prefer the spelling of the default literal
      // type.
      if (auto defaultType = tc.getDefaultType(protocol, dc)) {
        if (defaultType->isEqual(type))
          type = defaultType;
      }

      SmallVector<TupleTypeElt, 3> elements;

      ProtocolDecl *builtinProtocol;
      Identifier literalType;
      DeclName literalFuncName;
      DeclName builtinLiteralFuncName;
      Diag<> brokenProtocolDiag;
      Diag<> brokenBuiltinProtocolDiag;

      if (isStringLiteral) {
        // If the string contains only ASCII, force a UTF8 representation
        bool forceASCII = stringLiteral != nullptr;
        if (forceASCII) {
          for (auto c: stringLiteral->getValue()) {
            if (c & (1 << 7)) {
              forceASCII = false;
              break;
            }
          }
        }
        
        literalType = tc.Context.Id_StringLiteralType;

        literalFuncName = DeclName(tc.Context, tc.Context.Id_init,
                                   { tc.Context.Id_stringLiteral });

        // If the string contains non-ASCII and the type can handle
        // UTF-16 string literals, prefer them.
        builtinProtocol = tc.getProtocol(
            expr->getLoc(),
            KnownProtocolKind::BuiltinUTF16StringLiteralConvertible);
        if (!forceASCII &&
            tc.conformsToProtocol(type, builtinProtocol, cs.DC,
                                  ConformanceCheckFlags::InExpression)) {
          builtinLiteralFuncName 
            = DeclName(tc.Context, tc.Context.Id_init,
                       { tc.Context.Id_builtinUTF16StringLiteral,
                         tc.Context.getIdentifier("numberOfCodeUnits") });

          elements.push_back(
            TupleTypeElt(tc.Context.TheRawPointerType,
                         tc.Context.Id_builtinUTF16StringLiteral));
          elements.push_back(
            TupleTypeElt(BuiltinIntegerType::getWordType(tc.Context),
                         tc.Context.getIdentifier("numberOfCodeUnits")));
          if (stringLiteral)
            stringLiteral->setEncoding(StringLiteralExpr::UTF16);
          else
            magicLiteral->setStringEncoding(StringLiteralExpr::UTF16);
        } else {
          // Otherwise, fall back to UTF-8.
          builtinProtocol = tc.getProtocol(
              expr->getLoc(),
              KnownProtocolKind::BuiltinStringLiteralConvertible);
          builtinLiteralFuncName 
            = DeclName(tc.Context, tc.Context.Id_init,
                       { tc.Context.Id_builtinStringLiteral,
                         tc.Context.getIdentifier("byteSize"),
                         tc.Context.getIdentifier("isASCII") });
          elements.push_back(TupleTypeElt(tc.Context.TheRawPointerType,
                                         tc.Context.Id_builtinStringLiteral));
          elements.push_back(
            TupleTypeElt(BuiltinIntegerType::getWordType(tc.Context),
                         tc.Context.getIdentifier("byteSize")));
          elements.push_back(
            TupleTypeElt(BuiltinIntegerType::get(1, tc.Context),
                         tc.Context.getIdentifier("isASCII")));
          if (stringLiteral)
            stringLiteral->setEncoding(StringLiteralExpr::UTF8);
          else
            magicLiteral->setStringEncoding(StringLiteralExpr::UTF8);
        }
        brokenProtocolDiag = diag::string_literal_broken_proto;
        brokenBuiltinProtocolDiag = diag::builtin_string_literal_broken_proto;
      } else if (isGraphemeClusterLiteral) {
        literalType = tc.Context.Id_ExtendedGraphemeClusterLiteralType;
        literalFuncName
          = DeclName(tc.Context, tc.Context.Id_init,
                     {tc.Context.Id_extendedGraphemeClusterLiteral});
        builtinLiteralFuncName
          = DeclName(tc.Context, tc.Context.Id_init,
                     { tc.Context.Id_builtinExtendedGraphemeClusterLiteral,
                       tc.Context.getIdentifier("byteSize"),
                       tc.Context.getIdentifier("isASCII") });

        builtinProtocol = tc.getProtocol(
            expr->getLoc(),
            KnownProtocolKind::BuiltinExtendedGraphemeClusterLiteralConvertible);
        elements.push_back(
          TupleTypeElt(tc.Context.TheRawPointerType,
                       tc.Context.Id_builtinExtendedGraphemeClusterLiteral));
        elements.push_back(
          TupleTypeElt(BuiltinIntegerType::getWordType(tc.Context),
                       tc.Context.getIdentifier("byteSize")));
        elements.push_back(
          TupleTypeElt(BuiltinIntegerType::get(1, tc.Context),
                       tc.Context.getIdentifier("isASCII")));
        brokenProtocolDiag =
            diag::extended_grapheme_cluster_literal_broken_proto;
        brokenBuiltinProtocolDiag =
            diag::builtin_extended_grapheme_cluster_literal_broken_proto;
      } else {
        // Otherwise, we should have just one Unicode scalar.
        literalType = tc.Context.Id_UnicodeScalarLiteralType;

        literalFuncName
          = DeclName(tc.Context, tc.Context.Id_init,
                     {tc.Context.Id_unicodeScalarLiteral});
        builtinLiteralFuncName
          = DeclName(tc.Context, tc.Context.Id_init,
                     {tc.Context.Id_builtinUnicodeScalarLiteral});

        builtinProtocol = tc.getProtocol(
            expr->getLoc(),
            KnownProtocolKind::BuiltinUnicodeScalarLiteralConvertible);
        builtinProtocol = tc.getProtocol(
            expr->getLoc(),
            KnownProtocolKind::BuiltinUnicodeScalarLiteralConvertible);

        elements.push_back(BuiltinIntegerType::get(32, tc.Context));

        brokenProtocolDiag = diag::unicode_scalar_literal_broken_proto;
        brokenBuiltinProtocolDiag =
            diag::builtin_unicode_scalar_literal_broken_proto;

        stringLiteral->setEncoding(StringLiteralExpr::OneUnicodeScalar);
      }

      return convertLiteral(expr,
                            type,
                            expr->getType(),
                            protocol,
                            literalType,
                            literalFuncName,
                            builtinProtocol,
                            TupleType::get(elements, tc.Context),
                            builtinLiteralFuncName,
                            nullptr,
                            brokenProtocolDiag,
                            brokenBuiltinProtocolDiag);
    }
    
    Expr *visitStringLiteralExpr(StringLiteralExpr *expr) {
      return handleStringLiteralExpr(expr);
    }

    Expr *
    visitInterpolatedStringLiteralExpr(InterpolatedStringLiteralExpr *expr) {
      // Figure out the string type we're converting to.
      auto openedType = expr->getType();
      auto type = simplifyType(openedType);
      expr->setType(type);

      // Find the string interpolation protocol we need.
      auto &tc = cs.getTypeChecker();
      auto interpolationProto
        = tc.getProtocol(expr->getLoc(),
                         KnownProtocolKind::StringInterpolationConvertible);
      assert(interpolationProto && "Missing string interpolation protocol?");

      DeclName name(tc.Context, tc.Context.Id_init,
                    { tc.Context.Id_stringInterpolation });
      auto member
        = findNamedWitnessImpl<ConstructorDecl>(
            tc, dc, type,
            interpolationProto, name,
            diag::interpolation_broken_proto);

      DeclName segmentName(tc.Context, tc.Context.Id_init,
                           { tc.Context.Id_stringInterpolationSegment });
      auto segmentMember
        = findNamedWitnessImpl<ConstructorDecl>(
            tc, dc, type, interpolationProto, segmentName,
            diag::interpolation_broken_proto);
      if (!member || !segmentMember)
        return nullptr;

      // Build a reference to the init(stringInterpolation:) initializer.
      // FIXME: This location info is bogus.
      auto typeRef = TypeExpr::createImplicitHack(expr->getStartLoc(),
                                                  type, tc.Context);
      Expr *memberRef = new (tc.Context) MemberRefExpr(typeRef,
                                                       expr->getStartLoc(),
                                                       member,
                                                       expr->getStartLoc(),
                                                       /*Implicit=*/true);
      bool failed = tc.typeCheckExpressionShallow(memberRef, cs.DC);
      assert(!failed && "Could not reference string interpolation witness");
      (void)failed;

      // Create a tuple containing all of the segments.
      SmallVector<Expr *, 4> segments;
      SmallVector<TupleTypeElt, 4> typeElements;
      SmallVector<Identifier, 4> names;
      unsigned index = 0;
      ConstraintLocatorBuilder locatorBuilder(cs.getConstraintLocator(expr));
      for (auto segment : expr->getSegments()) {
        auto locator = cs.getConstraintLocator(
                      locatorBuilder.withPathElement(
                          LocatorPathElt::getInterpolationArgument(index++)));

        // Find the initializer we chose.
        auto choice = getOverloadChoice(locator);

        auto arg = TupleExpr::create(
                     tc.Context, SourceLoc(), { segment },
                     { tc.Context.Id_stringInterpolationSegment },
                     { }, SourceLoc(), /*HasTrailingClosure=*/false,
                     /*Implicit=*/true,
                     TupleType::get(
                       { TupleTypeElt(
                           segment->getType(),
                           tc.Context.Id_stringInterpolationSegment) },
                       tc.Context));

        auto memberRef = buildMemberRef(
                           typeRef, choice.openedFullType,
                           segment->getStartLoc(), choice.choice.getDecl(),
                           segment->getStartLoc(), choice.openedType,
                           locator, locator, /*Implicit=*/true,
                           AccessSemantics::Ordinary,
                           /*isDynamic=*/false);
        ApplyExpr *apply =
            new (tc.Context) CallExpr(memberRef, arg, /*Implicit=*/true);

        auto converted = finishApply(apply, openedType, locatorBuilder);
        if (!converted)
          return nullptr;

        segments.push_back(converted);

        if (index == 1) {
          typeElements.push_back(
            TupleTypeElt(converted->getType(),
                         tc.Context.Id_stringInterpolation));
          names.push_back(tc.Context.Id_stringInterpolation);
        } else {
          typeElements.push_back(converted->getType());
          names.push_back(Identifier());
        }
      }

      Expr *argument = TupleExpr::create(tc.Context,
                                         expr->getStartLoc(),
                                         segments,
                                         names,
                                         { },
                                         expr->getStartLoc(),
                                         /*hasTrailingClosure=*/false,
                                         /*Implicit=*/true,
                                         TupleType::get(typeElements,
                                                        tc.Context));

      // Call the init(stringInterpolation:) initializer with the arguments.
      ApplyExpr *apply = new (tc.Context) CallExpr(memberRef, argument,
                                                   /*Implicit=*/true);
      expr->setSemanticExpr(finishApply(apply, openedType, locatorBuilder));
      return expr;
    }
    
    Expr *visitMagicIdentifierLiteralExpr(MagicIdentifierLiteralExpr *expr) {
      switch (expr->getKind()) {
      case MagicIdentifierLiteralExpr::File:
      case MagicIdentifierLiteralExpr::Function:
        return handleStringLiteralExpr(expr);

      case MagicIdentifierLiteralExpr::Line:
      case MagicIdentifierLiteralExpr::Column:
        return handleIntegerLiteralExpr(expr);

      case MagicIdentifierLiteralExpr::DSOHandle:
        return expr;
      }
    }

    Expr *visitObjectLiteralExpr(ObjectLiteralExpr *expr) {
      if (expr->getType() && !expr->getType()->hasTypeVariable())
        return expr;

      auto &ctx = cs.getASTContext();
      auto &tc = cs.getTypeChecker();

      // Figure out the type we're converting to.
      auto openedType = expr->getType();
      auto type = simplifyType(openedType);
      expr->setType(type);

      Type conformingType = type;
      if (auto baseType = conformingType->getAnyOptionalObjectType()) {
        // The type may be optional due to a failable initializer in the
        // protocol.
        conformingType = baseType;
      }

      // Find the appropriate object literal protocol.
      auto proto = tc.getLiteralProtocol(expr);
      assert(proto && "Missing object literal protocol?");
      ProtocolConformance *conformance = nullptr;
      bool conforms = tc.conformsToProtocol(conformingType, proto, cs.DC,
                                            ConformanceCheckFlags::InExpression,
                                            &conformance);
      (void)conforms;
      assert(conforms && "object literal type conforms to protocol");
        
      DeclName constrName(tc.getObjectLiteralConstructorName(expr));
      Expr *arg = expr->getArg();
      Expr *base = TypeExpr::createImplicitHack(expr->getLoc(), conformingType,
                                                ctx);
      Expr *semanticExpr = tc.callWitness(base, dc, proto, conformance,
                                          constrName, arg,
                                          diag::object_literal_broken_proto);
      expr->setSemanticExpr(semanticExpr);
      return expr;
    }

    /// \brief Retrieve the type of a reference to the given declaration.
    Type getTypeOfDeclReference(ValueDecl *decl, bool isSpecialized) {
      if (auto typeDecl = dyn_cast<TypeDecl>(decl)) {
        // Resolve the reference to this type declaration in our
        // current context.
        auto type = cs.getTypeChecker().resolveTypeInContext(typeDecl, dc,
                                                             TR_InExpression,
                                                             isSpecialized);
        if (!type)
          return nullptr;

        // Refer to the metatype of this type.
        return MetatypeType::get(type);
      }

      return cs.TC.getUnopenedTypeOfReference(decl, Type(), dc,
                                              /*base=*/nullptr,
                                              /*wantInterfaceType=*/true);
    }

    Expr *visitDeclRefExpr(DeclRefExpr *expr) {
      auto locator = cs.getConstraintLocator(expr);

      // Find the overload choice used for this declaration reference.
      auto selected = getOverloadChoiceIfAvailable(locator);
      if (!selected.hasValue()) {
        assert(expr->getDecl()->getType()->is<UnresolvedType>() &&
               "should only happen for closure arguments in CSDiags");
        expr->setType(expr->getDecl()->getType());
        return expr;
      }
      
      auto choice = selected->choice;
      auto decl = choice.getDecl();

      // FIXME: Cannibalize the existing DeclRefExpr rather than allocating a
      // new one?
      return buildDeclRef(decl, expr->getLoc(), selected->openedFullType,
                                  locator, expr->isSpecialized(),
                                  expr->isImplicit(),
                                  expr->getAccessSemantics());
    }

    Expr *visitSuperRefExpr(SuperRefExpr *expr) {
      simplifyExprType(expr);
      return expr;
    }

    Expr *visitTypeExpr(TypeExpr *expr) {
      auto toType = simplifyType(expr->getTypeLoc().getType());
      expr->getTypeLoc().setType(toType, /*validated=*/true);
      expr->setType(MetatypeType::get(toType));
      
      return expr;
    }

    Expr *visitOtherConstructorDeclRefExpr(OtherConstructorDeclRefExpr *expr) {
      expr->setType(expr->getDecl()->getInitializerType());
      return expr;
    }

    Expr *visitUnresolvedConstructorExpr(UnresolvedConstructorExpr *expr) {
      // Resolve the callee to the constructor declaration selected.
      auto ctorLocator = cs.getConstraintLocator(
                           expr,
                           ConstraintLocator::ConstructorMember);
      
      auto selected = getOverloadChoiceIfAvailable(ctorLocator);
      
      // If we didn't form a ConstructorMember constraint, then it
      // acts like a normal member reference to '.init'.
      if (!selected) {
        return applyMemberRefExpr(expr, expr->getSubExpr(), expr->getDotLoc(),
                                expr->getConstructorLoc(), expr->isImplicit());
      }
        
      
      auto choice = selected->choice;
      auto *ctor = cast<ConstructorDecl>(choice.getDecl());

      auto arg = expr->getSubExpr()->getSemanticsProvidingExpr();
      auto &tc = cs.getTypeChecker();

      // If the subexpression is a metatype, build a direct reference to the
      // constructor.
      if (arg->getType()->is<AnyMetatypeType>()) {
        return buildMemberRef(expr->getSubExpr(),
                              selected->openedFullType,
                              expr->getDotLoc(),
                              ctor,
                              expr->getConstructorLoc(),
                              expr->getType(),
                              ConstraintLocatorBuilder(
                                cs.getConstraintLocator(expr)),
                              ctorLocator,
                              expr->isImplicit(),
                              AccessSemantics::Ordinary,
                              /*isDynamic=*/false);
      }

      // The subexpression must be either 'self' or 'super'.
      if (!arg->isSuperExpr()) {
        // 'super' references have already been fully checked; handle the
        // 'self' case below.
        bool diagnoseBadInitRef = true;
        if (auto dre = dyn_cast<DeclRefExpr>(arg)) {
          if (dre->getDecl()->getName() == cs.getASTContext().Id_self) {
            // We have a reference to 'self'.
            diagnoseBadInitRef = false;

            // Make sure the reference to 'self' occurs within an initializer.
            if (!dyn_cast_or_null<ConstructorDecl>(
                   cs.DC->getInnermostMethodContext())) {
              if (!SuppressDiagnostics)
                tc.diagnose(expr->getDotLoc(),
                            diag::init_delegation_outside_initializer);
              return nullptr;
            }
          }
        }

        // If we need to diagnose this as a bad reference to an initializer,
        // do so now.
        if (diagnoseBadInitRef) {
          // Determine whether 'super' would have made sense as a base.
          bool hasSuper = false;
          if (auto func = cs.DC->getInnermostMethodContext()) {
            if (auto nominalType
                       = func->getDeclContext()->getDeclaredTypeOfContext()) {
              if (auto classDecl = nominalType->getClassOrBoundGenericClass()) {
                hasSuper = classDecl->hasSuperclass();
              }
            }
          }
          
          if (SuppressDiagnostics)
            return nullptr;

          tc.diagnose(expr->getDotLoc(), diag::bad_init_ref_base, hasSuper);
        }
      }
      
      // Build a partial application of the delegated initializer.
      Expr *ctorRef = buildOtherConstructorRef(
                        selected->openedFullType,
                        ctor, expr->getConstructorLoc(),
                        cs.getConstraintLocator(
                          expr,
                          ConstraintLocator::ConstructorMember),
                        expr->isImplicit());
      auto *call
        = new (cs.getASTContext()) DotSyntaxCallExpr(ctorRef,
                                                     expr->getDotLoc(),
                                                     expr->getSubExpr());
      return finishApply(call, expr->getType(),
                         ConstraintLocatorBuilder(
                           cs.getConstraintLocator(expr)));
    }

    Expr *visitDotSyntaxBaseIgnoredExpr(DotSyntaxBaseIgnoredExpr *expr) {
      return simplifyExprType(expr);
    }

    Expr *visitOverloadedDeclRefExpr(OverloadedDeclRefExpr *expr) {
      // Determine the declaration selected for this overloaded reference.
      auto locator = cs.getConstraintLocator(expr);
      auto selected = getOverloadChoice(locator);
      auto choice = selected.choice;
      auto decl = choice.getDecl();

      return buildDeclRef(decl, expr->getLoc(), selected.openedFullType,
                          locator, expr->isSpecialized(), expr->isImplicit(),
                          AccessSemantics::Ordinary);
    }

    Expr *visitOverloadedMemberRefExpr(OverloadedMemberRefExpr *expr) {
      auto memberLocator = cs.getConstraintLocator(expr,
                                                   ConstraintLocator::Member);
      auto selected = getOverloadChoice(memberLocator);
      bool isDynamic = selected.choice.getKind()
                         == OverloadChoiceKind::DeclViaDynamic;
      return buildMemberRef(expr->getBase(),
                            selected.openedFullType,
                            expr->getDotLoc(),
                            selected.choice.getDecl(), expr->getMemberLoc(),
                            selected.openedType,
                            cs.getConstraintLocator(expr),
                            memberLocator,
                            expr->isImplicit(), expr->getAccessSemantics(),
                            isDynamic);
    }

    Expr *visitUnresolvedDeclRefExpr(UnresolvedDeclRefExpr *expr) {
      // FIXME: We should have generated an overload set from this, in which
      // case we can emit a typo-correction error here but recover well.
      return nullptr;
    }

    Expr *visitUnresolvedSpecializeExpr(UnresolvedSpecializeExpr *expr) {
      // Our specializations should have resolved the subexpr to the right type.
      if (auto DRE = dyn_cast<DeclRefExpr>(expr->getSubExpr())) {
        assert(DRE->getGenericArgs().empty() ||
            DRE->getGenericArgs().size() == expr->getUnresolvedParams().size());
        if (DRE->getGenericArgs().empty()) {
          SmallVector<TypeRepr *, 8> GenArgs;
          for (auto TL : expr->getUnresolvedParams())
            GenArgs.push_back(TL.getTypeRepr());
          DRE->setGenericArgs(GenArgs);
        }
      }
      return expr->getSubExpr();
    }

    Expr *visitMemberRefExpr(MemberRefExpr *expr) {
      auto memberLocator = cs.getConstraintLocator(expr,
                                                   ConstraintLocator::Member);
      auto selected = getOverloadChoice(memberLocator);
      bool isDynamic
        = selected.choice.getKind() == OverloadChoiceKind::DeclViaDynamic;
      return buildMemberRef(expr->getBase(),
                            selected.openedFullType,
                            expr->getDotLoc(),
                            selected.choice.getDecl(), expr->getNameLoc(),
                            selected.openedType,
                            cs.getConstraintLocator(expr),
                            memberLocator,
                            expr->isImplicit(),
                            expr->getAccessSemantics(),
                            isDynamic);
    }

    Expr *visitDynamicMemberRefExpr(DynamicMemberRefExpr *expr) {
      llvm_unreachable("already type-checked?");
    }

    Expr *visitUnresolvedMemberExpr(UnresolvedMemberExpr *expr) {
      // Dig out the type of the base, which will be the result
      // type of this expression.
      Type resultTy = simplifyType(expr->getType());
      Type baseTy = resultTy->getRValueType();
      auto &tc = cs.getTypeChecker();

      // Find the selected member.
      auto memberLocator = cs.getConstraintLocator(
                             expr, ConstraintLocator::UnresolvedMember);
      auto selected = getOverloadChoice(memberLocator);
      auto member = selected.choice.getDecl();
      
      // If the member came by optional unwrapping, then unwrap the base type.
      if (selected.choice.getKind()
                              == OverloadChoiceKind::DeclViaUnwrappedOptional) {
        baseTy = baseTy->getAnyOptionalObjectType();
        assert(baseTy
               && "got unwrapped optional decl from non-optional base?!");
      }

      // The base expression is simply the metatype of the base type.
      // FIXME: This location info is bogus.
      auto base = TypeExpr::createImplicitHack(expr->getDotLoc(),
                                               baseTy, tc.Context);

      // Build the member reference.
      bool isDynamic
        = selected.choice.getKind() == OverloadChoiceKind::DeclViaDynamic;
      auto result = buildMemberRef(base,
                                   selected.openedFullType,
                                   expr->getDotLoc(), member, 
                                   expr->getNameLoc(),
                                   selected.openedType,
                                   cs.getConstraintLocator(expr),
                                   memberLocator,
                                   expr->isImplicit(),
                                   AccessSemantics::Ordinary,
                                   isDynamic);
      if (!result)
        return nullptr;

      // If there was an argument, apply it.
      if (auto arg = expr->getArgument()) {
        ApplyExpr *apply = new (tc.Context) CallExpr(result, arg, 
                                                     /*Implicit=*/false);
        result = finishApply(apply, Type(), cs.getConstraintLocator(expr));
      }

      result = coerceToType(result, resultTy, cs.getConstraintLocator(expr));
      return result;
    }
    
  private:
    /// A list of "suspicious" optional injections that come from
    /// forced downcasts.
    SmallVector<InjectIntoOptionalExpr *, 4> SuspiciousOptionalInjections;

  public:
    /// A list of optional injections that have been diagnosed.
    llvm::SmallPtrSet<InjectIntoOptionalExpr *, 4>  DiagnosedOptionalInjections;
  private:

    Expr *applyMemberRefExpr(Expr *expr,
                             Expr *base,
                             SourceLoc dotLoc,
                             SourceLoc nameLoc,
                             bool implicit) {
      // Determine the declaration selected for this overloaded reference.
      auto memberLocator = cs.getConstraintLocator(expr,
                                                   ConstraintLocator::Member);
      auto selected = getOverloadChoice(memberLocator);

      switch (selected.choice.getKind()) {
      case OverloadChoiceKind::DeclViaBridge: {
        // Look through an implicitly unwrapped optional.
        auto baseTy = base->getType()->getRValueType();
        if (auto objTy = cs.lookThroughImplicitlyUnwrappedOptionalType(baseTy)){
          base = coerceImplicitlyUnwrappedOptionalToValue(base, objTy,
                                         cs.getConstraintLocator(base));

          baseTy = base->getType()->getRValueType();
        }

        if (auto baseMetaTy = baseTy->getAs<MetatypeType>()) {
          auto &tc = cs.getTypeChecker();
          auto classTy = tc.getBridgedToObjC(cs.DC,
                                             baseMetaTy->getInstanceType());
          
          // FIXME: We're dropping side effects in the base here!
          base = TypeExpr::createImplicitHack(base->getLoc(), classTy, 
                                              tc.Context);
        } else {
          // Bridge the base to its corresponding Objective-C object.
          base = bridgeToObjectiveC(base);
        }

        // Fall through to build the member reference.
        SWIFT_FALLTHROUGH;
      }

      case OverloadChoiceKind::Decl:
      case OverloadChoiceKind::DeclViaUnwrappedOptional:
      case OverloadChoiceKind::DeclViaDynamic: {
        bool isDynamic
          = selected.choice.getKind() == OverloadChoiceKind::DeclViaDynamic;
        auto member = buildMemberRef(base,
                                     selected.openedFullType,
                                     dotLoc,
                                     selected.choice.getDecl(),
                                     nameLoc,
                                     selected.openedType,
                                     cs.getConstraintLocator(expr),
                                     memberLocator,
                                     implicit,
                                     AccessSemantics::Ordinary,
                                     isDynamic);

        return member;
      }

      case OverloadChoiceKind::TupleIndex: {
        auto baseTy = base->getType()->getRValueType();
        if (auto objTy = cs.lookThroughImplicitlyUnwrappedOptionalType(baseTy)) {
          base = coerceImplicitlyUnwrappedOptionalToValue(base, objTy,
                                         cs.getConstraintLocator(base));
        }

        return new (cs.getASTContext()) TupleElementExpr(
                                          base,
                                          dotLoc,
                                          selected.choice.getTupleIndex(),
                                          nameLoc,
                                          simplifyType(expr->getType()));
      }

      case OverloadChoiceKind::BaseType: {
        // FIXME: Losing ".0" sugar here.
        return base;
      }

      case OverloadChoiceKind::TypeDecl:
        llvm_unreachable("Nonsensical overload choice");
      }
    }
    
  public:
    Expr *visitUnresolvedSelectorExpr(UnresolvedSelectorExpr *expr) {
      return applyMemberRefExpr(expr, expr->getBase(), expr->getDotLoc(),
                                expr->getNameRange().Start,
                                expr->isImplicit());
    }
    
    
    Expr *visitUnresolvedDotExpr(UnresolvedDotExpr *expr) {
      return applyMemberRefExpr(expr, expr->getBase(), expr->getDotLoc(),
                                expr->getNameLoc(), expr->isImplicit());
    }

    Expr *visitSequenceExpr(SequenceExpr *expr) {
      llvm_unreachable("Expression wasn't parsed?");
    }

    Expr *visitIdentityExpr(IdentityExpr *expr) {
      expr->setType(expr->getSubExpr()->getType());
      return expr;
    }

    Expr *visitAnyTryExpr(AnyTryExpr *expr) {
      expr->setType(expr->getSubExpr()->getType());
      return expr;
    }

    Expr *visitOptionalTryExpr(OptionalTryExpr *expr) {
      return simplifyExprType(expr);
    }

    Expr *visitParenExpr(ParenExpr *expr) {
      auto &ctx = cs.getASTContext();
      expr->setType(ParenType::get(ctx, expr->getSubExpr()->getType()));
      return expr;
    }

    Expr *visitTupleExpr(TupleExpr *expr) {
      return simplifyExprType(expr);
    }

    Expr *visitSubscriptExpr(SubscriptExpr *expr) {
      return buildSubscript(expr->getBase(), expr->getIndex(),
                            cs.getConstraintLocator(expr),
                            expr->isImplicit(),
                            expr->getAccessSemantics());
    }

    Expr *visitArrayExpr(ArrayExpr *expr) {
      Type openedType = expr->getType();
      Type arrayTy = simplifyType(openedType);
      auto &tc = cs.getTypeChecker();

      ProtocolDecl *arrayProto
        = tc.getProtocol(expr->getLoc(),
                         KnownProtocolKind::ArrayLiteralConvertible);
      assert(arrayProto && "type-checked array literal w/o protocol?!");

      ProtocolConformance *conformance = nullptr;
      bool conforms = tc.conformsToProtocol(arrayTy, arrayProto,
                                            cs.DC,
                                            ConformanceCheckFlags::InExpression,
                                            &conformance);
      (void)conforms;
      assert(conforms && "Type does not conform to protocol?");

      // Call the witness that builds the array literal.
      // FIXME: callWitness() may end up re-doing some work we already did
      // to convert the array literal elements to the element type. It would
      // be nicer to re-use them.

      // FIXME: This location info is bogus.
      Expr *typeRef = TypeExpr::createImplicitHack(expr->getLoc(),
                                                   arrayTy, tc.Context);
      DeclName name(tc.Context, tc.Context.Id_init,
                    { tc.Context.Id_arrayLiteral });

      // Restructure the argument to provide the appropriate labels in the
      // tuple.
      SmallVector<TupleTypeElt, 4> typeElements;
      SmallVector<Identifier, 4> names;
      bool first = true;
      for (auto elt : expr->getElements()) {
        if (first) {
          typeElements.push_back(TupleTypeElt(elt->getType(),
                                              tc.Context.Id_arrayLiteral));
          names.push_back(tc.Context.Id_arrayLiteral);

          first = false;
          continue;
        } 

        typeElements.push_back(elt->getType());
        names.push_back(Identifier());
      }

      Type argType = TupleType::get(typeElements, tc.Context);
      Expr *arg = TupleExpr::create(tc.Context, SourceLoc(), 
                                    expr->getElements(),
                                    names,
                                    { },
                                    SourceLoc(), /*HasTrailingClosure=*/false,
                                    /*Implicit=*/true,
                                    argType);
      Expr *result = tc.callWitness(typeRef, dc, arrayProto, conformance,
                                    name, arg, diag::array_protocol_broken);
      if (!result)
        return nullptr;

      expr->setSemanticExpr(result);
      expr->setType(arrayTy);
      return expr;
    }

    Expr *visitDictionaryExpr(DictionaryExpr *expr) {
      Type openedType = expr->getType();
      Type dictionaryTy = simplifyType(openedType);
      auto &tc = cs.getTypeChecker();

      ProtocolDecl *dictionaryProto
        = tc.getProtocol(expr->getLoc(),
                         KnownProtocolKind::DictionaryLiteralConvertible);

      ProtocolConformance *conformance = nullptr;
      bool conforms = tc.conformsToProtocol(dictionaryTy, dictionaryProto,
                                            cs.DC,
                                            ConformanceCheckFlags::InExpression,
                                            &conformance);
      if (!conforms)
        return nullptr;

      // Call the witness that builds the dictionary literal.
      // FIXME: callWitness() may end up re-doing some work we already did
      // to convert the dictionary literal elements to the (key, value) tuple.
      // It would be nicer to re-use them.
      // FIXME: Cache the name.
      // FIXME: This location info is bogus.
      Expr *typeRef = TypeExpr::createImplicitHack(expr->getLoc(),
                                                   dictionaryTy, tc.Context);

      DeclName name(tc.Context, tc.Context.Id_init,
                    { tc.Context.Id_dictionaryLiteral });

      // Restructure the argument to provide the appropriate labels in the
      // tuple.
      SmallVector<TupleTypeElt, 4> typeElements;
      SmallVector<Identifier, 4> names;
      bool first = true;
      for (auto elt : expr->getElements()) {
        if (first) {
          typeElements.push_back(TupleTypeElt(elt->getType(),
                                              tc.Context.Id_dictionaryLiteral));
          names.push_back(tc.Context.Id_dictionaryLiteral);

          first = false;
          continue;
        } 

        typeElements.push_back(elt->getType());
        names.push_back(Identifier());
      }

      Type argType = TupleType::get(typeElements, tc.Context);
      Expr *arg = TupleExpr::create(tc.Context, expr->getLBracketLoc(),
                                    expr->getElements(),
                                    names,
                                    { },
                                    expr->getRBracketLoc(),
                                    /*HasTrailingClosure=*/false,
                                    /*Implicit=*/false,
                                    argType);

      Expr *result = tc.callWitness(typeRef, dc, dictionaryProto,
                                    conformance, name, arg,
                                    diag::dictionary_protocol_broken);
      if (!result)
        return nullptr;

      expr->setSemanticExpr(result);
      expr->setType(dictionaryTy);
      return expr;
    }

    Expr *visitDynamicSubscriptExpr(DynamicSubscriptExpr *expr) {
      return buildSubscript(expr->getBase(), expr->getIndex(),
                            cs.getConstraintLocator(expr),
                            expr->isImplicit(), AccessSemantics::Ordinary);
    }

    Expr *visitTupleElementExpr(TupleElementExpr *expr) {
      // Handle accesses that implicitly look through ImplicitlyUnwrappedOptional<T>.
      auto base = expr->getBase();
      auto baseTy = base->getType()->getRValueType();
      if (auto objTy = cs.lookThroughImplicitlyUnwrappedOptionalType(baseTy)) {
        base = coerceImplicitlyUnwrappedOptionalToValue(base, objTy,
                                              cs.getConstraintLocator(base));
        expr->setBase(base);
      }

      simplifyExprType(expr);
      return expr;
    }

    Expr *visitCaptureListExpr(CaptureListExpr *expr) {
      // The type of the capture list is the type of the closure contained
      // inside it.
      expr->setType(expr->getClosureBody()->getType());
      return expr;
    }

    Expr *visitClosureExpr(ClosureExpr *expr) {
      llvm_unreachable("Handled by the walker directly");
    }

    Expr *visitAutoClosureExpr(AutoClosureExpr *expr) {
      llvm_unreachable("Already type-checked");
    }

    Expr *visitInOutExpr(InOutExpr *expr) {
      // The default assumption is that inouts are read-write.  It's easier
      // to do this unconditionally here and then overwrite in the exception
      // case (when we turn the inout into an UnsafePointer) than to try to
      // discover that we're in that case right now.
      expr->getSubExpr()->propagateLValueAccessKind(AccessKind::ReadWrite);
      auto objectTy = expr->getSubExpr()->getType()->getRValueType();

      // The type is simply inout of whatever the lvalue's object type was.
      expr->setType(InOutType::get(objectTy));
      return expr;
    }

    Expr *visitDynamicTypeExpr(DynamicTypeExpr *expr) {
      auto &tc = cs.getTypeChecker();

      Expr *base = expr->getBase();
      base = tc.coerceToRValue(base);
      if (!base) return nullptr;
      expr->setBase(base);

      return simplifyExprType(expr);
    }

    Expr *visitOpaqueValueExpr(OpaqueValueExpr *expr) {
      llvm_unreachable("Already type-checked");
    }

    Expr *visitDefaultValueExpr(DefaultValueExpr *expr) {
      llvm_unreachable("Already type-checked");
    }

    Expr *visitApplyExpr(ApplyExpr *expr) {
      return finishApply(expr, expr->getType(),
                         ConstraintLocatorBuilder(
                           cs.getConstraintLocator(expr)));
    }

    Expr *visitRebindSelfInConstructorExpr(RebindSelfInConstructorExpr *expr) {
      // A non-failable initializer cannot delegate to a failable
      // initializer.
      OptionalTypeKind calledOTK;
      Expr *unwrappedSubExpr = expr->getSubExpr()->getSemanticsProvidingExpr();
      Type valueTy
        = unwrappedSubExpr->getType()->getAnyOptionalObjectType(calledOTK);
      auto inCtor = cast<ConstructorDecl>(cs.DC->getInnermostMethodContext());
      if (calledOTK != OTK_None && inCtor->getFailability() == OTK_None) {
        bool isError = (calledOTK == OTK_Optional);

        // If we're suppressing diagnostics, just fail.
        if (isError && SuppressDiagnostics)
          return nullptr;

        bool isChaining;
        auto *otherCtorRef = expr->getCalledConstructor(isChaining);

        auto &tc = cs.getTypeChecker();
        auto &ctx = tc.Context;

        if (isError) {
          if (auto *optTry = dyn_cast<OptionalTryExpr>(unwrappedSubExpr)) {
            tc.diagnose(optTry->getTryLoc(),
                        diag::delegate_chain_nonoptional_to_optional_try,
                        isChaining);
            tc.diagnose(optTry->getTryLoc(), diag::init_delegate_force_try)
              .fixItReplace({optTry->getTryLoc(), optTry->getQuestionLoc()},
                            "try!");
            tc.diagnose(inCtor->getLoc(), diag::init_propagate_failure)
              .fixItInsertAfter(inCtor->getLoc(), "?");
          } else {
            // Give the user the option of adding '!' or making the enclosing
            // initializer failable.
            ConstructorDecl *ctor = otherCtorRef->getDecl();
            tc.diagnose(otherCtorRef->getLoc(),
                        diag::delegate_chain_nonoptional_to_optional,
                        isChaining, ctor->getFullName());
            tc.diagnose(otherCtorRef->getLoc(), diag::init_force_unwrap)
              .fixItInsertAfter(expr->getEndLoc(), "!");
            tc.diagnose(inCtor->getLoc(), diag::init_propagate_failure)
              .fixItInsertAfter(inCtor->getLoc(), "?");
          }
        }

        // Recover by injecting the force operation (the first option).
        Expr *newSub = new (ctx) ForceValueExpr(expr->getSubExpr(),
                                                expr->getEndLoc());
        newSub->setType(valueTy);
        newSub->setImplicit();
        expr->setSubExpr(newSub);
      }

      return expr;
    }

    Expr *visitIfExpr(IfExpr *expr) {
      auto resultTy = simplifyType(expr->getType());
      expr->setType(resultTy);

      // Convert the condition to a logic value.
      auto cond
        = solution.convertBooleanTypeToBuiltinI1(expr->getCondExpr(),
                                                 cs.getConstraintLocator(expr));
      if (!cond) {
        cond->setType(ErrorType::get(cs.getASTContext()));
      } else {
        expr->setCondExpr(cond);
      }

      // Coerce the then/else branches to the common type.
      expr->setThenExpr(coerceToType(expr->getThenExpr(), resultTy,
                               cs.getConstraintLocator(expr->getThenExpr())));
      expr->setElseExpr(coerceToType(expr->getElseExpr(), resultTy,
                                 cs.getConstraintLocator(expr->getElseExpr())));

      return expr;
    }
    
    Expr *visitImplicitConversionExpr(ImplicitConversionExpr *expr) {
      llvm_unreachable("Already type-checked");
    }

    Expr *visitIsExpr(IsExpr *expr) {
      // Turn the subexpression into an rvalue.
      auto &tc = cs.getTypeChecker();
      auto toType = simplifyType(expr->getCastTypeLoc().getType());
      auto sub = tc.coerceToRValue(expr->getSubExpr());
      if (!sub)
        return nullptr;
      
      expr->setSubExpr(sub);

      // Set the type we checked against.
      expr->getCastTypeLoc().setType(toType, /*validated=*/true);
      auto fromType = sub->getType();
      auto castKind = tc.typeCheckCheckedCast(
                        fromType, toType, cs.DC,
                        expr->getLoc(),
                        sub->getSourceRange(),
                        expr->getCastTypeLoc().getSourceRange(),
                        [&](Type commonTy) -> bool {
                          return tc.convertToType(sub, commonTy, cs.DC);
                        },
                        SuppressDiagnostics);

      switch (castKind) {
      case CheckedCastKind::Unresolved:
        // Invalid type check.
        return nullptr;
      case CheckedCastKind::Coercion:
        // Check is trivially true.
        tc.diagnose(expr->getLoc(), diag::isa_is_always_true, "is");
        expr->setCastKind(castKind);
        break;
      case CheckedCastKind::ArrayDowncast:
      case CheckedCastKind::DictionaryDowncast:
      case CheckedCastKind::DictionaryDowncastBridged:
      case CheckedCastKind::SetDowncast:
      case CheckedCastKind::SetDowncastBridged:
      case CheckedCastKind::ValueCast:
      case CheckedCastKind::BridgeFromObjectiveC:
        // Valid checks.
        expr->setCastKind(castKind);
        break;
      }

      // SIL-generation magically turns this into a Bool; make sure it can.
      if (!cs.getASTContext().getGetBoolDecl(&cs.getTypeChecker())) {
        tc.diagnose(expr->getLoc(), diag::bool_intrinsics_not_found);
        // Continue anyway.
      }

      // Dig through the optionals in the from/to types.
      SmallVector<Type, 2> fromOptionals;
      fromType->lookThroughAllAnyOptionalTypes(fromOptionals);
      SmallVector<Type, 2> toOptionals;
      toType->lookThroughAllAnyOptionalTypes(toOptionals);

      // If we have an imbalance of optionals or a collection
      // downcast, handle this as a checked cast followed by a
      // a 'hasValue' check.
      if (fromOptionals.size() != toOptionals.size() ||
          castKind == CheckedCastKind::ArrayDowncast ||
          castKind == CheckedCastKind::DictionaryDowncast ||
          castKind == CheckedCastKind::DictionaryDowncastBridged ||
          castKind == CheckedCastKind::SetDowncast ||
          castKind == CheckedCastKind::SetDowncastBridged) {
        auto toOptType = OptionalType::get(toType);
        ConditionalCheckedCastExpr *cast
          = new (tc.Context) ConditionalCheckedCastExpr(
                               sub, expr->getLoc(), SourceLoc(),
                               TypeLoc::withoutLoc(toType));
        cast->setType(toOptType);
        if (expr->isImplicit())
          cast->setImplicit();

        // Type-check this conditional case.
        Expr *result = visitConditionalCheckedCastExpr(cast);
        if (!result)
          return nullptr;

        // Extract a Bool from the resulting expression.
        return solution.convertOptionalToBool(result,
                                              cs.getConstraintLocator(expr));
      }

      return expr;
    }

    /// Handle optional operands and results in an explicit cast.
    Expr *handleOptionalBindings(ExplicitCastExpr *cast, 
                                 Type finalResultType,
                                 bool conditionalCast) {
      auto &tc = cs.getTypeChecker();

      unsigned destExtraOptionals = conditionalCast ? 1 : 0;

      // FIXME: some of this work needs to be delayed until runtime to
      // properly account for archetypes dynamically being optional
      // types.  For example, if we're casting T to NSView?, that
      // should succeed if T=NSObject? and its value is actually nil.
      Expr *subExpr = cast->getSubExpr();
      Type srcType = subExpr->getType();

      SmallVector<Type, 4> srcOptionals;
      srcType = srcType->lookThroughAllAnyOptionalTypes(srcOptionals);

      SmallVector<Type, 4> destOptionals;
      auto destValueType
        = finalResultType->lookThroughAllAnyOptionalTypes(destOptionals);

      // Complain about conditional casts to foreign class types; they can't
      // actually be conditionally checked.
      if (conditionalCast) {
        auto destObjectType = destValueType;
        if (auto metaTy = destObjectType->getAs<MetatypeType>())
          destObjectType = metaTy->getInstanceType();
        if (auto destClass = destObjectType->getClassOrBoundGenericClass()) {
          if (destClass->isForeign()) {
            if (SuppressDiagnostics)
              return nullptr;

            tc.diagnose(cast->getLoc(), diag::conditional_downcast_foreign,
                        destValueType);
          }
        }
      }

      // There's nothing special to do if the operand isn't optional
      // and we don't need any bridging.
      if (srcOptionals.empty()) {
        cast->setType(finalResultType);
        return cast;
      }

      // If this is a conditional cast, the result type will always
      // have at least one level of optional, which should become the
      // type of the checked-cast expression.
      if (conditionalCast) {
        assert(!destOptionals.empty() &&
               "result of checked cast is not an optional type");
        cast->setType(destOptionals.back());
      } else {
        cast->setType(destValueType);
      }

      // The result type (without the final optional) is a subtype of
      // the operand type, so it will never have a higher depth.
      assert(destOptionals.size() - destExtraOptionals <= srcOptionals.size());

      // The outermost N levels of optionals on the operand must all
      // be present or the cast fails.  The innermost M levels of
      // optionals on the operand are reflected in the requested
      // destination type, so we should map these nils into the result.
      unsigned numRequiredOptionals =
        srcOptionals.size() - (destOptionals.size() - destExtraOptionals);

      // The number of OptionalEvaluationExprs between the point of the
      // inner cast and the enclosing OptionalEvaluationExpr (exclusive)
      // which represents failure for the entire operation.
      unsigned failureDepth = destOptionals.size() - destExtraOptionals;

      // Drill down on the operand until it's non-optional.
      SourceLoc fakeQuestionLoc = subExpr->getEndLoc();
      for (unsigned i : indices(srcOptionals)) {
        Type valueType =
          (i + 1 == srcOptionals.size() ? srcType : srcOptionals[i+1]);

        // As we move into the range of mapped optionals, start
        // lowering the depth.
        unsigned depth = failureDepth;
        if (i >= numRequiredOptionals) {
          depth -= (i - numRequiredOptionals) + 1;
        } else if (!conditionalCast) {
          // For a forced cast, force the required optionals.
          subExpr = new (tc.Context) ForceValueExpr(subExpr, fakeQuestionLoc);
          subExpr->setType(valueType);
          subExpr->setImplicit(true);
          continue;
        }

        subExpr = new (tc.Context) BindOptionalExpr(subExpr, fakeQuestionLoc,
                                                    depth, valueType);
        subExpr->setImplicit(true);
      }
      cast->setSubExpr(subExpr);

      // If we're casting to an optional type, we need to capture the
      // final M bindings.
      Expr *result = cast;

      if (destOptionals.size() > destExtraOptionals) {
        if (conditionalCast) {
          // If the innermost cast fails, the entire expression fails.  To
          // get this behavior, we have to bind and then re-inject the result.
          // (SILGen should know how to peephole this.)
          result = new (tc.Context) BindOptionalExpr(result, cast->getEndLoc(),
                                                     failureDepth,
                                                     destValueType);
          result->setImplicit(true);
        }

        for (unsigned i = destOptionals.size(); i != 0; --i) {
          Type destType = destOptionals[i-1];
          result = new (tc.Context) InjectIntoOptionalExpr(result, destType);
          result = new (tc.Context) OptionalEvaluationExpr(result, destType);
        }

      // Otherwise, we just need to capture the failure-depth binding.
      } else if (conditionalCast) {
        result = new (tc.Context) OptionalEvaluationExpr(result,
                                                         finalResultType);
      }

      return result;
    }

    Expr *visitCoerceExpr(CoerceExpr *expr) {
      // Simplify the type we're casting to.
      auto toType = simplifyType(expr->getCastTypeLoc().getType());
      expr->getCastTypeLoc().setType(toType, /*validated=*/true);

      // Determine whether we performed a coercion or downcast.
      if (cs.shouldAttemptFixes()) {
        auto locator = cs.getConstraintLocator(expr);
        unsigned choice = solution.getDisjunctionChoice(locator);
        (void) choice;
        assert(choice == 0 &&
          "checked cast branch of disjunction should have resulted in Fix");
      }

      auto &tc = cs.getTypeChecker();
      auto sub = tc.coerceToRValue(expr->getSubExpr());

      // The subexpression is always an rvalue.
      if (!sub)
        return nullptr;

      // Convert the subexpression.
      bool failed = tc.convertToType(sub, toType, cs.DC);
      (void)failed;
      assert(!failed && "Not convertible?");

      expr->setSubExpr(sub);
      expr->setType(toType);
      return expr;
    }

    Expr *visitForcedCheckedCastExpr(ForcedCheckedCastExpr *expr) {
      // Simplify the type we're casting to.
      auto toType = simplifyType(expr->getCastTypeLoc().getType());
      expr->getCastTypeLoc().setType(toType, /*validated=*/true);

      // The subexpression is always an rvalue.
      auto &tc = cs.getTypeChecker();
      auto sub = tc.coerceToRValue(expr->getSubExpr());
      if (!sub)
        return nullptr;
      expr->setSubExpr(sub);

      auto fromType = sub->getType();
      auto castKind = tc.typeCheckCheckedCast(
                        fromType, toType, cs.DC,
                        expr->getLoc(),
                        sub->getSourceRange(),
                        expr->getCastTypeLoc().getSourceRange(),
                        [&](Type commonTy) -> bool {
                          return tc.convertToType(sub, commonTy,
                                                 cs.DC);
                        },
                        SuppressDiagnostics);
      switch (castKind) {
        /// Invalid cast.
      case CheckedCastKind::Unresolved:
        return nullptr;
      case CheckedCastKind::Coercion: {
        if (SuppressDiagnostics)
          return nullptr;

        if (sub->getType()->isEqual(toType)) {
          tc.diagnose(expr->getLoc(), diag::forced_downcast_noop, toType)
            .fixItRemove(SourceRange(expr->getLoc(),
                                 expr->getCastTypeLoc().getSourceRange().End));
          return sub;
        }
        tc.diagnose(expr->getLoc(), diag::forced_downcast_coercion,
                    sub->getType(), toType)
          .fixItReplace(SourceRange(expr->getLoc(), expr->getExclaimLoc()),
                        "as");

        // Convert the subexpression.
        bool failed = tc.convertToType(sub, toType, cs.DC);
        (void)failed;
        assert(!failed && "Not convertible?");

        // Transmute the checked cast into a coercion expression.
        Expr *result = new (tc.Context) CoerceExpr(sub, expr->getLoc(),
                                                   expr->getCastTypeLoc());

        // The result type is the type we're converting to.
        result->setType(toType);
        return result;
      }

      // Valid casts.
      case CheckedCastKind::ArrayDowncast:
      case CheckedCastKind::DictionaryDowncast:
      case CheckedCastKind::DictionaryDowncastBridged:
      case CheckedCastKind::SetDowncast:
      case CheckedCastKind::SetDowncastBridged:
      case CheckedCastKind::ValueCast:
      case CheckedCastKind::BridgeFromObjectiveC:
        expr->setCastKind(castKind);
        break;
      }
      
      return handleOptionalBindings(expr, simplifyType(expr->getType()),
                                    /*conditionalCast=*/false);
    }

    Expr *visitConditionalCheckedCastExpr(ConditionalCheckedCastExpr *expr) {
      // Simplify the type we're casting to.
      auto toType = simplifyType(expr->getCastTypeLoc().getType());
      expr->getCastTypeLoc().setType(toType, /*validated=*/true);

      // The subexpression is always an rvalue.
      auto &tc = cs.getTypeChecker();
      auto sub = tc.coerceToRValue(expr->getSubExpr());
      if (!sub)
        return nullptr;
      expr->setSubExpr(sub);

      auto fromType = sub->getType();
      auto castKind = tc.typeCheckCheckedCast(
                        fromType, toType, cs.DC,
                        expr->getLoc(),
                        sub->getSourceRange(),
                        expr->getCastTypeLoc().getSourceRange(),
                        [&](Type commonTy) -> bool {
                          return tc.convertToType(sub, commonTy,
                                                 cs.DC);
                        },
                        SuppressDiagnostics);
      switch (castKind) {
        /// Invalid cast.
      case CheckedCastKind::Unresolved:
        return nullptr;
      case CheckedCastKind::Coercion: {
        if (SuppressDiagnostics)
          return nullptr;

        tc.diagnose(expr->getLoc(), diag::conditional_downcast_coercion,
                    sub->getType(), toType);

        // Convert the subexpression.
        bool failed = tc.convertToType(sub, toType, cs.DC);
        (void)failed;
        assert(!failed && "Not convertible?");

        // Transmute the checked cast into a coercion expression.
        Expr *result = new (tc.Context) CoerceExpr(sub, expr->getLoc(),
                                                   expr->getCastTypeLoc());

        // The result type is the type we're converting to.
        result->setType(toType);

        // Wrap the result in an optional.
        return new (tc.Context) InjectIntoOptionalExpr(
                                  result,
                                  OptionalType::get(toType));
      }

      // Valid casts.
      case CheckedCastKind::ArrayDowncast:
      case CheckedCastKind::DictionaryDowncast:
      case CheckedCastKind::DictionaryDowncastBridged:
      case CheckedCastKind::SetDowncast:
      case CheckedCastKind::SetDowncastBridged:
      case CheckedCastKind::ValueCast:
      case CheckedCastKind::BridgeFromObjectiveC:
        expr->setCastKind(castKind);
        break;
      }
      
      return handleOptionalBindings(expr, simplifyType(expr->getType()),
                                    /*conditionalCast=*/true);
    }

    Expr *visitAssignExpr(AssignExpr *expr) {
      // Compute the type to which the source must be converted to allow
      // assignment to the destination.
      //
      // FIXME: This is also computed when the constraint system is set up.
      auto destTy = cs.computeAssignDestType(expr->getDest(), expr->getLoc());
      if (!destTy)
        return nullptr;
      expr->getDest()->propagateLValueAccessKind(AccessKind::Write);

      // Convert the source to the simplified destination type.
      auto locator =
        ConstraintLocatorBuilder(cs.getConstraintLocator(expr->getSrc()));
      Expr *src = coerceToType(expr->getSrc(), destTy, locator);
      if (!src)
        return nullptr;
      expr->setSrc(src);
      return expr;
    }
    
    Expr *visitDiscardAssignmentExpr(DiscardAssignmentExpr *expr) {
      return simplifyExprType(expr);
    }
    
    Expr *visitUnresolvedPatternExpr(UnresolvedPatternExpr *expr) {
      llvm_unreachable("should have been eliminated during name binding");
    }
    
    Expr *visitBindOptionalExpr(BindOptionalExpr *expr) {
      Type valueType = simplifyType(expr->getType());
      expr->setType(valueType);
      return expr;
    }

    Expr *visitOptionalEvaluationExpr(OptionalEvaluationExpr *expr) {
      Type optType = simplifyType(expr->getType());

      // If this is an optional chain that isn't chaining anything, and if the
      // subexpression is already optional (not IUO), then this is a noop:
      // reject it.  This avoids confusion of the model (where the programmer
      // thought it was doing something) and keeps pointless ?'s out of the
      // code.
      if (!SuppressDiagnostics)
        if (auto *Bind = dyn_cast<BindOptionalExpr>(
                        expr->getSubExpr()->getSemanticsProvidingExpr())) {
          if (Bind->getSubExpr()->getType()->isEqual(optType))
            cs.TC.diagnose(expr->getLoc(), diag::optional_chain_noop,
                           optType).fixItRemove(Bind->getQuestionLoc());
          else
            cs.TC.diagnose(expr->getLoc(), diag::optional_chain_isnt_chaining);
        }

      Expr *subExpr = coerceToType(expr->getSubExpr(), optType,
                                   cs.getConstraintLocator(expr));
      if (!subExpr) return nullptr;

      expr->setSubExpr(subExpr);
      expr->setType(optType);
      return expr;
    }

    Expr *visitForceValueExpr(ForceValueExpr *expr) {
      Type valueType = simplifyType(expr->getType());
      expr->setType(valueType);
      
      // Coerce the object type, if necessary.
      auto subExpr = expr->getSubExpr();
      if (auto objectTy = subExpr->getType()->getAnyOptionalObjectType()) {        
        if (objectTy && !objectTy->isEqual(valueType)) {
          auto coercedSubExpr = coerceToType(subExpr,
                                             OptionalType::get(valueType),
                                             cs.getConstraintLocator(subExpr));
          
          expr->setSubExpr(coercedSubExpr);
        }
      }
      
      return expr;
    }

    Expr *visitOpenExistentialExpr(OpenExistentialExpr *expr) {
      llvm_unreachable("Already type-checked");
    }
    
    Expr *visitEditorPlaceholderExpr(EditorPlaceholderExpr *E) {
      return E;
    }

    /// Interface for ExprWalker
    void walkToExprPre(Expr *expr) {
      ExprStack.push_back(expr);
    }

    Expr *walkToExprPost(Expr *expr) {
      Expr *result = visit(expr);

      assert(expr == ExprStack.back());
      ExprStack.pop_back();

      return result;
    }

    void finalize(Expr *&result) {
      assert(ExprStack.empty());
      assert(OpenedExistentials.empty());

      auto &tc = cs.getTypeChecker();

      // Look at all of the suspicious optional injections
      for (auto injection : SuspiciousOptionalInjections) {
        // If we already diagnosed this injection, we're done.
        if (DiagnosedOptionalInjections.count(injection)) {
          continue;
        }

        auto *cast = findForcedDowncast(tc.Context, injection->getSubExpr());
        if (!cast)
          continue;

        if (isa<ParenExpr>(injection->getSubExpr()))
          continue;

        tc.diagnose(injection->getLoc(), diag::inject_forced_downcast,
                    injection->getSubExpr()->getType()->getRValueType());
        auto exclaimLoc = cast->getExclaimLoc();
        tc.diagnose(exclaimLoc, diag::forced_to_conditional_downcast, 
                    injection->getType()->getAnyOptionalObjectType())
          .fixItReplace(exclaimLoc, "?");
        tc.diagnose(cast->getStartLoc(), diag::silence_inject_forced_downcast)
          .fixItInsert(cast->getStartLoc(), "(")
          .fixItInsertAfter(cast->getEndLoc(), ")");
      }
    }

    /// Diagnose an optional injection that is probably not what the
    /// user wanted, because it comes from a forced downcast.
    void diagnoseOptionalInjection(InjectIntoOptionalExpr *injection) {
      // Don't diagnose when we're injecting into 
      auto toOptionalType = injection->getType();
      if (toOptionalType->getImplicitlyUnwrappedOptionalObjectType())
        return;

      // Check whether we have a forced downcast.
      auto &tc = cs.getTypeChecker();
      auto *cast = findForcedDowncast(tc.Context, injection->getSubExpr());
      if (!cast)
        return;
      
      SuspiciousOptionalInjections.push_back(injection);
    }
  };
}

/// \brief Given a constraint locator, find the owner of default arguments for
/// that tuple, i.e., a FuncDecl.
static ConcreteDeclRef
findDefaultArgsOwner(ConstraintSystem &cs, const Solution &solution,
                     ConstraintLocator *locator) {
  if (locator->getPath().empty() || !locator->getAnchor())
    return nullptr;

  // If the locator points to a function application, find the function itself.
  if (locator->getPath().back().getKind() == ConstraintLocator::ApplyArgument) {
    assert(locator->getPath().back().getNewSummaryFlags() == 0 &&
           "ApplyArgument adds no flags");
    SmallVector<LocatorPathElt, 4> newPath;
    newPath.append(locator->getPath().begin(), locator->getPath().end()-1);
    unsigned newFlags = locator->getSummaryFlags();

    // If we have an interpolation argument, dig out the constructor if we
    // can.
    // FIXME: This representation is actually quite awful
    if (newPath.size() == 1 &&
        newPath[0].getKind() == ConstraintLocator::InterpolationArgument) {
      newPath.push_back(ConstraintLocator::ConstructorMember);

      locator = cs.getConstraintLocator(locator->getAnchor(), newPath, newFlags);
      auto known = solution.overloadChoices.find(locator);
      if (known != solution.overloadChoices.end()) {
        auto &choice = known->second.choice;
        if (choice.getKind() == OverloadChoiceKind::Decl)
          return cast<AbstractFunctionDecl>(choice.getDecl());
      }
      return nullptr;
    } else {
      newPath.push_back(ConstraintLocator::ApplyFunction);
    }
    assert(newPath.back().getNewSummaryFlags() == 0 &&
           "added element that changes the flags?");
    locator = cs.getConstraintLocator(locator->getAnchor(), newPath, newFlags);
  }

  // Simplify the locator.
  SourceRange range;
  locator = simplifyLocator(cs, locator, range);

  // If we didn't map down to a specific expression, we can't handle a default
  // argument.
  if (!locator->getAnchor() || !locator->getPath().empty())
    return nullptr;

  if (auto resolved
        = resolveLocatorToDecl(cs, locator,
            [&](ConstraintLocator *locator) -> Optional<SelectedOverload> {
              auto known = solution.overloadChoices.find(locator);
              if (known == solution.overloadChoices.end()) {
                return None;
              }

              return known->second;
            },
            [&](ValueDecl *decl,
                Type openedType) -> ConcreteDeclRef {
              if (decl->getPotentialGenericDeclContext()->isGenericContext()) {
                SmallVector<Substitution, 4> subs;
                solution.computeSubstitutions(
                  decl->getType(),
                  decl->getPotentialGenericDeclContext(),
                  openedType, locator, subs);
                return ConcreteDeclRef(cs.getASTContext(), decl, subs);
              }
              
              return decl;
            })) {
    return resolved.getDecl();
  }

  return nullptr;
}

/// Produce the caller-side default argument for this default argument, or
/// null if the default argument will be provided by the callee.
static std::pair<Expr *, DefaultArgumentKind>
getCallerDefaultArg(TypeChecker &tc, DeclContext *dc,
                    SourceLoc loc, ConcreteDeclRef &owner,
                    unsigned index) {
  auto ownerFn = cast<AbstractFunctionDecl>(owner.getDecl());
  auto defArg = ownerFn->getDefaultArg(index);
  MagicIdentifierLiteralExpr::Kind magicKind;
  switch (defArg.first) {
  case DefaultArgumentKind::None:
    llvm_unreachable("No default argument here?");

  case DefaultArgumentKind::Normal:
    return {nullptr, defArg.first};

  case DefaultArgumentKind::Inherited:
    // Update the owner to reflect inheritance here.
    owner = ownerFn->getOverriddenDecl();
    return getCallerDefaultArg(tc, dc, loc, owner, index);

  case DefaultArgumentKind::Column:
    magicKind = MagicIdentifierLiteralExpr::Column;
    break;

  case DefaultArgumentKind::File:
    magicKind = MagicIdentifierLiteralExpr::File;
    break;
    
  case DefaultArgumentKind::Line:
    magicKind = MagicIdentifierLiteralExpr::Line;
    break;
      
  case DefaultArgumentKind::Function:
    magicKind = MagicIdentifierLiteralExpr::Function;
    break;

  case DefaultArgumentKind::DSOHandle:
    magicKind = MagicIdentifierLiteralExpr::DSOHandle;
    break;
  }

  // Create the default argument, which is a converted magic identifier
  // literal expression.
  Expr *init = new (tc.Context) MagicIdentifierLiteralExpr(magicKind, loc,
                                                           /*Implicit=*/true);
  bool invalid = tc.typeCheckExpression(init, dc, defArg.second,CTP_CannotFail);
  assert(!invalid && "conversion cannot fail");
  (void)invalid;
  return {init, defArg.first};
}

static Expr *lookThroughIdentityExprs(Expr *expr) {
  while (true) {
    if (auto ident = dyn_cast<IdentityExpr>(expr)) {
      expr = ident->getSubExpr();
    } else if (auto anyTry = dyn_cast<AnyTryExpr>(expr)) {
      if (isa<OptionalTryExpr>(anyTry))
        return expr;
      expr = anyTry->getSubExpr();
    } else {
      return expr;
    }
  }
}

/// Rebuild the ParenTypes for the given expression, whose underlying expression
/// should be set to the given type.  This has to apply to exactly the same
/// levels of sugar that were stripped off by lookThroughIdentityExprs.
static Type rebuildIdentityExprs(ASTContext &ctx, Expr *expr, Type type) {
  if (auto paren = dyn_cast<ParenExpr>(expr)) {
    type = rebuildIdentityExprs(ctx, paren->getSubExpr(), type);
    paren->setType(ParenType::get(ctx, type));
    return paren->getType();
  }

  if (auto ident = dyn_cast<IdentityExpr>(expr)) {
    type = rebuildIdentityExprs(ctx, ident->getSubExpr(), type);
    ident->setType(type);
    return ident->getType();
  }

  if (auto ident = dyn_cast<AnyTryExpr>(expr)) {
    if (isa<OptionalTryExpr>(ident))
      return type;

    type = rebuildIdentityExprs(ctx, ident->getSubExpr(), type);
    ident->setType(type);
    return ident->getType();
  }

  return type;
}

Expr *ExprRewriter::coerceTupleToTuple(Expr *expr, TupleType *fromTuple,
                                       TupleType *toTuple,
                                       ConstraintLocatorBuilder locator,
                                       SmallVectorImpl<int> &sources,
                                       SmallVectorImpl<unsigned> &variadicArgs){
  auto &tc = cs.getTypeChecker();

  // Capture the tuple expression, if there is one.
  Expr *innerExpr = lookThroughIdentityExprs(expr);
  TupleExpr *fromTupleExpr = dyn_cast<TupleExpr>(innerExpr);

  /// Check each of the tuple elements in the destination.
  bool hasVariadic = false;
  unsigned variadicParamIdx = toTuple->getNumElements();
  bool anythingShuffled = false;
  bool hasInits = false;
  SmallVector<TupleTypeElt, 4> toSugarFields;
  SmallVector<TupleTypeElt, 4> fromTupleExprFields(
                                 fromTuple->getElements().size());
  SmallVector<Expr *, 2> callerDefaultArgs;
  ConcreteDeclRef defaultArgsOwner;

  for (unsigned i = 0, n = toTuple->getNumElements(); i != n; ++i) {
    const auto &toElt = toTuple->getElement(i);
    auto toEltType = toElt.getType();

    // If we're default-initializing this member, there's nothing to do.
    if (sources[i] == TupleShuffleExpr::DefaultInitialize) {
      // Dig out the owner of the default arguments.
      ConcreteDeclRef argOwner;
      if (!defaultArgsOwner) {
        argOwner
          = findDefaultArgsOwner(cs, solution,
                                 cs.getConstraintLocator(locator));
        assert(argOwner && "Missing default arguments owner?");
      } else {
        argOwner = defaultArgsOwner;
      }

      anythingShuffled = true;
      hasInits = true;
      toSugarFields.push_back(toElt);

      // Create a caller-side default argument, if we need one.
      if (auto defArg = getCallerDefaultArg(tc, dc, expr->getLoc(),
                                            argOwner, i).first) {
        callerDefaultArgs.push_back(defArg);
        sources[i] = TupleShuffleExpr::CallerDefaultInitialize;
      }
      if (!defaultArgsOwner) {
        defaultArgsOwner = argOwner;
      } else {
        assert(defaultArgsOwner == argOwner &&
               "default args on same func have different owners");
      }
      continue;
    }

    // If this is the variadic argument, note it.
    if (sources[i] == TupleShuffleExpr::Variadic) {
      assert(!hasVariadic && "two variadic parameters?");
      toSugarFields.push_back(toElt);
      hasVariadic = true;
      variadicParamIdx = i;
      anythingShuffled = true;
      continue;
    }

    // If the source and destination index are different, we'll be shuffling.
    if ((unsigned)sources[i] != i) {
      anythingShuffled = true;
    }

    // We're matching one element to another. If the types already
    // match, there's nothing to do.
    const auto &fromElt = fromTuple->getElement(sources[i]);
    auto fromEltType = fromElt.getType();
    if (fromEltType->isEqual(toEltType)) {
      // Get the sugared type directly from the tuple expression, if there
      // is one.
      if (fromTupleExpr)
        fromEltType = fromTupleExpr->getElement(sources[i])->getType();

      toSugarFields.push_back(TupleTypeElt(fromEltType,
                                           toElt.getName(),
                                           toElt.getDefaultArgKind(),
                                           toElt.isVararg()));
      fromTupleExprFields[sources[i]] = fromElt;
      hasInits |= toElt.hasInit();
      continue;
    }

    // We need to convert the source element to the destination type.
    if (!fromTupleExpr) {
      // FIXME: Lame! We can't express this in the AST.
      tc.diagnose(expr->getLoc(),
                  diag::tuple_conversion_not_expressible,
                  fromTuple, toTuple);
      return nullptr;
    }

    // Actually convert the source element.
    auto convertedElt
      = coerceToType(fromTupleExpr->getElement(sources[i]), toEltType,
                     locator.withPathElement(
                       LocatorPathElt::getTupleElement(sources[i])));
    if (!convertedElt)
      return nullptr;

    fromTupleExpr->setElement(sources[i], convertedElt);

    // Record the sugared field name.
    toSugarFields.push_back(TupleTypeElt(convertedElt->getType(),
                                         toElt.getName(),
                                         toElt.getDefaultArgKind(),
                                         toElt.isVararg()));
    fromTupleExprFields[sources[i]] = TupleTypeElt(convertedElt->getType(),
                                                   fromElt.getName(),
                                                   fromElt.getDefaultArgKind(),
                                                   fromElt.isVararg());
    hasInits |= toElt.hasInit();
  }

  // Convert all of the variadic arguments to the destination type.
  ArraySliceType *arrayType = nullptr;
  if (hasVariadic) {
    Type toEltType = toTuple->getElements()[variadicParamIdx].getVarargBaseTy();
    for (int fromFieldIdx : variadicArgs) {
      const auto &fromElt = fromTuple->getElement(fromFieldIdx);
      Type fromEltType = fromElt.getType();

      // If the source and destination types match, there's nothing to do.
      if (toEltType->isEqual(fromEltType)) {
        fromTupleExprFields[fromFieldIdx] = fromElt;
        continue;
      }

      // We need to convert the source element to the destination type.
      if (!fromTupleExpr) {
        // FIXME: Lame! We can't express this in the AST.
        tc.diagnose(expr->getLoc(),
                    diag::tuple_conversion_not_expressible,
                    fromTuple, toTuple);
        return nullptr;
      }

      // Actually convert the source element.
      auto convertedElt = coerceToType(
                            fromTupleExpr->getElement(fromFieldIdx),
                            toEltType,
                            locator.withPathElement(
                              LocatorPathElt::getTupleElement(fromFieldIdx)));
      if (!convertedElt)
        return nullptr;

      fromTupleExpr->setElement(fromFieldIdx, convertedElt);

      fromTupleExprFields[fromFieldIdx] = TupleTypeElt(
                                            convertedElt->getType(),
                                            fromElt.getName(),
                                            fromElt.getDefaultArgKind(),
                                            fromElt.isVararg());
    }

    // Find the appropriate injection function.
    if (tc.requireArrayLiteralIntrinsics(expr->getStartLoc()))
      return nullptr;
    arrayType = cast<ArraySliceType>(
          toTuple->getElements()[variadicParamIdx].getType().getPointer());
  }

  // Compute the updated 'from' tuple type, since we may have
  // performed some conversions in place.
  Type fromTupleType = TupleType::get(fromTupleExprFields, tc.Context);
  if (fromTupleExpr) {
    fromTupleExpr->setType(fromTupleType);

    // Update the types of parentheses around the tuple expression.
    rebuildIdentityExprs(cs.getASTContext(), expr, fromTupleType);
  }

  // Compute the re-sugared tuple type.
  Type toSugarType = hasInits? toTuple
                             : TupleType::get(toSugarFields, tc.Context);

  // If we don't have to shuffle anything, we're done.
  if (!anythingShuffled && fromTupleExpr) {
    fromTupleExpr->setType(toSugarType);

    // Update the types of parentheses around the tuple expression.
    rebuildIdentityExprs(cs.getASTContext(), expr, toSugarType);

    return expr;
  }
  
  // Create the tuple shuffle.
  ArrayRef<int> mapping = tc.Context.AllocateCopy(sources);
  auto callerDefaultArgsCopy = tc.Context.AllocateCopy(callerDefaultArgs);
  auto shuffle = new (tc.Context) TupleShuffleExpr(
                                    expr, mapping,
                                    TupleShuffleExpr::SourceIsTuple,
                                    defaultArgsOwner,
                                    tc.Context.AllocateCopy(variadicArgs),
                                    callerDefaultArgsCopy,
                                    toSugarType);
  shuffle->setVarargsArrayType(arrayType);
  return shuffle;
}



Expr *ExprRewriter::coerceScalarToTuple(Expr *expr, TupleType *toTuple,
                                        int toScalarIdx,
                                        ConstraintLocatorBuilder locator) {
  auto &tc = solution.getConstraintSystem().getTypeChecker();

  // If the destination type is variadic, compute the injection function to use.
  Type arrayType = nullptr;
  const auto &lastField = toTuple->getElements().back();

  if (lastField.isVararg()) {
    // Find the appropriate injection function.
    arrayType = cast<ArraySliceType>(lastField.getType().getPointer());
    if (tc.requireArrayLiteralIntrinsics(expr->getStartLoc()))
      return nullptr;
  }

  // If we're initializing the varargs list, use its base type.
  const auto &field = toTuple->getElement(toScalarIdx);
  Type toScalarType;
  if (field.isVararg())
    toScalarType = field.getVarargBaseTy();
  else
    toScalarType = field.getType();

  // Coerce the expression to the scalar type.
  expr = coerceToType(expr, toScalarType,
                      locator.withPathElement(
                        ConstraintLocator::ScalarToTuple));
  if (!expr)
    return nullptr;

  // Preserve the sugar of the scalar field.
  // FIXME: This doesn't work if the type has default values because they fail
  // to canonicalize.
  SmallVector<TupleTypeElt, 4> sugarFields;
  bool hasInit = false;
  int i = 0;
  for (auto &field : toTuple->getElements()) {
    if (field.hasInit()) {
      hasInit = true;
      break;
    }

    if (i == toScalarIdx) {
      if (field.isVararg()) {
        assert(expr->getType()->isEqual(field.getVarargBaseTy()) &&
               "scalar field is not equivalent to dest vararg field?!");

        sugarFields.push_back(TupleTypeElt(field.getType(),
                                           field.getName(),
                                           field.getDefaultArgKind(),
                                           true));
      }
      else {
        assert(expr->getType()->isEqual(field.getType()) &&
               "scalar field is not equivalent to dest tuple field?!");
        sugarFields.push_back(TupleTypeElt(expr->getType(),
                                           field.getName()));
      }

      // Record the
    } else {
      sugarFields.push_back(field);
    }
    ++i;
  }

  // Compute the elements of the resulting tuple.
  SmallVector<int, 4> elements;
  SmallVector<unsigned, 1> variadicArgs;
  SmallVector<Expr*, 4> callerDefaultArgs;
  ConcreteDeclRef defaultArgsOwner = nullptr;
  i = 0;
  for (auto &field : toTuple->getElements()) {
    if (field.isVararg()) {
      elements.push_back(TupleShuffleExpr::Variadic);
      if (i == toScalarIdx) {
        variadicArgs.push_back(i);
        ++i;
        continue;
      }
    }

    // If this is the scalar field, act like we're shuffling the 0th element.
    if (i == toScalarIdx) {
      elements.push_back(0);
      ++i;
      continue;
    }

    assert(field.hasInit() && "Expected a default argument");

    ConcreteDeclRef argOwner;
    // Dig out the owner of the default arguments.
    if (!defaultArgsOwner) {
      argOwner
      = findDefaultArgsOwner(cs, solution,
                             cs.getConstraintLocator(locator));
      assert(argOwner && "Missing default arguments owner?");
    } else {
      argOwner = defaultArgsOwner;
    }

    // Create a caller-side default argument, if we need one.
    if (auto defArg = getCallerDefaultArg(tc, dc, expr->getLoc(),
                                          argOwner, i).first) {
      // Record the caller-side default argument expression.
      // FIXME: Do we need to record what this was synthesized from?
      elements.push_back(TupleShuffleExpr::CallerDefaultInitialize);
      callerDefaultArgs.push_back(defArg);
    } else {
      // Record the owner of the default argument.
      elements.push_back(TupleShuffleExpr::DefaultInitialize);
    }
    if (!defaultArgsOwner) {
      defaultArgsOwner = argOwner;
    } else {
      assert(defaultArgsOwner == argOwner &&
             "default args on same func have different owners");
    }

    ++i;
  }

  Type destSugarTy = hasInit? toTuple
                            : TupleType::get(sugarFields, tc.Context);

  return new (tc.Context) TupleShuffleExpr(expr,
                            tc.Context.AllocateCopy(elements),
                            TupleShuffleExpr::SourceIsScalar,
                            defaultArgsOwner,
                            tc.Context.AllocateCopy(variadicArgs),
                            tc.Context.AllocateCopy(callerDefaultArgs),
                            destSugarTy);
}

/// Collect the conformances for all the protocols of an existential type.
/// If the source type is also existential, we don't want to check conformance
/// because most protocols do not conform to themselves -- however we still
/// allow the conversion here, except the ErasureExpr ends up with trivial
/// conformances.
static ArrayRef<ProtocolConformance*>
collectExistentialConformances(TypeChecker &tc, Type fromType, Type toType,
                               DeclContext *DC) {
  SmallVector<ProtocolDecl *, 4> protocols;
  toType->getAnyExistentialTypeProtocols(protocols);

  SmallVector<ProtocolConformance *, 4> conformances;
  for (auto proto : protocols) {
    ProtocolConformance *conformance;
    bool conforms = tc.containsProtocol(fromType, proto, DC,
                                        (ConformanceCheckFlags::InExpression|
                                         ConformanceCheckFlags::Used),
                                        &conformance);
    assert(conforms && "Type does not conform to protocol?");
    (void)conforms;
    conformances.push_back(conformance);
  }

  return tc.Context.AllocateCopy(conformances);
}

Expr *ExprRewriter::coerceExistential(Expr *expr, Type toType,
                                      ConstraintLocatorBuilder locator) {
  auto &tc = solution.getConstraintSystem().getTypeChecker();
  Type fromType = expr->getType();

  // Handle existential coercions that implicitly look through ImplicitlyUnwrappedOptional<T>.
  if (auto ty = cs.lookThroughImplicitlyUnwrappedOptionalType(fromType)) {
    expr = coerceImplicitlyUnwrappedOptionalToValue(expr, ty, locator);
    
    fromType = expr->getType();
    assert(!fromType->is<AnyMetatypeType>());

    // FIXME: Hack. We shouldn't try to coerce existential when there is no
    // existential upcast to perform.
    if (fromType->isEqual(toType))
      return expr;
  }

  Type fromInstanceType = fromType;
  Type toInstanceType = toType;

  // Look through metatypes
  while (fromInstanceType->is<AnyMetatypeType>() &&
         toInstanceType->is<ExistentialMetatypeType>()) {
    fromInstanceType = fromInstanceType->castTo<AnyMetatypeType>()->getInstanceType();
    toInstanceType = toInstanceType->castTo<ExistentialMetatypeType>()->getInstanceType();
  }

  ASTContext &ctx = tc.Context;

  auto conformances =
    collectExistentialConformances(tc, fromInstanceType, toInstanceType, cs.DC);

  // For existential-to-existential coercions, open the source existential.
  if (fromType->isAnyExistentialType()) {
    fromType = ArchetypeType::getAnyOpened(fromType);
    
    auto archetypeVal = new (ctx) OpaqueValueExpr(expr->getLoc(), fromType);
    
    auto result = new (ctx) ErasureExpr(archetypeVal, toType, conformances);
    return new (ctx) OpenExistentialExpr(expr, archetypeVal, result);
  }

  return new (ctx) ErasureExpr(expr, toType, conformances);
}

static uint getOptionalBindDepth(const BoundGenericType *bgt) {
  
  if (bgt->getDecl()->classifyAsOptionalType()) {
    auto tyarg = bgt->getGenericArgs()[0];
    
    uint innerDepth = 0;
    
    if (auto wrappedBGT = dyn_cast<BoundGenericType>(tyarg->
                                                     getCanonicalType())) {
      innerDepth = getOptionalBindDepth(wrappedBGT);
    }
    
    return 1 + innerDepth;
  }
  
  return 0;
}

static Type getOptionalBaseType(const Type &type) {
  
  if (auto bgt = dyn_cast<BoundGenericType>(type->
                                            getCanonicalType())) {
    if (bgt->getDecl()->classifyAsOptionalType()) {
      return getOptionalBaseType(bgt->getGenericArgs()[0]);
    }
  }
  
  return type;
}

Expr *ExprRewriter::coerceOptionalToOptional(Expr *expr, Type toType,
                                             ConstraintLocatorBuilder locator) {
  auto &tc = cs.getTypeChecker();
  Type fromType = expr->getType();
  
  auto fromGenericType = fromType->castTo<BoundGenericType>();
  auto toGenericType = toType->castTo<BoundGenericType>();
  assert(fromGenericType->getDecl()->classifyAsOptionalType());
  assert(toGenericType->getDecl()->classifyAsOptionalType());
  tc.requireOptionalIntrinsics(expr->getLoc());
  
  Type fromValueType = fromGenericType->getGenericArgs()[0];
  Type toValueType = toGenericType->getGenericArgs()[0];

  
  // If the option kinds are the same, and the wrapped types are the same,
  // but the arities are different, we can peephole the optional-to-optional
  // conversion into a series of nested injections.
  auto toDepth = getOptionalBindDepth(toGenericType);
  auto fromDepth = getOptionalBindDepth(fromGenericType);
  
  if (toDepth > fromDepth) {
    
    auto toBaseType = getOptionalBaseType(toGenericType);
    auto fromBaseType = getOptionalBaseType(fromGenericType);
    
    if ((toGenericType->getDecl() == fromGenericType->getDecl()) &&
        toBaseType->isEqual(fromBaseType)) {
    
      auto diff = toDepth - fromDepth;
      auto isIUO = fromGenericType->getDecl()->
                    classifyAsOptionalType() == OTK_ImplicitlyUnwrappedOptional;
      
      while (diff) {
        const Type &t = expr->getType();
        const Type &wrapped = isIUO ?
                                Type(ImplicitlyUnwrappedOptionalType::get(t)) :
                                Type(OptionalType::get(t));
        expr = new (tc.Context) InjectIntoOptionalExpr(expr, wrapped);
        diagnoseOptionalInjection(cast<InjectIntoOptionalExpr>(expr));
        diff--;
      }
      
      return expr;
    }
  }

  expr = new (tc.Context) BindOptionalExpr(expr, expr->getSourceRange().End,
                                           /*depth*/ 0, fromValueType);
  expr->setImplicit(true);
  expr = coerceToType(expr, toValueType, locator);
  if (!expr) return nullptr;
      
  expr = new (tc.Context) InjectIntoOptionalExpr(expr, toType);
      
  expr = new (tc.Context) OptionalEvaluationExpr(expr, toType);
  expr->setImplicit(true);
  return expr;
}

Expr *ExprRewriter::coerceImplicitlyUnwrappedOptionalToValue(Expr *expr, Type objTy,
                                            ConstraintLocatorBuilder locator) {
  auto optTy = expr->getType();
  // Coerce to an r-value.
  if (optTy->is<LValueType>())
    objTy = LValueType::get(objTy);

  expr = new (cs.getTypeChecker().Context) ForceValueExpr(expr,
                                                          expr->getEndLoc());
  expr->setType(objTy);
  expr->setImplicit();
  return expr;
}

Expr *ExprRewriter::coerceCallArguments(Expr *arg, Type paramType,
                                        ConstraintLocatorBuilder locator) {

  bool allParamsMatch = arg->getType()->isEqual(paramType);
  
  // Determine the parameter bindings.
  auto params = decomposeArgParamType(paramType);
  auto args = decomposeArgParamType(arg->getType());

  // Quickly test if any further fix-ups for the argument types are necessary.
  // FIXME: This hack is only necessary to work around some problems we have
  // for inferring the type of an unresolved member reference expression in
  // an optional context. We should seek a more holistic fix for this.
  if (allParamsMatch &&
      (params.size() == args.size())) {
    if (auto argTuple = dyn_cast<TupleExpr>(arg)) {
      auto argElts = argTuple->getElements();
    
      for (size_t i = 0; i < params.size(); i++) {
        if (auto dotExpr = dyn_cast<DotSyntaxCallExpr>(argElts[i])) {
          auto paramTy = params[i].Ty->getLValueOrInOutObjectType();
          auto argTy = dotExpr->getType()->getLValueOrInOutObjectType();
          if (!paramTy->isEqual(argTy)) {
            allParamsMatch = false;
            break;
          }
        }
      }
    }
  }
  
  if (allParamsMatch)
    return arg;
  
  MatchCallArgumentListener listener;
  
  SmallVector<ParamBinding, 4> parameterBindings;
  bool failed = constraints::matchCallArguments(args, params,
                                                hasTrailingClosure(locator),
                                                /*allowFixes=*/false, listener,
                                                parameterBindings);
  assert(!failed && "Call arguments did not match up?");
  (void)failed;

  // We should either have parentheses or a tuple.
  TupleExpr *argTuple = dyn_cast<TupleExpr>(arg);
  ParenExpr *argParen = dyn_cast<ParenExpr>(arg);
  // FIXME: Eventually, we want to enforce that we have either argTuple or
  // argParen here.

  // Local function to extract the ith argument expression, which papers
  // over some of the weirdness with tuples vs. parentheses.
  auto getArg = [&](unsigned i) -> Expr * {
    if (argTuple)
      return argTuple->getElement(i);
    assert(i == 0 && "Scalar only has a single argument");

    if (argParen)
      return argParen->getSubExpr();

    return arg;
  };

  // Local function to extract the ith argument label, which papers over some
  // of the weirdndess with tuples vs. parentheses.
  auto getArgLabel = [&](unsigned i) -> Identifier {
    if (argTuple)
      return argTuple->getElementName(i);

    assert(i == 0 && "Scalar only has a single argument");
    return Identifier();
  };

  // Local function to produce a locator to refer to the ith element of the
  // argument tuple.
  auto getArgLocator = [&](unsigned argIdx, unsigned paramIdx)
                         -> ConstraintLocatorBuilder {
    return locator.withPathElement(
             LocatorPathElt::getApplyArgToParam(argIdx, paramIdx));
  };

  auto &tc = getConstraintSystem().getTypeChecker();
  bool anythingShuffled = false;
  SmallVector<TupleTypeElt, 4> toSugarFields;
  SmallVector<TupleTypeElt, 4> fromTupleExprFields(
                                 argTuple? argTuple->getNumElements() : 1);
  SmallVector<Expr*, 4> fromTupleExpr(argTuple? argTuple->getNumElements() : 1);
  SmallVector<unsigned, 4> variadicArgs;
  SmallVector<Expr *, 2> callerDefaultArgs;
  ConcreteDeclRef defaultArgsOwner = nullptr;
  Type sliceType = nullptr;
  SmallVector<int, 4> sources;
  for (unsigned paramIdx = 0, numParams = parameterBindings.size();
       paramIdx != numParams; ++paramIdx) {
    // Extract the parameter.
    const auto &param = params[paramIdx];

    // Handle variadic parameters.
    if (param.Variadic) {
      // Find the appropriate injection function.
      if (tc.requireArrayLiteralIntrinsics(arg->getStartLoc()))
        return nullptr;

      // Record this parameter.
      auto paramBaseType = param.Ty;
      assert(sliceType.isNull() && "Multiple variadic parameters?");
      sliceType = tc.getArraySliceType(arg->getLoc(), paramBaseType);
      toSugarFields.push_back(TupleTypeElt(sliceType, param.Label,
                                           DefaultArgumentKind::None, true));
      anythingShuffled = true;
      sources.push_back(TupleShuffleExpr::Variadic);

      // Convert the arguments.
      for (auto argIdx : parameterBindings[paramIdx]) {
        auto arg = getArg(argIdx);
        auto argType = arg->getType();
        variadicArgs.push_back(argIdx);

        // If the argument type exactly matches, this just works.
        if (argType->isEqual(paramBaseType)) {
          fromTupleExprFields[argIdx] = TupleTypeElt(argType,
                                                     getArgLabel(argIdx));
          fromTupleExpr[argIdx] = arg;
          continue;
        }

        // Convert the argument.
        auto convertedArg = coerceToType(arg, paramBaseType,
                                         getArgLocator(argIdx, paramIdx));
        if (!convertedArg)
          return nullptr;

        // Add the converted argument.
        fromTupleExpr[argIdx] = convertedArg;
        fromTupleExprFields[argIdx] = TupleTypeElt(convertedArg->getType(),
                                                   getArgLabel(argIdx));
      }

      continue;
    }

    // If we are using a default argument, handle it now.
    if (parameterBindings[paramIdx].empty()) {
      // Dig out the owner of the default arguments.
      ConcreteDeclRef argOwner;
      if (!defaultArgsOwner) {
        argOwner
        = findDefaultArgsOwner(cs, solution,
                               cs.getConstraintLocator(locator));
        assert(argOwner && "Missing default arguments owner?");
      } else {
        argOwner = defaultArgsOwner;
      }

      // Create a caller-side default argument, if we need one.
      Expr *defArg;
      DefaultArgumentKind defArgKind;
      std::tie(defArg, defArgKind) = getCallerDefaultArg(tc, dc, arg->getLoc(),
                                                         argOwner, paramIdx);

      // Note that we'll be doing a shuffle involving default arguments.
      anythingShuffled = true;
      toSugarFields.push_back(TupleTypeElt(
                                param.Variadic
                                  ? tc.getArraySliceType(arg->getLoc(),
                                                         param.Ty)
                                  : param.Ty,
                                param.Label,
                                defArgKind,
                                param.Variadic));

      if (defArg) {
        callerDefaultArgs.push_back(defArg);
        sources.push_back(TupleShuffleExpr::CallerDefaultInitialize);
      } else {
        sources.push_back(TupleShuffleExpr::DefaultInitialize);
      }
      if (!defaultArgsOwner) {
        defaultArgsOwner = argOwner;
      } else {
        assert(defaultArgsOwner == argOwner &&
               "default args on same func have different owners");
      }
      continue;
    }

    // Extract the argument used to initialize this parameter.
    assert(parameterBindings[paramIdx].size() == 1);
    unsigned argIdx = parameterBindings[paramIdx].front();
    auto arg = getArg(argIdx);
    auto argType = arg->getType();

    // If the argument and parameter indices differ, or if the names differ,
    // this is a shuffle.
    sources.push_back(argIdx);
    if (argIdx != paramIdx || getArgLabel(argIdx) != param.Label) {
      anythingShuffled = true;
    }

    // If the types exactly match, this is easy.
    auto paramType = param.Ty;
    if (argType->isEqual(paramType)) {
      toSugarFields.push_back(TupleTypeElt(argType, param.Label));
      fromTupleExprFields[argIdx] = TupleTypeElt(paramType, param.Label);
      fromTupleExpr[argIdx] = arg;
      continue;
    }

    // Convert the argument.
    auto convertedArg = coerceToType(arg, paramType,
                                     getArgLocator(argIdx, paramIdx));
    if (!convertedArg)
      return nullptr;

    // Add the converted argument.
    fromTupleExpr[argIdx] = convertedArg;
    fromTupleExprFields[argIdx] = TupleTypeElt(convertedArg->getType(),
                                               getArgLabel(argIdx));
    toSugarFields.push_back(TupleTypeElt(argType, param.Label));
  }

  // Compute a new 'arg', from the bits we have.  We have three cases: the
  // scalar case, the paren case, and the tuple literal case.
  if (!argTuple && !argParen) {
    assert(fromTupleExpr.size() == 1 && fromTupleExpr[0]);
    arg = fromTupleExpr[0];
  } else if (argParen) {
    // If the element changed, rebuild a new ParenExpr.
    assert(fromTupleExpr.size() == 1 && fromTupleExpr[0]);
    if (fromTupleExpr[0] != argParen->getSubExpr()) {
      argParen = new (tc.Context) ParenExpr(argParen->getLParenLoc(),
                                            fromTupleExpr[0],
                                            argParen->getRParenLoc(),
                                            argParen->hasTrailingClosure(),
                                            fromTupleExpr[0]->getType());
      arg = argParen;
    } else {
      // coerceToType may have updated the element type of the ParenExpr in
      // place.  If so, propagate the type out to the ParenExpr as well.
      argParen->setType(fromTupleExpr[0]->getType());
    }
  } else {
    assert(argTuple);

    bool anyChanged = false;
    for (unsigned i = 0, e = argTuple->getNumElements(); i != e; ++i)
      if (fromTupleExpr[i] != argTuple->getElement(i)) {
        anyChanged = true;
        break;
      }

    // If anything about the TupleExpr changed, rebuild a new one.
    Type argTupleType = TupleType::get(fromTupleExprFields, tc.Context);
    if (anyChanged || !argTuple->getType()->isEqual(argTupleType)) {
      auto EltNames = argTuple->getElementNames();
      auto EltNameLocs = argTuple->getElementNameLocs();
      argTuple = TupleExpr::create(tc.Context, argTuple->getLParenLoc(),
                                   fromTupleExpr, EltNames, EltNameLocs,
                                   argTuple->getRParenLoc(),
                                   argTuple->hasTrailingClosure(),
                                   argTuple->isImplicit(),
                                   argTupleType);
      arg = argTuple;
    }
  }

  // If we don't have to shuffle anything, we're done.
  if (arg->getType()->isEqual(paramType))
    return arg;

  // If we came from a scalar, create a scalar-to-tuple conversion.
  auto isSourceScalar = TupleShuffleExpr::SourceIsScalar_t(argTuple == nullptr);

  // Create the tuple shuffle.
  ArrayRef<int> mapping = tc.Context.AllocateCopy(sources);
  auto callerDefaultArgsCopy = tc.Context.AllocateCopy(callerDefaultArgs);
  auto shuffle = new (tc.Context) TupleShuffleExpr(
                                    arg, mapping,
                                    isSourceScalar,
                                    defaultArgsOwner,
                                    tc.Context.AllocateCopy(variadicArgs),
                                    callerDefaultArgsCopy,
                                    paramType);
  shuffle->setVarargsArrayType(sliceType);
  return shuffle;
}

/// If the expression is an explicit closure expression (potentially wrapped in
/// IdentityExprs), change the type of the closure and identities to the
/// specified type and return true.  Otherwise, return false with no effect.
static bool applyTypeToClosureExpr(Expr *expr, Type toType) {
  // Look through identity expressions, like parens.
  if (auto IE = dyn_cast<IdentityExpr>(expr)) {
    if (!applyTypeToClosureExpr(IE->getSubExpr(), toType)) return false;
    IE->setType(toType);
    return true;
  }

  // If we found an explicit ClosureExpr, update its type.
  if (auto CE = dyn_cast<ClosureExpr>(expr)) {
    CE->setType(toType);
    return true;
  }
  // Otherwise fail.
  return false;
}

ClosureExpr *ExprRewriter::coerceClosureExprToVoid(ClosureExpr *closureExpr) {
  auto &tc = cs.getTypeChecker();

  // Re-write the single-expression closure to return '()'
  assert(closureExpr->hasSingleExpressionBody());
  
  auto member = closureExpr->getBody()->getElement(0);
  
  // A single-expression body contains a single return statement.
  auto returnStmt = dyn_cast<ReturnStmt>(member.get<Stmt *>());
  auto singleExpr = returnStmt->getResult();
  auto voidExpr = TupleExpr::createEmpty(tc.Context,
                                         singleExpr->getStartLoc(),
                                         singleExpr->getEndLoc(),
                                         /*implicit*/true);
  returnStmt->setResult(voidExpr);

  // For l-value types, reset to the object type. This might not be strictly
  // necessary any more, but it's probably still a good idea.
  if (singleExpr->getType()->getAs<LValueType>())
    singleExpr->setType(singleExpr->getType()->getLValueOrInOutObjectType());

  tc.checkIgnoredExpr(singleExpr);

  SmallVector<ASTNode, 2> elements;
  elements.push_back(singleExpr);
  elements.push_back(returnStmt);
  
  auto braceStmt = BraceStmt::create(tc.Context,
                                     closureExpr->getStartLoc(),
                                     elements,
                                     closureExpr->getEndLoc(),
                                     /*implicit*/true);
  
  closureExpr->setImplicit();
  closureExpr->setIsVoidConversionClosure();
  closureExpr->setBody(braceStmt, /*isSingleExpression*/true);

  auto fnType = closureExpr->getType()->getAs<FunctionType>();
  Type inputType = fnType->getInput();
  Type resultType = voidExpr->getType();
  auto newClosureType = FunctionType::get(inputType,
                                          resultType,
                                          fnType->getExtInfo());
  closureExpr->setType(newClosureType);
  
  return closureExpr;
}

static void
maybeDiagnoseUnsupportedFunctionConversion(TypeChecker &tc, Expr *expr,
                                           AnyFunctionType *toType) {
  Type fromType = expr->getType();
  auto fromFnType = fromType->getAs<AnyFunctionType>();
  
  // Conversions to C function pointer type are limited. Since a C function
  // pointer captures no context, we can only do the necessary thunking or
  // codegen if the original function is a direct reference to a global function
  // or context-free closure or local function.
  if (toType->getRepresentation()
       == AnyFunctionType::Representation::CFunctionPointer) {
    // Can convert from an ABI-compatible C function pointer.
    if (fromFnType
        && fromFnType->getRepresentation()
            == AnyFunctionType::Representation::CFunctionPointer)
      return;
    
    // Can convert a decl ref to a global or local function that doesn't
    // capture context. Look through ignored bases too.
    // TODO: Look through static method applications to the type.
    auto semanticExpr = expr->getSemanticsProvidingExpr();
    while (auto ignoredBase = dyn_cast<DotSyntaxBaseIgnoredExpr>(semanticExpr)){
      semanticExpr = ignoredBase->getRHS()->getSemanticsProvidingExpr();
    }
    
    auto maybeDiagnoseFunctionRef = [&](FuncDecl *fn) {
      // TODO: We could allow static (or class final) functions too by
      // "capturing" the metatype in a thunk.
      if (fn->getDeclContext()->isTypeContext()) {
        tc.diagnose(expr->getLoc(),
                    diag::c_function_pointer_from_method);
      } else if (fn->getGenericParams()) {
        tc.diagnose(expr->getLoc(),
                    diag::c_function_pointer_from_generic_function);
      } else {
        tc.maybeDiagnoseCaptures(expr, fn);
      }
    };
    
    if (auto declRef = dyn_cast<DeclRefExpr>(semanticExpr)) {
      if (auto fn = dyn_cast<FuncDecl>(declRef->getDecl())) {
        return maybeDiagnoseFunctionRef(fn);
      }
    }
    
    if (auto memberRef = dyn_cast<MemberRefExpr>(semanticExpr)) {
      if (auto fn = dyn_cast<FuncDecl>(memberRef->getMember().getDecl())) {
        return maybeDiagnoseFunctionRef(fn);
      }
    }

    // Unwrap closures with explicit capture lists.
    if (auto capture = dyn_cast<CaptureListExpr>(semanticExpr))
      semanticExpr = capture->getClosureBody();
    
    // Can convert a literal closure that doesn't capture context.
    if (auto closure = dyn_cast<ClosureExpr>(semanticExpr)) {
      tc.maybeDiagnoseCaptures(expr, closure);
      return;
    }
    
    tc.diagnose(expr->getLoc(),
                diag::invalid_c_function_pointer_conversion_expr);
  }
}

Expr *ExprRewriter::coerceToType(Expr *expr, Type toType,
                                 ConstraintLocatorBuilder locator) {
  auto &tc = cs.getTypeChecker();

  // The type we're converting from.
  Type fromType = expr->getType();

  // If the types are already equivalent, we don't have to do anything.
  if (fromType->isEqual(toType))
    return expr;

  // If the solver recorded what we should do here, just do it immediately.
  auto knownRestriction = solution.ConstraintRestrictions.find(
                            { fromType->getCanonicalType(),
                              toType->getCanonicalType() });
  if (knownRestriction != solution.ConstraintRestrictions.end()) {
    switch (knownRestriction->second) {
    case ConversionRestrictionKind::TupleToTuple: {
      auto fromTuple = fromType->castTo<TupleType>();
      auto toTuple = toType->castTo<TupleType>();
      SmallVector<int, 4> sources;
      SmallVector<unsigned, 4> variadicArgs;
      bool failed = computeTupleShuffle(fromTuple, toTuple,
                                        sources, variadicArgs);
      assert(!failed && "Couldn't convert tuple to tuple?");
      (void)failed;
      return coerceTupleToTuple(expr, fromTuple, toTuple, locator, sources,
                                variadicArgs);
    }

    case ConversionRestrictionKind::ScalarToTuple: {
      auto toTuple = toType->castTo<TupleType>();
      return coerceScalarToTuple(expr, toTuple,
                                 toTuple->getElementForScalarInit(), locator);
    }

    case ConversionRestrictionKind::TupleToScalar: {
      // If this was a single-element tuple expression, reach into that
      // subexpression.
      // FIXME: This is a hack to deal with @lvalue-ness issues. It loses
      // source information.
      if (auto fromTupleExpr = dyn_cast<TupleExpr>(expr)) {
        if (fromTupleExpr->getNumElements() == 1) {
          return coerceToType(fromTupleExpr->getElement(0), toType,
                              locator.withPathElement(
                                LocatorPathElt::getTupleElement(0)));
        }
      }

      // Extract the element.
      auto fromTuple = fromType->castTo<TupleType>();
      expr = new (cs.getASTContext()) TupleElementExpr(
                                        expr,
                                        expr->getLoc(),
                                        0,
                                        expr->getLoc(),
                                        fromTuple->getElementType(0));
      expr->setImplicit(true);

      // Coerce the element to the expected type.
      return coerceToType(expr, toType,
                          locator.withPathElement(
                            LocatorPathElt::getTupleElement(0)));
    }

    case ConversionRestrictionKind::DeepEquality:
      llvm_unreachable("Equality handled above");

    case ConversionRestrictionKind::Superclass: {
      // Coercion from archetype to its (concrete) superclass.
      if (auto fromArchetype = fromType->getAs<ArchetypeType>()) {
        expr = new (tc.Context) ArchetypeToSuperExpr(
                                  expr,
                                  fromArchetype->getSuperclass());

        // If we are done succeeded, use the coerced result.
        if (expr->getType()->isEqual(toType)) {
          return expr;
        }

        fromType = expr->getType();
      }
      
      // Coercion from subclass to superclass.
      return new (tc.Context) DerivedToBaseExpr(expr, toType);
    }

    case ConversionRestrictionKind::LValueToRValue: {
      // Load from the lvalue.
      expr->propagateLValueAccessKind(AccessKind::Read);
      expr = new (tc.Context) LoadExpr(expr, fromType->getRValueType());

      // Coerce the result.
      return coerceToType(expr, toType, locator);
    }

    case ConversionRestrictionKind::Existential:
    case ConversionRestrictionKind::MetatypeToExistentialMetatype:
      return coerceExistential(expr, toType, locator);

    case ConversionRestrictionKind::ClassMetatypeToAnyObject: {
      assert(tc.getLangOpts().EnableObjCInterop
             && "metatypes can only be cast to objects w/ objc runtime!");
      return new (tc.Context) ClassMetatypeToObjectExpr(expr, toType);
    }
    case ConversionRestrictionKind::ExistentialMetatypeToAnyObject: {
      assert(tc.getLangOpts().EnableObjCInterop
             && "metatypes can only be cast to objects w/ objc runtime!");
      return new (tc.Context) ExistentialMetatypeToObjectExpr(expr, toType);
    }
    case ConversionRestrictionKind::ProtocolMetatypeToProtocolClass: {
      return new (tc.Context) ProtocolMetatypeToObjectExpr(expr, toType);
    }
        
    case ConversionRestrictionKind::ValueToOptional: {
      auto toGenericType = toType->castTo<BoundGenericType>();
      assert(toGenericType->getDecl()->classifyAsOptionalType());
      tc.requireOptionalIntrinsics(expr->getLoc());

      Type valueType = toGenericType->getGenericArgs()[0];
      expr = coerceToType(expr, valueType, locator);
      if (!expr) return nullptr;

      auto *result = new (tc.Context) InjectIntoOptionalExpr(expr, toType);
      diagnoseOptionalInjection(result);
      return result;
    }

    case ConversionRestrictionKind::OptionalToImplicitlyUnwrappedOptional:
    case ConversionRestrictionKind::ImplicitlyUnwrappedOptionalToOptional:
    case ConversionRestrictionKind::OptionalToOptional:
      return coerceOptionalToOptional(expr, toType, locator);

    case ConversionRestrictionKind::ForceUnchecked: {
      auto valueTy = fromType->getImplicitlyUnwrappedOptionalObjectType();
      assert(valueTy);
      expr = coerceImplicitlyUnwrappedOptionalToValue(expr, valueTy, locator);
      return coerceToType(expr, toType, locator);
    }

    case ConversionRestrictionKind::ArrayUpcast: {
      // Look through implicitly unwrapped optionals.
      if (auto objTy= cs.lookThroughImplicitlyUnwrappedOptionalType(fromType)) {
        expr = coerceImplicitlyUnwrappedOptionalToValue(expr, objTy, locator);
      }

      // Form the upcast.
      bool isBridged = !cs.getBaseTypeForArrayType(fromType.getPointer())
                          ->isBridgeableObjectType();
      return new (tc.Context) CollectionUpcastConversionExpr(expr, toType,
                                                             isBridged);
    }

    case ConversionRestrictionKind::DictionaryUpcast: {
      // Look through implicitly unwrapped optionals.
      if (auto objTy
            = cs.lookThroughImplicitlyUnwrappedOptionalType(expr->getType())) {
        expr = coerceImplicitlyUnwrappedOptionalToValue(expr, objTy, locator);
      }

      // If the source key and value types are object types, this is an upcast.
      // Otherwise, it's bridged.
      Type sourceKey, sourceValue;
      std::tie(sourceKey, sourceValue) = *cs.isDictionaryType(expr->getType());

      bool isBridged = !sourceKey->isBridgeableObjectType() ||
                       !sourceValue->isBridgeableObjectType();
      return new (tc.Context) CollectionUpcastConversionExpr(expr, toType,
                                                             isBridged);
    }

    case ConversionRestrictionKind::SetUpcast: {
      // Look through implicitly unwrapped optionals.
      if (auto objTy
            = cs.lookThroughImplicitlyUnwrappedOptionalType(expr->getType())) {
        expr = coerceImplicitlyUnwrappedOptionalToValue(expr, objTy, locator);
      }

      bool isBridged = !cs.getBaseTypeForSetType(fromType.getPointer())
                          ->isBridgeableObjectType();
      return new (tc.Context) CollectionUpcastConversionExpr(expr, toType,
                                                             isBridged);
    }

    case ConversionRestrictionKind::InoutToPointer: {
      // Overwrite the l-value access kind to be read-only if we're
      // converting to a non-mutable pointer type.
      PointerTypeKind pointerKind;
      auto toEltType = toType->getAnyPointerElementType(pointerKind);
      assert(toEltType && "not a pointer type?"); (void) toEltType;
      if (pointerKind == PTK_UnsafePointer) {
        cast<InOutExpr>(expr->getValueProvidingExpr())->getSubExpr()
          ->propagateLValueAccessKind(AccessKind::Read, /*overwrite*/ true);
      }

      tc.requirePointerArgumentIntrinsics(expr->getLoc());
      return new (tc.Context) InOutToPointerExpr(expr, toType);
    }
    
    case ConversionRestrictionKind::ArrayToPointer: {
      tc.requirePointerArgumentIntrinsics(expr->getLoc());
      return new (tc.Context) ArrayToPointerExpr(expr, toType);
    }
    
    case ConversionRestrictionKind::StringToPointer: {
      tc.requirePointerArgumentIntrinsics(expr->getLoc());
      return new (tc.Context) StringToPointerExpr(expr, toType);
    }
    
    case ConversionRestrictionKind::PointerToPointer: {
      tc.requirePointerArgumentIntrinsics(expr->getLoc());
      return new (tc.Context) PointerToPointerExpr(expr, toType);
    }

    case ConversionRestrictionKind::BridgeToObjC: {
      Expr *objcExpr = bridgeToObjectiveC(expr);
      if (!objcExpr)
        return nullptr;

      return coerceToType(objcExpr, toType, locator);
    }
    
    case ConversionRestrictionKind::BridgeToNSError: {
      // Tell the ErrorType to become an NSError, using _bridgeErrorTypeToNSError.
      auto fn = tc.Context.getBridgeErrorTypeToNSError(&tc);
      if (!fn) {
        tc.diagnose(expr->getLoc(), diag::missing_nserror_bridging_function);
        return nullptr;
      }
      tc.validateDecl(fn);
      ConcreteDeclRef fnDeclRef(fn);
      Expr *fnRef = new (tc.Context) DeclRefExpr(fnDeclRef, expr->getLoc(),
                                                 /*Implicit=*/true);
      fnRef->setType(fn->getInterfaceType());
      Expr *call = new (tc.Context) CallExpr(fnRef, expr,
                                             /*implicit*/ true);
      if (tc.typeCheckExpressionShallow(call, dc))
        return nullptr;
      
      // The return type of _bridgeErrorTypeToNSError is formally 'AnyObject' to avoid
      // stdlib-to-Foundation dependencies, but it's really NSError.
      // Abuse CovariantReturnConversionExpr to fix this.
      
      return new (tc.Context) CovariantReturnConversionExpr(call, toType);
    }

    case ConversionRestrictionKind::BridgeFromObjC:
      return forceBridgeFromObjectiveC(expr, toType);

    case ConversionRestrictionKind::CFTollFreeBridgeToObjC: {
      auto foreignClass = fromType->getClassOrBoundGenericClass();
      auto objcType = foreignClass->getAttrs().getAttribute<ObjCBridgedAttr>()
                        ->getObjCClass()->getDeclaredInterfaceType();
      auto asObjCClass = new (tc.Context) ForeignObjectConversionExpr(expr,
                                                                      objcType);
      return coerceToType(asObjCClass, toType, locator);
    }

    case ConversionRestrictionKind::ObjCTollFreeBridgeToCF: {
      auto foreignClass = toType->getClassOrBoundGenericClass();
      auto objcType = foreignClass->getAttrs().getAttribute<ObjCBridgedAttr>()
                        ->getObjCClass()->getDeclaredInterfaceType();
      Expr *result = coerceToType(expr, objcType, locator);
      if (!result)
        return nullptr;

      return new (tc.Context) ForeignObjectConversionExpr(result, toType);
    }
    }
  }

  // Tuple-to-scalar conversion.
  if (auto fromTuple = fromType->getAs<TupleType>()) {
    if (fromTuple->getNumElements() == 1 &&
        !fromTuple->getElement(0).isVararg() &&
        !toType->is<TupleType>()) {
      expr = new (cs.getASTContext()) TupleElementExpr(
                                        expr,
                                        expr->getLoc(),
                                        0,
                                        expr->getLoc(),
                                        fromTuple->getElementType(0));
      expr->setImplicit(true);
    }
  }

  // Coercions from an lvalue: load or perform implicit address-of. We perform
  // these coercions first because they are often the first step in a multi-step
  // coercion.
  if (auto fromLValue = fromType->getAs<LValueType>()) {
    if (auto *toIO = toType->getAs<InOutType>()) {
      (void)toIO;
      // In an 'inout' operator like "++i", the operand is converted from
      // an implicit lvalue to an inout argument.
      assert(toIO->getObjectType()->isEqual(fromLValue->getObjectType()));
      expr->propagateLValueAccessKind(AccessKind::ReadWrite);
      return new (tc.Context) InOutExpr(expr->getStartLoc(), expr,
                                            toType, /*isImplicit*/true);
    }

    // If we're actually turning this into an lvalue tuple element, don't
    // load.
    bool performLoad = true;
    if (auto toTuple = toType->getAs<TupleType>()) {
      int scalarIdx = toTuple->getElementForScalarInit();
      if (scalarIdx >= 0 &&
          toTuple->getElementType(scalarIdx)->is<InOutType>())
        performLoad = false;
    }

    if (performLoad) {
      // Load from the lvalue.
      expr->propagateLValueAccessKind(AccessKind::Read);
      expr = new (tc.Context) LoadExpr(expr, fromLValue->getObjectType());

      // Coerce the result.
      return coerceToType(expr, toType, locator);
    }
  }

  // Coercions to tuple type.
  if (auto toTuple = toType->getAs<TupleType>()) {
    // Coerce from a tuple to a tuple.
    if (auto fromTuple = fromType->getAs<TupleType>()) {
      SmallVector<int, 4> sources;
      SmallVector<unsigned, 4> variadicArgs;
      if (!computeTupleShuffle(fromTuple, toTuple, sources, variadicArgs)) {
        return coerceTupleToTuple(expr, fromTuple, toTuple,
                                  locator, sources, variadicArgs);
      }
    }

    // Coerce scalar to tuple.
    int toScalarIdx = toTuple->getElementForScalarInit();
    if (toScalarIdx != -1) {
      return coerceScalarToTuple(expr, toTuple, toScalarIdx, locator);
    }
  }

  // Coercion from a subclass to a superclass.
  if (fromType->mayHaveSuperclass() &&
      toType->getClassOrBoundGenericClass()) {
    for (auto fromSuperClass = tc.getSuperClassOf(fromType);
         fromSuperClass;
         fromSuperClass = tc.getSuperClassOf(fromSuperClass)) {
      if (fromSuperClass->isEqual(toType)) {
        
        // Coercion from archetype to its (concrete) superclass.
        if (auto fromArchetype = fromType->getAs<ArchetypeType>()) {
          expr = new (tc.Context) ArchetypeToSuperExpr(
                                    expr,
                                    fromArchetype->getSuperclass());

          // If we succeeded, use the coerced result.
          if (expr->getType()->isEqual(toType))
            return expr;
        }

        // Coercion from subclass to superclass.
        return new (tc.Context) DerivedToBaseExpr(expr, toType);
      }
    }
  }

  // Coercions to function type.
  if (auto toFunc = toType->getAs<FunctionType>()) {
    // Coercion to an autoclosure type produces an implicit closure.
    // FIXME: The type checker is more lenient, and allows @autoclosures to
    // be subtypes of non-@autoclosures, which is bogus.
    if (toFunc->isAutoClosure()) {
      // Convert the value to the expected result type of the function.
      expr = coerceToType(expr, toFunc->getResult(),
                          locator.withPathElement(ConstraintLocator::Load));

      // We'll set discriminator values on all the autoclosures in a
      // later pass.
      auto discriminator = AutoClosureExpr::InvalidDiscriminator;
      auto closure = new (tc.Context) AutoClosureExpr(expr, toType,
                                                      discriminator, dc);
      Pattern *pattern = TuplePattern::create(tc.Context, expr->getLoc(),
                                              ArrayRef<TuplePatternElt>(),
                                              expr->getLoc());
      pattern->setType(TupleType::getEmpty(tc.Context));
      closure->setParams(pattern);

      // Compute the capture list, now that we have analyzed the expression.
      tc.ClosuresWithUncomputedCaptures.push_back(closure);

      return closure;
    }

    // Coercion from one function type to another, this produces a
    // FunctionConversionExpr in its full generality.
    if (auto fromFunc = fromType->getAs<FunctionType>()) {
      // If toType is a NoEscape or NoReturn function type and the expression is
      // a ClosureExpr, propagate these bits onto the ClosureExpr.  Do not
      // *remove* any bits that are already on the closure though.
      // Note that in this case, we do not want to propagate the 'throws' bit
      // to the closure type, as the closure has already been analyzed for
      // throwing subexpressions. We also don't want to change the convention
      // of the original closure.
      auto fromEI = fromFunc->getExtInfo(), toEI = toFunc->getExtInfo();
      if ((toEI.isNoEscape() && !fromEI.isNoEscape()) ||
          (toEI.isNoReturn() && !fromEI.isNoReturn())) {
        swift::AnyFunctionType::ExtInfo newEI(fromEI.getRepresentation(),
                                        toEI.isNoReturn() | fromEI.isNoReturn(),
                                        toEI.isAutoClosure(),
                                        toEI.isNoEscape() | fromEI.isNoEscape(),
                                        toEI.throws() & fromEI.throws());
        auto newToType = FunctionType::get(fromFunc->getInput(),
                                           fromFunc->getResult(), newEI);
        if (applyTypeToClosureExpr(expr, newToType)) {
          fromFunc = newToType;
          // Propagating the bits in might have satisfied the entire
          // conversion.  If so, we're done, otherwise keep converting.
          if (fromFunc->isEqual(toType))
            return expr;
        }
      }

      maybeDiagnoseUnsupportedFunctionConversion(tc, expr, toFunc);

      return new (tc.Context) FunctionConversionExpr(expr, toType);
    }
  }

  // Coercions from a type to an existential type.
  if (toType->isAnyExistentialType()) {
    return coerceExistential(expr, toType, locator);
  }

  // Coercion to Optional<T>.
  if (auto toGenericType = toType->getAs<BoundGenericType>()) {
    if (toGenericType->getDecl()->classifyAsOptionalType()) {
      tc.requireOptionalIntrinsics(expr->getLoc());

      Type valueType = toGenericType->getGenericArgs()[0];
      expr = coerceToType(expr, valueType, locator);
      if (!expr) return nullptr;

      auto *result = new (tc.Context) InjectIntoOptionalExpr(expr, toType);
      diagnoseOptionalInjection(result);
      return result;
    }
  }

  // Coercion from one metatype to another.
  if (fromType->is<MetatypeType>()) {
    auto toMeta = toType->castTo<MetatypeType>();
    return new (tc.Context) MetatypeConversionExpr(expr, toMeta);
  }

  // Conversion to/from UnresolvedType.
  if (fromType->is<UnresolvedType>() || toType->is<UnresolvedType>())
    return new (tc.Context) UnresolvedTypeConversionExpr(expr, toType);

  llvm_unreachable("Unhandled coercion");
}

/// Adjust the given type to become the self type when referring to
/// the given member.
static Type adjustSelfTypeForMember(Type baseTy, ValueDecl *member,
                                    AccessSemantics semantics,
                                    DeclContext *UseDC) {
  auto baseObjectTy = baseTy->getLValueOrInOutObjectType();
  if (auto func = dyn_cast<AbstractFunctionDecl>(member)) {
    // If 'self' is an inout type, turn the base type into an lvalue
    // type with the same qualifiers.
    auto selfTy = func->getType()->getAs<AnyFunctionType>()->getInput();
    if (selfTy->is<InOutType>()) {
      // Unless we're looking at a nonmutating existential member.  In which
      // case, the member will be modeled as an inout but ExistentialMemberRef
      // and ArchetypeMemberRef want to take the base as an rvalue.
      if (auto *fd = dyn_cast<FuncDecl>(func))
        if (!fd->isMutating() &&
            baseObjectTy->hasDependentProtocolConformances())
          return baseObjectTy;
      
      return InOutType::get(baseObjectTy);
    }

    // Otherwise, return the rvalue type.
    return baseObjectTy;
  }

  // If the base of the access is mutable, then we may be invoking a getter or
  // setter that requires the base to be mutable.
  if (auto *SD = dyn_cast<AbstractStorageDecl>(member)) {
    bool isSettableFromHere = SD->isSettable(UseDC)
      && (!UseDC->getASTContext().LangOpts.EnableAccessControl
          || SD->isSetterAccessibleFrom(UseDC));

    // If neither the property's getter nor its setter are mutating, the base
    // can be an rvalue.
    if (!SD->isGetterMutating()
        && (!isSettableFromHere || SD->isSetterNonMutating()))
      return baseObjectTy;

    // If we're calling an accessor, keep the base as an inout type, because the
    // getter may be mutating.
    if (SD->hasAccessorFunctions() && baseTy->is<InOutType>() &&
        semantics != AccessSemantics::DirectToStorage)
      return InOutType::get(baseObjectTy);
  }

  // Accesses to non-function members in value types are done through an @lvalue
  // type.
  if (baseTy->is<InOutType>())
    return LValueType::get(baseObjectTy);
  
  // Accesses to members in values of reference type (classes, metatypes) are
  // always done through a the reference to self.  Accesses to value types with
  // a non-mutable self are also done through the base type.
  return baseTy;
}

Expr *
ExprRewriter::coerceObjectArgumentToType(Expr *expr,
                                         Type baseTy, ValueDecl *member,
                                         AccessSemantics semantics,
                                         ConstraintLocatorBuilder locator) {
  Type toType = adjustSelfTypeForMember(baseTy, member, semantics, dc);

  // If our expression already has the right type, we're done.
  Type fromType = expr->getType();
  if (fromType->isEqual(toType))
    return expr;

  // If we're coercing to an rvalue type, just do it.
  if (!toType->is<InOutType>())
    return coerceToType(expr, toType, locator);

  assert(fromType->is<LValueType>() && "Can only convert lvalues to inout");

  auto &ctx = cs.getTypeChecker().Context;

  // Use InOutExpr to convert it to an explicit inout argument for the
  // receiver.
  expr->propagateLValueAccessKind(AccessKind::ReadWrite);
  return new (ctx) InOutExpr(expr->getStartLoc(), expr,
                                 toType, /*isImplicit*/true);
}

Expr *ExprRewriter::convertLiteral(Expr *literal,
                                   Type type,
                                   Type openedType,
                                   ProtocolDecl *protocol,
                                   TypeOrName literalType,
                                   DeclName literalFuncName,
                                   ProtocolDecl *builtinProtocol,
                                   TypeOrName builtinLiteralType,
                                   DeclName builtinLiteralFuncName,
                                   bool (*isBuiltinArgType)(Type),
                                   Diag<> brokenProtocolDiag,
                                   Diag<> brokenBuiltinProtocolDiag) {
  auto &tc = cs.getTypeChecker();

  // If coercing a literal to an unresolved type, we don't try to look up the
  // witness members, just do it.
  if (type->is<UnresolvedType>()) {
    // Instead of updating the literal expr in place, allocate a new node.  This
    // avoids issues where Builtin types end up on expr nodes and pollute
    // diagnostics.
    literal = cast<LiteralExpr>(literal)->shallowClone(tc.Context);
    
    // The literal expression has this type.
    literal->setType(type);
    return literal;
  }
  
  // Check whether this literal type conforms to the builtin protocol.
  ProtocolConformance *builtinConformance = nullptr;
  if (builtinProtocol &&
      tc.conformsToProtocol(type, builtinProtocol, cs.DC,
                            ConformanceCheckFlags::InExpression,
                            &builtinConformance)) {
    // Find the builtin argument type we'll use.
    Type argType;
    if (builtinLiteralType.is<Type>())
      argType = builtinLiteralType.get<Type>();
    else
      argType = tc.getWitnessType(type, builtinProtocol,
                                  builtinConformance,
                                  builtinLiteralType.get<Identifier>(),
                                  brokenBuiltinProtocolDiag);

    if (!argType)
      return nullptr;

    // Make sure it's of an appropriate builtin type.
    if (isBuiltinArgType && !isBuiltinArgType(argType)) {
      tc.diagnose(builtinProtocol->getLoc(), brokenBuiltinProtocolDiag);
      return nullptr;
    }

    // Instead of updating the literal expr in place, allocate a new node.  This
    // avoids issues where Builtin types end up on expr nodes and pollute
    // diagnostics.
    literal = cast<LiteralExpr>(literal)->shallowClone(tc.Context);

    // The literal expression has this type.
    literal->setType(argType);

    // Call the builtin conversion operation.
    // FIXME: Bogus location info.
    Expr *base = TypeExpr::createImplicitHack(literal->getLoc(), type,
                                              tc.Context);
    Expr *result = tc.callWitness(base, dc,
                                  builtinProtocol, builtinConformance,
                                  builtinLiteralFuncName,
                                  literal,
                                  brokenBuiltinProtocolDiag);
    if (result)
      result->setType(type);
    return result;
  }

  // This literal type must conform to the (non-builtin) protocol.
  assert(protocol && "requirements should have stopped recursion");
  ProtocolConformance *conformance = nullptr;
  bool conforms = tc.conformsToProtocol(type, protocol, cs.DC,
                                        ConformanceCheckFlags::InExpression,
                                        &conformance);
  assert(conforms && "must conform to literal protocol");
  (void)conforms;

  // Figure out the (non-builtin) argument type if there is one.
  Type argType;
  if (literalType.is<Identifier>() &&
      literalType.get<Identifier>().empty()) {
    // If there is no argument to the constructor function, then just pass in
    // the empty tuple.
    literal = TupleExpr::createEmpty(tc.Context, literal->getLoc(),
                                     literal->getLoc(), /*implicit*/true);
  } else {
    // Otherwise, figure out the type of the constructor function and coerce to
    // it.
    if (literalType.is<Type>())
      argType = literalType.get<Type>();
    else
      argType = tc.getWitnessType(type, protocol, conformance,
                                  literalType.get<Identifier>(),
                                  brokenProtocolDiag);
    if (!argType)
      return nullptr;

    // If the argument type is in error, we're done.
    if (argType->is<ErrorType>())
      return nullptr;

    // Convert the literal to the non-builtin argument type via the
    // builtin protocol, first.
    // FIXME: Do we need an opened type here?
    literal = convertLiteral(literal, argType, argType, nullptr, Identifier(),
                             Identifier(), builtinProtocol,
                             builtinLiteralType, builtinLiteralFuncName,
                             isBuiltinArgType, brokenProtocolDiag,
                             brokenBuiltinProtocolDiag);
    if (!literal)
      return nullptr;
  }

  // Convert the resulting expression to the final literal type.
  // FIXME: Bogus location info.
  Expr *base = TypeExpr::createImplicitHack(literal->getLoc(), type,
                                            tc.Context);
  literal = tc.callWitness(base, dc,
                           protocol, conformance, literalFuncName,
                           literal, brokenProtocolDiag);
  if (literal)
    literal->setType(type);
  return literal;
}

/// Determine whether the given type refers to a non-final class (or
/// dynamic self of one).
static bool isNonFinalClass(Type type) {
  if (auto dynamicSelf = type->getAs<DynamicSelfType>())
    type = dynamicSelf->getSelfType();

  if (auto classDecl = type->getClassOrBoundGenericClass())
    return !classDecl->isFinal();

  if (auto archetype = type->getAs<ArchetypeType>())
    if (auto super = archetype->getSuperclass())
      return isNonFinalClass(super);

  return false;
}

// Non-required constructors may not be not inherited. Therefore when
// constructing a class object, either the metatype must be statically
// derived (rather than an arbitrary value of metatype type) or the referenced
// constructor must be required.
bool
TypeChecker::diagnoseInvalidDynamicConstructorReferences(Expr *base,
                                                     SourceLoc memberRefLoc,
                                                     AnyMetatypeType *metaTy,
                                                     ConstructorDecl *ctorDecl,
                                                     bool SuppressDiagnostics) {
  auto ty = metaTy->getInstanceType();
  
  // FIXME: The "hasClangNode" check here is a complete hack.
  if (isNonFinalClass(ty) &&
      !base->isStaticallyDerivedMetatype() &&
      !ctorDecl->hasClangNode() &&
      !(ctorDecl->isRequired() ||
        ctorDecl->getDeclContext()->isProtocolOrProtocolExtensionContext())) {
    if (SuppressDiagnostics)
      return false;

    diagnose(memberRefLoc, diag::dynamic_construct_class, ty)
      .highlight(base->getSourceRange());
    auto ctor = cast<ConstructorDecl>(ctorDecl);
    diagnose(ctorDecl, diag::note_nonrequired_initializer,
             ctor->isImplicit(), ctor->getFullName());
  // Constructors cannot be called on a protocol metatype, because there is no
  // metatype to witness it.
  } else if (isa<ConstructorDecl>(ctorDecl) &&
             isa<MetatypeType>(metaTy) &&
             ty->isExistentialType()) {
    if (SuppressDiagnostics)
      return false;

    if (base->isStaticallyDerivedMetatype()) {
      diagnose(memberRefLoc, diag::construct_protocol_by_name, ty)
        .highlight(base->getSourceRange());
    } else {
      diagnose(memberRefLoc, diag::construct_protocol_value, metaTy)
        .highlight(base->getSourceRange());
    }
  }
  return true;
}

Expr *ExprRewriter::finishApply(ApplyExpr *apply, Type openedType,
                                ConstraintLocatorBuilder locator) {
  TypeChecker &tc = cs.getTypeChecker();
  
  auto fn = apply->getFn();
  
  // The function is always an rvalue.
  fn = tc.coerceToRValue(fn);
  assert(fn && "Rvalue conversion failed?");
  if (!fn)
    return nullptr;
  
  // Handle applications that look through ImplicitlyUnwrappedOptional<T>.
  if (auto fnTy = cs.lookThroughImplicitlyUnwrappedOptionalType(fn->getType()))
    fn = coerceImplicitlyUnwrappedOptionalToValue(fn, fnTy, locator);

  // If we're applying a function that resulted from a covariant
  // function conversion, strip off that conversion.
  // FIXME: It would be nicer if we could build the ASTs properly in the
  // first shot.
  Type covariantResultType;
  if (auto covariant = dyn_cast<CovariantFunctionConversionExpr>(fn)) {
    // Strip off one layer of application from the covariant result.
    covariantResultType
      = covariant->getType()->castTo<AnyFunctionType>()->getResult();
   
    // Use the subexpression as the function.
    fn = covariant->getSubExpr();
  }
  
  apply->setFn(fn);

  // Check whether the argument is 'super'.
  bool isSuper = apply->getArg()->isSuperExpr();

  // For function application, convert the argument to the input type of
  // the function.
  if (auto fnType = fn->getType()->getAs<FunctionType>()) {
    auto origArg = apply->getArg();

    Expr *arg = coerceCallArguments(origArg, fnType->getInput(),
                                    locator.withPathElement(
                                      ConstraintLocator::ApplyArgument));
    if (!arg) {
      return nullptr;
    }

    apply->setArg(arg);
    apply->setType(fnType->getResult());
    apply->setIsSuper(isSuper);

    assert(!apply->getType()->is<PolymorphicFunctionType>() &&
           "Polymorphic function type slipped through");
    Expr *result = tc.substituteInputSugarTypeForResult(apply);

    // Try closing the existential, if there is one.
    closeExistential(result);

    // If we have a covariant result type, perform the conversion now.
    if (covariantResultType) {
      if (covariantResultType->is<FunctionType>())
        result = new (tc.Context) CovariantFunctionConversionExpr(
                                    result,
                                    covariantResultType);
      else
        result = new (tc.Context) CovariantReturnConversionExpr(
                                    result, 
                                    covariantResultType);
    }

    return result;
  }

  // We have a type constructor.
  auto metaTy = fn->getType()->castTo<AnyMetatypeType>();
  auto ty = metaTy->getInstanceType();
  
  // If the metatype value isn't a type expression, the user should reference
  // '.init' explicitly, for clarity.
  if (!fn->isTypeReference()) {
    cs.TC.diagnose(apply->getArg()->getStartLoc(),
                   diag::missing_init_on_metatype_initialization)
      .fixItInsert(apply->getArg()->getStartLoc(), ".init");
  }

  // If we're "constructing" a tuple type, it's simply a conversion.
  if (auto tupleTy = ty->getAs<TupleType>()) {
    // FIXME: Need an AST to represent this properly.
    return coerceToType(apply->getArg(), tupleTy, locator);
  }

  // We're constructing a value of nominal type. Look for the constructor or
  // enum element to use.
  assert(ty->getNominalOrBoundGenericNominal() || ty->is<DynamicSelfType>() ||
         ty->hasDependentProtocolConformances());
  auto ctorLocator = cs.getConstraintLocator(
                 locator.withPathElement(ConstraintLocator::ApplyFunction)
                        .withPathElement(ConstraintLocator::ConstructorMember));
  auto selected = getOverloadChoiceIfAvailable(ctorLocator);

  // We have the constructor.
  auto choice = selected->choice;
  auto decl = choice.getDecl();
  
  // Consider the constructor decl reference expr 'implicit', but the
  // constructor call expr itself has the apply's 'implicitness'.
  bool isDynamic = choice.getKind() == OverloadChoiceKind::DeclViaDynamic;
  Expr *declRef = buildMemberRef(fn,
                                 selected->openedFullType,
                                 /*DotLoc=*/SourceLoc(),
                                 decl, fn->getEndLoc(),
                                 selected->openedType,
                                 locator,
                                 ctorLocator,
                                 /*Implicit=*/true, AccessSemantics::Ordinary,
                                 isDynamic);
  if (!declRef)
    return nullptr;
  declRef->setImplicit(apply->isImplicit());
  apply->setFn(declRef);

  // Tail-recur to actually call the constructor.
  return finishApply(apply, openedType, locator);
}

/// Diagnose an argument labeling issue, returning true if we successfully
/// diagnosed the issue.
bool ConstraintSystem::
diagnoseArgumentLabelError(Expr *expr, ArrayRef<Identifier> newNames,
                           bool isSubscript) {
  auto tuple = dyn_cast<TupleExpr>(expr);
  if (!tuple) {
    if (newNames[0].empty()) {
      // This is probably a conversion from a value of labeled tuple type to
      // a scalar.
      // FIXME: We want this issue to disappear completely when single-element
      // labelled tuples go away.
      if (auto tupleTy = expr->getType()->getRValueType()->getAs<TupleType>()) {
        int scalarFieldIdx = tupleTy->getElementForScalarInit();
        if (scalarFieldIdx >= 0) {
          auto &field = tupleTy->getElement(scalarFieldIdx);
          if (field.hasName()) {
            llvm::SmallString<16> str;
            str = ".";
            str += field.getName().str();
            TC.diagnose(expr->getStartLoc(),
                        diag::extra_named_single_element_tuple,
                        field.getName().str())
              .fixItInsertAfter(expr->getEndLoc(), str);
            return true;
          }
        }
      }

      // We don't know what to do with this.
      return false;
    }

    // This is a scalar-to-tuple conversion. Add the name.  We "know"
    // that we're inside a ParenExpr, because ParenExprs are required
    // by the syntax and locator resolution looks through on level of
    // them.

    // Look through the paren expression, if there is one.
    if (auto parenExpr = dyn_cast<ParenExpr>(expr))
      expr = parenExpr->getSubExpr();

    llvm::SmallString<16> str;
    str += newNames[0].str();
    str += ": ";
    TC.diagnose(expr->getStartLoc(), diag::missing_argument_labels, false,
                str.substr(0, str.size()-1), isSubscript)
      .fixItInsert(expr->getStartLoc(), str);
    return true;
  }

  // Figure out how many extraneous, missing, and wrong labels are in
  // the call.
  unsigned numExtra = 0, numMissing = 0, numWrong = 0;
  unsigned n = std::max(tuple->getNumElements(), (unsigned)newNames.size());

  llvm::SmallString<16> missingBuffer;
  llvm::SmallString<16> extraBuffer;
  for (unsigned i = 0; i != n; ++i) {
    Identifier oldName;
    if (i < tuple->getNumElements())
      oldName = tuple->getElementName(i);
    Identifier newName;
    if (i < newNames.size())
      newName = newNames[i];

    if (oldName == newName ||
        (tuple->hasTrailingClosure() && i == tuple->getNumElements()-1))
      continue;

    if (oldName.empty()) {
      ++numMissing;
      missingBuffer += newName.str();
      missingBuffer += ":";
    } else if (newName.empty()) {
      ++numExtra;
      extraBuffer += oldName.str();
      extraBuffer += ':';
    } else
      ++numWrong;
  }

  // Emit the diagnostic.
  assert(numMissing > 0 || numExtra > 0 || numWrong > 0);
  llvm::SmallString<16> haveBuffer; // note: diagOpt has references to this
  llvm::SmallString<16> expectedBuffer; // note: diagOpt has references to this
  Optional<InFlightDiagnostic> diagOpt;

  // If we had any wrong labels, or we have both missing and extra labels,
  // emit the catch-all "wrong labels" diagnostic.
  bool plural = (numMissing + numExtra + numWrong) > 1;
  if (numWrong > 0 || (numMissing > 0 && numExtra > 0)) {
    for(unsigned i = 0, n = tuple->getNumElements(); i != n; ++i) {
      auto haveName = tuple->getElementName(i);
      if (haveName.empty())
        haveBuffer += '_';
      else
        haveBuffer += haveName.str();
      haveBuffer += ':';
    }

    for (auto expected : newNames) {
      if (expected.empty())
        expectedBuffer += '_';
      else
        expectedBuffer += expected.str();
      expectedBuffer += ':';
    }

    StringRef haveStr = haveBuffer;
    StringRef expectedStr = expectedBuffer;
    diagOpt.emplace(TC.diagnose(expr->getLoc(), diag::wrong_argument_labels,
                                plural, haveStr, expectedStr, isSubscript));
  } else if (numMissing > 0) {
    StringRef missingStr = missingBuffer;
    diagOpt.emplace(TC.diagnose(expr->getLoc(), diag::missing_argument_labels,
                                plural, missingStr, isSubscript));
  } else {
    assert(numExtra > 0);
    StringRef extraStr = extraBuffer;
    diagOpt.emplace(TC.diagnose(expr->getLoc(), diag::extra_argument_labels,
                                plural, extraStr, isSubscript));
  }

  // Emit Fix-Its to correct the names.
  auto &diag = *diagOpt;
  for (unsigned i = 0, n = tuple->getNumElements(); i != n; ++i) {
    Identifier oldName = tuple->getElementName(i);
    Identifier newName;
    if (i < newNames.size())
      newName = newNames[i];

    if (oldName == newName || (i == n-1 && tuple->hasTrailingClosure()))
      continue;

    if (newName.empty()) {
      // Delete the old name.
      diag.fixItRemoveChars(tuple->getElementNameLocs()[i],
                            tuple->getElement(i)->getStartLoc());
      continue;
    }

    tok newNameKind =
      Lexer::kindOfIdentifier(newName.str(), /*inSILMode=*/false);
    bool newNameIsReserved = newNameKind != tok::identifier;
    llvm::SmallString<16> newStr;
    if (newNameIsReserved)
      newStr += "`";
    newStr += newName.str();
    if (newNameIsReserved)
      newStr += "`";

    if (oldName.empty()) {
      // Insert the name.
      newStr += ": ";
      diag.fixItInsert(tuple->getElement(i)->getStartLoc(), newStr);
      continue;
    }
    
    // Change the name.
    diag.fixItReplace(tuple->getElementNameLocs()[i], newStr);
  }

  return true;
}

// Return the precedence-yielding parent of 'expr', along with the index of
// 'expr' as the child of that parent. The precedence-yielding parent is the
// nearest ancestor of 'expr' which imposes a minimum precedence on 'expr'.
// Right now that just means skipping over TupleExpr instances that only exist
// to hold arguments to binary operators.
static std::pair<Expr *, unsigned> getPrecedenceParentAndIndex(Expr *expr,
                                                               Expr *rootExpr)
{
  auto parentMap = rootExpr->getParentMap();
  auto it = parentMap.find(expr);
  if (it == parentMap.end()) {
    return { nullptr, 0 };
  }
  Expr *parent = it->second;

  // Handle all cases where the answer isn't just going to be { parent, 0 }.
  if (auto tuple = dyn_cast<TupleExpr>(parent)) {
    // Get index of expression in tuple.
    auto tupleElems = tuple->getElements();
    auto elemIt = std::find(tupleElems.begin(), tupleElems.end(), expr);
    assert(elemIt != tupleElems.end() && "expr not found in parent TupleExpr");
    unsigned index = elemIt - tupleElems.begin();

    it = parentMap.find(parent);
    if (it != parentMap.end()) {
      Expr *gparent = it->second;

      // Was this tuple just constructed for a binop?
      if (isa<BinaryExpr>(gparent)) {
        return { gparent, index };
      }
    }

    // Must be a tuple literal, function arg list, collection, etc.
    return { parent, index };
  } else if (auto ifExpr = dyn_cast<IfExpr>(parent)) {
    unsigned index;
    if (expr == ifExpr->getCondExpr()) {
      index = 0;
    } else if (expr == ifExpr->getThenExpr()) {
      index = 1;
    } else if (expr == ifExpr->getElseExpr()) {
      index = 2;
    } else {
      llvm_unreachable("expr not found in parent IfExpr");
    }
    return { ifExpr, index };
  } else if (auto assignExpr = dyn_cast<AssignExpr>(parent)) {
    unsigned index;
    if (expr == assignExpr->getSrc()) {
      index = 0;
    } else if (expr == assignExpr->getDest()) {
      index = 1;
    } else {
      llvm_unreachable("expr not found in parent AssignExpr");
    }
    return { assignExpr, index };
  }

  return { parent, 0 };
}

// Return infix data representing the precedence of E.
// FIXME: unify this with getInfixData() in lib/Sema/TypeCheckExpr.cpp; the
// function there is meant to return infix data for expressions that have not
// yet been folded, so currently the correct behavor for this infixData() and
// that one are mutually exclusive.
static InfixData getInfixDataForFixIt(DeclContext *DC, Expr *E) {
  assert(E);
  if (isa<IfExpr>(E)) {
    return InfixData(IntrinsicPrecedences::IfExpr,
                     Associativity::Right,
                     /*assignment*/ false);
  } else if (isa<AssignExpr>(E)) {
    return InfixData(IntrinsicPrecedences::AssignExpr,
                     Associativity::Right,
                     /*assignment*/ true);
  } else if (isa<ExplicitCastExpr>(E)) {
    return InfixData(IntrinsicPrecedences::ExplicitCastExpr,
                     Associativity::None,
                     /*assignment*/ false);
  } else if (auto *binary = dyn_cast<BinaryExpr>(E)) {
    auto *fn = binary->getFn();
    if (auto *DRE = dyn_cast<DeclRefExpr>(fn)) {
      SourceFile *SF = DC->getParentSourceFile();
      Identifier name = DRE->getDecl()->getName();
      bool isCascading = DC->isCascadingContextForLookup(true);
      if (InfixOperatorDecl *op = SF->lookupInfixOperator(name, isCascading,
                                                          E->getLoc()))
        return op->getInfixData();
    } else if (auto *OO = dyn_cast<OverloadedDeclRefExpr>(fn)) {
      SourceFile *SF = DC->getParentSourceFile();
      Identifier name = OO->getDecls()[0]->getName();
      bool isCascading = DC->isCascadingContextForLookup(true);
      if (InfixOperatorDecl *op = SF->lookupInfixOperator(name, isCascading,
                                                          E->getLoc()))
        return op->getInfixData();
    }
  // These cases are /not/ in the version in TypeCheckExpr, since they never
  // appear in operator position. They are only used for determining whether
  // it is necessary to add parentheses in a fix-it.
  } else if (isa<PrefixUnaryExpr>(E)) {
    return InfixData(IntrinsicPrecedences::PrefixUnaryExpr,
                     Associativity::Left,
                     /*assignment*/ false);
  } else if (isa<OptionalTryExpr>(E)) {
    return InfixData(IntrinsicPrecedences::MinPrecedence,
                     Associativity::Left,
                     /*assignment*/ false);
  }
  
  return InfixData(IntrinsicPrecedences::MaxPrecedence,
                   Associativity::Left,
                   /*assignment*/ false);
}

// Return the minimum precedence that an expression in the place of 'expr' must
// have without needing to be surrounded by parentheses.
static unsigned char getMinPrecedenceForExpr(DeclContext *DC, Expr *expr,
                                             Expr *rootExpr) {
  Expr *parent;
  unsigned index;
  std::tie(parent, index) = getPrecedenceParentAndIndex(expr, rootExpr);
  if (!parent || isa<TupleExpr>(parent) || isa<ParenExpr>(parent)) {
    return IntrinsicPrecedences::MinPrecedence;
  } else if (isa<BinaryExpr>(parent) || isa<IfExpr>(parent) ||
             isa<AssignExpr>(parent) || isa<ExplicitCastExpr>(parent)) {
    auto infixData = getInfixDataForFixIt(DC, parent);
    unsigned result = infixData.getPrecedence();
    if (result < IntrinsicPrecedences::MaxPrecedence &&
        ((index == 0 && !infixData.isLeftAssociative()) ||
         (index > 0 && !infixData.isRightAssociative()))) {
      result++;
    }
    return result;
  } else {
    return IntrinsicPrecedences::MaxPrecedence;
  }
}

// Return true if, when replacing "<expr>" with "<expr> as T", parentheses need
// to be added around <expr> first in order to maintain the correct precedence.
static bool exprNeedsParensBeforeAddingAs(DeclContext *DC, Expr *expr) {
  return (getInfixDataForFixIt(DC, expr).getPrecedence() <
          IntrinsicPrecedences::ExplicitCastExpr);
}

// Return true if, when replacing "<expr>" with "<expr> as T", parentheses need
// to be added around the new expression in order to maintain the correct
// precedence.
static bool exprNeedsParensAfterAddingAs(DeclContext *DC, Expr *expr,
                                         Expr *rootExpr) {
  return (IntrinsicPrecedences::ExplicitCastExpr <
          getMinPrecedenceForExpr(DC, expr, rootExpr));
}

namespace {
  class ExprWalker : public ASTWalker {
    ExprRewriter &Rewriter;
    SmallVector<ClosureExpr *, 4> closuresToTypeCheck;

  public:
    ExprWalker(ExprRewriter &Rewriter) : Rewriter(Rewriter) { }

    ~ExprWalker() {
      auto &cs = Rewriter.getConstraintSystem();
      auto &tc = cs.getTypeChecker();
      for (auto *closure : closuresToTypeCheck)
        tc.typeCheckClosureBody(closure);
    }

    std::pair<bool, Expr *> walkToExprPre(Expr *expr) override {
      // For a default-value expression, do nothing.
      if (isa<DefaultValueExpr>(expr))
        return { false, expr };

      // For closures, update the parameter types and check the body.
      if (auto closure = dyn_cast<ClosureExpr>(expr)) {
        Rewriter.simplifyExprType(expr);
        auto &cs = Rewriter.getConstraintSystem();
        auto &tc = cs.getTypeChecker();

        // Coerce the pattern, in case we resolved something.
        auto fnType = closure->getType()->castTo<FunctionType>();
        Pattern *params = closure->getParams();
        TypeResolutionOptions TROptions;
        TROptions |= TR_OverrideType;
        TROptions |= TR_FromNonInferredPattern;
        TROptions |= TR_InExpression;
        TROptions |= TR_ImmediateFunctionInput;
        if (tc.coercePatternToType(params, closure, fnType->getInput(),
                                   TROptions))
          return { false, nullptr };
        closure->setParams(params);

        // If this is a single-expression closure, convert the expression
        // in the body to the result type of the closure.
        if (closure->hasSingleExpressionBody()) {
          // Enter the context of the closure when type-checking the body.
          llvm::SaveAndRestore<DeclContext *> savedDC(Rewriter.dc, closure);
          Expr *body = closure->getSingleExpressionBody()->walk(*this);
          
          if (body != closure->getSingleExpressionBody())
            closure->setSingleExpressionBody(body);
          
          if (body) {
            
            if (fnType->getResult()->isVoid() && !body->getType()->isVoid()) {
              closure = Rewriter.coerceClosureExprToVoid(closure);
            } else {
            
              body = Rewriter.coerceToType(body,
                                           fnType->getResult(),
                                           cs.getConstraintLocator(
                                             closure,
                                             ConstraintLocator::ClosureResult));
              if (!body)
                return { false, nullptr } ;

              closure->setSingleExpressionBody(body);
            }
          }
        } else {
          // For other closures, type-check the body once we've finished with
          // the expression.
          closuresToTypeCheck.push_back(closure);
        }

        tc.ClosuresWithUncomputedCaptures.push_back(closure);

        return { false, closure };
      }

      Rewriter.walkToExprPre(expr);
      return { true, expr };
    }

    Expr *walkToExprPost(Expr *expr) override {
      return Rewriter.walkToExprPost(expr);
    }

    /// \brief Ignore statements.
    std::pair<bool, Stmt *> walkToStmtPre(Stmt *stmt) override {
      return { false, stmt };
    }

    /// \brief Ignore declarations.
    bool walkToDeclPre(Decl *decl) override { return false; }
  };
}

/// Emit the fixes computed as part of the solution, returning true if we were
/// able to emit an error message, or false if none of the fixits worked out.
bool ConstraintSystem::applySolutionFixes(Expr *E, const Solution &solution) {
  bool diagnosed = false;
  for (unsigned i = 0, e = solution.Fixes.size(); i != e; ++i)
    diagnosed |= applySolutionFix(E, solution, i);

  return diagnosed;
}




/// \brief Apply the specified Fix # to this solution, producing a fixit hint
/// diagnostic for it and returning true.  If the fixit hint turned out to be
/// bogus, this returns false and doesn't emit anything.
bool ConstraintSystem::applySolutionFix(Expr *expr,
                                        const Solution &solution,
                                        unsigned fixNo) {
  auto &fix = solution.Fixes[fixNo];
  
  // Some fixes need more information from the locator itself, including
  // tweaking the locator. Deal with those now.
  ConstraintLocator *locator = fix.second;

  // Removing a nullary call to a non-function requires us to have an
  // 'ApplyFunction', which we strip.
  if (fix.first.getKind() == FixKind::RemoveNullaryCall) {
    auto anchor = locator->getAnchor();
    auto path = locator->getPath();
    if (!path.empty() &&
        path.back().getKind() == ConstraintLocator::ApplyFunction) {
      locator = getConstraintLocator(anchor, path.slice(0, path.size()-1),
                                     locator->getSummaryFlags());
    } else {
      return false;
    }
  }

  // Resolve the locator to a specific expression.
  SourceRange range;
  ConstraintLocator *resolved = simplifyLocator(*this, locator, range);

  // If we didn't manage to resolve directly to an expression, we don't
  // have a great diagnostic to give, so bail.
  if (!resolved || !resolved->getAnchor())
    return false;
  
  if (!resolved->getPath().empty()) {
    // We allow OptionalToBoolean fixes with an opened type to refer to the
    // BooleanType conformance.
    if (fix.first.getKind() == FixKind::OptionalToBoolean &&
        resolved->getPath().size() == 1 &&
        resolved->getPath()[0].getKind() == ConstraintLocator::OpenedGeneric)
      ; /* ok */
    else
      return false;
  }

  Expr *affected = resolved->getAnchor();

  switch (fix.first.getKind()) {
  case FixKind::None:
    llvm_unreachable("no-fix marker should never make it into solution");

  case FixKind::RemoveNullaryCall:
    if (auto apply = dyn_cast<ApplyExpr>(affected)) {
      auto type = solution.simplifyType(TC, apply->getFn()->getType())
                    ->getRValueObjectType();
      TC.diagnose(affected->getLoc(), diag::extra_call_nonfunction, type)
        .fixItRemove(apply->getArg()->getSourceRange());
      return true;
    }
    return false;

  case FixKind::ForceOptional: {
    const Expr *unwrapped = affected->getValueProvidingExpr();
    auto type = solution.simplifyType(TC, affected->getType())
                  ->getRValueObjectType();

    if (auto tryExpr = dyn_cast<OptionalTryExpr>(unwrapped)) {
      TC.diagnose(tryExpr->getTryLoc(), diag::missing_unwrap_optional_try,
                  type)
        .fixItReplace({tryExpr->getTryLoc(), tryExpr->getQuestionLoc()},
                      "try!");

    } else {
      auto diag = TC.diagnose(affected->getLoc(),
                              diag::missing_unwrap_optional, type);
      bool parensNeeded =
          (getInfixDataForFixIt(DC, affected).getPrecedence() <
           IntrinsicPrecedences::PostfixUnaryExpr) ||
          isa<OptionalEvaluationExpr>(affected);

      if (parensNeeded) {
        diag.fixItInsert(affected->getStartLoc(), "(")
            .fixItInsertAfter(affected->getEndLoc(), ")!");
      } else {
        diag.fixItInsertAfter(affected->getEndLoc(), "!");
      }
    }
    return true;
  }

  case FixKind::ForceDowncast: {
    auto fromType = solution.simplifyType(TC, affected->getType())
                      ->getRValueObjectType();
    Type toType = solution.simplifyType(TC,
                                        fix.first.getTypeArgument(*this));
    bool useAs = TC.isExplicitlyConvertibleTo(fromType, toType, DC);
    bool useAsBang = !useAs && TC.checkedCastMaySucceed(fromType, toType,
                                                        DC);
    if (!useAs && !useAsBang)
      return false;

    bool needsParensInside = exprNeedsParensBeforeAddingAs(DC, affected);
    bool needsParensOutside = exprNeedsParensAfterAddingAs(DC, affected,
                                                           expr);
    llvm::SmallString<2> insertBefore;
    llvm::SmallString<32> insertAfter;
    if (needsParensOutside) {
      insertBefore += "(";
    }
    if (needsParensInside) {
      insertBefore += "(";
      insertAfter += ")";
    }
    insertAfter += useAs ? " as " : " as! ";
    insertAfter += toType.getString();
    if (needsParensOutside)
      insertAfter += ")";
    
    auto diagID = useAs ? diag::missing_explicit_conversion
                        : diag::missing_forced_downcast;
    auto diag = TC.diagnose(affected->getLoc(), diagID, fromType, toType);
    if (!insertBefore.empty()) {
      diag.fixItInsert(affected->getStartLoc(), insertBefore);
    }
    diag.fixItInsertAfter(affected->getEndLoc(), insertAfter);
    return true;
  }

  case FixKind::AddressOf: {
    auto type = solution.simplifyType(TC, affected->getType())
                  ->getRValueObjectType();
    TC.diagnose(affected->getLoc(), diag::missing_address_of, type)
      .fixItInsert(affected->getStartLoc(), "&");
    return true;
  }

  case FixKind::TupleToScalar: 
  case FixKind::ScalarToTuple:
  case FixKind::RelabelCallTuple:
    return diagnoseArgumentLabelError(affected,
                                      fix.first.getRelabelTupleNames(*this),
                           /*isSubscript=*/locator->getPath().back().getKind()
                           == ConstraintLocator::SubscriptIndex);
    
  case FixKind::OptionalToBoolean: {
    // If we're implicitly trying to treat an optional type as a boolean,
    // let the user know that they should be testing for a value manually
    // instead.
    Expr *errorExpr = expr;
    StringRef prefix = "((";
    StringRef suffix = ") != nil)";
    
    // In the common case of a !x, post the error against the inner
    // expression as an == comparison.
    if (auto PUE =
        dyn_cast<PrefixUnaryExpr>(errorExpr->getSemanticsProvidingExpr())){
      bool isNot = false;
      if (auto *D = PUE->getCalledValue())
        isNot = D->getNameStr() == "!";
      else if (auto *ODR = dyn_cast<OverloadedDeclRefExpr>(PUE->getFn()))
        isNot = ODR->getDecls()[0]->getNameStr() == "!";
      
      if (isNot) {
        suffix = ") == nil)";
        errorExpr = PUE->getArg();
        
        // Check if we need the inner parentheses.
        // Technically we only need them if there's something in 'expr' with
        // lower precedence than '!=', but the code actually comes out nicer
        // in most cases with parens on anything non-trivial.
        if (errorExpr->canAppendCallParentheses()) {
          prefix = prefix.drop_back();
          suffix = suffix.drop_front();
        }
        // FIXME: The outer parentheses may be superfluous too.

        
        TC.diagnose(errorExpr->getLoc(),diag::optional_used_as_true_boolean,
                    simplifyType(errorExpr->getType())->getRValueType())
          .fixItRemove(PUE->getLoc())
          .fixItInsert(errorExpr->getStartLoc(), prefix)
          .fixItInsertAfter(errorExpr->getEndLoc(), suffix);
        return true;
      }
    }
    
    // If we can, post the fix-it to the sub-expression if it's a better
    // fit.
    if (auto ifExpr = dyn_cast<IfExpr>(errorExpr))
      errorExpr = ifExpr->getCondExpr();
    if (auto prefixUnaryExpr = dyn_cast<PrefixUnaryExpr>(errorExpr))
      errorExpr = prefixUnaryExpr->getArg();
    

    // Check if we need the inner parentheses.
    // Technically we only need them if there's something in 'expr' with
    // lower precedence than '!=', but the code actually comes out nicer
    // in most cases with parens on anything non-trivial.
    if (errorExpr->canAppendCallParentheses()) {
      prefix = prefix.drop_back();
      suffix = suffix.drop_front();
    }
    // FIXME: The outer parentheses may be superfluous too.

    TC.diagnose(errorExpr->getLoc(), diag::optional_used_as_boolean,
                simplifyType(errorExpr->getType())->getRValueType())
      .fixItInsert(errorExpr->getStartLoc(), prefix)
      .fixItInsertAfter(errorExpr->getEndLoc(), suffix);
    
    return true;
  }

  case FixKind::FromRawToInit: {
    // Chase the parent map to find the reference to 'fromRaw' and
    // the call to it. We'll need these for the Fix-It.
    UnresolvedDotExpr *fromRawRef = nullptr;
    CallExpr *fromRawCall = nullptr;
    auto parentMap = expr->getParentMap();
    Expr *current = affected;
    do {
      if (!fromRawRef) {
        // We haven't found the reference to fromRaw yet, look for it now.
        fromRawRef = dyn_cast<UnresolvedDotExpr>(current);
        if (fromRawRef && fromRawRef->getName() != TC.Context.Id_fromRaw)
          fromRawRef = nullptr;

        current = parentMap[current];
        continue;
      } 
      
      // We previously found the reference to fromRaw, so we're
      // looking for the call.
      fromRawCall = dyn_cast<CallExpr>(current);
      if (fromRawCall)
        break;
      
      current = parentMap[current];
      continue;          
    } while (current);

    if (fromRawCall) {
      TC.diagnose(fromRawRef->getNameLoc(), 
                  diag::migrate_from_raw_to_init)
        .fixItReplace(SourceRange(fromRawRef->getDotLoc(),
                                  fromRawCall->getArg()->getStartLoc()),
                      "(rawValue: ");
    } else {
      // Diagnostic without Fix-It; we couldn't find what we needed.
      TC.diagnose(affected->getLoc(), diag::migrate_from_raw_to_init);
    }
    return true;
  }

  case FixKind::ToRawToRawValue: {
    // Chase the parent map to find the reference to 'toRaw' and
    // the call to it. We'll need these for the Fix-It.
    UnresolvedDotExpr *toRawRef = nullptr;
    CallExpr *toRawCall = nullptr;
    auto parentMap = expr->getParentMap();
    Expr *current = affected;
    do {
      if (!toRawRef) {
        // We haven't found the reference to toRaw yet, look for it now.
        toRawRef = dyn_cast<UnresolvedDotExpr>(current);
        if (toRawRef && toRawRef->getName() != TC.Context.Id_toRaw)
          toRawRef = nullptr;

        current = parentMap[current];
        continue;
      } 
      
      // We previously found the reference to toRaw, so we're
      // looking for the call.
      toRawCall = dyn_cast<CallExpr>(current);
      if (toRawCall)
        break;
      
      current = parentMap[current];
      continue;          
    } while (current);

    if (toRawCall) {
      TC.diagnose(toRawRef->getNameLoc(),
                  diag::migrate_to_raw_to_raw_value)
        .fixItReplace(SourceRange(toRawRef->getNameLoc(),
                                  toRawCall->getArg()->getEndLoc()),
                      "rawValue");
    } else {
      TC.diagnose(affected->getLoc(), diag::migrate_to_raw_to_raw_value);
    }
    return true;
  }
  case FixKind::AllZerosToInit: {
    // Chase the parent map to find the reference to 'allZeros' and
    // the call to it. We'll need these for the Fix-It.
    UnresolvedDotExpr *allZerosRef = nullptr;
    auto parentMap = expr->getParentMap();
    Expr *current = affected;
    do {
      // We haven't found the reference to allZeros yet, look for it now.
      if ((allZerosRef = dyn_cast<UnresolvedDotExpr>(current))) {
        if (allZerosRef->getName().str() == "allZeros")
          break;
        allZerosRef = nullptr;
      }

      current = parentMap[current];
    } while (current);

    if (allZerosRef) {
      TC.diagnose(allZerosRef->getNameLoc(),
                  diag::migrate_from_allZeros)
        .fixItReplace(SourceRange(allZerosRef->getDotLoc(),
                                  allZerosRef->getNameLoc()),
                      "()");
    } else {
      // Diagnostic without Fix-It; we couldn't find what we needed.
      TC.diagnose(affected->getLoc(), diag::migrate_from_allZeros);
    }
    return true;
  }

  case FixKind::CoerceToCheckedCast: {
    if (auto *coerceExpr = dyn_cast<CoerceExpr>(locator->getAnchor())) {
      Expr *subExpr = coerceExpr->getSubExpr();
      auto fromType =
        solution.simplifyType(TC, subExpr->getType())->getRValueType();
      auto toType =
        solution.simplifyType(TC, coerceExpr->getCastTypeLoc().getType());
      auto castKind = TC.typeCheckCheckedCast(
                        fromType, toType, DC,
                        coerceExpr->getLoc(),
                        subExpr->getSourceRange(),
                        coerceExpr->getCastTypeLoc().getSourceRange(),
                        [&](Type commonTy) -> bool {
                          return TC.convertToType(subExpr, commonTy, DC);
                        },
                        /*suppressDiagnostics=*/ true);

      switch (castKind) {
      // Invalid cast.
      case CheckedCastKind::Unresolved:
        // Fix didn't work, let diagnoseFailureForExpr handle this.
        return false;
      case CheckedCastKind::Coercion:
        llvm_unreachable("Coercions handled in other disjunction branch");

      // Valid casts.
      case CheckedCastKind::ArrayDowncast:
      case CheckedCastKind::DictionaryDowncast:
      case CheckedCastKind::DictionaryDowncastBridged:
      case CheckedCastKind::SetDowncast:
      case CheckedCastKind::SetDowncastBridged:
      case CheckedCastKind::ValueCast:
      case CheckedCastKind::BridgeFromObjectiveC:
        TC.diagnose(coerceExpr->getLoc(), diag::missing_forced_downcast,
                    fromType, toType)
          .highlight(coerceExpr->getSourceRange())
          .fixItReplace(coerceExpr->getLoc(), "as!");
        return true;
      }
    }
    return false;
  }
  }

  // FIXME: It would be really nice to emit a follow-up note showing where
  // we got the other type information from, e.g., the parameter we're
  // initializing.
  return false;
}


/// \brief Apply a given solution to the expression, producing a fully
/// type-checked expression.
Expr *ConstraintSystem::applySolution(Solution &solution, Expr *expr,
                                      Type convertType,
                                      bool discardedExpr,
                                      bool suppressDiagnostics) {
  // If any fixes needed to be applied to arrive at this solution, resolve
  // them to specific expressions.
  if (!solution.Fixes.empty()) {
    // If we can diagnose the problem with the fixits that we've pre-assumed,
    // do so now.
    if (applySolutionFixes(expr, solution))
      return nullptr;

    // If we didn't manage to diagnose anything well, so fall back to
    // diagnosing mining the system to construct a reasonable error message.
    diagnoseFailureForExpr(expr);
    return nullptr;
  }

  ExprRewriter rewriter(*this, solution, suppressDiagnostics);
  ExprWalker walker(rewriter);

  // Apply the solution to the expression.
  auto result = expr->walk(walker);
  if (!result)
    return nullptr;

  // If we're supposed to convert the expression to some particular type,
  // do so now.
  if (convertType) {
    result = rewriter.coerceToType(result, convertType,
                                   getConstraintLocator(expr));
    if (!result)
      return nullptr;
  } else if (result->getType()->isLValueType() && !discardedExpr) {
    // We referenced an lvalue. Load it.
    result = rewriter.coerceToType(result, result->getType()->getRValueType(),
                                   getConstraintLocator(expr));
  }

  if (result)
    rewriter.finalize(result);

  return result;
}

Expr *ConstraintSystem::applySolutionShallow(const Solution &solution,
                                             Expr *expr,
                                             bool suppressDiagnostics) {
  ExprRewriter rewriter(*this, solution, suppressDiagnostics);
  rewriter.walkToExprPre(expr);
  Expr *result = rewriter.walkToExprPost(expr);
  if (result)
    rewriter.finalize(result);
  return result;
}

Expr *Solution::coerceToType(Expr *expr, Type toType,
                             ConstraintLocator *locator,
                             bool ignoreTopLevelInjection) const {
  auto &cs = getConstraintSystem();
  ExprRewriter rewriter(cs, *this, /*suppressDiagnostics=*/false);
  Expr *result = rewriter.coerceToType(expr, toType, locator);
  if (!result)
    return nullptr;

  // If we were asked to ignore top-level optional injections, mark
  // the top-level injection (if any) as "diagnosed".
  if (ignoreTopLevelInjection) {
    if (auto injection = dyn_cast<InjectIntoOptionalExpr>(
                           result->getSemanticsProvidingExpr())) {
      rewriter.DiagnosedOptionalInjections.insert(injection);
    }
  }

  rewriter.finalize(result);
  return result;
}

// Determine whether this is a variadic witness.
static bool isVariadicWitness(AbstractFunctionDecl *afd) {
  unsigned index = 0;
  if (afd->getExtensionType())
    ++index;

  auto params = afd->getBodyParamPatterns()[index];
  if (auto *tuple = dyn_cast<TuplePattern>(params)) {
    return tuple->hasAnyEllipsis();
  }

  return false;
}

static bool argumentNamesMatch(Expr *arg, ArrayRef<Identifier> names) {
  auto tupleType = arg->getType()->getAs<TupleType>();
  if (!tupleType)
    return names.size() == 1 && names[0].empty();

  if (tupleType->getNumElements() != names.size())
    return false;

  for (unsigned i = 0, n = tupleType->getNumElements(); i != n; ++i) {
    if (tupleType->getElement(i).getName() != names[i])
      return false;
  }

  return true;
}

Expr *TypeChecker::callWitness(Expr *base, DeclContext *dc,
                               ProtocolDecl *protocol,
                               ProtocolConformance *conformance,
                               DeclName name,
                               MutableArrayRef<Expr *> arguments,
                               Diag<> brokenProtocolDiag) {
  // Construct an empty constraint system and solution.
  ConstraintSystem cs(*this, dc, ConstraintSystemOptions());

  // Find the witness we need to use.
  auto type = base->getType();
  if (auto metaType = type->getAs<AnyMetatypeType>())
    type = metaType->getInstanceType();
  
  auto witness = findNamedWitnessImpl<AbstractFunctionDecl>(
                   *this, dc, type->getRValueType(), protocol,
                   name, brokenProtocolDiag);
  if (!witness)
    return nullptr;

  // Form a reference to the witness itself.
  auto locator = cs.getConstraintLocator(base);
  Type openedFullType, openedType;
  std::tie(openedFullType, openedType)
    = cs.getTypeOfMemberReference(base->getType(), witness,
                                  /*isTypeReference=*/false,
                                  /*isDynamicResult=*/false,
                                  locator);

  // Form the call argument.
  // FIXME: Standardize all callers to always provide all argument names,
  // rather than hack around this.
  Expr *arg;
  if (arguments.size() == 1 &&
      (isVariadicWitness(witness) ||
       argumentNamesMatch(arguments[0], 
                          witness->getFullName().getArgumentNames()))) {
    arg = arguments[0];
  } else {
    SmallVector<TupleTypeElt, 4> elementTypes;
    auto names = witness->getFullName().getArgumentNames();
    unsigned i = 0;
    for (auto elt : arguments) {
      Identifier name;
      if (i < names.size())
        name = names[i];

      elementTypes.push_back(TupleTypeElt(elt->getType(), name));
      ++i;
    }

    arg = TupleExpr::create(Context,
                            base->getStartLoc(),
                            arguments,
                            names,
                            { },
                            base->getEndLoc(),
                            /*hasTrailingClosure=*/false,
                            /*Implicit=*/true,
                            TupleType::get(elementTypes, Context));
  }

  // Add the conversion from the argument to the function parameter type.
  cs.addConstraint(ConstraintKind::ArgumentTupleConversion, arg->getType(),
                   openedType->castTo<FunctionType>()->getInput(),
                   cs.getConstraintLocator(arg,
                                           ConstraintLocator::ApplyArgument));

  // Solve the system.
  SmallVector<Solution, 1> solutions;
  
  // If the system failed to produce a solution, post any available diagnostics.
  if (cs.solve(solutions) || solutions.size() != 1) {
    cs.salvage(solutions, base);
    return nullptr;
  }

  Solution &solution = solutions.front();
  ExprRewriter rewriter(cs, solution, /*suppressDiagnostics=*/false);

  auto memberRef = rewriter.buildMemberRef(base, openedFullType,
                                           base->getStartLoc(),
                                           witness, base->getEndLoc(),
                                           openedType, locator, locator,
                                           /*Implicit=*/true,
                                           AccessSemantics::Ordinary,
                                           /*isDynamic=*/false);

  // Call the witness.
  ApplyExpr *apply = new (Context) CallExpr(memberRef, arg, /*Implicit=*/true);
  Expr *result = rewriter.finishApply(apply, openedType,
                                      cs.getConstraintLocator(arg));
  if (!result)
    return nullptr;

  rewriter.finalize(result);
  return result;
}

/// \brief Convert an expression via a builtin protocol.
///
/// \param solution The solution to the expression's constraint system,
/// which must have included a constraint that the expression's type
/// conforms to the give \c protocol.
/// \param expr The expression to convert.
/// \param locator The locator describing where the conversion occurs.
/// \param protocol The protocol to use for conversion.
/// \param generalName The name of the protocol method to use for the
/// conversion.
/// \param builtinName The name of the builtin method to use for the
/// last step of the conversion.
/// \param brokenProtocolDiag Diagnostic to emit if the protocol
/// definition is missing.
/// \param brokenBuiltinDiag Diagnostic to emit if the builtin definition
/// is broken.
///
/// \returns the converted expression.
static Expr *convertViaBuiltinProtocol(const Solution &solution,
                                       Expr *expr,
                                       ConstraintLocator *locator,
                                       ProtocolDecl *protocol,
                                       Identifier generalName,
                                       Identifier builtinName,
                                       Diag<> brokenProtocolDiag,
                                       Diag<> brokenBuiltinDiag) {
  auto &cs = solution.getConstraintSystem();

  // FIXME: Cache name.
  auto &tc = cs.getTypeChecker();
  auto &ctx = tc.Context;
  auto type = expr->getType();

  // Look for the builtin name. If we don't have it, we need to call the
  // general name via the witness table.
  NameLookupOptions lookupOptions = defaultMemberLookupOptions;
  if (isa<AbstractFunctionDecl>(cs.DC))
    lookupOptions |= NameLookupFlags::KnownPrivate;
  auto witnesses = tc.lookupMember(cs.DC, type->getRValueType(), builtinName,
                                   lookupOptions);
  if (!witnesses) {
    auto protocolType = protocol->getType()->
                        getAs<MetatypeType>()->getInstanceType();
    
    // Find the witness we need to use.
    ValueDecl *witness = nullptr;
    
    if (!protocolType->isEqual(type)) {
      witness = findNamedPropertyWitness(tc, cs.DC, type -> getRValueType(),
                                         protocol, generalName,
                                         brokenProtocolDiag);
    } else {
      // If the expression is already typed to the protocol, lookup the protocol
      // method directly.
      witnesses = tc.lookupMember(cs.DC, type->getRValueType(), generalName,
                                  lookupOptions);
      if (!witnesses) {
        tc.diagnose(protocol->getLoc(), brokenProtocolDiag);
        return nullptr;
      }
      witness = witnesses[0];
    }
    
    // Form a reference to this member.
    Expr *memberRef = new (ctx) MemberRefExpr(expr, expr->getStartLoc(),
                                              witness, expr->getEndLoc(),
                                              /*Implicit=*/true);
    bool failed = tc.typeCheckExpressionShallow(memberRef, cs.DC);
    if (failed) {
      // If the member reference expression failed to type check, the Expr's
      // type does not conform to the given protocol.
      tc.diagnose(expr->getLoc(),
                  diag::type_does_not_conform,
                  type,
                  protocol->getType());
      return nullptr;
    }
    expr = memberRef;

    // At this point, we must have a type with the builtin member.
    type = expr->getType();
    witnesses = tc.lookupMember(cs.DC, type->getRValueType(), builtinName,
                                lookupOptions);
    if (!witnesses) {
      tc.diagnose(protocol->getLoc(), brokenProtocolDiag);
      return nullptr;
    }
  }

  // Find the builtin method.
  if (witnesses.size() != 1) {
    tc.diagnose(protocol->getLoc(), brokenBuiltinDiag);
    return nullptr;
  }
  FuncDecl *builtinMethod = dyn_cast<FuncDecl>(witnesses[0].Decl);
  if (!builtinMethod) {
    tc.diagnose(protocol->getLoc(), brokenBuiltinDiag);
    return nullptr;

  }

  // Form a reference to the builtin method.
  Expr *memberRef = new (ctx) MemberRefExpr(expr, SourceLoc(),
                                            builtinMethod, expr->getLoc(),
                                            /*Implicit=*/true);
  bool failed = tc.typeCheckExpressionShallow(memberRef, cs.DC);
  assert(!failed && "Could not reference witness?");
  (void)failed;

  // Call the builtin method.
  Expr *arg = TupleExpr::createEmpty(ctx, expr->getStartLoc(), 
                                     expr->getEndLoc(), /*Implicit=*/true);
  expr = new (ctx) CallExpr(memberRef, arg, /*Implicit=*/true);
  failed = tc.typeCheckExpressionShallow(expr, cs.DC);
  assert(!failed && "Could not call witness?");
  (void)failed;

  return expr;
}

Expr *
Solution::convertBooleanTypeToBuiltinI1(Expr *expr, ConstraintLocator *locator) const {
  auto &tc = getConstraintSystem().getTypeChecker();

  // FIXME: Cache names.
  auto result = convertViaBuiltinProtocol(
                  *this, expr, locator,
                  tc.getProtocol(expr->getLoc(),
                                 KnownProtocolKind::BooleanType),
                  tc.Context.Id_boolValue,
                  tc.Context.Id_getBuiltinLogicValue,
                  diag::condition_broken_proto,
                  diag::broken_bool);
  if (result && !result->getType()->isBuiltinIntegerType(1)) {
    tc.diagnose(expr->getLoc(), diag::broken_bool);
    return nullptr;
  }

  return result;
}

Expr *Solution::convertOptionalToBool(Expr *expr,
                                      ConstraintLocator *locator) const {
  auto &cs = getConstraintSystem();
  auto &tc = cs.getTypeChecker();
  tc.requireOptionalIntrinsics(expr->getLoc());

  // Find the library intrinsic.
  auto &ctx = tc.Context;
  auto *fn = ctx.getDoesOptionalHaveValueAsBoolDecl(&tc, OTK_Optional);
  tc.validateDecl(fn);

  // Form a reference to the function. This library intrinsic is generic, so we
  // need to form substitutions and compute the resulting type.
  auto unwrappedOptionalType = expr->getType()->getOptionalObjectType();
  auto fnGenericParams
    = fn->getGenericSignatureOfContext()->getGenericParams();
  auto firstArchetype
    = ArchetypeBuilder::mapTypeIntoContext(fn, fnGenericParams[0])
        ->castTo<ArchetypeType>();

  Substitution sub(firstArchetype, unwrappedOptionalType, {});
  ConcreteDeclRef fnSpecRef(ctx, fn, sub);
  auto *fnRef =
      new (ctx) DeclRefExpr(fnSpecRef, SourceLoc(), /*Implicit=*/true);

  TypeSubstitutionMap subMap;
  auto genericParam = fn->getGenericSignatureOfContext()->getGenericParams()[0];
  subMap[genericParam->getCanonicalType()->castTo<SubstitutableType>()] =
      unwrappedOptionalType;
  fnRef->setType(fn->getInterfaceType().subst(
      constraintSystem->DC->getParentModule(), subMap, None));

  Expr *call = new (ctx) CallExpr(fnRef, expr, /*Implicit=*/true);

  bool failed = tc.typeCheckExpressionShallow(call, cs.DC);
  assert(!failed && "Could not call library intrinsic?");
  (void)failed;
  return call;
}

