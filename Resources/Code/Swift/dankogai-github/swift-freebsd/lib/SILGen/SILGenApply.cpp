//===--- SILGenApply.cpp - Constructs call sites for SILGen ---------------===//
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

#include "ArgumentSource.h"
#include "LValue.h"
#include "RValue.h"
#include "Scope.h"
#include "Initialization.h"
#include "SpecializedEmitter.h"
#include "Varargs.h"
#include "swift/AST/ASTContext.h"
#include "swift/AST/DiagnosticsSIL.h"
#include "swift/AST/ForeignErrorConvention.h"
#include "swift/AST/Module.h"
#include "swift/Basic/Fallthrough.h"
#include "swift/Basic/Range.h"
#include "swift/SIL/SILArgument.h"
#include "swift/SIL/PrettyStackTrace.h"

using namespace swift;
using namespace Lowering;

/// Get the method dispatch mechanism for a method.
MethodDispatch
SILGenFunction::getMethodDispatch(AbstractFunctionDecl *method) {
  // Final methods can be statically referenced.
  if (method->isFinal())
    return MethodDispatch::Static;
  // Some methods are forced to be statically dispatched.
  if (method->hasForcedStaticDispatch())
    return MethodDispatch::Static;

  // If this declaration is in a class but not marked final, then it is
  // always dynamically dispatched.
  auto dc = method->getDeclContext();
  if (isa<ClassDecl>(dc))
    return MethodDispatch::Class;

  // Class extension methods are only dynamically dispatched if they're
  // dispatched by objc_msgSend, which happens if they're foreign or dynamic.
  if (auto declaredType = dc->getDeclaredTypeInContext())
    if (declaredType->getClassOrBoundGenericClass()) {
      if (method->hasClangNode())
        return MethodDispatch::Class;
      if (auto fd = dyn_cast<FuncDecl>(method)) {
        if (fd->isAccessor() && fd->getAccessorStorageDecl()->hasClangNode())
          return MethodDispatch::Class;
      }
      if (method->getAttrs().hasAttribute<DynamicAttr>())
        return MethodDispatch::Class;
    }

  // Otherwise, it can be referenced statically.
  return MethodDispatch::Static;
}

static SILDeclRef::Loc getLocForFunctionRef(AnyFunctionRef fn) {
  if (auto afd = fn.getAbstractFunctionDecl()) {
    return afd;
  } else {
    auto closure = fn.getAbstractClosureExpr();
    assert(closure);
    return closure;
  }
}

/// Collect the captures necessary to invoke a local function into an
/// ArgumentSource.
static std::pair<ArgumentSource, CanFunctionType>
emitCapturesAsArgumentSource(SILGenFunction &gen,
                             SILLocation loc,
                             AnyFunctionRef fn) {
  SmallVector<ManagedValue, 4> captures;
  gen.emitCaptures(loc, fn, captures);
  
  // The capture array should match the explosion schema of the closure's
  // first formal argument type.
  auto info = gen.SGM.Types.getConstantInfo(SILDeclRef(getLocForFunctionRef(fn)));
  auto subs = info.getForwardingSubstitutions(gen.getASTContext());
  auto origFormalTy = info.FormalInterfaceType;
  CanFunctionType formalTy;
  if (!subs.empty()) {
    auto genericOrigFormalTy = cast<GenericFunctionType>(origFormalTy);
    auto substTy = genericOrigFormalTy
      ->substGenericArgs(gen.SGM.SwiftModule, subs)
      ->getCanonicalType();
    formalTy = cast<FunctionType>(substTy);
  } else {
    formalTy = cast<FunctionType>(origFormalTy);
  }
  RValue rv(captures, formalTy.getInput());
  return {ArgumentSource(loc, std::move(rv)), formalTy};
}

/// Retrieve the type to use for a method found via dynamic lookup.
static CanAnyFunctionType getDynamicMethodFormalType(SILGenModule &SGM,
                                                     SILValue proto,
                                                     ValueDecl *member,
                                                     SILDeclRef methodName,
                                                     Type memberType) {
  auto &ctx = SGM.getASTContext();
  CanType selfTy;
  if (member->isInstanceMember()) {
    selfTy = ctx.TheUnknownObjectType;
  } else {
    selfTy = proto.getType().getSwiftType();
  }
  auto extInfo = FunctionType::ExtInfo()
                   .withRepresentation(FunctionType::Representation::Thin);

  return CanFunctionType::get(selfTy, memberType->getCanonicalType(),
                              extInfo);
}

/// Replace the 'self' parameter in the given type.
static CanSILFunctionType
replaceSelfTypeForDynamicLookup(ASTContext &ctx,
                                CanSILFunctionType fnType,
                                CanType newSelfType,
                                SILDeclRef methodName) {
  auto oldParams = fnType->getParameters();
  SmallVector<SILParameterInfo, 4> newParams;
  newParams.append(oldParams.begin(), oldParams.end() - 1);
  newParams.push_back({newSelfType, oldParams.back().getConvention()});

  auto newResult = fnType->getResult();
  // If the method returns Self, substitute AnyObject for the result type.
  if (auto fnDecl = dyn_cast<FuncDecl>(methodName.getDecl())) {
    if (fnDecl->hasDynamicSelf()) {
      auto anyObjectTy = ctx.getProtocol(KnownProtocolKind::AnyObject)
                                  ->getDeclaredType();
      auto newResultTy
        = newResult.getType()->replaceCovariantResultType(anyObjectTy, 0);
      newResult = SILResultInfo(newResultTy->getCanonicalType(),
                                newResult.getConvention());
    }
  }

  return SILFunctionType::get(nullptr,
                              fnType->getExtInfo(),
                              fnType->getCalleeConvention(),
                              newParams,
                              newResult,
                              fnType->getOptionalErrorResult(),
                              ctx);
}

static Type getExistentialArchetype(SILValue existential) {
  CanType ty = existential.getType().getSwiftRValueType();
  if (ty->is<ArchetypeType>())
    return ty;
  return cast<ProtocolType>(ty)->getDecl()->getProtocolSelf()->getArchetype();
}

/// Retrieve the type to use for a method found via dynamic lookup.
static CanSILFunctionType getDynamicMethodLoweredType(SILGenFunction &gen,
                                                      SILValue proto,
                                                      SILDeclRef methodName) {
  auto &ctx = gen.getASTContext();

  // Determine the opaque 'self' parameter type.
  CanType selfTy;
  if (methodName.getDecl()->isInstanceMember()) {
    selfTy = getExistentialArchetype(proto)->getCanonicalType();
  } else {
    selfTy = proto.getType().getSwiftType();
  }

  // Replace the 'self' parameter type in the method type with it.
  auto methodTy = gen.SGM.getConstantType(methodName).castTo<SILFunctionType>();
  return replaceSelfTypeForDynamicLookup(ctx, methodTy, selfTy, methodName);
}

namespace {

/// Abstractly represents a callee, and knows how to emit the entry point
/// reference for a callee at any valid uncurry level.
class Callee {
public:
  enum class Kind {
    /// An indirect function value.
    IndirectValue,
    /// A direct standalone function call, referenceable by a FunctionRefInst.
    StandaloneFunction,

    VirtualMethod_First,
      /// A method call using class method dispatch.
      ClassMethod = VirtualMethod_First,
      /// A method call using super method dispatch.
      SuperMethod,
    VirtualMethod_Last = SuperMethod,

    GenericMethod_First,
      /// A method call using archetype dispatch.
      WitnessMethod = GenericMethod_First,
      /// A method call using dynamic lookup.
      DynamicMethod,
    GenericMethod_Last = DynamicMethod
  };

  const Kind kind;

  // Move, don't copy.
  Callee(const Callee &) = delete;
  Callee &operator=(const Callee &) = delete;
private:
  union {
    ManagedValue IndirectValue;
    SILDeclRef StandaloneFunction;
    struct {
      SILValue SelfValue;
      SILDeclRef MethodName;
    } Method;
  };
  ArrayRef<Substitution> Substitutions;
  CanType OrigFormalOldType;
  CanType OrigFormalInterfaceType;
  CanAnyFunctionType SubstFormalType;
  Optional<SILLocation> SpecializeLoc;
  bool HasSubstitutions = false;

  // The pointer back to the AST node that produced the callee.
  SILLocation Loc;


private:

  Callee(ManagedValue indirectValue,
         CanType origFormalType,
         CanAnyFunctionType substFormalType,
         SILLocation L)
    : kind(Kind::IndirectValue),
      IndirectValue(indirectValue),
      OrigFormalOldType(origFormalType),
      OrigFormalInterfaceType(origFormalType),
      SubstFormalType(substFormalType),
      Loc(L)
  {}

  static CanAnyFunctionType getConstantFormalType(SILGenFunction &gen,
                                                  SILValue selfValue,
                                                  SILDeclRef fn)
  SIL_FUNCTION_TYPE_DEPRECATED {
    return gen.SGM.Types.getConstantInfo(fn.atUncurryLevel(0)).FormalType;
  }

  static CanAnyFunctionType getConstantFormalInterfaceType(SILGenFunction &gen,
                                                  SILValue selfValue,
                                                  SILDeclRef fn) {
    return gen.SGM.Types.getConstantInfo(fn.atUncurryLevel(0))
             .FormalInterfaceType;
  }

  Callee(SILGenFunction &gen, SILDeclRef standaloneFunction,
         CanAnyFunctionType substFormalType,
         SILLocation l)
    : kind(Kind::StandaloneFunction), StandaloneFunction(standaloneFunction),
      OrigFormalOldType(getConstantFormalType(gen, SILValue(),
                                              standaloneFunction)),
      OrigFormalInterfaceType(getConstantFormalInterfaceType(gen, SILValue(),
                                                           standaloneFunction)),
      SubstFormalType(substFormalType),
      Loc(l)
  {
  }

  Callee(Kind methodKind,
         SILGenFunction &gen,
         SILValue selfValue,
         SILDeclRef methodName,
         CanAnyFunctionType substFormalType,
         SILLocation l)
    : kind(methodKind), Method{selfValue, methodName},
      OrigFormalOldType(getConstantFormalType(gen, selfValue, methodName)),
      OrigFormalInterfaceType(getConstantFormalInterfaceType(gen, selfValue,
                                                             methodName)),
      SubstFormalType(substFormalType),
      Loc(l)
  {
  }

  static CanArchetypeType getArchetypeForSelf(CanType selfType) {
    if (auto mt = dyn_cast<MetatypeType>(selfType)) {
      return cast<ArchetypeType>(mt.getInstanceType());
    } else {
      return cast<ArchetypeType>(selfType);
    }
  }

  /// Build a clause that looks like 'origParamType' but uses 'selfType'
  /// in place of the underlying archetype.
  static CanType buildSubstSelfType(CanType origParamType, CanType selfType,
                                    ASTContext &ctx) {
    assert(!isa<LValueType>(origParamType) && "Self can't be @lvalue");
    if (auto lv = dyn_cast<InOutType>(origParamType)) {
      selfType = buildSubstSelfType(lv.getObjectType(), selfType, ctx);
      return CanInOutType::get(selfType);
    }

    if (auto tuple = dyn_cast<TupleType>(origParamType)) {
      assert(tuple->getNumElements() == 1);
      selfType = buildSubstSelfType(tuple.getElementType(0), selfType, ctx);

      auto field = tuple->getElement(0).getWithType(selfType);
      return CanType(TupleType::get(field, ctx));
    }

    // These first two asserts will crash before they return if
    // they're actually wrong.
    assert(getArchetypeForSelf(origParamType));
    assert(getArchetypeForSelf(selfType));
    assert(isa<MetatypeType>(origParamType) == isa<MetatypeType>(selfType));
    return selfType;
  }

  CanType getWitnessMethodSelfType() const {
    CanType type = SubstFormalType.getInput();
    if (auto tuple = dyn_cast<TupleType>(type)) {
      assert(tuple->getNumElements() == 1);
      type = tuple.getElementType(0);
    }
    if (auto lv = dyn_cast<InOutType>(type)) {
      type = lv.getObjectType();
    }
    assert(getArchetypeForSelf(type));
    return type;
  }

  CanSILFunctionType getSubstFunctionType(SILGenModule &SGM,
                                          CanSILFunctionType origFnType,
                                          CanAnyFunctionType origLoweredType,
                                          unsigned uncurryLevel,
                                          Optional<SILDeclRef> constant,
                 const Optional<ForeignErrorConvention> &foreignError) const {
    if (!HasSubstitutions) return origFnType;

    assert(origLoweredType);
    auto substLoweredType =
      SGM.Types.getLoweredASTFunctionType(SubstFormalType, uncurryLevel,
                                          origLoweredType->getExtInfo(),
                                          constant);
    auto substLoweredInterfaceType =
      SGM.Types.getLoweredASTFunctionType(SubstFormalType, uncurryLevel,
                                          origLoweredType->getExtInfo(),
                                          constant);

    return SGM.Types.substFunctionType(origFnType, origLoweredType,
                                       substLoweredType,
                                       substLoweredInterfaceType,
                                       foreignError);
  }

  /// Add the 'self' clause back to the substituted formal type of
  /// this protocol method.
  void addProtocolSelfToFormalType(SILGenModule &SGM, SILDeclRef name,
                                   CanType protocolSelfType) {
    // The result types of the expressions yielding protocol values
    // (reflected in SubstFormalType) reflect an implicit level of
    // function application, including some extra polymorphic
    // substitution.
    HasSubstitutions = true;

    auto &ctx = SGM.getASTContext();

    // Add the 'self' parameter back.  We want it to look like a
    // substitution of the appropriate clause from the original type.
    auto polyFormalType = cast<PolymorphicFunctionType>(OrigFormalOldType);
    auto substSelfType =
      buildSubstSelfType(polyFormalType.getInput(), protocolSelfType, ctx);

    auto extInfo = FunctionType::ExtInfo(FunctionType::Representation::Thin,
                                         /*noreturn*/ false,
                                         /*throws*/ polyFormalType->throws());

    SubstFormalType = CanFunctionType::get(substSelfType, SubstFormalType,
                                           extInfo);
  }

  /// Add the 'self' type to the substituted function type of this
  /// dynamic callee.
  void addDynamicCalleeSelfToFormalType(SILGenModule &SGM) {
    assert(kind == Kind::DynamicMethod);

    // Drop the original self clause.
    CanType methodType = OrigFormalOldType;
    methodType = cast<AnyFunctionType>(methodType).getResult();

    // Replace it with the dynamic self type.
    OrigFormalOldType = OrigFormalInterfaceType
      = getDynamicMethodFormalType(SGM, Method.SelfValue,
                                   Method.MethodName.getDecl(),
                                   Method.MethodName, methodType);

    // Add a self clause to the substituted type.
    auto origFormalType = cast<AnyFunctionType>(OrigFormalOldType);
    auto selfType = origFormalType.getInput();
    SubstFormalType
      = CanFunctionType::get(selfType, SubstFormalType,
                             origFormalType->getExtInfo());
  }

public:

  static Callee forIndirect(ManagedValue indirectValue,
                            CanType origFormalType,
                            CanAnyFunctionType substFormalType,
                            SILLocation l) {
    return Callee(indirectValue,
                  origFormalType,
                  substFormalType,
                  l);
  }
  static Callee forDirect(SILGenFunction &gen, SILDeclRef c,
                          CanAnyFunctionType substFormalType,
                          SILLocation l) {
    return Callee(gen, c, substFormalType, l);
  }
  static Callee forClassMethod(SILGenFunction &gen, SILValue selfValue,
                               SILDeclRef name,
                               CanAnyFunctionType substFormalType,
                               SILLocation l) {
    return Callee(Kind::ClassMethod, gen, selfValue, name,
                  substFormalType, l);
  }
  static Callee forSuperMethod(SILGenFunction &gen, SILValue selfValue,
                               SILDeclRef name,
                               CanAnyFunctionType substFormalType,
                               SILLocation l) {
    return Callee(Kind::SuperMethod, gen, selfValue, name,
                  substFormalType, l);
  }
  static Callee forArchetype(SILGenFunction &gen,
                             SILValue optOpeningInstruction,
                             CanType protocolSelfType,
                             SILDeclRef name,
                             CanAnyFunctionType substFormalType,
                             SILLocation l) {
    Callee callee(Kind::WitnessMethod, gen, optOpeningInstruction, name,
                  substFormalType, l);
    callee.addProtocolSelfToFormalType(gen.SGM, name, protocolSelfType);
    return callee;
  }
  static Callee forDynamic(SILGenFunction &gen, SILValue proto,
                           SILDeclRef name, CanAnyFunctionType substFormalType,
                           SILLocation l) {
    Callee callee(Kind::DynamicMethod, gen, proto, name,
                  substFormalType, l);
    callee.addDynamicCalleeSelfToFormalType(gen.SGM);
    return callee;
  }
  Callee(Callee &&) = default;
  Callee &operator=(Callee &&) = default;

  void setSubstitutions(SILGenFunction &gen,
                        SILLocation loc,
                        ArrayRef<Substitution> newSubs,
                        unsigned callDepth) {
    // Currently generic methods of generic types are the deepest we should
    // be able to stack specializations.
    // FIXME: Generic local functions can add type parameters to arbitrary
    // depth.
    assert(callDepth < 2 && "specialization below 'self' or argument depth?!");
    assert(Substitutions.empty() && "Already have substitutions?");
    Substitutions = newSubs;

    assert(getNaturalUncurryLevel() >= callDepth
           && "specializations below uncurry level?!");
    SpecializeLoc = loc;
    HasSubstitutions = true;
  }

  CanType getOrigFormalType() const {
    return OrigFormalOldType;
  }

  CanAnyFunctionType getSubstFormalType() const {
    return SubstFormalType;
  }

  unsigned getNaturalUncurryLevel() const {
    switch (kind) {
    case Kind::IndirectValue:
      return 0;

    case Kind::StandaloneFunction:
      return StandaloneFunction.uncurryLevel;

    case Kind::ClassMethod:
    case Kind::SuperMethod:
    case Kind::WitnessMethod:
    case Kind::DynamicMethod:
      return Method.MethodName.uncurryLevel;
    }
  }

