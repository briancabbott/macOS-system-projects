//===--- DynamicCasts.cpp - Utilities for dynamic casts -------------------===//
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

#include "swift/AST/Types.h"
#include "swift/SIL/SILArgument.h"
#include "swift/SIL/SILBuilder.h"
#include "swift/SIL/DynamicCasts.h"
#include "swift/SIL/TypeLowering.h"

using namespace swift;
using namespace Lowering;

static DynamicCastFeasibility weakenSuccess(DynamicCastFeasibility v) {
  if (v == DynamicCastFeasibility::WillSucceed)
    return DynamicCastFeasibility::MaySucceed;
  return v;
}

static unsigned getAnyMetatypeDepth(CanType type) {
  unsigned depth = 0;
  while (auto metatype = dyn_cast<AnyMetatypeType>(type)) {
    type = metatype.getInstanceType();
    depth++;
  }
  return depth;
}

static bool
mayBridgeToObjectiveC(Module *M, CanType T) {
  // If the target type is either an unknown dynamic type, or statically
  // known to bridge, the cast may succeed.
  // TODO: We could be more precise with the bridged-to type.
  if (T->hasArchetype())
    return true;
  
  if (T->isAnyExistentialType())
    return true;
  
  if (M->getASTContext().getBridgedToObjC(M, T, nullptr))
    return true;

  return false;
}

static bool canClassOrSuperclassesHaveExtensions(ClassDecl *CD,
                                                 bool isWholeModuleOpts) {
  while (CD) {
    // Public classes can always be extended
    if (CD->getEffectiveAccess() == Accessibility::Public)
      return true;

    // Internal classes can be extended, if we are not in
    // whole-module-optimization mode.
    if (CD->getEffectiveAccess() == Accessibility::Internal &&
        !isWholeModuleOpts)
      return true;

    if (!CD->hasSuperclass())
      break;

    CD = CD->getSuperclass()->getClassOrBoundGenericClass();
  }

  return false;
}

/// Try to classify a conversion from non-existential type
/// into an existential type by performing a static check
/// of protocol conformances if it is possible.
static DynamicCastFeasibility
classifyDynamicCastToProtocol(CanType source,
                              CanType target,
                              bool isWholeModuleOpts) {
  assert(target.isExistentialType() &&
         "target should be an existential type");

  if (source == target)
    return DynamicCastFeasibility::WillSucceed;

  auto *SourceNominalTy = source.getAnyNominal();

  if (!SourceNominalTy)
    return DynamicCastFeasibility::MaySucceed;

  auto *TargetProtocol = target.getAnyNominal();
  if (!TargetProtocol)
    return DynamicCastFeasibility::MaySucceed;

  auto SourceProtocols = SourceNominalTy->getAllProtocols();

  // Check all protocols implemented by the type.
  for (auto *Protocol : SourceProtocols) {
    if (Protocol == TargetProtocol)
      return DynamicCastFeasibility::WillSucceed;
  }

  // If we are casting a protocol, then the cast will fail
  // as we have not found any conformances and protocols cannot
  // be extended currently.
  // NOTE: If we allow protocol extensions in the future, this
  // conditional statement should be removed.
  if (isa<ProtocolType>(source)) {
    return DynamicCastFeasibility::WillFail;
  }

  // If it is a class and it can be proven that this class and its
  // superclasses cannot be extended, then it is safe to proceed.
  // No need to check this for structs, as they do not have any
  // superclasses.
  if (auto *CD = source.getClassOrBoundGenericClass()) {
    if (canClassOrSuperclassesHaveExtensions(CD, isWholeModuleOpts))
      return DynamicCastFeasibility::MaySucceed;
    // Derived types may conform to the protocol.
    if (!CD->isFinal()) {
      // TODO: If it is a private type or internal type and we
      // can prove that there are no derived types conforming to a
      // protocol, then we can still return WillFail.
      return DynamicCastFeasibility::MaySucceed;
    }
  }

  // If the source type is private or target protocol is private,
  // then conformances cannot be changed at run-time, because only this
  // file could have implemented them, but no conformances were found.
  // Therefore it is safe to make a negative decision at compile-time.
  if (SourceNominalTy->getEffectiveAccess() == Accessibility::Private ||
      TargetProtocol->getEffectiveAccess() == Accessibility::Private) {
    // This cast is always false. Replace it with a branch to the
    // failure block.
    return DynamicCastFeasibility::WillFail;
  }

  // If we are in a whole-module compilation and
  // if the source type is internal or target protocol is internal,
  // then conformances cannot be changed at run-time, because only this
  // module could have implemented them, but no conformances were found.
  // Therefore it is safe to make a negative decision at compile-time.
  if (isWholeModuleOpts &&
      (SourceNominalTy->getEffectiveAccess() == Accessibility::Internal ||
       TargetProtocol->getEffectiveAccess() == Accessibility::Internal)) {
    return DynamicCastFeasibility::WillFail;
  }

  return DynamicCastFeasibility::MaySucceed;
}