  std::tuple<ManagedValue, CanSILFunctionType,
             Optional<ForeignErrorConvention>, ApplyOptions>
  getAtUncurryLevel(SILGenFunction &gen, unsigned level) const {
    ManagedValue mv;
    ApplyOptions options = ApplyOptions::None;
    SILConstantInfo constantInfo;
    Optional<SILDeclRef> constant = None;

    switch (kind) {
    case Kind::IndirectValue:
      assert(level == 0 && "can't curry indirect function");
      mv = IndirectValue;
      assert(!HasSubstitutions);
      break;

    case Kind::StandaloneFunction: {
      assert(level <= StandaloneFunction.uncurryLevel
             && "uncurrying past natural uncurry level of standalone function");
      constant = StandaloneFunction.atUncurryLevel(level);

      // If we're currying a direct reference to a class-dispatched method,
      // make sure we emit the right set of thunks.
      if (constant->isCurried && StandaloneFunction.hasDecl())
        if (auto func = StandaloneFunction.getAbstractFunctionDecl())
          if (gen.getMethodDispatch(func) == MethodDispatch::Class)
            constant = constant->asDirectReference(true);
      
      constantInfo = gen.getConstantInfo(*constant);
      SILValue ref = gen.emitGlobalFunctionRef(Loc, *constant, constantInfo);
      mv = ManagedValue::forUnmanaged(ref);
      break;
    }
    case Kind::ClassMethod: {
      assert(level <= Method.MethodName.uncurryLevel
             && "uncurrying past natural uncurry level of method");
      constant = Method.MethodName.atUncurryLevel(level);
      constantInfo = gen.getConstantInfo(*constant);

      // If the call is curried, emit a direct call to the curry thunk.
      if (level < Method.MethodName.uncurryLevel) {
        SILValue ref = gen.emitGlobalFunctionRef(Loc, *constant, constantInfo);
        mv = ManagedValue::forUnmanaged(ref);
        break;
      }

      // Otherwise, do the dynamic dispatch inline.
      SILValue methodVal = gen.B.createClassMethod(Loc,
                                                   Method.SelfValue,
                                                   *constant,
                                                   /*volatile*/
                                                     constant->isForeign);

      mv = ManagedValue::forUnmanaged(methodVal);
      break;
    }
    case Kind::SuperMethod: {
      assert(level <= Method.MethodName.uncurryLevel
             && "uncurrying past natural uncurry level of method");
      assert(level >= 1
             && "currying 'self' of super method dispatch not yet supported");

      constant = Method.MethodName.atUncurryLevel(level);
      constantInfo = gen.getConstantInfo(*constant);
      SILValue methodVal = gen.B.createSuperMethod(Loc,
                                                   Method.SelfValue,
                                                   *constant,
                                                   constantInfo.getSILType(),
                                                   /*volatile*/
                                                     constant->isForeign);

      mv = ManagedValue::forUnmanaged(methodVal);
      break;
    }
    case Kind::WitnessMethod: {
      assert(level <= Method.MethodName.uncurryLevel
             && "uncurrying past natural uncurry level of method");
      constant = Method.MethodName.atUncurryLevel(level);
      constantInfo = gen.getConstantInfo(*constant);

      // If the call is curried, emit a direct call to the curry thunk.
      if (level < Method.MethodName.uncurryLevel) {
        SILValue ref = gen.emitGlobalFunctionRef(Loc, *constant, constantInfo);
        mv = ManagedValue::forUnmanaged(ref);
        break;
      }

      // Look up the witness for the archetype.
      auto selfType = getWitnessMethodSelfType();
      auto archetype = getArchetypeForSelf(selfType);
      // Get the openend existential value if the archetype is an opened
      // existential type.
      SILValue OpenedExistential;
      if (!archetype->getOpenedExistentialType().isNull())
        OpenedExistential = Method.SelfValue;

      SILValue fn = gen.B.createWitnessMethod(Loc,
                                  archetype,
                                  /*conformance*/ nullptr,
                                  *constant,
                                  constantInfo.getSILType(),
                                  OpenedExistential,
                                  constant->isForeign);
      mv = ManagedValue::forUnmanaged(fn);
      break;
    }
    case Kind::DynamicMethod: {
      assert(level >= 1
             && "currying 'self' of dynamic method dispatch not yet supported");
      assert(level <= Method.MethodName.uncurryLevel
             && "uncurrying past natural uncurry level of method");

      auto constant = Method.MethodName.atUncurryLevel(level);
      constantInfo = gen.getConstantInfo(constant);

      auto closureType =
        replaceSelfTypeForDynamicLookup(gen.getASTContext(),
                                constantInfo.SILFnType,
                                Method.SelfValue.getType().getSwiftRValueType(),
                                Method.MethodName);

      SILValue fn = gen.B.createDynamicMethod(Loc,
                          Method.SelfValue,
                          constant,
                          SILType::getPrimitiveObjectType(closureType),
                          /*volatile*/ constant.isForeign);
      mv = ManagedValue::forUnmanaged(fn);
      break;
    }
    }

    Optional<ForeignErrorConvention> foreignError;
    if (constant && constant->isForeign) {
      foreignError = cast<AbstractFunctionDecl>(constant->getDecl())
                       ->getForeignErrorConvention();
    }

    CanSILFunctionType substFnType =
      getSubstFunctionType(gen.SGM, mv.getType().castTo<SILFunctionType>(),
                           constantInfo.LoweredType, level, constant,
                           foreignError);

    return std::make_tuple(mv, substFnType, foreignError, options);
  }

  ArrayRef<Substitution> getSubstitutions() const {
    return Substitutions;
  }

  /// Return a specialized emission function if this is a function with a known
  /// lowering, such as a builtin, or return null if there is no specialized
  /// emitter.
  Optional<SpecializedEmitter>
  getSpecializedEmitter(SILGenModule &SGM, unsigned uncurryLevel) const {
    // Currently we have no curried known functions.
    if (uncurryLevel != 0)
      return None;

    switch (kind) {
    case Kind::StandaloneFunction: {
      return SpecializedEmitter::forDecl(SGM, StandaloneFunction);
    }
    case Kind::IndirectValue:
    case Kind::ClassMethod:
    case Kind::SuperMethod:
    case Kind::WitnessMethod:
    case Kind::DynamicMethod:
      return None;
    }
    llvm_unreachable("bad callee kind");
  }
};

/// Given that we've applied some sort of trivial transform to the
/// value of the given ManagedValue, enter a cleanup for the result if
/// the original had a cleanup.
static ManagedValue maybeEnterCleanupForTransformed(SILGenFunction &gen,
                                                    ManagedValue orig,
                                                    SILValue result) {
  if (orig.hasCleanup()) {
    orig.forwardCleanup(gen);
    return gen.emitManagedBufferWithCleanup(result);
  } else {
    return ManagedValue::forUnmanaged(result);
  }
}

static Callee prepareArchetypeCallee(SILGenFunction &gen, SILLocation loc,
                                     SILDeclRef constant,
                                     ArgumentSource &selfValue,
                                     CanAnyFunctionType substFnType,
                                     ArrayRef<Substitution> &substitutions) {
  auto fd = cast<AbstractFunctionDecl>(constant.getDecl());
  auto protocol = cast<ProtocolDecl>(fd->getDeclContext());

  // Method calls through ObjC protocols require ObjC dispatch.
  constant = constant.asForeign(protocol->isObjC());

  CanType selfTy = selfValue.getSubstRValueType();

  SILParameterInfo _selfParam;
  auto getSelfParameter = [&]() -> SILParameterInfo {
    if (_selfParam != SILParameterInfo()) return _selfParam;
    auto constantFnType = gen.SGM.Types.getConstantFunctionType(constant);
    return (_selfParam = constantFnType->getSelfParameter());
  };
  auto getSGFContextForSelf = [&]() -> SGFContext {
    return (getSelfParameter().isConsumed()
              ? SGFContext() : SGFContext::AllowGuaranteedPlusZero);
  };

  auto setSelfValueToAddress = [&](SILLocation loc, ManagedValue address) {
    assert(address.getType().isAddress());
    assert(address.getType().is<ArchetypeType>());
    auto formalTy = address.getType().getSwiftRValueType();

    if (getSelfParameter().isIndirectInOut()) {
      // Be sure not to consume the cleanup for an inout argument.
      auto selfLV = ManagedValue::forLValue(address.getValue());
      selfValue = ArgumentSource(loc,
                    LValue::forAddress(selfLV, AbstractionPattern(formalTy),
                                       formalTy));
    } else {
      selfValue = ArgumentSource(loc, RValue(address, formalTy));
    }
  };

  // If we're calling a member of a non-class-constrained protocol,
  // but our archetype refines it to be class-bound, then
  // we have to materialize the value in order to pass it indirectly.
  auto materializeSelfIfNecessary = [&] {
    // Only an instance method of a non-class protocol is ever passed
    // indirectly.
    if (!fd->isInstanceMember() ||
        protocol->requiresClass() ||
        selfValue.hasLValueType() ||
        !cast<ArchetypeType>(selfValue.getSubstRValueType())->requiresClass())
      return;

    auto selfParameter = getSelfParameter();
    assert(selfParameter.isIndirect());
    (void)selfParameter;

    SILLocation selfLoc = selfValue.getLocation();

    // Evaluate the reference into memory.
    ManagedValue address = [&]() -> ManagedValue {
      // Do so at +0 if we can.
      auto ref = std::move(selfValue)
                   .getAsSingleValue(gen, getSGFContextForSelf());

      // If we're already in memory for some reason, great.
      if (ref.getType().isAddress())
        return ref;

      // Store the reference into a temporary.
      auto temp =
        gen.emitTemporaryAllocation(selfLoc, ref.getValue().getType());
      gen.B.createStore(selfLoc, ref.getValue(), temp);

      // If we had a cleanup, create a cleanup at the new address.
      return maybeEnterCleanupForTransformed(gen, ref, temp);
    }();

    setSelfValueToAddress(selfLoc, address);
  };

  // Construct an archetype call.

  // Link back to something to create a data dependency if we have
  // an opened type.
  SILValue openingSite;
  auto archetype =
    cast<ArchetypeType>(CanType(selfTy->getRValueInstanceType()));
  if (archetype->getOpenedExistentialType()) {
    openingSite = gen.getArchetypeOpeningSite(archetype);
  }

  materializeSelfIfNecessary();

  // The protocol self is implicitly decurried.
  substFnType = cast<AnyFunctionType>(substFnType.getResult());

  return Callee::forArchetype(gen, openingSite, selfTy,
                              constant, substFnType, loc);
}

/// An ASTVisitor for building SIL function calls.
///
/// Nested ApplyExprs applied to an underlying curried function or method
/// reference are flattened into a single SIL apply to the most uncurried entry
/// point fitting the call site, avoiding pointless intermediate closure
/// construction.
class SILGenApply : public Lowering::ExprVisitor<SILGenApply> {
public:
  /// The SILGenFunction that we are emitting SIL into.
  SILGenFunction &SGF;

  /// The apply callee that abstractly represents the entry point that is being
  /// called.
  Optional<Callee> ApplyCallee;

  /// The lvalue or rvalue representing the argument source of self.
  ArgumentSource SelfParam;
  Expr *SelfApplyExpr = nullptr;
  Type SelfType;
  std::vector<ApplyExpr*> CallSites;
  Expr *SideEffect = nullptr;

  /// The depth of uncurries that we have seen.
  ///
  /// *NOTE* This counter is incremented *after* we return from visiting a call
  /// site's children. This means that it is not valid until we finish visiting
  /// the expression.
  unsigned CallDepth = 0;

  /// When visiting expressions, sometimes we need to emit self before we know
  /// what the actual callee is. In such cases, we assume that we are passing
  /// self at +0 and then after we know what the callee is, we check if the
  /// self is passed at +1. If so, we add an extra retain.
  bool AssumedPlusZeroSelf = false;

  SILGenApply(SILGenFunction &gen)
    : SGF(gen)
  {}

  void setCallee(Callee &&c) {
    assert((SelfParam ? CallDepth == 1 : CallDepth == 0)
           && "setting callee at non-zero call depth?!");
    assert(!ApplyCallee && "already set callee!");
    ApplyCallee.emplace(std::move(c));
  }

  void setSideEffect(Expr *sideEffectExpr) {
    assert(!SideEffect && "already set side effect!");
    SideEffect = sideEffectExpr;
  }

  void setSelfParam(ArgumentSource &&theSelfParam, Expr *theSelfApplyExpr) {
    assert(!SelfParam && "already set this!");
    SelfParam = std::move(theSelfParam);
    SelfApplyExpr = theSelfApplyExpr;
    SelfType = theSelfApplyExpr->getType();
    ++CallDepth;
  }
  void setSelfParam(ArgumentSource &&theSelfParam, Type selfType) {
    assert(!SelfParam && "already set this!");
    SelfParam = std::move(theSelfParam);
    SelfApplyExpr = nullptr;
    SelfType = selfType;
    ++CallDepth;
  }

  void decompose(Expr *e) {
    visit(e);
  }

  /// Get the type of the function for substitution purposes.
  ///
  /// \param otherCtorRefUsesAllocating If true, the OtherConstructorDeclRef
  /// refers to the initializing
  CanFunctionType getSubstFnType(bool otherCtorRefUsesAllocating = false) {
    // TODO: optimize this if there are no specializes in play
    auto getSiteType = [&](ApplyExpr *site, bool otherCtorRefUsesAllocating) {
      if (otherCtorRefUsesAllocating) {
        // We have a reference to an initializing constructor, but we will
        // actually be using the allocating constructor. Update the type
        // appropriately.
        // FIXME: Re-derive the type from the declaration + substitutions?
        auto ctorRef = cast<OtherConstructorDeclRefExpr>(
                         site->getFn()->getSemanticsProvidingExpr());
        auto fnType = ctorRef->getType()->castTo<FunctionType>();
        auto selfTy = MetatypeType::get(
                        fnType->getInput()->getInOutObjectType());
        return CanFunctionType::get(selfTy->getCanonicalType(),
                                    fnType->getResult()->getCanonicalType(),
                                    fnType->getExtInfo());
      }

      return cast<FunctionType>(site->getFn()->getType()->getCanonicalType());
    };

    CanFunctionType fnType;

    auto addSite = [&](ApplyExpr *site, bool otherCtorRefUsesAllocating) {
      auto siteType = getSiteType(site, otherCtorRefUsesAllocating);

      // If this is the first call site, use its formal type directly.
      if (!fnType) {
        fnType = siteType;
        return;
      }

      fnType = CanFunctionType::get(siteType.getInput(), fnType,
                                    siteType->getExtInfo());
    };

    for (auto callSite : CallSites) {
      addSite(callSite, false);
    }

    // The self application might be a DynamicMemberRefExpr.
    if (auto selfApply = dyn_cast_or_null<ApplyExpr>(SelfApplyExpr)) {
      addSite(selfApply, otherCtorRefUsesAllocating);
    }

    assert(fnType && "found no call sites?");
    return fnType;
  }

  /// Fall back to an unknown, indirect callee.
  void visitExpr(Expr *e) {
    ManagedValue fn = SGF.emitRValueAsSingleValue(e);
    auto origType = cast<AnyFunctionType>(e->getType()->getCanonicalType());
    setCallee(Callee::forIndirect(fn, origType, getSubstFnType(), e));
  }

  void visitLoadExpr(LoadExpr *e) {
    // TODO: preserve the function pointer at its original abstraction level
    ManagedValue fn = SGF.emitRValueAsSingleValue(e);
    auto origType = cast<AnyFunctionType>(e->getType()->getCanonicalType());
    setCallee(Callee::forIndirect(fn, origType, getSubstFnType(), e));
  }

  /// Add a call site to the curry.
  void visitApplyExpr(ApplyExpr *e) {
    if (e->isSuper()) {
      applySuper(e);
    } else if (applyInitDelegation(e)) {
      // Already done
    } else {
      CallSites.push_back(e);
      visit(e->getFn());
    }
    ++CallDepth;
  }

  /// Given a metatype value for the type, allocate an Objective-C
  /// object (with alloc_ref_dynamic) of that type.
  ///
  /// \returns the self object.
  ManagedValue allocateObjCObject(ManagedValue selfMeta, SILLocation loc) {
    auto metaType = selfMeta.getType().castTo<AnyMetatypeType>();
    CanType type = metaType.getInstanceType();

    // Convert to an Objective-C metatype representation, if needed.
    ManagedValue selfMetaObjC;
    if (metaType->getRepresentation() == MetatypeRepresentation::ObjC) {
      selfMetaObjC = selfMeta;
    } else {
      CanAnyMetatypeType objcMetaType;
      if (isa<MetatypeType>(metaType)) {
        objcMetaType = CanMetatypeType::get(type, MetatypeRepresentation::ObjC);
      } else {
        objcMetaType = CanExistentialMetatypeType::get(type,
                                                  MetatypeRepresentation::ObjC);
      }
      selfMetaObjC = ManagedValue(
                       SGF.B.emitThickToObjCMetatype(
                         loc, selfMeta.getValue(),
                         SGF.SGM.getLoweredType(objcMetaType)),
                       selfMeta.getCleanup());
    }

    // Allocate the object.
    return ManagedValue(SGF.B.createAllocRefDynamic(
                          loc,
                          selfMetaObjC.getValue(),
                          SGF.SGM.getLoweredType(type),
                          /*objc=*/true),
                          selfMetaObjC.getCleanup());
  }