/// Check if a given type conforms to _BridgedToObjectiveC protocol.
bool swift::isObjectiveCBridgeable(Module *M, CanType Ty) {
  // Retrieve the _BridgedToObjectiveC protocol.
  auto bridgedProto =
      M->getASTContext().getProtocol(KnownProtocolKind::ObjectiveCBridgeable);

  if (bridgedProto) {
    // Find the conformance of the value type to _BridgedToObjectiveC.
    // Check whether the type conforms to _BridgedToObjectiveC.
    auto conformance = M->lookupConformance(Ty, bridgedProto, nullptr);

    return (conformance.getInt() != ConformanceKind::DoesNotConform);
  }
  return false;
}

/// Check if a given type conforms to _Error protocol.
bool swift::isErrorType(Module *M, CanType Ty) {
  // Retrieve the ErrorType protocol.
  auto errorTypeProto =
      M->getASTContext().getProtocol(KnownProtocolKind::ErrorType);

  if (errorTypeProto) {
    // Find the conformance of the value type to _BridgedToObjectiveC.
    // Check whether the type conforms to _BridgedToObjectiveC.
    auto conformance = M->lookupConformance(Ty, errorTypeProto, nullptr);

    return (conformance.getInt() != ConformanceKind::DoesNotConform);
  }
  return false;
}

/// Try to classify the dynamic-cast relationship between two types.
DynamicCastFeasibility
swift::classifyDynamicCast(Module *M,
                           CanType source,
                           CanType target,
                           bool isSourceTypeExact,
                           bool isWholeModuleOpts) {
  if (source == target) return DynamicCastFeasibility::WillSucceed;

  auto sourceObject = source.getAnyOptionalObjectType();
  auto targetObject = target.getAnyOptionalObjectType();

  // A common level of optionality doesn't affect the feasibility.
  if (sourceObject && targetObject) {
    return classifyDynamicCast(M, sourceObject, targetObject);

  // Nor does casting to a more optional type.
  } else if (targetObject) {
    return classifyDynamicCast(M, source, targetObject,
                               /* isSourceTypeExact */ false,
                               isWholeModuleOpts);

  // Casting to a less-optional type can always fail.
  } else if (sourceObject) {
    return weakenSuccess(classifyDynamicCast(M, sourceObject, target,
                                             /* isSourceTypeExact */ false,
                                             isWholeModuleOpts));
  }
  assert(!sourceObject && !targetObject);

  // Assume that casts to or from existential types or involving
  // dependent types can always succeed.  This is over-conservative.
  if (source->hasArchetype() || source.isExistentialType() ||
      target->hasArchetype() || target.isExistentialType()) {

    auto *SourceNominalTy = source.getAnyNominal();

    // Check conversions from non-protocol types into protocol types.
    if (!source.isExistentialType() &&
        SourceNominalTy &&
        target.isExistentialType())
      return classifyDynamicCastToProtocol(source, target, isWholeModuleOpts);

    // Casts from class existential into a non-class can never succeed.
    if (source->isClassExistentialType() &&
        !target.isAnyExistentialType() &&
        !target.getClassOrBoundGenericClass() &&
        !isa<ArchetypeType>(target) &&
        !mayBridgeToObjectiveC(M, target)) {
      assert((target.getEnumOrBoundGenericEnum() ||
              target.getStructOrBoundGenericStruct() ||
              isa<TupleType>(target) ||
              isa<SILFunctionType>(target) ||
              isa<FunctionType>(target) ||
              isa<MetatypeType>(target)) &&
             "Target should be an enum, struct, tuple, metatype or function type");
      return DynamicCastFeasibility::WillFail;
    }

    return DynamicCastFeasibility::MaySucceed;
  }

  // Metatype casts.
  if (auto sourceMetatype = dyn_cast<AnyMetatypeType>(source)) {
    auto targetMetatype = dyn_cast<AnyMetatypeType>(target);
    if (!targetMetatype) return DynamicCastFeasibility::WillFail;

    source = sourceMetatype.getInstanceType();
    target = targetMetatype.getInstanceType();

    if (source == target &&
        targetMetatype.isAnyExistentialType() ==
            sourceMetatype.isAnyExistentialType())
      return DynamicCastFeasibility::WillSucceed;

    if (targetMetatype.isAnyExistentialType() &&
        (isa<ProtocolType>(target) || isa<ProtocolCompositionType>(target))) {
      auto Feasibility = classifyDynamicCastToProtocol(source,
                                                       target,
                                                       isWholeModuleOpts);
      // Cast from existential metatype to existential metatype may still
      // succeed, even if we cannot prove anything statically.
      if (Feasibility != DynamicCastFeasibility::WillFail ||
          !sourceMetatype.isAnyExistentialType())
        return Feasibility;
    }

    // If isSourceTypeExact is true, we know we are casting the result of a
    // MetatypeInst instruction.
    if (isSourceTypeExact) {
      // If source or target are existentials, then it can be cast
      // successfully only into itself.
      if ((target.isAnyExistentialType() || source.isAnyExistentialType()) &&
          target != source)
        return DynamicCastFeasibility::WillFail;
    }

    // Casts from class existential metatype into a concrete non-class metatype
    // can never succeed.
    if (source->isClassExistentialType() &&
        !target.isAnyExistentialType() &&
        !target.getClassOrBoundGenericClass())
      return DynamicCastFeasibility::WillFail;

    // TODO: prove that some conversions to existential metatype will
    // obviously succeed/fail.
    // TODO: prove that some conversions from class existential metatype
    // to a concrete non-class metatype will obviously fail.
    // TODO: class metatype to/from AnyObject
    // TODO: protocol concrete metatype to/from ObjCProtocol
    if (isa<ExistentialMetatypeType>(sourceMetatype) ||
        isa<ExistentialMetatypeType>(targetMetatype))
      return (getAnyMetatypeDepth(source) == getAnyMetatypeDepth(target)
              ? DynamicCastFeasibility::MaySucceed
              : DynamicCastFeasibility::WillFail);

    // If both metatypes are class metatypes, check if classes can be
    // cast.
    if (source.getClassOrBoundGenericClass() &&
        target.getClassOrBoundGenericClass())
      return classifyDynamicCast(M, source, target, false, isWholeModuleOpts);

    // Different structs cannot be cast to each other.
    if (source.getStructOrBoundGenericStruct() &&
        target.getStructOrBoundGenericStruct() &&
        source != target)
      return DynamicCastFeasibility::WillFail;

    // Different enums cannot be cast to each other.
    if (source.getEnumOrBoundGenericEnum() &&
        target.getEnumOrBoundGenericEnum() &&
        source != target)
      return DynamicCastFeasibility::WillFail;

    // If we don't know any better, assume that the cast may succeed.
    return DynamicCastFeasibility::MaySucceed;
  }
  
  // Function casts.
  if (auto sourceFunction = dyn_cast<FunctionType>(source)) {
    if (auto targetFunction = dyn_cast<FunctionType>(target)) {
      // A function cast can succeed if the function types can be identical,
      // or if the target type is throwier than the original.

      // A non-throwing source function can be cast to a throwing target type,
      // but not vice versa.
      if (sourceFunction->throws() && !targetFunction->throws())
        return DynamicCastFeasibility::WillFail;
      
      // A noreturn source function can be cast to a returning target type,
      // but not vice versa.
      // (noreturn isn't really reified at runtime though.)
      if (targetFunction->isNoReturn() && !sourceFunction->isNoReturn())
        return DynamicCastFeasibility::WillFail;
      
      // The cast can't change the representation at runtime.
      if (targetFunction->getRepresentation()
            != sourceFunction->getRepresentation())
        return DynamicCastFeasibility::WillFail;
      
      if (sourceFunction.getInput() == targetFunction.getInput()
          && sourceFunction.getResult() == targetFunction.getResult())
        return DynamicCastFeasibility::WillSucceed;

      auto isSubstitutable = [](CanType a, CanType b) -> bool {
        // FIXME: Unnecessarily conservative; should structurally check for
        // substitutability.
        return a == b || a->hasArchetype() || b->hasArchetype();
      };
    
      if (isSubstitutable(sourceFunction.getInput(), targetFunction.getInput())
          && isSubstitutable(targetFunction.getInput(),
                             targetFunction.getResult()))
        return DynamicCastFeasibility::MaySucceed;
      
      return DynamicCastFeasibility::WillFail;
    }
  }

  // Class casts.
  auto sourceClass = source.getClassOrBoundGenericClass();
  auto targetClass = target.getClassOrBoundGenericClass();
  if (sourceClass) {
    if (targetClass) {
      if (target->isSuperclassOf(source, nullptr))
        return DynamicCastFeasibility::WillSucceed;
      if (source->isSuperclassOf(target, nullptr))
        return DynamicCastFeasibility::MaySucceed;

      // FIXME: bridged types, e.g. CF <-> NS (but not for metatypes).
      return DynamicCastFeasibility::WillFail;
    }

    // In the Objective-C runtime, class metatypes are also class instances.
    // The cast may succeed if the target type can be inhabited by a class
    // metatype.
    // TODO: Narrow this to the sourceClass being exactly NSObject.
    if (M->getASTContext().LangOpts.EnableObjCInterop) {
      if (auto targetMeta = dyn_cast<MetatypeType>(target)) {
        if (isa<ArchetypeType>(targetMeta.getInstanceType())
            || targetMeta.getInstanceType()->mayHaveSuperclass())
          return DynamicCastFeasibility::MaySucceed;
      } else if (isa<ExistentialMetatypeType>(target)) {
        return DynamicCastFeasibility::MaySucceed;
      }
    }
  }

  // If the source is not existential, an archetype, or (under the ObjC runtime)
  // a class, and the destination is a metatype, there is no way the cast can
  // succeed.
  if (target->is<AnyMetatypeType>()) return DynamicCastFeasibility::WillFail;

  // FIXME: tuple conversions?

  // FIXME: Be more careful with briding conversions from
  // NSArray, NSDictionary and NSSet as they may fail?

  // Check if there might be a bridging conversion.
  if (source->isBridgeableObjectType() && mayBridgeToObjectiveC(M, target)) {
    // Try to get the ObjC type which is bridged to target type.
    assert(!target.isAnyExistentialType());
    Optional<Type> ObjCTy = M->getASTContext().getBridgedToObjC(
        M, target, nullptr);
    if (ObjCTy && ObjCTy.getValue()) {
      // If the bridged ObjC type is known, check if
      // source type can be cast into it.
      return classifyDynamicCast(M, source,
          ObjCTy.getValue().getCanonicalTypeOrNull(),
          /* isSourceTypeExact */ false, isWholeModuleOpts);
    }
    return DynamicCastFeasibility::MaySucceed;
  }
  
  if (target->isBridgeableObjectType() && mayBridgeToObjectiveC(M, source)) {
    // Try to get the ObjC type which is bridged to source type.
    assert(!source.isAnyExistentialType());
    Optional<Type> ObjCTy = M->getASTContext().getBridgedToObjC(
        M, source, nullptr);
    if (ObjCTy && ObjCTy.getValue()) {
      // If the bridged ObjC type is known, check if
      // this type can be cast into target type.
      return classifyDynamicCast(M,
          ObjCTy.getValue().getCanonicalTypeOrNull(),
          target,
          /* isSourceTypeExact */ false, isWholeModuleOpts);
    }
    return DynamicCastFeasibility::MaySucceed;
  }

  // Check if it is a cast between bridged error types.
  if (isErrorType(M, source) && isErrorType(M, target)) {
    // TODO: Cast to NSError succeeds always.
    return DynamicCastFeasibility::MaySucceed;
  }

  return DynamicCastFeasibility::WillFail;
}