  //
  // Known callees.
  //
  void visitDeclRefExpr(DeclRefExpr *e) {
    // If we need to perform dynamic dispatch for the given function,
    // emit class_method to do so.
    if (auto afd = dyn_cast<AbstractFunctionDecl>(e->getDecl())) {
      Optional<SILDeclRef::Kind> kind;
      bool isDynamicallyDispatched;
      bool requiresAllocRefDynamic = false;

      // Determine whether the method is dynamically dispatched.
      if (auto *proto = dyn_cast<ProtocolDecl>(afd->getDeclContext())) {
        // We have four cases to deal with here:
        //
        //  1) for a "static" / "type" method, the base is a metatype.
        //  2) for a classbound protocol, the base is a class-bound protocol rvalue,
        //     which is loadable.
        //  3) for a mutating method, the base has inout type.
        //  4) for a nonmutating method, the base is a general archetype
        //     rvalue, which is address-only.  The base is passed at +0, so it isn't
        //     consumed.
        //
        // In the last case, the AST has this call typed as being applied
        // to an rvalue, but the witness is actually expecting a pointer
        // to the +0 value in memory.  We just pass in the address since
        // archetypes are address-only.

        CanAnyFunctionType substFnType = getSubstFnType();
        assert(!CallSites.empty());
        ApplyExpr *thisCallSite = CallSites.back();
        CallSites.pop_back();

        ArgumentSource selfValue = thisCallSite->getArg();

        ArrayRef<Substitution> subs = e->getDeclRef().getSubstitutions();

        SILDeclRef::Kind kind = SILDeclRef::Kind::Func;
        if (isa<ConstructorDecl>(afd)) {
          if (proto->isObjC()) {
            SILLocation loc = thisCallSite->getArg();

            // For Objective-C initializers, we only have an initializing
            // initializer. We need to allocate the object ourselves.
            kind = SILDeclRef::Kind::Initializer;

            auto metatype = std::move(selfValue).getAsSingleValue(SGF);
            auto allocated = allocateObjCObject(metatype, loc);
            auto allocatedType = allocated.getType().getSwiftRValueType();
            selfValue = ArgumentSource(loc, RValue(allocated, allocatedType));
          } else {
            // For non-Objective-C initializers, we have an allocating
            // initializer to call.
            kind = SILDeclRef::Kind::Allocator;
          }
        }

        SILDeclRef constant = SILDeclRef(afd, kind);

        // Prepare the callee.  This can modify both selfValue and subs.
        Callee theCallee = prepareArchetypeCallee(SGF, e, constant, selfValue,
                                                  substFnType, subs);

        setSelfParam(std::move(selfValue), thisCallSite);
        setCallee(std::move(theCallee));

        // If there are substitutions, add them now.
        if (!subs.empty()) {
          ApplyCallee->setSubstitutions(SGF, e, subs, CallDepth);
        }

        return;
      }

      if (e->getAccessSemantics() != AccessSemantics::Ordinary) {
        isDynamicallyDispatched = false;
      } else {
        switch (SGF.getMethodDispatch(afd)) {
        case MethodDispatch::Class:
          isDynamicallyDispatched = true;
          break;
        case MethodDispatch::Static:
          isDynamicallyDispatched = false;
          break;
        }
      }

      if (isa<FuncDecl>(afd) && isDynamicallyDispatched) {
        kind = SILDeclRef::Kind::Func;
      } else if (auto ctor = dyn_cast<ConstructorDecl>(afd)) {
        ApplyExpr *thisCallSite = CallSites.back();
        // Required constructors are dynamically dispatched when the 'self'
        // value is not statically derived.
        if (ctor->isRequired() &&
            thisCallSite->getArg()->getType()->is<AnyMetatypeType>() &&
            !thisCallSite->getArg()->isStaticallyDerivedMetatype()) {
          if (SGF.SGM.requiresObjCDispatch(afd)) {
            // When we're performing Objective-C dispatch, we don't have an
            // allocating constructor to call. So, perform an alloc_ref_dynamic
            // and pass that along to the initializer.
            requiresAllocRefDynamic = true;
            kind = SILDeclRef::Kind::Initializer;
          } else {
            kind = SILDeclRef::Kind::Allocator;
          }
        } else {
          isDynamicallyDispatched = false;
        }
      }

      if (isDynamicallyDispatched) {
        ApplyExpr *thisCallSite = CallSites.back();
        CallSites.pop_back();

        // Emit the rvalue for self, allowing for guaranteed plus zero if we
        // have a func.
        bool AllowPlusZero = kind && *kind == SILDeclRef::Kind::Func;
        RValue self =
          SGF.emitRValue(thisCallSite->getArg(),
                         AllowPlusZero ? SGFContext::AllowGuaranteedPlusZero :
                                         SGFContext());

        // If we allowed for PlusZero and we *did* get the value back at +0,
        // then we assumed that self could be passed at +0. We will check later
        // if the actual callee passes self at +1 later when we know its actual
        // type.
        AssumedPlusZeroSelf =
          AllowPlusZero && self.peekIsPlusZeroRValueOrTrivial();

        // If we require a dynamic allocation of the object here, do so now.
        if (requiresAllocRefDynamic) {
          SILLocation loc = thisCallSite->getArg();
          auto selfValue = allocateObjCObject(
                             std::move(self).getAsSingleValue(SGF, loc),
                             loc);
          self = RValue(SGF, loc, selfValue.getType().getSwiftRValueType(),
                        selfValue);
        }

        auto selfValue = self.peekScalarValue();

        setSelfParam(ArgumentSource(thisCallSite->getArg(), std::move(self)),
                     thisCallSite);
        SILDeclRef constant(afd, kind.getValue(),
                            SILDeclRef::ConstructAtBestResilienceExpansion,
                            SILDeclRef::ConstructAtNaturalUncurryLevel,
                            SGF.SGM.requiresObjCDispatch(afd));

        setCallee(Callee::forClassMethod(SGF, selfValue,
                                         constant, getSubstFnType(), e));

        // setSelfParam bumps the callDepth, but we aren't really past the
        // 'self' call depth in this case.
        --CallDepth;

        // If there are substitutions, add them.
        if (e->getDeclRef().isSpecialized()) {
          ApplyCallee->setSubstitutions(SGF, e,
                                        e->getDeclRef().getSubstitutions(),
                                        CallDepth);
        }

        return;
      }
    }

    // If this is a direct reference to a vardecl, it must be a let constant
    // (which doesn't need to be loaded).  Just emit its value directly.
    if (auto *vd = dyn_cast<VarDecl>(e->getDecl())) {
      (void)vd;
      assert(vd->isLet() && "Direct reference to vardecl that isn't a let?");
      visitExpr(e);
      return;
    }

    SILDeclRef constant(e->getDecl(),
                        SILDeclRef::ConstructAtBestResilienceExpansion,
                        SILDeclRef::ConstructAtNaturalUncurryLevel,
                        SGF.SGM.requiresObjCDispatch(e->getDecl()));

    // Otherwise, we have a direct call.
    CanFunctionType substFnType = getSubstFnType();
    ArrayRef<Substitution> subs;
    
    // If the decl ref requires captures, emit the capture params.
    // The capture params behave like a "self" parameter as the first curry
    // level of the function implementation.
    auto afd = dyn_cast<AbstractFunctionDecl>(e->getDecl());
    if (afd) {
      if (afd->getCaptureInfo().hasLocalCaptures()) {
        assert(!e->getDeclRef().isSpecialized()
               && "generic local fns not implemented");
        
        auto captures = emitCapturesAsArgumentSource(SGF, e, afd);
        substFnType = captures.second;
        setSelfParam(std::move(captures.first), captures.second.getInput());
      }

      // FIXME: We should be checking hasLocalCaptures() on the lowered
      // captures in the constant info too, to generate more efficient
      // code for mutually recursive local functions which otherwise
      // capture no state.
      auto constantInfo = SGF.getConstantInfo(constant);

      // Forward local substitutions to a non-generic local function.
      if (afd->getParent()->isLocalContext() && !afd->getGenericParams())
        subs = constantInfo.getForwardingSubstitutions(SGF.getASTContext());
    }
    
    if (e->getDeclRef().isSpecialized()) {
      assert(subs.empty() && "nested local generics not yet supported");
      subs = e->getDeclRef().getSubstitutions();
    }
    
    setCallee(Callee::forDirect(SGF, constant, substFnType, e));
  
    // If there are substitutions, add them, always at depth 0.
    if (!subs.empty())
      ApplyCallee->setSubstitutions(SGF, e, subs, 0);
  }
  
  void visitAbstractClosureExpr(AbstractClosureExpr *e) {
    // A directly-called closure can be emitted as a direct call instead of
    // really producing a closure object.
    SILDeclRef constant(e);
    // Emit the closure function if we haven't yet.
    if (!SGF.SGM.hasFunction(constant))
      SGF.SGM.emitClosure(e);
    
    // If the closure requires captures, emit them.
    // The capture params behave like a "self" parameter as the first curry
    // level of the function implementation.
    ArrayRef<Substitution> subs;
    CanFunctionType substFnType = getSubstFnType();
    if (e->getCaptureInfo().hasLocalCaptures()) {
      auto captures = emitCapturesAsArgumentSource(SGF, e, e);
      substFnType = captures.second;
      setSelfParam(std::move(captures.first), captures.second.getInput());
    }
    
    // FIXME: We should be checking hasLocalCaptures() on the lowered
    // captures in the constant info above, to generate more efficient
    // code for mutually recursive local functions which otherwise
    // capture no state.
    auto constantInfo = SGF.getConstantInfo(constant);
    subs = constantInfo.getForwardingSubstitutions(SGF.getASTContext());

    setCallee(Callee::forDirect(SGF, constant, substFnType, e));
    // If there are substitutions, add them, always at depth 0.
    if (!subs.empty())
      ApplyCallee->setSubstitutions(SGF, e, subs, 0);
  }
  
  void visitOtherConstructorDeclRefExpr(OtherConstructorDeclRefExpr *e) {
    // FIXME: We might need to go through ObjC dispatch for references to
    // constructors imported from Clang (which won't have a direct entry point)
    // or to delegate to a designated initializer.
    setCallee(Callee::forDirect(SGF,
                SILDeclRef(e->getDecl(), SILDeclRef::Kind::Initializer),
                                getSubstFnType(), e));

    // If there are substitutions, add them.
    if (e->getDeclRef().isSpecialized())
      ApplyCallee->setSubstitutions(SGF, e, e->getDeclRef().getSubstitutions(),
                                    CallDepth);
  }
  void visitDotSyntaxBaseIgnoredExpr(DotSyntaxBaseIgnoredExpr *e) {
    setSideEffect(e->getLHS());
    visit(e->getRHS());
  }

  /// Skip over all of the 'Self'-related substitutions within the given set
  /// of substitutions.
  ArrayRef<Substitution> getNonSelfSubstitutions(ArrayRef<Substitution> subs) {
    unsigned innerIdx = 0, n = subs.size();
    for (; innerIdx != n; ++innerIdx) {
      auto archetype = subs[innerIdx].getArchetype();
      while (archetype->getParent())
        archetype = archetype->getParent();
      if (!archetype->getSelfProtocol())
        break;
    }

    return subs.slice(innerIdx);
  }

  void visitFunctionConversionExpr(FunctionConversionExpr *e) {
    // FIXME: Check whether this function conversion requires us to build a
    // thunk.
    visit(e->getSubExpr());
  }

  void visitCovariantFunctionConversionExpr(CovariantFunctionConversionExpr *e){
    // FIXME: These expressions merely adjust the result type for DynamicSelf
    // in an unchecked, ABI-compatible manner. They shouldn't prevent us form
    // forming a complete call.
    visitExpr(e);
  }

  void visitIdentityExpr(IdentityExpr *e) {
    visit(e->getSubExpr());
  }

  void applySuper(ApplyExpr *apply) {
    // Load the 'super' argument.
    Expr *arg = apply->getArg();
    ManagedValue super = SGF.emitRValueAsSingleValue(arg);

    // The callee for a super call has to be either a method or constructor.
    Expr *fn = apply->getFn();
    ArrayRef<Substitution> substitutions;
    SILDeclRef constant;
    if (auto *ctorRef = dyn_cast<OtherConstructorDeclRefExpr>(fn)) {
      constant = SILDeclRef(ctorRef->getDecl(), SILDeclRef::Kind::Initializer,
                         SILDeclRef::ConstructAtBestResilienceExpansion,
                         SILDeclRef::ConstructAtNaturalUncurryLevel,
                         SGF.SGM.requiresObjCSuperDispatch(ctorRef->getDecl()));

      if (ctorRef->getDeclRef().isSpecialized())
        substitutions = ctorRef->getDeclRef().getSubstitutions();
    } else if (auto *declRef = dyn_cast<DeclRefExpr>(fn)) {
      assert(isa<FuncDecl>(declRef->getDecl()) && "non-function super call?!");
      constant = SILDeclRef(declRef->getDecl(),
                         SILDeclRef::ConstructAtBestResilienceExpansion,
                         SILDeclRef::ConstructAtNaturalUncurryLevel,
                         SGF.SGM.requiresObjCSuperDispatch(declRef->getDecl()));

      if (declRef->getDeclRef().isSpecialized())
        substitutions = declRef->getDeclRef().getSubstitutions();
    } else
      llvm_unreachable("invalid super callee");

    CanType superFormalType = arg->getType()->getCanonicalType();
    setSelfParam(ArgumentSource(arg, RValue(SGF, apply, superFormalType, super)),
                 apply);

    SILValue superMethod;
    if (constant.isForeign) {
      SILValue Input = super.getValue();
      while (auto *UI = dyn_cast<UpcastInst>(Input))
        Input = UI->getOperand();
      // ObjC super calls require dynamic dispatch.
      setCallee(Callee::forSuperMethod(SGF, Input, constant,
                                       getSubstFnType(), fn));
    } else {
      // Native Swift super calls are direct.
      setCallee(Callee::forDirect(SGF, constant, getSubstFnType(), fn));
    }

    // If there are any substitutions for the callee, apply them now.
    if (!substitutions.empty())
      ApplyCallee->setSubstitutions(SGF, fn, substitutions, CallDepth-1);
  }

  /// Walk the given \c selfArg expression that produces the appropriate
  /// `self` for a call, applying the same transformations to the provided
  /// \c selfValue (which might be a metatype).
  ///
  /// This is used for initializer delegation, so it covers only the narrow
  /// subset of expressions used there.
  ManagedValue emitCorrespondingSelfValue(ManagedValue selfValue,
                                          Expr *selfArg) {
    while (true) {
      // Handle archetype-to-super and derived-to-base upcasts.
      if (isa<ArchetypeToSuperExpr>(selfArg) ||
          isa<DerivedToBaseExpr>(selfArg)) {
        auto ice = cast<ImplicitConversionExpr>(selfArg);
        auto resultTy = ice->getType()->getCanonicalType();

        // If the 'self' value is a metatype, update the target type
        // accordingly.
        if (auto selfMetaTy
                    = selfValue.getSwiftType()->getAs<AnyMetatypeType>()) {
          resultTy = CanMetatypeType::get(resultTy,
                                          selfMetaTy->getRepresentation());
        }
        auto loweredResultTy = SGF.getLoweredLoadableType(resultTy);
        if (loweredResultTy != selfValue.getType()) {
          auto upcast = SGF.B.createUpcast(ice,
                                           selfValue.getValue(),
                                           loweredResultTy);
          selfValue = ManagedValue(upcast, selfValue.getCleanup());
        }

        selfArg = ice->getSubExpr();
        continue;
      }

      // Skip over loads.
      if (auto load = dyn_cast<LoadExpr>(selfArg)) {
        selfArg = load->getSubExpr();
        continue;
      }

      // Skip over inout expressions.
      if (auto inout = dyn_cast<InOutExpr>(selfArg)) {
        selfArg = inout->getSubExpr();
        continue;
      }

      // Declaration references terminate the search.
      if (isa<DeclRefExpr>(selfArg))
        break;

      llvm_unreachable("unhandled conversion for metatype value");
    }

    return selfValue;
  }

  /// Try to emit the given application as initializer delegation.
  bool applyInitDelegation(ApplyExpr *expr) {
    // Dig out the constructor we're delegating to.
    Expr *fn = expr->getFn();
    auto ctorRef = dyn_cast<OtherConstructorDeclRefExpr>(
                     fn->getSemanticsProvidingExpr());
    if (!ctorRef)
      return false;

    // Determine whether we'll need to use an allocating constructor (vs. the
    // initializing constructor).
    auto nominal = ctorRef->getDecl()->getDeclContext()
                     ->getDeclaredTypeOfContext()->getAnyNominal();
    bool useAllocatingCtor;
    
    // Value types only have allocating initializers.
    if (isa<StructDecl>(nominal) || isa<EnumDecl>(nominal))
      useAllocatingCtor = true;
    // Protocols only witness allocating initializers, except for @objc
    // protocols, which only witness initializing initializers.
    else if (auto proto = dyn_cast<ProtocolDecl>(nominal)) {
      useAllocatingCtor = !proto->isObjC();
    // Factory initializers are effectively "allocating" initializers with no
    // corresponding initializing entry point.
    } else if (ctorRef->getDecl()->isFactoryInit()) {
      useAllocatingCtor = true;
    } else {
      // We've established we're in a class initializer or a protocol extension
      // initializer for a class-bound protocol, In either case, we're
      // delegating initialization, but we only have an instance in the former
      // case.
      assert(isa<ClassDecl>(nominal)
             && "some new kind of init context we haven't implemented");
      useAllocatingCtor = static_cast<bool>(SGF.AllocatorMetatype) &&
                          !ctorRef->getDecl()->isObjC();
    }

    // Load the 'self' argument.
    Expr *arg = expr->getArg();
    ManagedValue self;
    CanType selfFormalType = arg->getType()->getCanonicalType();

    // If we're using the allocating constructor, we need to pass along the
    // metatype.
    if (useAllocatingCtor) {
      selfFormalType = CanMetatypeType::get(
          selfFormalType->getInOutObjectType()->getCanonicalType());

      if (SGF.AllocatorMetatype)
        self = emitCorrespondingSelfValue(
                 ManagedValue::forUnmanaged(SGF.AllocatorMetatype),
                 arg);
      else
        self = ManagedValue::forUnmanaged(SGF.emitMetatypeOfValue(expr, arg));
    } else {
      // If we're in a protocol extension initializer, we haven't allocated
      // "self" yet at this point. Do so. Use alloc_ref_dynamic since we should
      // only ever get here in ObjC protocol extensions currently.
      if (SGF.AllocatorMetatype) {
        assert(ctorRef->getDecl()->isObjC()
               && "only expect to delegate an initializer from an allocator "
                  "in objc protocol extensions");
        
        self = allocateObjCObject(
                        ManagedValue::forUnmanaged(SGF.AllocatorMetatype), arg);

        // Perform any adjustments needed to 'self'.
        self = emitCorrespondingSelfValue(self, arg);
      } else {
        self = SGF.emitRValueAsSingleValue(arg);
      }
    }

    setSelfParam(ArgumentSource(arg, RValue(SGF, expr, selfFormalType, self)),
                 expr);

    // Determine the callee. For structs and enums, this is the allocating
    // constructor (because there is no initializing constructor). For protocol
    // default implementations, we also use the allocating constructor, because
    // that's the only thing that's witnessed. For classes,
    // this is the initializing constructor, to which we will dynamically
    // dispatch.
    if (SelfParam.getSubstRValueType()->getRValueInstanceType()->is<ArchetypeType>()
        && isa<ProtocolDecl>(ctorRef->getDecl()->getDeclContext())) {
      // Look up the witness for the constructor.
      auto constant = SILDeclRef(ctorRef->getDecl(),
                             useAllocatingCtor
                               ? SILDeclRef::Kind::Allocator
                               : SILDeclRef::Kind::Initializer,
                             SILDeclRef::ConstructAtBestResilienceExpansion,
                             SILDeclRef::ConstructAtNaturalUncurryLevel,
                             SGF.SGM.requiresObjCDispatch(ctorRef->getDecl()));
      setCallee(Callee::forArchetype(SGF, SILValue(),
                     self.getType().getSwiftRValueType(), constant,
                     cast<AnyFunctionType>(expr->getType()->getCanonicalType()),
                     expr));
    } else if (SGF.getMethodDispatch(ctorRef->getDecl())
                 == MethodDispatch::Class) {
      // Dynamic dispatch to the initializer.
      setCallee(Callee::forClassMethod(
                  SGF,
                  self.getValue(),
                  SILDeclRef(ctorRef->getDecl(),
                             useAllocatingCtor
                               ? SILDeclRef::Kind::Allocator
                               : SILDeclRef::Kind::Initializer,
                             SILDeclRef::ConstructAtBestResilienceExpansion,
                             SILDeclRef::ConstructAtNaturalUncurryLevel,
                             SGF.SGM.requiresObjCDispatch(ctorRef->getDecl())),
                  getSubstFnType(), fn));
    } else {
      // Directly call the peer constructor.
      setCallee(Callee::forDirect(SGF,
                                  SILDeclRef(ctorRef->getDecl(),
                                             useAllocatingCtor
                                               ? SILDeclRef::Kind::Allocator
                                               : SILDeclRef::Kind::Initializer,
                               SILDeclRef::ConstructAtBestResilienceExpansion),
                                  getSubstFnType(useAllocatingCtor), fn));
    }

    // Set up the substitutions, if we have any.
    if (ctorRef->getDeclRef().isSpecialized())
      ApplyCallee->setSubstitutions(SGF, fn,
                               ctorRef->getDeclRef().getSubstitutions(),
                               CallDepth-1);

    return true;
  }

  Callee getCallee() {
    assert(ApplyCallee && "did not find callee?!");
    return *std::move(ApplyCallee);
  }

  /// Ignore parentheses and implicit conversions.
  static Expr *ignoreParensAndImpConversions(Expr *expr) {
    while (true) {
      if (auto ice = dyn_cast<ImplicitConversionExpr>(expr)) {
        expr = ice->getSubExpr();
        continue;
      }

      // Simple optional-to-optional conversions.  This doesn't work
      // for the full generality of OptionalEvaluationExpr, but it
      // works given that we check the result for certain forms.
      if (auto eval = dyn_cast<OptionalEvaluationExpr>(expr)) {
        if (auto inject = dyn_cast<InjectIntoOptionalExpr>(eval->getSubExpr())) {
          if (auto bind = dyn_cast<BindOptionalExpr>(inject->getSubExpr())) {
            if (bind->getDepth() == 0)
              return bind->getSubExpr();
          }
        }
      }

      auto valueProviding = expr->getValueProvidingExpr();
      if (valueProviding != expr) {
        expr = valueProviding;
        continue;
      }

      return expr;
    }
  }

  void visitForceValueExpr(ForceValueExpr *e) {
    // If this application is a dynamic member reference that is forced to
    // succeed with the '!' operator, emit it as a direct invocation of the
    // method we found.
    if (emitForcedDynamicMemberRef(e))
      return;

    visitExpr(e);
  }