static unsigned getOptionalDepth(CanType type) {
  unsigned depth = 0;
  while (CanType objectType = type.getAnyOptionalObjectType()) {
    depth++;
    type = objectType;
  }
  return depth;
}

namespace {
  struct Source {
    SILValue Value;
    CanType FormalType;
    CastConsumptionKind Consumption;

    bool isAddress() const { return Value.getType().isAddress(); }
    IsTake_t shouldTake() const {
      return shouldTakeOnSuccess(Consumption);
    }

    Source() = default;
    Source(SILValue value, CanType formalType, CastConsumptionKind consumption)
      : Value(value), FormalType(formalType), Consumption(consumption) {}
  };

  struct Target {
    SILValue Address;
    SILType LoweredType;
    CanType FormalType;

    bool isAddress() const { return (bool) Address; }

    Source asAddressSource() const {
      assert(isAddress());
      return { Address, FormalType, CastConsumptionKind::TakeAlways };
    }
    Source asScalarSource(SILValue value) const {
      assert(!isAddress());
      assert(!value.getType().isAddress());
      return { value, FormalType, CastConsumptionKind::TakeAlways };
    }

    Target() = default;
    Target(SILValue address, CanType formalType)
      : Address(address), LoweredType(address.getType()),
        FormalType(formalType) {
      assert(LoweredType.isAddress());
    }
    Target(SILType loweredType, CanType formalType)
      : Address(), LoweredType(loweredType), FormalType(formalType) {
      assert(!loweredType.isAddress());
    }
  };