  /// If this application forces a dynamic member reference with !, emit
  /// a direct reference to the member.
  bool emitForcedDynamicMemberRef(ForceValueExpr *e) {
    // Check whether the argument is a dynamic member reference.
    auto arg = ignoreParensAndImpConversions(e->getSubExpr());

    auto openExistential = dyn_cast<OpenExistentialExpr>(arg);
    if (openExistential)
      arg = openExistential->getSubExpr();

    auto dynamicMemberRef = dyn_cast<DynamicMemberRefExpr>(arg);
    if (!dynamicMemberRef)
      return false;

    // Since we'll be collapsing this call site, make sure there's another
    // call site that will actually perform the invocation.
    if (CallSites.empty())
      return false;

    // Only @objc methods can be forced.
    auto *fd = dyn_cast<FuncDecl>(dynamicMemberRef->getMember().getDecl());
    if (!fd || !fd->isObjC())
      return false;

    // Local function that actually emits the dynamic member reference.
    auto emitDynamicMemberRef = [&] {
      // We found it. Emit the base.
      ManagedValue base =
        SGF.emitRValueAsSingleValue(dynamicMemberRef->getBase());

      setSelfParam(ArgumentSource(dynamicMemberRef->getBase(),
                                  RValue(SGF, dynamicMemberRef,
                                    base.getType().getSwiftRValueType(), base)),
                   dynamicMemberRef);

      // Determine the type of the method we referenced, by replacing the
      // class type of the 'Self' parameter with Builtin.UnknownObject.
      SILDeclRef member(fd, SILDeclRef::ConstructAtBestResilienceExpansion,
                        SILDeclRef::ConstructAtNaturalUncurryLevel,
                        /*isObjC=*/true);

      setCallee(Callee::forDynamic(SGF, base.getValue(), member,
                                   getSubstFnType(), e));
    };

    // When we have an open existential, open it and then emit the
    // member reference.
    if (openExistential) {
      SGF.emitOpenExistentialExpr(openExistential,
                                  [&](Expr*) { emitDynamicMemberRef(); });
    } else {
      emitDynamicMemberRef();
    }
    return true;
  }
};

} // end anonymous namespace

#ifndef NDEBUG
static bool areOnlyAbstractionDifferent(CanType type1, CanType type2) {
  assert(type1->isLegalSILType());
  assert(type2->isLegalSILType());

  // Exact equality is fine.
  if (type1 == type2) return true;

  // Either both types should be tuples or neither should be.
  if (auto tuple1 = dyn_cast<TupleType>(type1)) {
    auto tuple2 = dyn_cast<TupleType>(type2);
    if (!tuple2) return false;
    if (tuple1->getNumElements() != tuple2->getNumElements()) return false;
    for (auto i : indices(tuple2->getElementTypes()))
      if (!areOnlyAbstractionDifferent(tuple1.getElementType(i),
                                       tuple2.getElementType(i)))
        return false;
    return true;
  }
  if (isa<TupleType>(type2)) return false;

  // Either both types should be metatypes or neither should be.
  if (auto meta1 = dyn_cast<AnyMetatypeType>(type1)) {
    auto meta2 = dyn_cast<AnyMetatypeType>(type2);
    if (!meta2) return false;
    if (meta1.getInstanceType() != meta2.getInstanceType()) return false;
    return true;
  }

  // Either both types should be functions or neither should be.
  if (auto fn1 = dyn_cast<SILFunctionType>(type1)) {
    auto fn2 = dyn_cast<SILFunctionType>(type2);
    if (!fn2) return false;
    // TODO: maybe there are checks we can do here?
    (void) fn1; (void) fn2;
    return true;
  }
  if (isa<SILFunctionType>(type2)) return false;

  llvm_unreachable("no other types should differ by abstraction");
}
#endif

/// Given two SIL types which are representations of the same type,
/// check whether they have an abstraction difference.
static bool hasAbstractionDifference(SILFunctionTypeRepresentation rep,
                                     SILType type1, SILType type2) {
  CanType ct1 = type1.getSwiftRValueType();
  CanType ct2 = type2.getSwiftRValueType();
  assert(getSILFunctionLanguage(rep) == SILFunctionLanguage::C ||
         areOnlyAbstractionDifferent(ct1, ct2));
  (void)ct1;
  (void)ct2;

  // Assuming that we've applied the same substitutions to both types,
  // abstraction equality should equal type equality.
  return (type1 != type2);
}

/// Emit either an 'apply' or a 'try_apply', with the error branch of
/// the 'try_apply' simply branching out of all cleanups and throwing.
SILValue SILGenFunction::emitApplyWithRethrow(SILLocation loc,
                                              SILValue fn,
                                              SILType substFnType,
                                              ArrayRef<Substitution> subs,
                                              ArrayRef<SILValue> args) {
  CanSILFunctionType silFnType = substFnType.castTo<SILFunctionType>();
  SILType resultType = silFnType->getResult().getSILType();

  if (!silFnType->hasErrorResult()) {
    return B.createApply(loc, fn, substFnType, resultType, subs, args);
  }

  SILBasicBlock *errorBB = createBasicBlock();
  SILBasicBlock *normalBB = createBasicBlock();
  B.createTryApply(loc, fn, substFnType, subs, args, normalBB, errorBB);

  // Emit the rethrow logic.
  {
    B.emitBlock(errorBB);
    SILValue error =
      errorBB->createBBArg(silFnType->getErrorResult().getSILType());

    B.createBuiltin(loc, SGM.getASTContext().getIdentifier("willThrow"),
                    SGM.Types.getEmptyTupleType(), {}, {error});

    Cleanups.emitCleanupsForReturn(CleanupLocation::get(loc));
    B.createThrow(loc, error);
  }

  // Enter the normal path.
  B.emitBlock(normalBB);
  return normalBB->createBBArg(resultType);
}

/// Emit a raw apply operation, performing no additional lowering of
/// either the arguments or the result.
static SILValue emitRawApply(SILGenFunction &gen,
                             SILLocation loc,
                             ManagedValue fn,
                             ArrayRef<Substitution> subs,
                             ArrayRef<ManagedValue> args,
                             CanSILFunctionType substFnType,
                             ApplyOptions options,
                             SILValue resultAddr) {
  // Get the callee value.
  SILValue fnValue = substFnType->isCalleeConsumed()
    ? fn.forward(gen)
    : fn.getValue();

  SmallVector<SILValue, 4> argValues;

  // Add the buffer for the indirect return if needed.
  assert(bool(resultAddr) == substFnType->hasIndirectResult());
  if (substFnType->hasIndirectResult()) {
    assert(resultAddr.getType() ==
             substFnType->getIndirectResult().getSILType().getAddressType());
    argValues.push_back(resultAddr);
  }

  auto inputTypes = substFnType->getParametersWithoutIndirectResult();
  assert(inputTypes.size() == args.size());

  // Gather the arguments.
  for (auto i : indices(args)) {
    auto argValue = (inputTypes[i].isConsumed() ? args[i].forward(gen)
                                                : args[i].getValue());
#ifndef NDEBUG
    if (argValue.getType() != inputTypes[i].getSILType()) {
      auto &out = llvm::errs();
      out << "TYPE MISMATCH IN ARGUMENT " << i << " OF APPLY AT ";
      printSILLocationDescription(out, loc, gen.getASTContext());
      out << "  argument value: ";
      argValue.print(out);
      out << "  parameter type: ";
      inputTypes[i].print(out);
      out << "\n";
      abort();
    }
#endif
    argValues.push_back(argValue);
  }

  auto resultType = substFnType->getResult().getSILType();
  auto calleeType = SILType::getPrimitiveObjectType(substFnType);

  // If we don't have an error result, we can make a simple 'apply'.
  SILValue result;
  if (!substFnType->hasErrorResult()) {
    result = gen.B.createApply(loc, fnValue, calleeType,
                               resultType, subs, argValues);

  // Otherwise, we need to create a try_apply.
  } else {
    SILBasicBlock *normalBB = gen.createBasicBlock();
    result = normalBB->createBBArg(resultType);

    SILBasicBlock *errorBB =
      gen.getTryApplyErrorDest(loc, substFnType->getErrorResult(),
                               options & ApplyOptions::DoesNotThrow);

    gen.B.createTryApply(loc, fnValue, calleeType, subs, argValues,
                         normalBB, errorBB);
    gen.B.emitBlock(normalBB);
  }

  // Given any guaranteed arguments that are not being passed at +0, insert the
  // decrement here instead of at the end of scope. Guaranteed just means that
  // we guarantee the lifetime of the object for the duration of the call.
  // Be sure to use a CleanupLocation so that unreachable code diagnostics don't
  // trigger.
  for (auto i : indices(args)) {
    if (!inputTypes[i].isGuaranteed() || args[i].isPlusZeroRValueOrTrivial())
      continue;

    SILValue argValue = args[i].forward(gen);
    SILType argType = argValue.getType();
    CleanupLocation cleanupLoc = CleanupLocation::get(loc);
    if (!argType.isAddress())
      gen.getTypeLowering(argType).emitDestroyRValue(gen.B, cleanupLoc, argValue);
    else
      gen.getTypeLowering(argType).emitDestroyAddress(gen.B, cleanupLoc, argValue);
  }

  return result;
}

static std::pair<ManagedValue, ManagedValue>
emitForeignErrorArgument(SILGenFunction &gen,
                         SILLocation loc,
                         SILParameterInfo errorParameter) {
  // We assume that there's no interesting reabstraction here.
  auto errorPtrType = errorParameter.getType();

  PointerTypeKind ptrKind;
  auto errorType = CanType(errorPtrType->getAnyPointerElementType(ptrKind));
  auto &errorTL = gen.getTypeLowering(errorType);

  // Allocate a temporary.
  SILValue errorTemp =
    gen.emitTemporaryAllocation(loc, errorTL.getLoweredType());

  // Nil-initialize it.
  gen.emitInjectOptionalNothingInto(loc, errorTemp, errorTL);

  // Enter a cleanup to destroy the value there.
  auto managedErrorTemp = gen.emitManagedBufferWithCleanup(errorTemp, errorTL);

  // Create the appropriate pointer type.
  LValue lvalue = LValue::forAddress(ManagedValue::forLValue(errorTemp),
                                     AbstractionPattern(errorType),
                                     errorType);
  auto pointerValue = gen.emitLValueToPointer(loc, std::move(lvalue),
                                              errorPtrType, ptrKind,
                                              AccessKind::ReadWrite);

  return {managedErrorTemp, pointerValue};
}

/// Emit a function application, assuming that the arguments have been
/// lowered appropriately for the abstraction level but that the
/// result does need to be turned back into something matching a
/// formal type.
ManagedValue SILGenFunction::emitApply(
                            SILLocation loc,
                            ManagedValue fn,
                            ArrayRef<Substitution> subs,
                            ArrayRef<ManagedValue> args,
                            CanSILFunctionType substFnType,
                            AbstractionPattern origResultType,
                            CanType substResultType,
                            ApplyOptions options,
                            Optional<SILFunctionTypeRepresentation> overrideRep,
                      const Optional<ForeignErrorConvention> &foreignError,
                            SGFContext evalContext) {
  auto &formalResultTL = getTypeLowering(substResultType);
  auto loweredFormalResultType = formalResultTL.getLoweredType();
  auto rep = overrideRep ? *overrideRep : substFnType->getRepresentation();

  SILType actualResultType = substFnType->getSemanticResultSILType();

  // Check whether there are abstraction differences (beyond just
  // direct vs. indirect) between the lowered formal result type and
  // the actual result type we got back.  Note that this will also
  // include bridging differences.
  bool hasAbsDiffs =
    hasAbstractionDifference(rep, loweredFormalResultType,
                             actualResultType);

  // Prepare a result address if necessary.
  SILValue resultAddr;
  bool emittedIntoContext = false;
  if (substFnType->hasIndirectResult()) {
    // Get the result type with the abstraction prescribed by the
    // function we're calling.
    assert(actualResultType.isAddress());

    // The context will expect the natural representation of the
    // result type.  If there's no abstraction difference between that
    // and the actual form of the result, then we can emit directly
    // into the context.
    emittedIntoContext = !hasAbsDiffs;
    if (emittedIntoContext) {
      resultAddr = getBufferForExprResult(loc, actualResultType,
                                              evalContext);

    // Otherwise, we need a temporary of the right abstraction.
    } else {
      resultAddr =
        emitTemporaryAllocation(loc, actualResultType.getObjectType());
    }
  }

  // If the function returns an inner pointer, we'll need to lifetime-extend
  // the 'self' parameter.
  SILValue lifetimeExtendedSelf;
  if (substFnType->getResult().getConvention()
        == ResultConvention::UnownedInnerPointer) {
    auto selfMV = args.back();
    lifetimeExtendedSelf = selfMV.getValue();

    switch (substFnType->getParameters().back().getConvention()) {
    case ParameterConvention::Direct_Owned:
      // If the callee will consume the 'self' parameter, let's retain it so we
      // can keep it alive.
      B.emitRetainValueOperation(loc, lifetimeExtendedSelf);
      break;
    case ParameterConvention::Direct_Guaranteed:
    case ParameterConvention::Direct_Unowned:
      // We'll manually manage the argument's lifetime after the
      // call. Disable its cleanup, forcing a copy if it was emitted +0.
      if (selfMV.hasCleanup()) {
        selfMV.forwardCleanup(*this);
      } else {
        lifetimeExtendedSelf = selfMV.copyUnmanaged(*this, loc).forward(*this);
      }
      break;

    // If self is already deallocating, self does not need to be retained or
    // released since the deallocating bit has been set.
    case ParameterConvention::Direct_Deallocating:
      break;

    case ParameterConvention::Indirect_In_Guaranteed:
    case ParameterConvention::Indirect_In:
    case ParameterConvention::Indirect_Inout:
    case ParameterConvention::Indirect_Out:
      // We may need to support this at some point, but currently only imported
      // objc methods are returns_inner_pointer.
      llvm_unreachable("indirect self argument to method that"
                       " returns_inner_pointer?!");
    }
  }

  // If there's an foreign error parameter, fill it in.
  Optional<WritebackScope> errorTempWriteback;
  ManagedValue errorTemp;
  if (foreignError) {
    // Error-temporary emission may need writeback.
    errorTempWriteback.emplace(*this);

    auto errorParamIndex = foreignError->getErrorParameterIndex();
    auto errorParam =
      substFnType->getParametersWithoutIndirectResult()[errorParamIndex];

    // This is pretty evil.
    auto &errorArgSlot = const_cast<ManagedValue&>(args[errorParamIndex]);

    std::tie(errorTemp, errorArgSlot)
      = emitForeignErrorArgument(*this, loc, errorParam);
  }

  // Emit the raw application.
  SILValue scalarResult = emitRawApply(*this, loc, fn, subs, args,
                                       substFnType, options, resultAddr);

  // If we emitted into the eval context, then it's because there was
  // no abstraction difference *or* bridging to do.  But our caller
  // might not expect to get a result indirectly.
  if (emittedIntoContext) {
    assert(substFnType->hasIndirectResult());
    assert(!hasAbsDiffs);
    assert(!foreignError);
    auto managedBuffer =
      manageBufferForExprResult(resultAddr, formalResultTL, evalContext);

    // managedBuffer will be null here to indicate that we satisfied
    // the evalContext.  If so, we're done.
    // We are also done if the expected type is address-only.
    if (managedBuffer.isInContext() || formalResultTL.isAddressOnly())
      return managedBuffer;

    // Otherwise, deactivate the cleanup we just entered; we're about
    // to take from the address.
    resultAddr = managedBuffer.forward(*this);
  }

  // Get the type lowering for the actual result representation.
  // This is very likely to be the same as that for the formal result.
  auto &actualResultTL
    = (actualResultType == loweredFormalResultType
         ? formalResultTL : getTypeLowering(actualResultType));

  // If the expected result is an address, manage the result address.
  if (formalResultTL.isAddressOnly()) {
    assert(resultAddr);
    assert(!foreignError);
    auto managedActualResult =
      emitManagedBufferWithCleanup(resultAddr, actualResultTL);

    if (!hasAbsDiffs) return managedActualResult;
    return emitOrigToSubstValue(loc, managedActualResult, origResultType,
                                substResultType, evalContext);
  }

  // Okay, we want a scalar result.
  assert(!actualResultTL.isAddressOnly() &&
         "actual result is address-only when formal result is not?");

  ManagedValue managedScalar;

  // If we got an indirect result, emit a take out of the result address.
  if (substFnType->hasIndirectResult()) {
    assert(!foreignError);
    managedScalar = emitLoad(loc, resultAddr, actualResultTL,
                             SGFContext(), IsTake);

  // Otherwise, manage the direct result.
  } else {
    switch (substFnType->getResult().getConvention()) {
    case ResultConvention::Owned:
      // Already retained.
      break;

    case ResultConvention::Autoreleased:
      // Autoreleased. Retain using retain_autoreleased.
      B.createStrongRetainAutoreleased(loc, scalarResult);
      break;

    case ResultConvention::UnownedInnerPointer:
      // Autorelease the 'self' value to lifetime-extend it.
      assert(lifetimeExtendedSelf.isValid()
             && "did not save lifetime-extended self param");
      B.createAutoreleaseValue(loc, lifetimeExtendedSelf);
      SWIFT_FALLTHROUGH;

    case ResultConvention::Unowned:
      // Unretained. Retain the value.
      actualResultTL.emitRetainValue(B, loc, scalarResult);
      break;
    }

    managedScalar = emitManagedRValueWithCleanup(scalarResult,
                                                 actualResultTL);
  }

  // If there was a foreign error convention, consider it.
  if (foreignError) {
    // Force immediate writeback to the error temporary.
    errorTempWriteback.reset();

    bool doesNotThrow = (options & ApplyOptions::DoesNotThrow);
    managedScalar = emitForeignErrorCheck(loc, managedScalar, errorTemp,
                                          doesNotThrow, *foreignError);
  }

  // Fast path: no abstraction differences or bridging.
  if (!hasAbsDiffs) return managedScalar;

  // Remove abstraction differences.
  if (!origResultType.isExactType(substResultType)) {
    managedScalar = emitOrigToSubstValue(loc, managedScalar,
                                         origResultType,
                                         substResultType);
  }

  // Convert the result to a native value.
  return emitBridgedToNativeValue(loc, managedScalar, rep,
                                  substResultType);
}

ManagedValue SILGenFunction::emitMonomorphicApply(SILLocation loc,
                                                  ManagedValue fn,
                                                  ArrayRef<ManagedValue> args,
                                                  CanType resultType,
                                                  ApplyOptions options,
                           Optional<SILFunctionTypeRepresentation> overrideRep,
                     const Optional<ForeignErrorConvention> &foreignError){
  auto fnType = fn.getType().castTo<SILFunctionType>();
  assert(!fnType->isPolymorphic());
  return emitApply(loc, fn, {}, args, fnType,
                   AbstractionPattern(resultType), resultType,
                   options, overrideRep, foreignError, SGFContext());
}

/// Count the number of SILParameterInfos that are needed in order to
/// pass the given argument.
static unsigned getFlattenedValueCount(AbstractionPattern origType,
                                       CanType substType) {
  // The count is always 1 unless the substituted type is a tuple.
  auto substTuple = dyn_cast<TupleType>(substType);
  if (!substTuple) return 1;

  // If the original type is opaque and the substituted type is
  // materializable, the count is 1 anyway.
  if (origType.isOpaque() && substTuple->isMaterializable())
    return 1;

  // Otherwise, add up the elements.
  unsigned count = 0;
  for (auto i : indices(substTuple.getElementTypes())) {
    count += getFlattenedValueCount(origType.getTupleElementType(i),
                                    substTuple.getElementType(i));
  }
  return count;
}

static AbstractionPattern claimNextParamClause(AbstractionPattern &type) {
  auto result = type.getFunctionInputType();
  type = type.getFunctionResultType();
  return result;
}