  class CastEmitter {
    SILBuilder &B;
    SILModule &M;
    ASTContext &Ctx;
    SILLocation Loc;
  public:
    CastEmitter(SILBuilder &B, Module *swiftModule, SILLocation loc)
      : B(B), M(B.getModule()), Ctx(M.getASTContext()), Loc(loc) {}

    Source emitTopLevel(Source source, Target target) {
      unsigned sourceOptDepth = getOptionalDepth(source.FormalType);
      unsigned targetOptDepth = getOptionalDepth(target.FormalType);      

      assert(sourceOptDepth <= targetOptDepth);
      return emitAndInjectIntoOptionals(source, target,
                                        targetOptDepth - sourceOptDepth);
    }

  private:
    const TypeLowering &getTypeLowering(SILType type) {
      return M.Types.getTypeLowering(type);
    }

    SILValue getOwnedScalar(Source source, const TypeLowering &srcTL) {
      assert(!source.isAddress());
      if (!source.shouldTake())
        srcTL.emitRetainValue(B, Loc, source.Value);
      return source.Value;
    }

    Source putOwnedScalar(SILValue scalar, Target target) {
      assert(scalar.getType() == target.LoweredType.getObjectType());
      if (!target.isAddress())
        return target.asScalarSource(scalar);

      auto &targetTL = getTypeLowering(target.LoweredType);
      targetTL.emitStoreOfCopy(B, Loc, scalar, target.Address,
                               IsInitialization);
      return target.asAddressSource();
    }

    Source emitSameType(Source source, Target target) {
      assert(source.FormalType == target.FormalType);

      auto &srcTL = getTypeLowering(source.Value.getType());

      // The destination always wants a +1 value, so make the source
      // +1 if it's a scalar.
      if (!source.isAddress()) {
        source.Value = getOwnedScalar(source, srcTL);
        source.Consumption = CastConsumptionKind::TakeAlways;
      }

      // If we've got a scalar and want a scalar, the source is
      // exactly right.
      if (!target.isAddress() && !source.isAddress())
        return source;

      // If the destination wants a non-address value, load
      if (!target.isAddress()) {
        SILValue value = srcTL.emitLoadOfCopy(B, Loc, source.Value,
                                              source.shouldTake());
        return target.asScalarSource(value);
      }

      if (source.isAddress()) {
        srcTL.emitCopyInto(B, Loc, source.Value, target.Address,
                           source.shouldTake(), IsInitialization);
      } else {
        srcTL.emitStoreOfCopy(B, Loc, source.Value, target.Address,
                              IsInitialization);
      }
      return target.asAddressSource();
    }

    Source emit(Source source, Target target) {
      if (source.FormalType == target.FormalType)
        return emitSameType(source, target);

      // Handle subtype conversions involving optionals.
      OptionalTypeKind sourceOptKind;
      if (auto sourceObjectType =
            source.FormalType.getAnyOptionalObjectType(sourceOptKind)) {
        return emitOptionalToOptional(source, sourceOptKind, sourceObjectType,
                                      target);
      }
      assert(!target.FormalType.getAnyOptionalObjectType());

      // The only other thing we return WillSucceed for currently is
      // an upcast.
      // FIXME: Upcasts between existential metatypes are not handled yet.
      // We should generate for it:
      // %openedSrcMetatype = open_existential srcMetatype
      // init_existental dstMetatype, %openedSrcMetatype
      auto &srcTL = getTypeLowering(source.Value.getType());
      SILValue value;
      if (source.isAddress()) {
        value = srcTL.emitLoadOfCopy(B, Loc, source.Value, source.shouldTake());
      } else {
        value = getOwnedScalar(source, srcTL);
      }
      value = B.createUpcast(Loc, value, target.LoweredType.getObjectType());
      return putOwnedScalar(value, target);
    }

    Source emitAndInjectIntoOptionals(Source source, Target target,
                                      unsigned depth) {
      if (depth == 0)
        return emit(source, target);

      // Recurse.
      EmitSomeState state;
      Target objectTarget = prepareForEmitSome(target, state);
      Source objectSource =
        emitAndInjectIntoOptionals(source, objectTarget, depth - 1);
      return emitSome(objectSource, target, state);
    }