static CanType claimNextParamClause(CanAnyFunctionType &type) {
  auto result = type.getInput();
  type = dyn_cast<AnyFunctionType>(type.getResult());
  return result;
}

using InOutArgument = std::pair<LValue, SILLocation>;

/// Begin all the formal accesses for a set of inout arguments.
static void beginInOutFormalAccesses(SILGenFunction &gen,
                                     MutableArrayRef<InOutArgument> inoutArgs,
                         MutableArrayRef<SmallVector<ManagedValue, 4>> args) {
  assert(!inoutArgs.empty());

  SmallVector<std::pair<SILValue, SILLocation>, 4> emittedInoutArgs;
  auto inoutNext = inoutArgs.begin();

  // The assumption we make is that 'args' and 'inoutArgs' were built
  // up in parallel, with empty spots being dropped into 'args'
  // wherever there's an inout argument to insert.
  //
  // Note that this also begins the formal accesses in evaluation order.
  for (auto &siteArgs : args) {
    for (ManagedValue &siteArg : siteArgs) {
      if (siteArg) continue;

      LValue &inoutArg = inoutNext->first;
      SILLocation loc = inoutNext->second;
      ManagedValue address = gen.emitAddressOfLValue(loc, std::move(inoutArg),
                                                     AccessKind::ReadWrite);
      siteArg = address;
      emittedInoutArgs.push_back({address.getValue(), loc});

      if (++inoutNext == inoutArgs.end())
        goto done;
    }
  }

  llvm_unreachable("ran out of null arguments before we ran out of inouts");

 done:

  // Check to see if we have multiple inout arguments which obviously
  // alias.  Note that we could do this in a later SILDiagnostics pass
  // as well: this would be stronger (more equivalences exposed) but
  // would have worse source location information.
  for (auto i = emittedInoutArgs.begin(), e = emittedInoutArgs.end();
         i != e; ++i) {
    for (auto j = emittedInoutArgs.begin(); j != i; ++j) {
      // TODO: This uses exact SILValue equivalence to detect aliases,
      // we could do something stronger here to catch other obvious cases.
      if (i->first != j->first) continue;

      gen.SGM.diagnose(i->second, diag::inout_argument_alias)
        .highlight(i->second.getSourceRange());
      gen.SGM.diagnose(j->second, diag::previous_inout_alias)
        .highlight(j->second.getSourceRange());
    }
  }
}

/// Given a scalar value, materialize it into memory with the
/// exact same level of cleanup it had before.
static ManagedValue emitMaterializeIntoTemporary(SILGenFunction &gen,
                                                 SILLocation loc,
                                                 ManagedValue object) {
  auto temporary = gen.emitTemporaryAllocation(loc, object.getType());
  bool hadCleanup = object.hasCleanup();
  gen.B.createStore(loc, object.forward(gen), temporary);

  // The temporary memory is +0 if the value was.
  if (hadCleanup) {
    return ManagedValue(temporary, gen.enterDestroyCleanup(temporary));
  } else {
    return ManagedValue::forUnmanaged(temporary);
  }
}

namespace {
  /// A destination for an argument other than just "onto to the end
  /// of the arguments lists".
  ///
  /// This allows us to re-use the argument expression emitter for
  /// some weird cases, like a shuffled tuple where some of the
  /// arguments are going into a varargs array.
  struct ArgSpecialDest {
    VarargsInfo *SharedInfo;
    unsigned Index;
    CleanupHandle Cleanup;

    ArgSpecialDest() : SharedInfo(nullptr) {}
    explicit ArgSpecialDest(VarargsInfo &info, unsigned index)
      : SharedInfo(&info), Index(index) {}

    // Reference semantics: need to preserve the cleanup handle.
    ArgSpecialDest(const ArgSpecialDest &) = delete;
    ArgSpecialDest &operator=(const ArgSpecialDest &) = delete;
    ArgSpecialDest(ArgSpecialDest &&other)
      : SharedInfo(other.SharedInfo), Index(other.Index),
        Cleanup(other.Cleanup) {
      other.SharedInfo = nullptr;
    }
    ArgSpecialDest &operator=(ArgSpecialDest &&other) {
      assert(!isValid() && "overwriting valid special destination!");
      SharedInfo = other.SharedInfo;
      Index = other.Index;
      Cleanup = other.Cleanup;
      other.SharedInfo = nullptr;
      return *this;
    }

    ~ArgSpecialDest() {
      assert(!isValid() && "failed to deactivate special dest");
    }

    /// Is this a valid special destination?
    ///
    /// Most of the time, most arguments don't have special
    /// destinations, and making an array of Optional<Special special
    /// destinations has t
    bool isValid() const { return SharedInfo != nullptr; }

    /// Fill this special destination with a value.
    void fill(SILGenFunction &gen, ArgumentSource &&arg,
              AbstractionPattern _unused_origType,
              SILType loweredSubstParamType) {
      assert(isValid() && "filling an invalid destination");

      SILLocation loc = arg.getLocation();
      auto destAddr = SharedInfo->getBaseAddress();
      if (Index != 0) {
        SILValue index = gen.B.createIntegerLiteral(loc,
                    SILType::getBuiltinWordType(gen.getASTContext()), Index);
        destAddr = gen.B.createIndexAddr(loc, destAddr, index);
      }

      assert(destAddr.getType() == loweredSubstParamType.getAddressType());

      auto &destTL = SharedInfo->getBaseTypeLowering();
      Cleanup = gen.enterDormantTemporaryCleanup(destAddr, destTL);

      TemporaryInitialization init(destAddr, Cleanup);
      std::move(arg).forwardInto(gen, SharedInfo->getBaseAbstractionPattern(),
                                 &init, destTL);
    }

    /// Deactive this special destination.  Must always be called
    /// before destruction.
    void deactivate(SILGenFunction &gen) {
      assert(isValid() && "deactivating an invalid destination");
      if (Cleanup.isValid())
        gen.Cleanups.forwardCleanup(Cleanup);
      SharedInfo = nullptr;
    }
  };

  using ArgSpecialDestArray = MutableArrayRef<ArgSpecialDest>;

  class ArgEmitter {
    SILGenFunction &SGF;
    SILFunctionTypeRepresentation Rep;
    const Optional<ForeignErrorConvention> &ForeignError;
    ArrayRef<SILParameterInfo> ParamInfos;
    SmallVectorImpl<ManagedValue> &Args;

    /// Track any inout arguments that are emitted.  Each corresponds
    /// in order to a "hole" (a null value) in Args.
    SmallVectorImpl<InOutArgument> &InOutArguments;

    Optional<ArgSpecialDestArray> SpecialDests;
  public:
    ArgEmitter(SILGenFunction &SGF, SILFunctionTypeRepresentation Rep,
               ArrayRef<SILParameterInfo> paramInfos,
               SmallVectorImpl<ManagedValue> &args,
               SmallVectorImpl<InOutArgument> &inoutArgs,
               const Optional<ForeignErrorConvention> &foreignError,
               Optional<ArgSpecialDestArray> specialDests = None)
      : SGF(SGF), Rep(Rep), ForeignError(foreignError), ParamInfos(paramInfos),
        Args(args), InOutArguments(inoutArgs), SpecialDests(specialDests) {
      assert(!specialDests || specialDests->size() == paramInfos.size());
    }

    void emitTopLevel(ArgumentSource &&arg, AbstractionPattern origParamType) {
      emit(std::move(arg), origParamType);
      maybeEmitForeignErrorArgument();
    }

  private:
    void emit(ArgumentSource &&arg, AbstractionPattern origParamType) {
      // If it was a tuple in the original type, the parameters will
      // have been exploded.
      if (origParamType.isTuple()) {
        emitExpanded(std::move(arg), origParamType);
        return;
      }

      auto substArgType = arg.getSubstType();

      // Otherwise, if the substituted type is a tuple, then we should
      // emit the tuple in its most general form, because there's a
      // substitution of an opaque archetype to a tuple or function
      // type in play.  The most general convention is generally to
      // pass the entire tuple indirectly, but if it's not
      // materializable, the convention is actually to break it up
      // into materializable chunks.  See the comment in SILType.cpp.
      if (isUnmaterializableTupleType(substArgType)) {
        assert(origParamType.isOpaque());
        emitExpanded(std::move(arg), origParamType);
        return;
      }

      // Okay, everything else will be passed as a single value, one
      // way or another.

      // Adjust for the foreign-error argument if necessary.
      maybeEmitForeignErrorArgument();

      // The substituted parameter type.  Might be different from the
      // substituted argument type by abstraction and/or bridging.
      SILParameterInfo param = claimNextParameter();
      ArgSpecialDest *specialDest = claimNextSpecialDest();

      // Make sure we use the same value category for these so that we
      // can hereafter just use simple equality checks to test for
      // abstraction.
      SILType loweredSubstArgType = SGF.getLoweredType(substArgType);
      SILType loweredSubstParamType =
        SILType::getPrimitiveType(param.getType(),
                                  loweredSubstArgType.getCategory());

      // If the caller takes the argument indirectly, the argument has an
      // inout type.
      if (param.isIndirectInOut()) {
        assert(!specialDest);
        assert(isa<InOutType>(substArgType));
        emitInOut(std::move(arg), loweredSubstArgType, loweredSubstParamType,
                  origParamType, substArgType);
        return;
      }

      // If the original type is passed indirectly, copy to memory if
      // it's not already there.  (Note that this potentially includes
      // conventions which pass indirectly without transferring
      // ownership, like Itanium C++.)
      if (param.isIndirect()) {
        assert(!param.isIndirectResult());
        if (specialDest) {
          emitIndirectInto(std::move(arg), origParamType,
                           loweredSubstParamType, *specialDest);
          Args.push_back(ManagedValue::forInContext());
        } else {
          auto value = emitIndirect(std::move(arg), loweredSubstArgType,
                                    origParamType, param);
          Args.push_back(value);
        }
        return;
      }

      // Okay, if the original parameter is passed directly, then we
      // just need to handle abstraction differences and bridging.
      assert(!specialDest);
      emitDirect(std::move(arg), loweredSubstArgType, origParamType, param);
    }

    SILParameterInfo claimNextParameter() {
      assert(!ParamInfos.empty());
      auto param = ParamInfos.front();
      ParamInfos = ParamInfos.slice(1);
      return param;
    }

    /// Claim the next destination, returning a null pointer if there
    /// is no special destination.
    ArgSpecialDest *claimNextSpecialDest() {
      if (!SpecialDests) return nullptr;
      assert(!SpecialDests->empty());
      auto dest = &SpecialDests->front();
      SpecialDests = SpecialDests->slice(1);
      return (dest->isValid() ? dest : nullptr);
    }

    bool isUnmaterializableTupleType(CanType type) {
      if (auto tuple = dyn_cast<TupleType>(type))
        if (!tuple->isMaterializable())
          return true;
      return false;
    }

    /// Emit an argument as an expanded tuple.
    void emitExpanded(ArgumentSource &&arg, AbstractionPattern origParamType) {
      CanTupleType substArgType = cast<TupleType>(arg.getSubstType());

      // The original type isn't necessarily a tuple.
      assert(origParamType.matchesTuple(substArgType));

      assert(!arg.isLValue() && "argument is l-value but parameter is tuple?");

      // If we're working with an r-value, just expand it out and emit
      // all the elements individually.
      if (arg.isRValue()) {
        auto loc = arg.getKnownRValueLocation();
        SmallVector<RValue, 4> elts;
        std::move(arg).asKnownRValue().extractElements(elts);
        for (auto i : indices(substArgType.getElementTypes())) {
          emit({ loc, std::move(elts[i]) },
               origParamType.getTupleElementType(i));
        }
        return;
      }

      // Otherwise, we're working with an expression.
      Expr *e = std::move(arg).asKnownExpr();
      e = e->getSemanticsProvidingExpr();

      // If the source expression is a tuple literal, we can break it
      // up directly.
      if (auto tuple = dyn_cast<TupleExpr>(e)) {
        for (auto i : indices(tuple->getElements())) {
          emit(tuple->getElement(i),
               origParamType.getTupleElementType(i));
        }
        return;
      }

      if (auto shuffle = dyn_cast<TupleShuffleExpr>(e)) {
        emitShuffle(shuffle, origParamType);
        return;
      }

      // Fall back to the r-value case.
      emitExpanded({ e, SGF.emitRValue(e) }, origParamType);
    }

    void emitShuffle(Expr *inner,
                     Expr *outer,
                     ArrayRef<TupleTypeElt> innerElts,
                     ConcreteDeclRef defaultArgsOwner,
                     ArrayRef<Expr*> callerDefaultArgs,
                     ArrayRef<int> elementMapping,
                     ArrayRef<unsigned> variadicArgs,
                     Type varargsArrayType,
                     AbstractionPattern origParamType);

    void emitShuffle(TupleShuffleExpr *shuffle, AbstractionPattern origType);

    ManagedValue emitIndirect(ArgumentSource &&arg,
                              SILType loweredSubstArgType,
                              AbstractionPattern origParamType,
                              SILParameterInfo param) {
      auto contexts = getRValueEmissionContexts(loweredSubstArgType, param);

      // If no abstraction is required, try to honor the emission contexts.
      if (loweredSubstArgType.getSwiftRValueType() == param.getType()) {
        auto loc = arg.getLocation();
        ManagedValue result =
          std::move(arg).getAsSingleValue(SGF, contexts.ForEmission);

        // If it's already in memory, great.
        if (result.getType().isAddress()) {
          return result;

        // Otherwise, put it there.
        } else {
          return emitMaterializeIntoTemporary(SGF, loc, result);
        }
      }

      // Otherwise, simultaneously emit and reabstract.
      return std::move(arg).materialize(SGF, origParamType, param.getSILType());
    }

    void emitIndirectInto(ArgumentSource &&arg,
                          AbstractionPattern origType,
                          SILType loweredSubstParamType,
                          ArgSpecialDest &dest) {
      dest.fill(SGF, std::move(arg), origType, loweredSubstParamType);
    }

    void emitInOut(ArgumentSource &&arg,
                   SILType loweredSubstArgType, SILType loweredSubstParamType,
                   AbstractionPattern origType, CanType substType) {
      SILLocation loc = arg.getLocation();

      LValue lv = [&]{
        // If the argument is already lowered to an LValue, it must be the
        // receiver of a self argument, which will be the first inout.
        if (arg.isLValue()) {
          return std::move(arg).asKnownLValue();

        // This is logically wrong, but propagating l-values within
        // RValues is hard to avoid in custom argument-emission code
        // without making ArgumentSource capable of holding mixed
        // RValue/LValue tuples.  (materializeForSet has to do this,
        // for one.)  The onus is on the caller to ensure that formal
        // access semantics are honored.
        } else if (arg.isRValue()) {
          auto address = std::move(arg).asKnownRValue()
            .getAsSingleValue(SGF, arg.getKnownRValueLocation());
          assert(address.isLValue());
          auto substObjectType = cast<InOutType>(substType).getObjectType();
          return LValue::forAddress(address,
                                    AbstractionPattern(substObjectType),
                                    substObjectType);
        } else {
          auto *e = cast<InOutExpr>(std::move(arg).asKnownExpr()->
                                    getSemanticsProvidingExpr());
          return SGF.emitLValue(e->getSubExpr(), AccessKind::ReadWrite);
        }
      }();

      if (hasAbstractionDifference(Rep, loweredSubstParamType,
                                   loweredSubstArgType)) {
        AbstractionPattern origObjectType = origType.transformType(
          [](CanType type)->CanType {
            return CanType(type->getInOutObjectType());
          });
        lv.addSubstToOrigComponent(origObjectType, loweredSubstParamType);
      }

      // Leave an empty space in the ManagedValue sequence and
      // remember that we had an inout argument.
      InOutArguments.push_back({std::move(lv), loc});
      Args.push_back(ManagedValue());
      return;
    }

    void emitDirect(ArgumentSource &&arg, SILType loweredSubstArgType,
                    AbstractionPattern origParamType,
                    SILParameterInfo param) {
      auto contexts = getRValueEmissionContexts(loweredSubstArgType, param);
      if (arg.isRValue()) {
        emitDirect(arg.getKnownRValueLocation(), std::move(arg).asKnownRValue(),
                   origParamType, param, contexts.ForReabstraction);
      } else {
        Expr *e = std::move(arg).asKnownExpr();
        emitDirect(e, SGF.emitRValue(e, contexts.ForEmission),
                   origParamType, param, contexts.ForReabstraction);
      }
    }

    void emitDirect(SILLocation loc, RValue &&arg,
                    AbstractionPattern origParamType,
                    SILParameterInfo param, SGFContext ctxt) {
      auto value = std::move(arg).getScalarValue();
      switch (getSILFunctionLanguage(Rep)) {
      case SILFunctionLanguage::Swift:
        value = SGF.emitSubstToOrigValue(loc, value, origParamType,
                                         arg.getType(), ctxt);
        break;
      case SILFunctionLanguage::C:
        value = SGF.emitNativeToBridgedValue(loc, value, Rep, origParamType,
                                             arg.getType(), param.getType());
        break;
      }
      Args.push_back(value);
    }

    void maybeEmitForeignErrorArgument() {
      if (!ForeignError ||
          ForeignError->getErrorParameterIndex() != Args.size())
        return;

      SILParameterInfo param = claimNextParameter();
      ArgSpecialDest *specialDest = claimNextSpecialDest();

      assert(param.getConvention() == ParameterConvention::Direct_Unowned);
      assert(!specialDest && "special dest for error argument?");
      (void) param; (void) specialDest;

      // Leave a placeholder in the position.
      Args.push_back(ManagedValue::forInContext());
    }

    struct EmissionContexts {
      /// The context for emitting the r-value.
      SGFContext ForEmission;
      /// The context for reabstracting the r-value.
      SGFContext ForReabstraction;
    };
    static EmissionContexts getRValueEmissionContexts(SILType loweredArgType,
                                                      SILParameterInfo param) {
      // If the parameter is consumed, we have to emit at +1.
      if (param.isConsumed()) {
        return { SGFContext(), SGFContext() };
      }

      // Otherwise, we can emit the final value at +0 (but only with a
      // guarantee that the value will survive).
      //
      // TODO: we can pass at +0 (immediate) to an unowned parameter
      // if we know that there will be no arbitrary side-effects
      // between now and the call.
      SGFContext finalContext = SGFContext::AllowGuaranteedPlusZero;

      // If the r-value doesn't require reabstraction, the final context
      // is the emission context.
      if (loweredArgType.getSwiftRValueType() == param.getType()) {
        return { finalContext, SGFContext() };
      }

      // Otherwise, the final context is the reabstraction context.
      return { SGFContext(), finalContext };
    }
  };
}