    Source emitOptionalToOptional(Source source,
                                  OptionalTypeKind sourceOptKind,
                                  CanType sourceObjectType,
                                  Target target) {
      // Switch on the incoming value.
      SILBasicBlock *contBB = B.splitBlockForFallthrough();
      SILBasicBlock *noneBB = B.splitBlockForFallthrough();
      SILBasicBlock *someBB = B.splitBlockForFallthrough();

      // Emit the switch.
      std::pair<EnumElementDecl*, SILBasicBlock*> cases[] = {
        { Ctx.getOptionalSomeDecl(sourceOptKind), someBB },
        { Ctx.getOptionalNoneDecl(sourceOptKind), noneBB },
      };
      if (source.isAddress()) {
        B.createSwitchEnumAddr(Loc, source.Value, /*default*/ nullptr, cases);
      } else {
        B.createSwitchEnum(Loc, source.Value, /*default*/ nullptr, cases);
      }

      // Create the Some block, which recurses.
      B.setInsertionPoint(someBB);
      {
        auto sourceSomeDecl = Ctx.getOptionalSomeDecl(sourceOptKind);

        SILType loweredSourceObjectType =
          source.Value.getType().getEnumElementType(sourceSomeDecl, M);

        // Form the target for the optional object.
        EmitSomeState state;
        Target objectTarget = prepareForEmitSome(target, state);

        // Form the source value.
        AllocStackInst *sourceTemp = nullptr;
        Source objectSource;
        if (source.isAddress()) {
          // TODO: add an instruction for non-destructively getting a
          // specific element's data.
          SILValue sourceAddr = source.Value;
          if (!source.shouldTake()) {
            sourceTemp = B.createAllocStack(Loc,
                                       sourceAddr.getType().getObjectType());
            sourceAddr = sourceTemp->getAddressResult();
            B.createCopyAddr(Loc, source.Value, sourceAddr, IsNotTake,
                             IsInitialization);
          }
          sourceAddr = B.createUncheckedTakeEnumDataAddr(Loc, sourceAddr,
                                    sourceSomeDecl, loweredSourceObjectType);
          objectSource = Source(sourceAddr, sourceObjectType,
                                CastConsumptionKind::TakeAlways);
        } else {
          SILValue sourceObjectValue =
            new (M) SILArgument(someBB, loweredSourceObjectType);
          objectSource = Source(sourceObjectValue, sourceObjectType,
                                source.Consumption);
        }

        Source resultObject = emit(objectSource, objectTarget);

        // Deallocate the source temporary if we needed one.
        if (sourceTemp) {
          B.createDeallocStack(Loc, sourceTemp->getContainerResult());
        }

        Source result = emitSome(resultObject, target, state);
        assert(result.isAddress() == target.isAddress());
        if (target.isAddress()) {
          B.createBranch(Loc, contBB);
        } else {
          B.createBranch(Loc, contBB, { result.Value });
        }
      }

      // Create the None block.
      B.setInsertionPoint(noneBB);
      {
        Source result = emitNone(target);
        assert(result.isAddress() == target.isAddress());
        if (target.isAddress()) {
          B.createBranch(Loc, contBB);
        } else {
          B.createBranch(Loc, contBB, { result.Value });
        }
      }

      // Continuation block.
      B.setInsertionPoint(contBB);
      if (target.isAddress()) {
        return target.asAddressSource();
      } else {
        SILValue result = new (M) SILArgument(contBB, target.LoweredType);
        return target.asScalarSource(result);
      }
    }

    struct EmitSomeState {
      EnumElementDecl *SomeDecl;
    };

    Target prepareForEmitSome(Target target, EmitSomeState &state) {
      OptionalTypeKind optKind;
      auto objectType = target.FormalType.getAnyOptionalObjectType(optKind);
      assert(objectType && "emitting Some into non-optional type");

      auto someDecl = Ctx.getOptionalSomeDecl(optKind);
      state.SomeDecl = someDecl;

      SILType loweredObjectType =
        target.LoweredType.getEnumElementType(someDecl, M);

      if (target.isAddress()) {
        SILValue objectAddr =
          B.createInitEnumDataAddr(Loc, target.Address, someDecl,
                                   loweredObjectType);
        return { objectAddr, objectType };
      } else {
        return { loweredObjectType, objectType };
      }
    }