void ArgEmitter::emitShuffle(Expr *inner,
                             Expr *outer,
                             ArrayRef<TupleTypeElt> innerElts,
                             ConcreteDeclRef defaultArgsOwner,
                             ArrayRef<Expr*> callerDefaultArgs,
                             ArrayRef<int> elementMapping,
                             ArrayRef<unsigned> variadicArgs,
                             Type varargsArrayType,
                             AbstractionPattern origParamType) {
  auto outerTuple = cast<TupleType>(outer->getType()->getCanonicalType());
  CanType canVarargsArrayType;
  if (varargsArrayType)
    canVarargsArrayType = varargsArrayType->getCanonicalType();

  // We could support dest addrs here, but it can't actually happen
  // with the current limitations on default arguments in tuples.
  assert(!SpecialDests && "shuffle nested within varargs expansion?");

  struct ElementExtent {
    /// The parameters which go into this tuple element.
    /// This is set in the first pass.
    ArrayRef<SILParameterInfo> Params;
    /// The destination index, if any.
    /// This is set in the first pass.
    unsigned DestIndex : 30;
    unsigned HasDestIndex : 1;
#ifndef NDEBUG
    unsigned Used : 1;
#endif
    /// The arguments which feed this tuple element.
    /// This is set in the second pass.
    ArrayRef<ManagedValue> Args;
    /// The inout arguments which feed this tuple element.
    /// This is set in the second pass.
    MutableArrayRef<InOutArgument> InOutArgs;

    ElementExtent() : HasDestIndex(false)
#ifndef NDEBUG
                    , Used(false)
#endif
    {}
  };

  // The original parameter type.
  SmallVector<AbstractionPattern, 8>
    origInnerElts(innerElts.size(), AbstractionPattern::getInvalid());
  AbstractionPattern innerOrigParamType = AbstractionPattern::getInvalid();
  // Flattened inner parameter sequence.
  SmallVector<SILParameterInfo, 8> innerParams;
  // Extents of the inner elements.
  SmallVector<ElementExtent, 8> innerExtents(innerElts.size());

  Optional<VarargsInfo> varargsInfo;
  SILParameterInfo variadicParamInfo; // innerExtents will point at this
  Optional<SmallVector<ArgSpecialDest, 8>> innerSpecialDests;

  // First, construct an abstraction pattern and parameter sequence
  // which we can use to emit the inner tuple.
  {
    unsigned nextParamIndex = 0;
    for (unsigned outerIndex : indices(outerTuple.getElementTypes())) {
      CanType substEltType = outerTuple.getElementType(outerIndex);
      AbstractionPattern origEltType =
        origParamType.getTupleElementType(outerIndex);
      unsigned numParams = getFlattenedValueCount(origEltType, substEltType);

      // Skip the foreign-error parameter.
      assert((!ForeignError ||
              ForeignError->getErrorParameterIndex() <= nextParamIndex ||
              ForeignError->getErrorParameterIndex() >= nextParamIndex + numParams)
             && "error parameter falls within shuffled range?");
      if (numParams && // Don't skip it twice if there's an empty tuple.
          ForeignError &&
          ForeignError->getErrorParameterIndex() == nextParamIndex) {
        nextParamIndex++;
      }

      // Grab the parameter infos corresponding to this tuple element
      // (but don't drop them from ParamInfos yet).
      auto eltParams = ParamInfos.slice(nextParamIndex, numParams);
      nextParamIndex += numParams;

      int innerIndex = elementMapping[outerIndex];
      if (innerIndex >= 0) {
#ifndef NDEBUG
        assert(!innerExtents[innerIndex].Used && "using element twice");
        innerExtents[innerIndex].Used = true;
#endif
        innerExtents[innerIndex].Params = eltParams;
        origInnerElts[innerIndex] = origEltType;
      } else if (innerIndex == TupleShuffleExpr::Variadic) {
        auto &varargsField = outerTuple->getElement(outerIndex);
        assert(varargsField.isVararg());
        assert(!varargsInfo.hasValue() && "already had varargs entry?");

        CanType varargsEltType = CanType(varargsField.getVarargBaseTy());
        unsigned numVarargs = variadicArgs.size();
        assert(canVarargsArrayType == substEltType);

        // Create the array value.
        varargsInfo.emplace(emitBeginVarargs(SGF, outer, varargsEltType,
                                             canVarargsArrayType, numVarargs));

        // If we have any varargs, we'll need to actually initialize
        // the array buffer.
        if (numVarargs) {
          // For this, we'll need special destinations.
          assert(!innerSpecialDests);
          innerSpecialDests.emplace();

          // Prepare the variadic "arguments" as single +1 indirect
          // parameters with the array's desired abstraction pattern.
          // The vararg element type should be materializable, and the
          // abstraction pattern should be opaque, so ArgEmitter's
          // lowering should always generate exactly one "argument"
          // per element even if the substituted element type is a tuple.
          variadicParamInfo =
            SILParameterInfo(varargsInfo->getBaseTypeLowering()
                               .getLoweredType().getSwiftRValueType(),
                             ParameterConvention::Indirect_In);

          unsigned i = 0;
          for (unsigned innerIndex : variadicArgs) {
            // Find out where the next varargs element is coming from.
            assert(innerIndex >= 0 && "special source for varargs element??");
#ifndef NDEBUG
            assert(!innerExtents[innerIndex].Used && "using element twice");
            innerExtents[innerIndex].Used = true;
#endif

            // Set the destination index.
            innerExtents[innerIndex].HasDestIndex = true;
            innerExtents[innerIndex].DestIndex = i++;

            // Use the singleton param info we prepared before.
            innerExtents[innerIndex].Params = variadicParamInfo;

            // Propagate the element abstraction pattern.
            origInnerElts[innerIndex] =
              varargsInfo->getBaseAbstractionPattern();
          }
        }
      }
    }

    // The inner abstraction pattern is opaque if we started with an
    // opaque pattern; otherwise, it's a tuple of the de-shuffled
    // tuple elements.
    innerOrigParamType = origParamType;
    if (!origParamType.isOpaque()) {
      // That "tuple" might not actually be a tuple.
      if (innerElts.size() == 1 && !innerElts[0].hasName()) {
        innerOrigParamType = origInnerElts[0];
      } else {
        innerOrigParamType = AbstractionPattern::getTuple(origInnerElts);
      }
    }

    // Flatten the parameters from innerExtents into innerParams, and
    // fill out varargsAddrs if necessary.
    for (auto &extent : innerExtents) {
      assert(extent.Used && "didn't use all the inner tuple elements!");
      innerParams.append(extent.Params.begin(), extent.Params.end());

      // Fill in the special destinations array.
      if (innerSpecialDests) {
        // Use the saved index if applicable.
        if (extent.HasDestIndex) {
          assert(extent.Params.size() == 1);
          innerSpecialDests->push_back(
                               ArgSpecialDest(*varargsInfo, extent.DestIndex));

        // Otherwise, fill in with the appropriate number of invalid
        // special dests.
        } else {
          // ArgSpecialDest isn't copyable, so we can't just use append.
          for (auto &p : extent.Params) {
            (void) p;
            innerSpecialDests->push_back(ArgSpecialDest());
          }
        }
      }
    }
  }

  // Emit the inner expression.
  SmallVector<ManagedValue, 8> innerArgs;
  SmallVector<InOutArgument, 2> innerInOutArgs;
  ArgEmitter(SGF, Rep, innerParams, innerArgs, innerInOutArgs,
             /*foreign error*/ None,
             (innerSpecialDests ? ArgSpecialDestArray(*innerSpecialDests)
                                : Optional<ArgSpecialDestArray>()))
    .emitTopLevel(ArgumentSource(inner), innerOrigParamType);

  // Make a second pass to split the inner arguments correctly.
  {
    ArrayRef<ManagedValue> nextArgs = innerArgs;
    MutableArrayRef<InOutArgument> nextInOutArgs = innerInOutArgs;
    for (auto &extent : innerExtents) {
      auto length = extent.Params.size();

      // Claim the next N inner args for this inner argument.
      extent.Args = nextArgs.slice(0, length);
      nextArgs = nextArgs.slice(length);

      // Claim the correct number of inout arguments as well.
      unsigned numInOut = 0;
      for (auto arg : extent.Args) {
        assert(!arg.isInContext() || extent.HasDestIndex);
        if (!arg) numInOut++;
      }
      extent.InOutArgs = nextInOutArgs.slice(0, numInOut);
      nextInOutArgs = nextInOutArgs.slice(numInOut);
    }

    assert(nextArgs.empty() && "didn't claim all args");
    assert(nextInOutArgs.empty() && "didn't claim all inout args");
  }

  // Make a final pass to emit default arguments and move things into
  // the outer arguments lists.
  unsigned nextCallerDefaultArg = 0;
  for (unsigned outerIndex = 0, e = outerTuple->getNumElements();
         outerIndex != e; ++outerIndex) {
    // If this comes from an inner element, move the appropriate
    // inner element values over.
    int innerIndex = elementMapping[outerIndex];
    if (innerIndex >= 0) {
      auto &extent = innerExtents[innerIndex];
      auto numArgs = extent.Args.size();

      maybeEmitForeignErrorArgument();

      // Drop N parameters off of ParamInfos.
      ParamInfos = ParamInfos.slice(numArgs);

      // Move the appropriate inner arguments over as outer arguments.
      Args.append(extent.Args.begin(), extent.Args.end());
      for (auto &inoutArg : extent.InOutArgs)
        InOutArguments.push_back(std::move(inoutArg));

    // If this is default initialization, call the default argument
    // generator.
    } else if (innerIndex == TupleShuffleExpr::DefaultInitialize) {
      // Otherwise, emit the default initializer, then map that as a
      // default argument.
      CanType eltType = outerTuple.getElementType(outerIndex);
      auto origType = origParamType.getTupleElementType(outerIndex);
      ManagedValue value =
        SGF.emitApplyOfDefaultArgGenerator(outer, defaultArgsOwner,
                                           outerIndex, eltType, origType);
      emit(ArgumentSource(outer, RValue(SGF, outer, eltType, value)),
           origType);

     // If this is caller default initialization, generate the
     // appropriate value.
    } else if (innerIndex == TupleShuffleExpr::CallerDefaultInitialize) {
      auto arg = callerDefaultArgs[nextCallerDefaultArg++];
      emit(ArgumentSource(arg), origParamType.getTupleElementType(outerIndex));

    // If we're supposed to create a varargs array with the rest, do so.
    } else if (innerIndex == TupleShuffleExpr::Variadic) {
      auto &varargsField = outerTuple->getElement(outerIndex);
      assert(varargsField.isVararg() &&
             "Cannot initialize nonvariadic element");
      assert(varargsInfo.hasValue());
      (void) varargsField;

      // We've successfully built the varargs array; deactivate all
      // the special destinations.
      if (innerSpecialDests) {
        for (auto &dest : *innerSpecialDests) {
          if (dest.isValid())
            dest.deactivate(SGF);
        }
      }

      CanType eltType = outerTuple.getElementType(outerIndex);
      ManagedValue varargs = emitEndVarargs(SGF, outer, std::move(*varargsInfo));
      emit(ArgumentSource(outer, RValue(SGF, outer, eltType, varargs)),
           origParamType.getTupleElementType(outerIndex));

    // That's the last special case defined so far.
    } else {
      llvm_unreachable("unexpected special case in tuple shuffle!");
    }
  }
}

void ArgEmitter::emitShuffle(TupleShuffleExpr *E,
                             AbstractionPattern origParamType) {
  ArrayRef<TupleTypeElt> srcElts;
  TupleTypeElt singletonSrcElt;
  if (E->isSourceScalar()) {
    singletonSrcElt = E->getSubExpr()->getType()->getCanonicalType();
    srcElts = singletonSrcElt;
  } else {
    srcElts = cast<TupleType>(E->getSubExpr()->getType()->getCanonicalType())
                     ->getElements();
  }
  emitShuffle(E->getSubExpr(), E, srcElts,
              E->getDefaultArgsOwner(),
              E->getCallerDefaultArgs(),
              E->getElementMapping(),
              E->getVariadicArgs(),
              E->getVarargsArrayTypeOrNull(),
              origParamType);
}

namespace {
  /// A structure for conveniently claiming sets of uncurried parameters.
  struct ParamLowering {
    ArrayRef<SILParameterInfo> Params;
    SILFunctionTypeRepresentation Rep;

    ParamLowering(CanSILFunctionType fnType)
      : Params(fnType->getParametersWithoutIndirectResult()),
        Rep(fnType->getRepresentation()) {}

    ArrayRef<SILParameterInfo>
    claimParams(AbstractionPattern origParamType, CanType substParamType,
                const Optional<ForeignErrorConvention> &foreignError) {
      unsigned count = getFlattenedValueCount(origParamType, substParamType);
      if (foreignError) count++;
      assert(count <= Params.size());
      auto result = Params.slice(Params.size() - count, count);
      Params = Params.slice(0, Params.size() - count);
      return result;
    }

    ~ParamLowering() {
      assert(Params.empty() && "didn't consume all the parameters");
    }
  };

  class CallSite {
  public:
    SILLocation Loc;
    CanType SubstResultType;

  private:
    ArgumentSource ArgValue;
    bool Throws;

  public:
    CallSite(ApplyExpr *apply)
      : Loc(apply), SubstResultType(apply->getType()->getCanonicalType()),
        ArgValue(apply->getArg()), Throws(apply->throws()) {
    }

    CallSite(SILLocation loc, ArgumentSource &&value,
             CanType resultType, bool throws)
      : Loc(loc), SubstResultType(resultType),
        ArgValue(std::move(value)), Throws(throws) {
    }

    CallSite(SILLocation loc, ArgumentSource &&value,
             CanAnyFunctionType fnType)
      : CallSite(loc, std::move(value), fnType.getResult(), fnType->throws()) {
    }

    /// Return the substituted, unlowered AST type of the argument.
    CanType getSubstArgType() const {
      return ArgValue.getSubstType();
    }

    /// Return the substituted, unlowered AST type of the result of
    /// this application.
    CanType getSubstResultType() const {
      return SubstResultType;
    }

    bool throws() const { return Throws; }

    void emit(SILGenFunction &gen, AbstractionPattern origParamType,
              ParamLowering &lowering, SmallVectorImpl<ManagedValue> &args,
              SmallVectorImpl<InOutArgument> &inoutArgs,
              const Optional<ForeignErrorConvention> &foreignError) && {
      auto params = lowering.claimParams(origParamType, getSubstArgType(),
                                         foreignError);

      ArgEmitter emitter(gen, lowering.Rep, params, args, inoutArgs,
                         foreignError);
      emitter.emitTopLevel(std::move(ArgValue), origParamType);
    }

    ArgumentSource &&forward() && {
      return std::move(ArgValue);
    }

    /// Returns true if the argument of this value is a single valued RValue
    /// that is passed either at plus zero or is trivial.
    bool isArgPlusZeroOrTrivialRValue() {
      if (!ArgValue.isRValue())
        return false;
      return ArgValue.peekRValue().peekIsPlusZeroRValueOrTrivial();
    }

    /// If callsite has an argument that is a plus zero or trivial rvalue, emit
    /// a retain so that the argument is at PlusOne.
    void convertToPlusOneFromPlusZero(SILGenFunction &gen) {
      assert(isArgPlusZeroOrTrivialRValue() && "Must have a plus zero or "
             "trivial rvalue as an argument.");
      SILValue ArgSILValue = ArgValue.peekRValue().peekScalarValue();
      SILType ArgTy = ArgSILValue.getType();

      // If we are trivial, there is no difference in between +1 and +0 since
      // a trivial object is not reference counted.
      if (ArgTy.isTrivial(gen.SGM.M))
        return;

      // Grab the SILLocation and the new managed value.
      SILLocation ArgLoc = ArgValue.getKnownRValueLocation();
      ManagedValue ArgManagedValue = gen.emitManagedRetain(ArgLoc, ArgSILValue);

      // Ok now we make our transformation. First set ArgValue to a used albeit
      // invalid, empty ArgumentSource.
      ArgValue = ArgumentSource();

      // Reassign ArgValue.
      RValue NewRValue = RValue(gen, ArgLoc, ArgTy.getSwiftRValueType(),
                                 ArgManagedValue);
      ArgValue = std::move(ArgumentSource(ArgLoc, std::move(NewRValue)));
    }
  };

  class CallEmission {
    SILGenFunction &gen;

    std::vector<CallSite> uncurriedSites;
    std::vector<CallSite> extraSites;
    Callee callee;
    WritebackScope InitialWritebackScope;
    unsigned uncurries;
    bool applied;
    bool AssumedPlusZeroSelf;

  public:
    CallEmission(SILGenFunction &gen, Callee &&callee,
                 WritebackScope &&writebackScope,
                 bool assumedPlusZeroSelf = false)
      : gen(gen),
        callee(std::move(callee)),
        InitialWritebackScope(std::move(writebackScope)),
        uncurries(callee.getNaturalUncurryLevel() + 1),
        applied(false),
        AssumedPlusZeroSelf(assumedPlusZeroSelf)
    {}

    void addCallSite(CallSite &&site) {
      assert(!applied && "already applied!");

      // Append to the main argument list if we have uncurry levels remaining.
      if (uncurries > 0) {
        --uncurries;
        uncurriedSites.push_back(std::move(site));
        return;
      }

      // Otherwise, apply these arguments to the result of the previous call.
      extraSites.push_back(std::move(site));
    }

    template<typename...T>
    void addCallSite(T &&...args) {
      addCallSite(CallSite{std::forward<T>(args)...});
    }

    /// If we assumed that self was being passed at +0 before we knew what the
    /// final uncurried level of the callee was, but given the final uncurried
    /// level of the callee, we are actually passing self at +1, add in a retain
    /// of self.
    void convertSelfToPlusOneFromPlusZero() {
      // Self is always the first callsite.
      if (!uncurriedSites[0].isArgPlusZeroOrTrivialRValue())
        return;

      // Insert an invalid ArgumentSource into uncurriedSites[0] so it is.
      uncurriedSites[0].convertToPlusOneFromPlusZero(gen);
    }