    Source emitSome(Source source, Target target, EmitSomeState &state) {
      // If our target is an address, prepareForEmitSome should have set this
      // up so that we emitted directly into 
      if (target.isAddress()) {
        B.createInjectEnumAddr(Loc, target.Address, state.SomeDecl);
        return target.asAddressSource();
      } else {
        auto &srcTL = getTypeLowering(source.Value.getType());
        auto sourceObject = getOwnedScalar(source, srcTL);
        auto source = B.createEnum(Loc, sourceObject, state.SomeDecl,
                                   target.LoweredType);
        return target.asScalarSource(source);
      }
    }

    Source emitNone(Target target) {
      OptionalTypeKind optKind;
      auto objectType = target.FormalType.getAnyOptionalObjectType(optKind);
      assert(objectType && "emitting None into non-optional type");
      (void) objectType;

      auto noneDecl = Ctx.getOptionalNoneDecl(optKind);
      
      if (target.isAddress()) {
        B.createInjectEnumAddr(Loc, target.Address, noneDecl);
        return target.asAddressSource();
      } else {
        SILValue res = B.createEnum(Loc, nullptr, noneDecl, target.LoweredType);
        return target.asScalarSource(res);
      }
    }
  };
}

/// Emit an unconditional scalar cast that's known to succeed.
SILValue
swift::emitSuccessfulScalarUnconditionalCast(SILBuilder &B, Module *M,
                                             SILLocation loc, SILValue value,
                                             SILType loweredTargetType,
                                             CanType sourceType,
                                             CanType targetType,
                                             SILInstruction *existingCast) {
  assert(classifyDynamicCast(M, sourceType, targetType)
           == DynamicCastFeasibility::WillSucceed);

  // Casts to/from existential types cannot be further improved.
  if (sourceType.isAnyExistentialType() ||
      targetType.isAnyExistentialType()) {
    if (existingCast)
      // Indicate that the existing cast cannot be further improved.
      return SILValue();

    llvm_unreachable("Casts to/from existentials are not supported yet");
  }

  // Fast path changes that don't change the type.
  if (sourceType == targetType)
    return value;

  Source source(value, sourceType, CastConsumptionKind::TakeAlways);
  Target target(loweredTargetType, targetType);
  Source result = CastEmitter(B, M, loc).emitTopLevel(source, target);
  assert(!result.isAddress());
  assert(result.Value.getType() == loweredTargetType);
  assert(result.Consumption == CastConsumptionKind::TakeAlways);
  return result.Value;
}

bool swift::emitSuccessfulIndirectUnconditionalCast(SILBuilder &B, Module *M,
                                                    SILLocation loc,
                                               CastConsumptionKind consumption,
                                                    SILValue src,
                                                    CanType sourceType,
                                                    SILValue dest,
                                                    CanType targetType,
                                                    SILInstruction *existingCast) {
  assert(classifyDynamicCast(M, sourceType, targetType)
           == DynamicCastFeasibility::WillSucceed);

  assert(src.getType().isAddress());
  assert(dest.getType().isAddress());

  // Casts between the same types can be always handled here.
  // Casts from non-existentials into existentials and
  // vice-versa cannot be improved yet.
  // Casts between a value type and a class cannot be optimized.
  // Therefore generate a simple unconditional_checked_cast_aadr.

  if (src.getType() != dest.getType())
  if (src.getType().isAnyExistentialType() !=
      dest.getType().isAnyExistentialType() ||
      !(src.getType().getClassOrBoundGenericClass() &&
       dest.getType().getClassOrBoundGenericClass())) {
    // If there is an existing cast with the same arguments,
    // indicate we cannot improve it.
    if (existingCast) {
      auto *UCCAI = dyn_cast<UnconditionalCheckedCastAddrInst>(existingCast);
      if (UCCAI && UCCAI->getSrc() == src && UCCAI->getDest() == dest &&
          UCCAI->getSourceType() == sourceType &&
          UCCAI->getTargetType() == targetType &&
          UCCAI->getConsumptionKind() == consumption) {
        // Indicate that the existing cast cannot be further improved.
        return false;
      }
    }

    B.createUnconditionalCheckedCastAddr(loc, consumption, src, sourceType,
                                         dest, targetType);
    return true;
  }

  Source source(src, sourceType, consumption);
  Target target(dest, targetType);
  Source result = CastEmitter(B, M, loc).emitTopLevel(source, target);
  assert(result.isAddress());
  assert(result.Value == dest);
  assert(result.Consumption == CastConsumptionKind::TakeAlways);
  (void) result;
  return true;
}