    ManagedValue apply(SGFContext C = SGFContext()) {
      assert(!applied && "already applied!");

      applied = true;

      // Get the callee value at the needed uncurry level.
      unsigned uncurryLevel = callee.getNaturalUncurryLevel() - uncurries;

      // Get either the specialized emitter for a known function, or the
      // function value for a normal callee.

      // Check for a specialized emitter.
      Optional<SpecializedEmitter> specializedEmitter =
        callee.getSpecializedEmitter(gen.SGM, uncurryLevel);

      CanSILFunctionType substFnType;
      ManagedValue mv;
      Optional<ForeignErrorConvention> foreignError;
      ApplyOptions initialOptions = ApplyOptions::None;

      AbstractionPattern origFormalType(callee.getOrigFormalType());
      CanAnyFunctionType formalType = callee.getSubstFormalType();

      if (specializedEmitter) {
        // We want to emit the arguments as fully-substituted values
        // because that's what the specialized emitters expect.
        origFormalType = AbstractionPattern(formalType);
        substFnType = gen.getLoweredType(formalType, uncurryLevel)
          .castTo<SILFunctionType>();
      } else {
        std::tie(mv, substFnType, foreignError, initialOptions) =
          callee.getAtUncurryLevel(gen, uncurryLevel);
      }

      // Now that we know the substFnType, check if we assumed that we were
      // passing self at +0. If we did and self is not actually passed at +0,
      // retain Self.
      if (AssumedPlusZeroSelf) {
        // If the final emitted function does not have a self param or it does
        // have a self param that is consumed, convert what we think is self to
        // be plus zero.
        if (!substFnType->hasSelfParam() ||
            substFnType->getSelfParameter().isConsumed()) {
          convertSelfToPlusOneFromPlusZero();
        }
      }

      // Emit the first level of call.
      ManagedValue result;

      // We use the context emit-into initialization only for the
      // outermost call.
      SGFContext uncurriedContext =
        (extraSites.empty() ? C : SGFContext());

      // If we have an early emitter, just let it take over for the
      // uncurried call site.
      if (specializedEmitter &&
          specializedEmitter->isEarlyEmitter()) {
        auto emitter = specializedEmitter->getEarlyEmitter();

        assert(uncurriedSites.size() == 1);
        CanFunctionType formalApplyType = cast<FunctionType>(formalType);
        assert(!formalApplyType->getExtInfo().throws());
        SILLocation uncurriedLoc = uncurriedSites[0].Loc;
        claimNextParamClause(origFormalType);
        claimNextParamClause(formalType);

        // We should be able to enforce that these arguments are
        // always still expressions.
        Expr *argument = std::move(uncurriedSites[0]).forward().asKnownExpr();
        result = emitter(gen, uncurriedLoc,
                         callee.getSubstitutions(),
                         argument,
                         formalApplyType,
                         uncurriedContext);

      // Otherwise, emit the uncurried arguments now and perform
      // the call.
      } else {
        // Emit the arguments.
        Optional<SILLocation> uncurriedLoc;
        SmallVector<SmallVector<ManagedValue, 4>, 2> args;
        SmallVector<InOutArgument, 2> inoutArgs;
        CanFunctionType formalApplyType;
        args.reserve(uncurriedSites.size());
        {
          ParamLowering paramLowering(substFnType);

          assert(!foreignError ||
                 uncurriedSites.size() == 1 ||
                 (uncurriedSites.size() == 2 &&
                  substFnType->hasSelfParam()));

          if (!uncurriedSites.back().throws()) {
            initialOptions |= ApplyOptions::DoesNotThrow;
          }

          // Collect the arguments to the uncurried call.
          for (auto &site : uncurriedSites) {
            AbstractionPattern origParamType =
              claimNextParamClause(origFormalType);
            formalApplyType = cast<FunctionType>(formalType);
            claimNextParamClause(formalType);
            uncurriedLoc = site.Loc;
            args.push_back({});

            std::move(site).emit(gen, origParamType, paramLowering,
                                 args.back(), inoutArgs,
                                 &site == &uncurriedSites.back()
                                   ? foreignError
                                   : static_cast<decltype(foreignError)>(None));
          }
        }
        assert(uncurriedLoc);
        assert(formalApplyType);

        // Begin the formal accesses to any inout arguments we have.
        if (!inoutArgs.empty()) {
          beginInOutFormalAccesses(gen, inoutArgs, args);
        }

        // Uncurry the arguments in calling convention order.
        SmallVector<ManagedValue, 4> uncurriedArgs;
        for (auto &argSet : reversed(args))
          uncurriedArgs.append(argSet.begin(), argSet.end());
        args = {};

        // Emit the uncurried call.
        if (!specializedEmitter) {
          result = gen.emitApply(uncurriedLoc.getValue(), mv,
                                 callee.getSubstitutions(),
                                 uncurriedArgs,
                                 substFnType,
                                 origFormalType,
                                 uncurriedSites.back().getSubstResultType(),
                                 initialOptions, None,
                                 foreignError,
                                 uncurriedContext);
        } else if (specializedEmitter->isLateEmitter()) {
          auto emitter = specializedEmitter->getLateEmitter();
          result = emitter(gen,
                           uncurriedLoc.getValue(),
                           callee.getSubstitutions(),
                           uncurriedArgs,
                           formalApplyType,
                           uncurriedContext);
        } else {
          assert(specializedEmitter->isNamedBuiltin());
          auto builtinName = specializedEmitter->getBuiltinName();
          SmallVector<SILValue, 4> consumedArgs;
          for (auto arg : uncurriedArgs) {
            consumedArgs.push_back(arg.forward(gen));
          }
          auto resultVal =
            gen.B.createBuiltin(uncurriedLoc.getValue(), builtinName,
                                substFnType->getResult().getSILType(),
                                callee.getSubstitutions(),
                                consumedArgs);
          result = gen.emitManagedRValueWithCleanup(resultVal);
        }
      }

      // End the initial writeback scope.
      InitialWritebackScope.pop();

      // If there are remaining call sites, apply them to the result function.
      // Each chained call gets its own writeback scope.
      for (unsigned i = 0, size = extraSites.size(); i < size; ++i) {
        WritebackScope writebackScope(gen);

        auto substFnType = result.getType().castTo<SILFunctionType>();
        ParamLowering paramLowering(substFnType);

        SmallVector<ManagedValue, 4> siteArgs;
        SmallVector<InOutArgument, 2> inoutArgs;

        // TODO: foreign errors for block or function pointer values?
        assert(substFnType->hasErrorResult() ||
               !cast<FunctionType>(formalType)->getExtInfo().throws());
        foreignError = None;

        // The result function has already been reabstracted to the substituted
        // type, so use the substituted formal type as the abstraction pattern
        // for argument passing now.
        AbstractionPattern origParamType(claimNextParamClause(formalType));
        std::move(extraSites[i]).emit(gen, origParamType, paramLowering,
                                      siteArgs, inoutArgs, foreignError);
        if (!inoutArgs.empty()) {
          beginInOutFormalAccesses(gen, inoutArgs, siteArgs);
        }

        SGFContext context = i == size - 1 ? C : SGFContext();
        SILLocation loc = extraSites[i].Loc;
        ApplyOptions options = ApplyOptions::None;
        result = gen.emitApply(loc, result, {}, siteArgs,
                               substFnType,
                               origFormalType,
                               extraSites[i].getSubstResultType(),
                               options, None, foreignError, context);
      }

      return result;
    }

    ~CallEmission() { assert(applied && "never applied!"); }

    // Movable, but not copyable.
    CallEmission(CallEmission &&e)
      : gen(e.gen),
        uncurriedSites(std::move(e.uncurriedSites)),
        extraSites(std::move(e.extraSites)),
        callee(std::move(e.callee)),
        InitialWritebackScope(std::move(e.InitialWritebackScope)),
        uncurries(e.uncurries),
        applied(e.applied) {
      e.applied = true;
    }

  private:
    CallEmission(const CallEmission &) = delete;
    CallEmission &operator=(const CallEmission &) = delete;
  };
} // end anonymous namespace

static CallEmission prepareApplyExpr(SILGenFunction &gen, Expr *e) {
  // Set up writebacks for the call(s).
  WritebackScope writebacks(gen);

  SILGenApply apply(gen);

  // Decompose the call site.
  apply.decompose(e);

  // Evaluate and discard the side effect if present.
  if (apply.SideEffect)
    gen.emitRValue(apply.SideEffect);

  // Build the call.
  // Pass the writeback scope on to CallEmission so it can thread scopes through
  // nested calls.
  CallEmission emission(gen, apply.getCallee(), std::move(writebacks),
                        apply.AssumedPlusZeroSelf);

  // Apply 'self' if provided.
  if (apply.SelfParam)
    emission.addCallSite(RegularLocation(e), std::move(apply.SelfParam),
                         apply.SelfType->getCanonicalType(), /*throws*/ false);

  // Apply arguments from call sites, innermost to outermost.
  for (auto site = apply.CallSites.rbegin(), end = apply.CallSites.rend();
       site != end;
       ++site) {
    emission.addCallSite(*site);
  }

  return emission;
}

RValue SILGenFunction::emitApplyExpr(Expr *e, SGFContext c) {
  return RValue(*this, e, prepareApplyExpr(*this, e).apply(c));
}

ManagedValue
SILGenFunction::emitApplyOfLibraryIntrinsic(SILLocation loc,
                                            FuncDecl *fn,
                                            ArrayRef<Substitution> subs,
                                            ArrayRef<ManagedValue> args,
                                            SGFContext ctx) {
  auto origFormalType =
    cast<AnyFunctionType>(fn->getType()->getCanonicalType());
  auto substFormalType = origFormalType;
  if (!subs.empty()) {
    auto polyFnType = cast<PolymorphicFunctionType>(substFormalType);
    auto applied = polyFnType->substGenericArgs(SGM.SwiftModule, subs);
    substFormalType = cast<FunctionType>(applied->getCanonicalType());
  }

  auto callee = Callee::forDirect(*this, SILDeclRef(fn), substFormalType, loc);
  callee.setSubstitutions(*this, loc, subs, 0);

  ManagedValue mv;
  CanSILFunctionType substFnType;
  Optional<ForeignErrorConvention> foreignError;
  ApplyOptions options;
  std::tie(mv, substFnType, foreignError, options)
    = callee.getAtUncurryLevel(*this, 0);

  assert(!foreignError);
  assert(substFnType->getExtInfo().getLanguage()
           == SILFunctionLanguage::Swift);

  return emitApply(loc, mv, subs, args, substFnType,
                   AbstractionPattern(origFormalType.getResult()),
                   substFormalType.getResult(),
                   options, None, None, ctx);
}

/// Allocate an uninitialized array of a given size, returning the array
/// and a pointer to its uninitialized contents, which must be initialized
/// before the array is valid.
std::pair<ManagedValue, SILValue>
SILGenFunction::emitUninitializedArrayAllocation(Type ArrayTy,
                                                 SILValue Length,
                                                 SILLocation Loc) {
  auto &Ctx = getASTContext();
  auto allocate = Ctx.getAllocateUninitializedArray(nullptr);
  auto allocateArchetypes = allocate->getGenericParams()->getAllArchetypes();

  auto arrayElementTy = ArrayTy->castTo<BoundGenericType>()
    ->getGenericArgs()[0];

  // Invoke the intrinsic, which returns a tuple.
  Substitution sub{allocateArchetypes[0], arrayElementTy, {}};
  auto result = emitApplyOfLibraryIntrinsic(Loc, allocate,
                                            sub,
                                            ManagedValue::forUnmanaged(Length),
                                            SGFContext());

  // Explode the tuple.
  TupleTypeElt elts[] = {ArrayTy, Ctx.TheRawPointerType};
  auto tupleTy = TupleType::get(elts, Ctx)->getCanonicalType();
  RValue resultTuple(*this, Loc, tupleTy, result);
  SmallVector<ManagedValue, 2> resultElts;
  std::move(resultTuple).getAll(resultElts);

  return {resultElts[0], resultElts[1].getUnmanagedValue()};
}

/// Deallocate an uninitialized array.
void SILGenFunction::emitUninitializedArrayDeallocation(SILLocation loc,
                                                        SILValue array) {
  auto &Ctx = getASTContext();
  auto deallocate = Ctx.getDeallocateUninitializedArray(nullptr);
  auto archetypes = deallocate->getGenericParams()->getAllArchetypes();

  CanType arrayElementTy =
    array.getType().castTo<BoundGenericType>().getGenericArgs()[0];

  // Invoke the intrinsic.
  Substitution sub{archetypes[0], arrayElementTy, {}};
  emitApplyOfLibraryIntrinsic(loc, deallocate, sub,
                              ManagedValue::forUnmanaged(array),
                              SGFContext());
}

namespace {
  /// A cleanup that deallocates an uninitialized array.
  class DeallocateUninitializedArray: public Cleanup {
    SILValue Array;
  public:
    DeallocateUninitializedArray(SILValue array)
      : Array(array) {}

    void emit(SILGenFunction &gen, CleanupLocation l) override {
      gen.emitUninitializedArrayDeallocation(l, Array);
    }
  };
}

CleanupHandle
SILGenFunction::enterDeallocateUninitializedArrayCleanup(SILValue array) {
  Cleanups.pushCleanup<DeallocateUninitializedArray>(array);
  return Cleanups.getTopCleanup();
}

static Callee getBaseAccessorFunctionRef(SILGenFunction &gen,
                                         SILLocation loc,
                                         SILDeclRef constant,
                                         ArgumentSource &selfValue,
                                         bool isSuper,
                                         bool isDirectUse,
                                         CanAnyFunctionType substAccessorType,
                                         ArrayRef<Substitution> &substitutions){
  auto *decl = cast<AbstractFunctionDecl>(constant.getDecl());

  // If this is a method in a protocol, generate it as a protocol call.
  if (isa<ProtocolDecl>(decl->getDeclContext())) {
    assert(!isDirectUse && "direct use of protocol accessor?");
    assert(!isSuper && "super call to protocol method?");

    return prepareArchetypeCallee(gen, loc, constant, selfValue,
                                  substAccessorType, substitutions);
  }

  bool isClassDispatch = false;
  if (!isDirectUse) {
    switch (gen.getMethodDispatch(decl)) {
    case MethodDispatch::Class:
      isClassDispatch = true;
      break;
    case MethodDispatch::Static:
      isClassDispatch = false;
      break;
    }
  }

  // Dispatch in a struct/enum or to an final method is always direct.
  if (!isClassDispatch || decl->isFinal())
    return Callee::forDirect(gen, constant, substAccessorType, loc);

  // Otherwise, if we have a non-final class dispatch to a normal method,
  // perform a dynamic dispatch.
  auto self = selfValue.forceAndPeekRValue(gen).peekScalarValue();
  if (!isSuper)
    return Callee::forClassMethod(gen, self, constant, substAccessorType,
                                  loc);

  // If this is a "super." dispatch, we either do a direct dispatch in the case
  // of swift classes or an objc super call.
  while (auto *upcast = dyn_cast<UpcastInst>(self))
    self = upcast->getOperand();

  if (constant.isForeign)
    return Callee::forSuperMethod(gen, self, constant, substAccessorType,loc);

  return Callee::forDirect(gen, constant, substAccessorType, loc);
}

static Callee
emitSpecializedAccessorFunctionRef(SILGenFunction &gen,
                                   SILLocation loc,
                                   SILDeclRef constant,
                                   ArrayRef<Substitution> substitutions,
                                   ArgumentSource &selfValue,
                                   bool isSuper,
                                   bool isDirectUse)
{
  SILConstantInfo constantInfo = gen.getConstantInfo(constant);

  // Collect captures if the accessor has them.
  auto accessorFn = cast<AbstractFunctionDecl>(constant.getDecl());
  if (accessorFn->getCaptureInfo().hasLocalCaptures()) {
    assert(!selfValue && "local property has self param?!");
    selfValue = emitCapturesAsArgumentSource(gen, loc, accessorFn).first;
  }

  // Apply substitutions to the callee type.
  CanAnyFunctionType substAccessorType = constantInfo.FormalType;
  if (!substitutions.empty()) {
    auto polyFn = cast<PolymorphicFunctionType>(substAccessorType);
    auto substFn = polyFn->substGenericArgs(gen.SGM.SwiftModule, substitutions);
    substAccessorType = cast<FunctionType>(substFn->getCanonicalType());
  }

  // Get the accessor function. The type will be a polymorphic function if
  // the Self type is generic.
  Callee callee = getBaseAccessorFunctionRef(gen, loc, constant, selfValue,
                                             isSuper, isDirectUse,
                                             substAccessorType, substitutions);

  // If there are substitutions, specialize the generic accessor.
  // FIXME: Generic subscript operator could add another layer of
  // substitutions.
  if (!substitutions.empty()) {
    callee.setSubstitutions(gen, loc, substitutions, 0);
  }
  return callee;
}

ArgumentSource SILGenFunction::prepareAccessorBaseArg(SILLocation loc,
                                                      ManagedValue base,
                                                      CanType baseFormalType,
                                                      SILDeclRef accessor) {
  auto accessorType = SGM.Types.getConstantFunctionType(accessor);
  SILParameterInfo selfParam = accessorType->getParameters().back();

  assert(!base.isInContext());
  assert(!base.isLValue() || !base.hasCleanup());
  SILType baseLoweredType = base.getType();

  // If the base is a boxed existential, we will open it later.
  if (baseLoweredType.getPreferredExistentialRepresentation(SGM.M)
        == ExistentialRepresentation::Boxed) {
    assert(!baseLoweredType.isAddress()
           && "boxed existential should not be an address");
  } else if (baseLoweredType.isAddress()) {
    // If the base is currently an address, we may have to copy it.
    auto needsLoad = [&] {
      switch (selfParam.getConvention()) {
      // If the accessor wants the value 'inout', always pass the
      // address we were given.  This is semantically required.
      case ParameterConvention::Indirect_Inout:
        return false;

      // If the accessor wants the value 'in', we have to copy if the
      // base isn't a temporary.  We aren't allowed to pass aliased
      // memory to 'in', and we have pass at +1.
      case ParameterConvention::Indirect_In:
      case ParameterConvention::Indirect_In_Guaranteed:
        // TODO: We shouldn't be able to get an lvalue here, but the AST
        // sometimes produces an inout base for non-mutating accessors.
        // rdar://problem/19782170
        // assert(!base.isLValue());
        return base.isLValue() || base.isPlusZeroRValueOrTrivial();

      case ParameterConvention::Indirect_Out:
        llvm_unreachable("out parameter not expected here");

      // If the accessor wants the value directly, we definitely have to
      // load.  TODO: don't load-and-retain if the value is passed at +0.
      case ParameterConvention::Direct_Owned:
      case ParameterConvention::Direct_Unowned:
      case ParameterConvention::Direct_Guaranteed:
      case ParameterConvention::Direct_Deallocating:
        return true;
      }
      llvm_unreachable("bad convention");
    };
    if (needsLoad()) {
      // The load can only be a take if the base is a +1 rvalue.
      auto shouldTake = IsTake_t(base.hasCleanup());

      base = emitLoad(loc, base.forward(*this), getTypeLowering(baseLoweredType),
                      SGFContext(), shouldTake);

    // Handle inout bases specially here.
    } else if (selfParam.isIndirectInOut()) {
      // It sometimes happens that we get r-value bases here,
      // e.g. when calling a mutating setter on a materialized
      // temporary.  Just don't claim the value.
      if (!base.isLValue()) {
        base = ManagedValue::forLValue(base.getValue());
      }

      // FIXME: this assumes that there's never meaningful
      // reabstraction of self arguments.
      return ArgumentSource(loc,
                 LValue::forAddress(base, AbstractionPattern(baseFormalType),
                                    baseFormalType));
    }

  // If the base is currently scalar, we may have to drop it in
  // memory or copy it.
  } else {
    assert(!base.isLValue());

    // We need to produce the value at +1 if it's going to be consumed.
    if (selfParam.isConsumed() && !base.hasCleanup()) {
      base = base.copyUnmanaged(*this, loc);
    }

    // If the parameter is indirect, we need to drop the value into
    // temporary memory.
    if (selfParam.isIndirect()) {
      // It's usually a really bad idea to materialize when we're
      // about to pass a value to an inout argument, because it's a
      // really easy way to silently drop modifications (e.g. from a
      // mutating getter in a writeback pair).  Our caller should
      // always take responsibility for that decision (by doing the
      // materialization itself).
      //
      // However, when the base is a reference type and the target is
      // a non-class protocol, this is innocuous.
#ifndef NDEBUG
      auto isNonClassProtocolMember = [](Decl *d) {
        auto p = d->getDeclContext()->isProtocolOrProtocolExtensionContext();
        return (p && !p->requiresClass());
      };
#endif
      assert((!selfParam.isIndirectInOut() ||
              (baseFormalType->isAnyClassReferenceType() &&
               isNonClassProtocolMember(accessor.getDecl()))) &&
             "passing unmaterialized r-value as inout argument");

      base = emitMaterializeIntoTemporary(*this, loc, base);

      if (selfParam.isIndirectInOut()) {
        // Drop the cleanup if we have one.
        auto baseLV = ManagedValue::forLValue(base.getValue());
        return ArgumentSource(loc, LValue::forAddress(baseLV,
                                               AbstractionPattern(baseFormalType),
                                                      baseFormalType));
      }
    }
  }

  return ArgumentSource(loc, RValue(*this, loc,
                               baseFormalType, base));
}

SILDeclRef SILGenFunction::getGetterDeclRef(AbstractStorageDecl *storage,
                                            bool isDirectUse) {
  return SILDeclRef(storage->getGetter(), SILDeclRef::Kind::Func,
                    SILDeclRef::ConstructAtBestResilienceExpansion,
                    SILDeclRef::ConstructAtNaturalUncurryLevel,
                    !isDirectUse && storage->requiresObjCGetterAndSetter());
}

/// Emit a call to a getter.
ManagedValue SILGenFunction::
emitGetAccessor(SILLocation loc, SILDeclRef get,
                ArrayRef<Substitution> substitutions,
                ArgumentSource &&selfValue,
                bool isSuper, bool isDirectUse,
                RValue &&subscripts, SGFContext c) {
  // Scope any further writeback just within this operation.
  WritebackScope writebackScope(*this);

  Callee getter = emitSpecializedAccessorFunctionRef(*this, loc, get,
                                                     substitutions, selfValue,
                                                     isSuper, isDirectUse);
  CanAnyFunctionType accessType = getter.getSubstFormalType();

  CallEmission emission(*this, std::move(getter), std::move(writebackScope));
  // Self ->
  if (selfValue) {
    emission.addCallSite(loc, std::move(selfValue), accessType);
    accessType = cast<AnyFunctionType>(accessType.getResult());
  }
  // Index or () if none.
  if (!subscripts)
    subscripts = emitEmptyTupleRValue(loc, SGFContext());

  emission.addCallSite(loc, ArgumentSource(loc, std::move(subscripts)),
                       accessType);

  // T
  return emission.apply(c);
}

SILDeclRef SILGenFunction::getSetterDeclRef(AbstractStorageDecl *storage,
                                            bool isDirectUse) {
  return SILDeclRef(storage->getSetter(), SILDeclRef::Kind::Func,
                    SILDeclRef::ConstructAtBestResilienceExpansion,
                    SILDeclRef::ConstructAtNaturalUncurryLevel,
                    !isDirectUse && storage->requiresObjCGetterAndSetter());
}

void SILGenFunction::emitSetAccessor(SILLocation loc, SILDeclRef set,
                                     ArrayRef<Substitution> substitutions,
                                     ArgumentSource &&selfValue,
                                     bool isSuper, bool isDirectUse,
                                     RValue &&subscripts, RValue &&setValue) {
  // Scope any further writeback just within this operation.
  WritebackScope writebackScope(*this);

  Callee setter = emitSpecializedAccessorFunctionRef(*this, loc, set,
                                                     substitutions, selfValue,
                                                     isSuper, isDirectUse);
  CanAnyFunctionType accessType = setter.getSubstFormalType();

  CallEmission emission(*this, std::move(setter), std::move(writebackScope));
  // Self ->
  if (selfValue) {
    emission.addCallSite(loc, std::move(selfValue), accessType);
    accessType = cast<AnyFunctionType>(accessType.getResult());
  }

  // (value)  or (value, indices)
  if (subscripts) {
    // If we have a value and index list, create a new rvalue to represent the
    // both of them together.  The value goes first.
    SmallVector<ManagedValue, 4> Elts;
    std::move(setValue).getAll(Elts);
    std::move(subscripts).getAll(Elts);
    setValue = RValue(Elts, accessType.getInput());
  } else {
    setValue.rewriteType(accessType.getInput());
  }
  emission.addCallSite(loc, ArgumentSource(loc, std::move(setValue)),
                       accessType);
  // ()
  emission.apply();
}

SILDeclRef
SILGenFunction::getMaterializeForSetDeclRef(AbstractStorageDecl *storage,
                                            bool isDirectUse) {
  return SILDeclRef(storage->getMaterializeForSetFunc(),
                    SILDeclRef::Kind::Func,
                    SILDeclRef::ConstructAtBestResilienceExpansion,
                    SILDeclRef::ConstructAtNaturalUncurryLevel,
                    /*foreign*/ false);
}

std::pair<SILValue, SILValue> SILGenFunction::
emitMaterializeForSetAccessor(SILLocation loc, SILDeclRef materializeForSet,
                              ArrayRef<Substitution> substitutions,
                              ArgumentSource &&selfValue,
                              bool isSuper, bool isDirectUse,
                              RValue &&subscripts, SILValue buffer,
                              SILValue callbackStorage) {
  // Scope any further writeback just within this operation.
  WritebackScope writebackScope(*this);

  assert(!materializeForSet.getDecl()
           ->getDeclContext()->isProtocolExtensionContext() &&
         "direct use of materializeForSet from a protocol extension is"
         " probably a miscompile");

  Callee callee = emitSpecializedAccessorFunctionRef(*this, loc,
                                                     materializeForSet,
                                                     substitutions, selfValue,
                                                     isSuper, isDirectUse);
  CanAnyFunctionType accessType = callee.getSubstFormalType();

  CallEmission emission(*this, std::move(callee), std::move(writebackScope));
  // Self ->
  if (selfValue) {
    emission.addCallSite(loc, std::move(selfValue), accessType);
    accessType = cast<AnyFunctionType>(accessType.getResult());
  }

  // (buffer, callbackStorage)  or (buffer, callbackStorage, indices) ->
  // Note that this "RValue" stores a mixed LValue/RValue tuple.
  RValue args = [&] {
    SmallVector<ManagedValue, 4> elts;

    auto bufferPtr =
      B.createAddressToPointer(loc, buffer,
                               SILType::getRawPointerType(getASTContext()));
    elts.push_back(ManagedValue::forUnmanaged(bufferPtr));

    elts.push_back(ManagedValue::forLValue(callbackStorage));

    if (subscripts) {
      std::move(subscripts).getAll(elts);
    }
    return RValue(elts, accessType.getInput());
  }();
  emission.addCallSite(loc, ArgumentSource(loc, std::move(args)), accessType);
  // (buffer, optionalCallback)
  SILValue pointerAndOptionalCallback = emission.apply().getUnmanagedValue();

  // Project out the materialized address.
  SILValue address = B.createTupleExtract(loc, pointerAndOptionalCallback, 0);
  address = B.createPointerToAddress(loc, address, buffer.getType());

  // Project out the optional callback.
  SILValue optionalCallback =
    B.createTupleExtract(loc, pointerAndOptionalCallback, 1);

  return { address, optionalCallback };
}

SILDeclRef SILGenFunction::getAddressorDeclRef(AbstractStorageDecl *storage,
                                               AccessKind accessKind,
                                               bool isDirectUse) {
  FuncDecl *addressorFunc = storage->getAddressorForAccess(accessKind);
  return SILDeclRef(addressorFunc, SILDeclRef::Kind::Func,
                    SILDeclRef::ConstructAtBestResilienceExpansion,
                    SILDeclRef::ConstructAtNaturalUncurryLevel,
                    /*foreign*/ false);
}

/// Emit a call to an addressor.
///
/// The first return value is the address, which will always be an
/// l-value managed value.  The second return value is the owner
/// pointer, if applicable.
std::pair<ManagedValue, ManagedValue> SILGenFunction::
emitAddressorAccessor(SILLocation loc, SILDeclRef addressor,
                      ArrayRef<Substitution> substitutions,
                      ArgumentSource &&selfValue,
                      bool isSuper, bool isDirectUse,
                      RValue &&subscripts, SILType addressType) {
  // Scope any further writeback just within this operation.
  WritebackScope writebackScope(*this);

  Callee callee =
    emitSpecializedAccessorFunctionRef(*this, loc, addressor,
                                       substitutions, selfValue,
                                       isSuper, isDirectUse);
  CanAnyFunctionType accessType = callee.getSubstFormalType();

  CallEmission emission(*this, std::move(callee), std::move(writebackScope));
  // Self ->
  if (selfValue) {
    emission.addCallSite(loc, std::move(selfValue), accessType);
    accessType = cast<AnyFunctionType>(accessType.getResult());
  }
  // Index or () if none.
  if (!subscripts)
    subscripts = emitEmptyTupleRValue(loc, SGFContext());

  emission.addCallSite(loc, ArgumentSource(loc, std::move(subscripts)),
                       accessType);

  // Unsafe{Mutable}Pointer<T> or
  // (Unsafe{Mutable}Pointer<T>, Builtin.UnknownPointer) or
  // (Unsafe{Mutable}Pointer<T>, Builtin.NativePointer) or
  // (Unsafe{Mutable}Pointer<T>, Builtin.NativePointer?) or
  SILValue result = emission.apply().forward(*this);

  SILValue pointer;
  ManagedValue owner;
  switch (cast<FuncDecl>(addressor.getDecl())->getAddressorKind()) {
  case AddressorKind::NotAddressor:
    llvm_unreachable("not an addressor!");
  case AddressorKind::Unsafe:
    pointer = result;
    owner = ManagedValue();
    break;
  case AddressorKind::Owning:
  case AddressorKind::NativeOwning:
  case AddressorKind::NativePinning:
    pointer = B.createTupleExtract(loc, result, 0);
    owner = emitManagedRValueWithCleanup(B.createTupleExtract(loc, result, 1));
    break;
  }

  // Drill down to the raw pointer using intrinsic knowledge of those types.
  auto pointerType =
    pointer.getType().castTo<BoundGenericStructType>()->getDecl();
  auto props = pointerType->getStoredProperties();
  assert(props.begin() != props.end());
  assert(std::next(props.begin()) == props.end());
  VarDecl *rawPointerField = *props.begin();
  pointer = B.createStructExtract(loc, pointer, rawPointerField,
                                  SILType::getRawPointerType(getASTContext()));

  // Convert to the appropriate address type and return.
  SILValue address = B.createPointerToAddress(loc, pointer, addressType);

  // Mark dependence as necessary.
  switch (cast<FuncDecl>(addressor.getDecl())->getAddressorKind()) {
  case AddressorKind::NotAddressor:
    llvm_unreachable("not an addressor!");
  case AddressorKind::Unsafe:
    // TODO: we should probably mark dependence on the base.
    break;
  case AddressorKind::Owning:
  case AddressorKind::NativeOwning:
  case AddressorKind::NativePinning:
    address = B.createMarkDependence(loc, address, owner.getValue());
    break;
  }

  return { ManagedValue::forLValue(address), owner };
}


ManagedValue SILGenFunction::emitApplyConversionFunction(SILLocation loc,
                                                         Expr *funcExpr,
                                                         Type resultType,
                                                         RValue &&operand) {
  // Walk the function expression, which should produce a reference to the
  // callee, leaving the final curry level unapplied.
  CallEmission emission = prepareApplyExpr(*this, funcExpr);
  // Rewrite the operand type to the expected argument type, to handle tuple
  // conversions etc.
  auto funcTy = cast<FunctionType>(funcExpr->getType()->getCanonicalType());
  operand.rewriteType(funcTy.getInput());
  // Add the operand as the final callsite.
  emission.addCallSite(loc, ArgumentSource(loc, std::move(operand)),
                       resultType->getCanonicalType(), funcTy->throws());
  return emission.apply();
}

// Create a partial application of a dynamic method, applying bridging thunks
// if necessary.
static SILValue emitDynamicPartialApply(SILGenFunction &gen,
                                        SILLocation loc,
                                        SILValue method,
                                        SILValue self,
                                        CanFunctionType methodTy) {
  // Pop the self type off of the function type.
  // Just to be weird, partially applying an objc method produces a native
  // function (?!)
  auto fnTy = method.getType().castTo<SILFunctionType>();
  // If the original method has an @unowned_inner_pointer return, the partial
  // application thunk will lifetime-extend 'self' for us.
  auto resultInfo = fnTy->getResult();
  if (resultInfo.getConvention() == ResultConvention::UnownedInnerPointer)
    resultInfo = SILResultInfo(resultInfo.getType(), ResultConvention::Unowned);

  auto partialApplyTy = SILFunctionType::get(fnTy->getGenericSignature(),
                     fnTy->getExtInfo()
                       .withRepresentation(SILFunctionType::Representation::Thick),
                     ParameterConvention::Direct_Owned,
                     fnTy->getParameters()
                       .slice(0, fnTy->getParameters().size() - 1),
                     resultInfo, fnTy->getOptionalErrorResult(),
                     gen.getASTContext());

  // Retain 'self' because the partial apply will take ownership.
  // We can't simply forward 'self' because the partial apply is conditional.
#if 0
  auto CMV = ConsumableManagedValue(ManagedValue::forUnmanaged(self),
                                    CastConsumptionKind::CopyOnSuccess);
  self = gen.getManagedValue(loc, CMV).forward(gen);
#else
  if (!self.getType().isAddress())
    gen.B.emitRetainValueOperation(loc, self);
#endif

  SILValue result = gen.B.createPartialApply(loc, method, method.getType(), {},
                        self, SILType::getPrimitiveObjectType(partialApplyTy));
  // If necessary, thunk to the native ownership conventions and bridged types.
  auto nativeTy = gen.getLoweredLoadableType(methodTy).castTo<SILFunctionType>();

  if (nativeTy != partialApplyTy) {
    result = gen.emitBlockToFunc(loc, ManagedValue::forUnmanaged(result),
                                 nativeTy).forward(gen);
  }

  return result;
}

RValue SILGenFunction::emitDynamicMemberRefExpr(DynamicMemberRefExpr *e,
                                                SGFContext c) {
  // Emit the operand.
  ManagedValue base = emitRValueAsSingleValue(e->getBase());

  SILValue operand = base.getValue();
  if (!e->getMember().getDecl()->isInstanceMember()) {
    auto metatype = operand.getType().castTo<MetatypeType>();
    assert(metatype->getRepresentation() == MetatypeRepresentation::Thick);
    metatype = CanMetatypeType::get(metatype.getInstanceType(),
                                    MetatypeRepresentation::ObjC);
    operand = B.createThickToObjCMetatype(e, operand,
                                    SILType::getPrimitiveObjectType(metatype));
  }

  // Create the continuation block.
  SILBasicBlock *contBB = createBasicBlock();

  // Create the no-member block.
  SILBasicBlock *noMemberBB = createBasicBlock();

  // Create the has-member block.
  SILBasicBlock *hasMemberBB = createBasicBlock();

  // The continuation block
  const TypeLowering &optTL = getTypeLowering(e->getType());
  auto loweredOptTy = optTL.getLoweredType();

  SILValue optTemp = emitTemporaryAllocation(e, loweredOptTy);

  // Create the branch.
  FuncDecl *memberFunc;
  if (auto *VD = dyn_cast<VarDecl>(e->getMember().getDecl()))
    memberFunc = VD->getGetter();
  else
    memberFunc = cast<FuncDecl>(e->getMember().getDecl());
  SILDeclRef member(memberFunc, SILDeclRef::Kind::Func,
                    SILDeclRef::ConstructAtBestResilienceExpansion,
                    SILDeclRef::ConstructAtNaturalUncurryLevel,
                    /*isObjC=*/true);
  B.createDynamicMethodBranch(e, operand, member, hasMemberBB, noMemberBB);

  // Create the has-member branch.
  {
    B.emitBlock(hasMemberBB);

    FullExpr hasMemberScope(Cleanups, CleanupLocation(e));

    // The argument to the has-member block is the uncurried method.
    auto valueTy = e->getType()->getCanonicalType().getAnyOptionalObjectType();
    auto methodTy = valueTy;

    // For a computed variable, we want the getter.
    if (isa<VarDecl>(e->getMember().getDecl()))
      methodTy = CanFunctionType::get(TupleType::getEmpty(getASTContext()),
                                      methodTy);

    auto dynamicMethodTy = getDynamicMethodLoweredType(*this, operand, member);
    auto loweredMethodTy = SILType::getPrimitiveObjectType(dynamicMethodTy);
    SILValue memberArg = new (F.getModule()) SILArgument(hasMemberBB,
                                                         loweredMethodTy);

    // Create the result value.
    SILValue result = emitDynamicPartialApply(*this, e, memberArg, operand,
                                              cast<FunctionType>(methodTy));
    if (isa<VarDecl>(e->getMember().getDecl())) {
      result = B.createApply(e, result, result.getType(),
                             getLoweredType(valueTy), {}, {});
    }

    // Package up the result in an optional.
    RValue resultRV = RValue(*this, e, valueTy,
                             emitManagedRValueWithCleanup(result));
    emitInjectOptionalValueInto(e, {e, std::move(resultRV)}, optTemp, optTL);

    // Branch to the continuation block.
    B.createBranch(e, contBB);
  }

  // Create the no-member branch.
  {
    B.emitBlock(noMemberBB);

    emitInjectOptionalNothingInto(e, optTemp, optTL);

    // Branch to the continuation block.
    B.createBranch(e, contBB);
  }

  // Emit the continuation block.
  B.emitBlock(contBB);

  // Package up the result.
  auto optResult = B.createLoad(e, optTemp);
  return RValue(*this, e, emitManagedRValueWithCleanup(optResult, optTL));
}

RValue SILGenFunction::emitDynamicSubscriptExpr(DynamicSubscriptExpr *e,
                                                SGFContext c) {
  // Emit the base operand.
  ManagedValue managedBase = emitRValueAsSingleValue(e->getBase());

  SILValue base = managedBase.getValue();

  // Emit the index.
  RValue index = emitRValue(e->getIndex());

  // Create the continuation block.
  SILBasicBlock *contBB = createBasicBlock();

  // Create the no-member block.
  SILBasicBlock *noMemberBB = createBasicBlock();

  // Create the has-member block.
  SILBasicBlock *hasMemberBB = createBasicBlock();

  const TypeLowering &optTL = getTypeLowering(e->getType());
  auto loweredOptTy = optTL.getLoweredType();
  SILValue optTemp = emitTemporaryAllocation(e, loweredOptTy);

  // Create the branch.
  auto subscriptDecl = cast<SubscriptDecl>(e->getMember().getDecl());
  SILDeclRef member(subscriptDecl->getGetter(),
                    SILDeclRef::Kind::Func,
                    SILDeclRef::ConstructAtBestResilienceExpansion,
                    SILDeclRef::ConstructAtNaturalUncurryLevel,
                    /*isObjC=*/true);
  B.createDynamicMethodBranch(e, base, member, hasMemberBB, noMemberBB);

  // Create the has-member branch.
  {
    B.emitBlock(hasMemberBB);

    FullExpr hasMemberScope(Cleanups, CleanupLocation(e));

    // The argument to the has-member block is the uncurried method.
    auto valueTy = e->getType()->getCanonicalType().getAnyOptionalObjectType();
    auto methodTy =
      subscriptDecl->getGetter()->getType()->castTo<AnyFunctionType>()
                      ->getResult()->getCanonicalType();
    auto dynamicMethodTy = getDynamicMethodLoweredType(*this, base, member);
    auto loweredMethodTy = SILType::getPrimitiveObjectType(dynamicMethodTy);
    SILValue memberArg = new (F.getModule()) SILArgument(hasMemberBB,
                                                         loweredMethodTy);
    // Emit the application of 'self'.
    SILValue result = emitDynamicPartialApply(*this, e, memberArg, base,
                                              cast<FunctionType>(methodTy));
    // Emit the index.
    llvm::SmallVector<SILValue, 1> indexArgs;
    std::move(index).forwardAll(*this, indexArgs);
    auto &valueTL = getTypeLowering(valueTy);
    result = B.createApply(e, result, result.getType(),
                           valueTL.getLoweredType(), {}, indexArgs);

    // Package up the result in an optional.
    RValue resultRV =
      RValue(*this, e, valueTy, emitManagedRValueWithCleanup(result, valueTL));
    emitInjectOptionalValueInto(e, {e, std::move(resultRV)}, optTemp, optTL);

    // Branch to the continuation block.
    B.createBranch(e, contBB);
  }

  // Create the no-member branch.
  {
    B.emitBlock(noMemberBB);

    emitInjectOptionalNothingInto(e, optTemp, optTL);

    // Branch to the continuation block.
    B.createBranch(e, contBB);
  }

  // Emit the continuation block.
  B.emitBlock(contBB);

  // Package up the result.
  auto optValue = B.createLoad(e, optTemp);
  return RValue(*this, e, emitManagedRValueWithCleanup(optValue, optTL));
}