/// Can the given cast be performed by the scalar checked-cast
/// instructions?
///
/// CAUTION: if you introduce bridging conversions to the set of
/// things handleable by the scalar checked casts --- and that's not
/// totally unreasonable --- you will need to make the scalar checked
/// casts take a cast consumption kind.
bool swift::canUseScalarCheckedCastInstructions(SILModule &M,
                                                CanType sourceType,
                                                CanType targetType) {
  // Look through one level of optionality on the source.
  auto objectType = sourceType;
  if (auto type = objectType.getAnyOptionalObjectType())
    objectType = type;

  // Casting to NSError needs to go through the indirect-cast case,
  // since it may conform to ErrorType and require ErrorType-to-NSError
  // bridging, unless we can statically see that the source type inherits
  // NSError.
  
  // A class-constrained archetype may be bound to NSError, unless it has a
  // non-NSError superclass constraint. Casts to archetypes thus must always be
  // indirect.
  if (auto archetype = targetType->getAs<ArchetypeType>()) {
    auto super = archetype->getSuperclass();
    if (super.isNull())
      return false;
    // A base class constraint that isn't NSError rules out the archetype being
    // bound to NSError.
    if (auto nserror = M.Types.getNSErrorType())
      return !super->isEqual(nserror);
    // If NSError wasn't loaded, any base class constraint must not be NSError.
    return true;
  }
  
  if (targetType == M.Types.getNSErrorType()) {
    // If we statically know the target is an NSError subclass, then the cast
    // can go through the scalar path (and it's trivially true so can be
    // killed).
    return targetType->isSuperclassOf(objectType, nullptr);
  }
  
  // Three supported cases:
  // - metatype to metatype
  // - metatype to object
  // - object to object
  if ((objectType.isAnyClassReferenceType() || isa<AnyMetatypeType>(objectType))
      && targetType->isAnyClassReferenceType())
    return true;

  if (isa<AnyMetatypeType>(objectType) && isa<AnyMetatypeType>(targetType))
    return true;
  
  // Otherwise, we need to use the general indirect-cast functions.
  return false;
}

/// Carry out the operations required for an indirect conditional cast
/// using a scalar cast operation.
void swift::
emitIndirectConditionalCastWithScalar(SILBuilder &B, Module *M,
                                      SILLocation loc,
                                      CastConsumptionKind consumption,
                                      SILValue src, CanType sourceType,
                                      SILValue dest, CanType targetType,
                                      SILBasicBlock *indirectSuccBB,
                                      SILBasicBlock *indirectFailBB) {
  assert(canUseScalarCheckedCastInstructions(B.getModule(),
                                             sourceType, targetType));

  // We only need a different failure block if the cast consumption
  // requires us to destroy the source value.
  SILBasicBlock *scalarFailBB;
  if (!shouldDestroyOnFailure(consumption)) {
    scalarFailBB = indirectFailBB;
  } else {
    scalarFailBB = B.splitBlockForFallthrough();
  }

  // We always need a different success block.
  SILBasicBlock *scalarSuccBB = B.splitBlockForFallthrough();

  auto &srcTL = B.getModule().Types.getTypeLowering(src.getType());

  // Always take; this works under an assumption that retaining the
  // result is equivalent to retaining the source.  That means that
  // these casts would not be appropriate for bridging-like conversions.
  SILValue srcValue = srcTL.emitLoadOfCopy(B, loc, src, IsTake);

  SILType targetValueType = dest.getType().getObjectType();
  B.createCheckedCastBranch(loc, /*exact*/ false, srcValue, targetValueType,
                            scalarSuccBB, scalarFailBB);

  // Emit the success block.
  B.setInsertionPoint(scalarSuccBB); {
    auto &targetTL = B.getModule().Types.getTypeLowering(targetValueType);
    SILValue succValue =
      new (B.getModule()) SILArgument(scalarSuccBB, targetValueType);
    if (!shouldTakeOnSuccess(consumption))
      targetTL.emitRetainValue(B, loc, succValue);
    targetTL.emitStoreOfCopy(B, loc, succValue, dest, IsInitialization);
    B.createBranch(loc, indirectSuccBB);
  }

  // Emit the failure block.
  if (shouldDestroyOnFailure(consumption)) {
    B.setInsertionPoint(scalarFailBB);
    srcTL.emitReleaseValue(B, loc, srcValue);
    B.createBranch(loc, indirectFailBB);
  }
}
