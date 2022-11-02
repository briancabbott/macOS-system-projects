//===--- GenEnum.cpp - Swift IR Generation For 'enum' Types -------------===//
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
//  This file implements IR generation for algebraic data types (ADTs,
//  or 'enum' types) in Swift.  This includes creating the IR type as
//  well as emitting the basic access operations.
//
//  An abstract enum value consists of a payload portion, sized to hold the
//  largest possible payload value, and zero or more tag bits, to select
//  between cases. The payload might be zero sized, if the enum consists
//  entirely of empty cases, or there might not be any tag bits, if the
//  lowering is able to pack all of them into the payload itself.
//
//  An abstract enum value can be thought of as supporting three primary
//  operations:
//  1) GetEnumTag: Getting the current case of an enum value.
//  2) ProjectEnumPayload: Destructively stripping tag bits from the enum
//     value, leaving behind the payload value, if any, at the beginning of
//     the old enum value.
//  3) InjectEnumCase: Given a payload value placed inside an uninitialized
//     enum value, inject tag bits for a specified case, producing a
//     fully-formed enum value.
//
//  Enum type lowering needs to derive implementations of the above for
//  an enum type. It does this by classifying the enum along two
//  orthogonal axes: the loadability of the enum value, and the payload
//  implementation strategy.
//
//  The first axis deals with special behavior of fixed-size loadable and
//  address-only enums. Fixed-size loadable enums are represented as
//  explosions of values, the address-only lowering uses more general
//  operations.
//
//  The second axis essentially deals with how the case discriminator, or
//  tag, is represented within an enum value. Payload and no-payload cases
//  are counted and the enum is classified into the following categories:
//
//  1) If the enum only has one case, it can be lowered as the type of the
//     case itself, and no tag bits are necessary.
//
//  2) If the enum only has no-payload cases, only tag bits are necessary,
//     with the cases mapping to integers, in AST order.
//
//  3) If the enum has a single payload case and one or more no-payload cases,
//     we attempt to map the no-payload cases to extra inhabitants of the
//     payload type. If enough extra inhabitants are available, no tag bits
//     are needed, otherwise more are added as necessary.
//
//     Extra inhabitant information can be obtained at runtime through the
//     value witness table, so there are no layout differences between a
//     generic enum type and a substituted type.
//
//     Since each extra inhabitant corresponds to a specific bit pattern
//     that is known to be invalid given the payload type, projection of
//     the payload value is a no-op.
//
//     For example, if the payload type is a single retainable pointer,
//     the first 4KB of memory addresses are known not to correspond to
//     valid memory, and so the enum can contain up to 4095 empty cases
//     in addition to the payload case before any additional storage is
//     required.
//
//  4) If the enum has multiple payload cases, the layout attempts to pack
//     the tag bits into the common spare bits of all of the payload cases.
//
//     Spare bit information is not available at runtime, so spare bits can
//     only be used if all payload types are fixed-size in all resilience
//     domains.
//
//     Since spare bits correspond to bits which are known to be zero for
//     all valid representations of the payload type, they must be stripped
//     out before the payload value can be manipulated. This means spare
//     bits cannot be used if the payload value is address-only, because
//     there is no way to strip the spare bits in that case without
//     modifying the value in-place.
//
//     For example, on a 64-bit platform, the least significant 3 bits of
//     a retainable pointer are always zero, so a multi-payload enum can
//     have up to 8 retainable pointer payload cases before any additional
//     storage is required.
//
//  Indirect enum cases are implemented by substituting in a SILBox type
//  for the payload, resulting in a fixed-size lowering for recursive
//  enums.
//
//  For all lowerings except ResilientEnumImplStrategy, the primary enum
//  operations are open-coded at usage sites. Resilient enums are accessed
//  by invoking the value witnesses for these operations.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "enum-layout"
#include "llvm/Support/Debug.h"

#include "GenEnum.h"

#include "swift/AST/Types.h"
#include "swift/AST/Decl.h"
#include "swift/AST/IRGenOptions.h"
#include "swift/Basic/Fallthrough.h"
#include "swift/SIL/SILModule.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/Analysis/CFG.h"

#include "IRGenModule.h"
#include "LoadableTypeInfo.h"
#include "NonFixedTypeInfo.h"
#include "ResilientTypeInfo.h"
#include "GenMeta.h"
#include "GenProto.h"
#include "GenType.h"
#include "IRGenDebugInfo.h"
#include "ScalarTypeInfo.h"
#include "StructLayout.h"

using namespace swift;
using namespace irgen;

SpareBitVector getBitVectorFromAPInt(const APInt &bits, unsigned startBit = 0) {
  if (startBit == 0) {
    return SpareBitVector::fromAPInt(bits);
  }
  SpareBitVector result;
  result.appendClearBits(startBit);
  result.append(SpareBitVector::fromAPInt(bits));
  return result;
}

void irgen::EnumImplStrategy::initializeFromParams(IRGenFunction &IGF,
                                                   Explosion &params,
                                                   Address dest,
                                                   SILType T) const {
  if (TIK >= Loadable)
    return initialize(IGF, params, dest);
  Address src = TI->getAddressForPointer(params.claimNext());
  TI->initializeWithTake(IGF, dest, src, T);
}

llvm::Constant *EnumImplStrategy::emitCaseNames() const {
  // Build the list of case names, payload followed by no-payload.
  llvm::SmallString<64> fieldNames;
  for (auto &payloadCase : getElementsWithPayload()) {
    fieldNames.append(payloadCase.decl->getName().str());
    fieldNames.push_back('\0');
  }
  for (auto &noPayloadCase : getElementsWithNoPayload()) {
    fieldNames.append(noPayloadCase.decl->getName().str());
    fieldNames.push_back('\0');
  }
  // The final null terminator is provided by getAddrOfGlobalString.
  return IGM.getAddrOfGlobalString(fieldNames);
}

llvm::Value *irgen::EnumImplStrategy::
loadRefcountedPtr(IRGenFunction &IGF, SourceLoc loc, Address addr) const {
  IGF.IGM.error(loc, "Can only load from an address of an optional "
                "reference.");
  llvm::report_fatal_error("loadRefcountedPtr: Invalid SIL in IRGen");
}

Address
irgen::EnumImplStrategy::projectDataForStore(IRGenFunction &IGF,
                                             EnumElementDecl *elt,
                                             Address enumAddr)
const {
  auto payloadI = std::find_if(ElementsWithPayload.begin(),
                               ElementsWithPayload.end(),
     [&](const Element &e) { return e.decl == elt; });

  // Empty payload addresses can be left undefined.
  if (payloadI == ElementsWithPayload.end()) {
    return IGF.getTypeInfoForUnlowered(elt->getArgumentType())
      .getUndefAddress();
  }

  // Payloads are all placed at the beginning of the value.
  return IGF.Builder.CreateBitCast(enumAddr,
                           payloadI->ti->getStorageType()->getPointerTo());
}

Address
irgen::EnumImplStrategy::destructiveProjectDataForLoad(IRGenFunction &IGF,
                                                       SILType enumType,
                                                       Address enumAddr,
                                                       EnumElementDecl *Case)
const {
  auto payloadI = std::find_if(ElementsWithPayload.begin(),
                           ElementsWithPayload.end(),
                           [&](const Element &e) { return e.decl == Case; });

  // Empty payload addresses can be left undefined.
  if (payloadI == ElementsWithPayload.end()) {
    return IGF.getTypeInfoForUnlowered(Case->getArgumentType())
      .getUndefAddress();
  }

  destructiveProjectDataForLoad(IGF, enumType, enumAddr);

  // Payloads are all placed at the beginning of the value.
  return IGF.Builder.CreateBitCast(enumAddr,
                           payloadI->ti->getStorageType()->getPointerTo());
}

unsigned
irgen::EnumImplStrategy::getTagIndex(EnumElementDecl *Case) const {
  unsigned tagIndex = 0;
  for (auto &payload : ElementsWithPayload) {
    if (payload.decl == Case)
      return tagIndex;
    ++tagIndex;
  }
  for (auto &payload : ElementsWithNoPayload) {
    if (payload.decl == Case)
      return tagIndex;
    ++tagIndex;
  }
  llvm_unreachable("couldn't find case");
}

namespace {
  /// Implementation strategy for singleton enums, with zero or one cases.
  class SingletonEnumImplStrategy final : public EnumImplStrategy {
    bool needsPayloadSizeInMetadata() const override { return false; }
    
    const TypeInfo *getSingleton() const {
      return ElementsWithPayload.empty() ? nullptr : ElementsWithPayload[0].ti;
    }

    const FixedTypeInfo *getFixedSingleton() const {
      return cast_or_null<FixedTypeInfo>(getSingleton());
    }

    const LoadableTypeInfo *getLoadableSingleton() const {
      return cast_or_null<LoadableTypeInfo>(getSingleton());
    }

    Address getSingletonAddress(IRGenFunction &IGF, Address addr) const {
      return IGF.Builder.CreateBitCast(addr,
                             getSingleton()->getStorageType()->getPointerTo());
    }

    SILType getSingletonType(IRGenModule &IGM, SILType T) const {
      assert(!ElementsWithPayload.empty());
      
      return T.getEnumElementType(ElementsWithPayload[0].decl,
                                  *IGM.SILMod);
    }

  public:
    SingletonEnumImplStrategy(IRGenModule &IGM,
                              TypeInfoKind tik,
                              IsFixedSize_t alwaysFixedSize,
                              unsigned NumElements,
                              std::vector<Element> &&WithPayload,
                              std::vector<Element> &&WithNoPayload)
      : EnumImplStrategy(IGM, tik, alwaysFixedSize,
                         NumElements,
                         std::move(WithPayload),
                         std::move(WithNoPayload))
    {
      assert(NumElements <= 1);
      assert(ElementsWithPayload.size() <= 1);
    }

    TypeInfo *completeEnumTypeLayout(TypeConverter &TC,
                                     SILType Type,
                                     EnumDecl *theEnum,
                                     llvm::StructType *enumTy) override;

    llvm::Value *
    emitGetEnumTag(IRGenFunction &IGF, SILType T, Address enumAddr)
    const override {
      return llvm::ConstantInt::get(IGF.IGM.Int32Ty, 0);
    }

    llvm::Value *
    emitValueCaseTest(IRGenFunction &IGF,
                      Explosion &value,
                      EnumElementDecl *Case) const override {
      value.claim(getExplosionSize());
      return IGF.Builder.getInt1(true);
    }
    llvm::Value *
    emitIndirectCaseTest(IRGenFunction &IGF, SILType T,
                         Address enumAddr,
                         EnumElementDecl *Case) const override {
      return IGF.Builder.getInt1(true);
    }

    void emitSingletonSwitch(IRGenFunction &IGF,
                    ArrayRef<std::pair<EnumElementDecl*,
                                       llvm::BasicBlock*>> dests,
                    llvm::BasicBlock *defaultDest) const {
      // No dispatch necessary. Branch straight to the destination.
      assert(dests.size() <= 1 && "impossible switch table for singleton enum");
      llvm::BasicBlock *dest = dests.size() == 1
        ? dests[0].second : defaultDest;
      IGF.Builder.CreateBr(dest);
    }

    void emitValueSwitch(IRGenFunction &IGF,
                         Explosion &value,
                         ArrayRef<std::pair<EnumElementDecl*,
                                            llvm::BasicBlock*>> dests,
                         llvm::BasicBlock *defaultDest) const override {
      value.claim(getExplosionSize());
      emitSingletonSwitch(IGF, dests, defaultDest);
    }

    void emitIndirectSwitch(IRGenFunction &IGF,
                            SILType T,
                            Address addr,
                            ArrayRef<std::pair<EnumElementDecl*,
                                               llvm::BasicBlock*>> dests,
                            llvm::BasicBlock *defaultDest) const override {
      emitSingletonSwitch(IGF, dests, defaultDest);
    }

    void emitValueProject(IRGenFunction &IGF,
                          Explosion &in,
                          EnumElementDecl *theCase,
                          Explosion &out) const override {
      // The projected value is the payload.
      if (getLoadableSingleton())
        getLoadableSingleton()->reexplode(IGF, in, out);
    }

    void emitValueInjection(IRGenFunction &IGF,
                            EnumElementDecl *elt,
                            Explosion &params,
                            Explosion &out) const override {
      // If the element carries no data, neither does the injection.
      // Otherwise, the result is identical.
      if (getLoadableSingleton())
        getLoadableSingleton()->reexplode(IGF, params, out);
    }

    void destructiveProjectDataForLoad(IRGenFunction &IGF,
                                       SILType T,
                                       Address enumAddr) const override {
      // No tag, nothing to do.
    }

    void storeTag(IRGenFunction &IGF,
                  SILType T,
                  Address enumAddr,
                  EnumElementDecl *Case) const override {
      // No tag, nothing to do.
    }

    void getSchema(ExplosionSchema &schema) const override {
      if (!getSingleton()) return;
      // If the payload is loadable, forward its explosion schema.
      if (TIK >= Loadable)
        return getSingleton()->getSchema(schema);
      // Otherwise, use an indirect aggregate schema with our storage
      // type.
      schema.add(ExplosionSchema::Element::forAggregate(getStorageType(),
                                      getSingleton()->getBestKnownAlignment()));
    }

    unsigned getExplosionSize() const override {
      if (!getLoadableSingleton()) return 0;
      return getLoadableSingleton()->getExplosionSize();
    }

    void loadAsCopy(IRGenFunction &IGF, Address addr,
                    Explosion &e) const override {
      if (!getLoadableSingleton()) return;
      getLoadableSingleton()->loadAsCopy(IGF, getSingletonAddress(IGF, addr),e);
    }

    void loadForSwitch(IRGenFunction &IGF, Address addr, Explosion &e) const {
      // Switching on a singleton does not require a value.
      return;
    }

    void loadAsTake(IRGenFunction &IGF, Address addr,
                    Explosion &e) const override {
      if (!getLoadableSingleton()) return;
      getLoadableSingleton()->loadAsTake(IGF, getSingletonAddress(IGF, addr),e);
    }

    void assign(IRGenFunction &IGF, Explosion &e, Address addr) const override {
      if (!getLoadableSingleton()) return;
      getLoadableSingleton()->assign(IGF, e, getSingletonAddress(IGF, addr));
    }

    void assignWithCopy(IRGenFunction &IGF, Address dest, Address src,
                        SILType T) const override {
      if (!getSingleton()) return;
      dest = getSingletonAddress(IGF, dest);
      src = getSingletonAddress(IGF, src);
      getSingleton()->assignWithCopy(IGF, dest, src,
                                     getSingletonType(IGF.IGM, T));
    }

    void assignWithTake(IRGenFunction &IGF, Address dest, Address src,
                        SILType T) const override {
      if (!getSingleton()) return;
      dest = getSingletonAddress(IGF, dest);
      src = getSingletonAddress(IGF, src);
      getSingleton()->assignWithTake(IGF, dest, src,
                                     getSingletonType(IGF.IGM, T));
    }

    void initialize(IRGenFunction &IGF, Explosion &e,
                    Address addr) const override {
      if (!getLoadableSingleton()) return;
      getLoadableSingleton()->initialize(IGF, e, getSingletonAddress(IGF, addr));
    }

    void initializeWithCopy(IRGenFunction &IGF, Address dest, Address src,
                            SILType T)
    const override {
      if (!getSingleton()) return;
      dest = getSingletonAddress(IGF, dest);
      src = getSingletonAddress(IGF, src);
      getSingleton()->initializeWithCopy(IGF, dest, src,
                                         getSingletonType(IGF.IGM, T));
    }

    void initializeWithTake(IRGenFunction &IGF, Address dest, Address src,
                            SILType T)
    const override {
      if (!getSingleton()) return;
      dest = getSingletonAddress(IGF, dest);
      src = getSingletonAddress(IGF, src);
      getSingleton()->initializeWithTake(IGF, dest, src,
                                         getSingletonType(IGF.IGM, T));
    }

    void reexplode(IRGenFunction &IGF, Explosion &src, Explosion &dest)
    const override {
      if (getLoadableSingleton()) getLoadableSingleton()->reexplode(IGF, src, dest);
    }

    void copy(IRGenFunction &IGF, Explosion &src, Explosion &dest)
    const override {
      if (getLoadableSingleton()) getLoadableSingleton()->copy(IGF, src, dest);
    }

    void consume(IRGenFunction &IGF, Explosion &src) const override {
      if (getLoadableSingleton()) getLoadableSingleton()->consume(IGF, src);
    }

    void fixLifetime(IRGenFunction &IGF, Explosion &src) const override {
      if (getLoadableSingleton()) getLoadableSingleton()->fixLifetime(IGF, src);
    }

    void destroy(IRGenFunction &IGF, Address addr, SILType T) const override {
      if (getSingleton() && !getSingleton()->isPOD(ResilienceScope::Component))
        getSingleton()->destroy(IGF, getSingletonAddress(IGF, addr),
                                getSingletonType(IGF.IGM, T));
    }

    void packIntoEnumPayload(IRGenFunction &IGF,
                             EnumPayload &payload,
                             Explosion &in,
                             unsigned offset) const override {
      if (getLoadableSingleton())
        return getLoadableSingleton()->packIntoEnumPayload(IGF, payload,
                                                           in, offset);
    }

    void unpackFromEnumPayload(IRGenFunction &IGF,
                               const EnumPayload &payload,
                               Explosion &dest,
                               unsigned offset) const override {
      if (!getLoadableSingleton()) return;
      getLoadableSingleton()->unpackFromEnumPayload(IGF, payload, dest, offset);
    }

    void initializeMetadata(IRGenFunction &IGF,
                            llvm::Value *metadata,
                            llvm::Value *vwtable,
                            SILType T) const override {
      // Fixed-size enums don't need dynamic witness table initialization.
      if (TIK >= Fixed) return;

      assert(!ElementsWithPayload.empty() &&
             "empty singleton enum should not be dynamic!");

      // Get the value witness table for the element.
      SILType eltTy = T.getEnumElementType(ElementsWithPayload[0].decl,
                                           *IGM.SILMod);
      llvm::Value *eltLayout = IGF.emitTypeLayoutRef(eltTy);

      Address vwtAddr(vwtable, IGF.IGM.getPointerAlignment());
      Address eltLayoutAddr(eltLayout, IGF.IGM.getPointerAlignment());

      auto getWitnessDestAndSrc = [&](ValueWitness witness,
                                      Address *dest,
                                      Address *src) {
        *dest = IGF.Builder.CreateConstArrayGEP(vwtAddr,
                                   unsigned(witness), IGF.IGM.getPointerSize());
        *src = IGF.Builder.CreateConstArrayGEP(eltLayoutAddr,
          unsigned(witness) - unsigned(ValueWitness::First_TypeLayoutWitness),
          IGF.IGM.getPointerSize());
      };

      auto copyWitnessFromElt = [&](ValueWitness witness) {
        Address dest, src;
        getWitnessDestAndSrc(witness, &dest, &src);
        auto val = IGF.Builder.CreateLoad(src);
        IGF.Builder.CreateStore(val, dest);
      };

      copyWitnessFromElt(ValueWitness::Size);
      copyWitnessFromElt(ValueWitness::Stride);

      // Copy flags over, adding HasEnumWitnesses flag
      auto copyFlagsFromElt = [&](ValueWitness witness) -> llvm::Value* {
        Address dest, src;
        getWitnessDestAndSrc(witness, &dest, &src);
        auto val = IGF.Builder.CreateLoad(src);
        auto ewFlag = IGF.Builder.CreatePtrToInt(val, IGF.IGM.SizeTy);
        auto ewMask
          = IGF.IGM.getSize(Size(ValueWitnessFlags::HasEnumWitnesses));
        ewFlag = IGF.Builder.CreateOr(ewFlag, ewMask);
        auto flag = IGF.Builder.CreateIntToPtr(ewFlag,
                                              dest.getType()->getElementType());
        IGF.Builder.CreateStore(flag, dest);
        return val;
      };

      auto flags = copyFlagsFromElt(ValueWitness::Flags);

      // If the original type had extra inhabitants, carry over its
      // extra inhabitant flags.
      auto xiBB = llvm::BasicBlock::Create(IGF.IGM.getLLVMContext());
      auto noXIBB = llvm::BasicBlock::Create(IGF.IGM.getLLVMContext());

      auto xiFlag = IGF.Builder.CreatePtrToInt(flags, IGF.IGM.SizeTy);
      auto xiMask
        = IGF.IGM.getSize(Size(ValueWitnessFlags::Enum_HasExtraInhabitants));
      xiFlag = IGF.Builder.CreateAnd(xiFlag, xiMask);
      auto xiBool = IGF.Builder.CreateICmpNE(xiFlag,
                                             IGF.IGM.getSize(Size(0)));
      IGF.Builder.CreateCondBr(xiBool, xiBB, noXIBB);

      IGF.Builder.emitBlock(xiBB);
      copyWitnessFromElt(ValueWitness::ExtraInhabitantFlags);
      IGF.Builder.CreateBr(noXIBB);

      IGF.Builder.emitBlock(noXIBB);
    }

    bool mayHaveExtraInhabitants(IRGenModule &IGM) const override {
      // FIXME: Hold off on registering extra inhabitants for dynamic enums
      // until initializeMetadata handles them.
      if (!getSingleton())
        return false;
      return getSingleton()->mayHaveExtraInhabitants(IGM);
    }

    llvm::Value *getExtraInhabitantIndex(IRGenFunction &IGF,
                                         Address src, SILType T)
    const override {
      if (!getSingleton()) {
        // Any empty value is a valid value.
        return llvm::ConstantInt::getSigned(IGF.IGM.Int32Ty, -1);
      }

      return getSingleton()->getExtraInhabitantIndex(IGF,
                                             getSingletonAddress(IGF, src),
                                             getSingletonType(IGF.IGM, T));
    }

    void storeExtraInhabitant(IRGenFunction &IGF,
                              llvm::Value *index,
                              Address dest, SILType T) const override {
      if (!getSingleton()) {
        // Nothing to store for empty singletons.
        return;
      }
      getSingleton()->storeExtraInhabitant(IGF, index,
                                           getSingletonAddress(IGF, dest),
                                           getSingletonType(IGF.IGM, T));
    }

    unsigned getFixedExtraInhabitantCount(IRGenModule &IGM) const override {
      assert(TIK >= Fixed);
      if (!getSingleton())
        return 0;
      return getFixedSingleton()->getFixedExtraInhabitantCount(IGM);
    }

    APInt
    getFixedExtraInhabitantValue(IRGenModule &IGM,
                                 unsigned bits,
                                 unsigned index) const override {
      assert(TIK >= Fixed);
      assert(getSingleton() && "empty singletons have no extra inhabitants");
      return getFixedSingleton()
        ->getFixedExtraInhabitantValue(IGM, bits, index);
    }
    
    APInt
    getFixedExtraInhabitantMask(IRGenModule &IGM) const override {
      assert(TIK >= Fixed);
      assert(getSingleton() && "empty singletons have no extra inhabitants");
      return getFixedSingleton()->getFixedExtraInhabitantMask(IGM);
    }

    ClusteredBitVector getTagBitsForPayloads() const override {
      // No tag bits, there's only one payload.
      ClusteredBitVector result;
      if (getSingleton())
        result.appendClearBits(
                        getFixedSingleton()->getFixedSize().getValueInBits());
      return result;
    }

    ClusteredBitVector
    getBitPatternForNoPayloadElement(EnumElementDecl *theCase) const override {
      // There's only a no-payload element if the type is empty.
      return {};
    }

    ClusteredBitVector
    getBitMaskForNoPayloadElements() const override {
      // All bits are significant.
      return ClusteredBitVector::getConstant(
                     cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits(),
                     true);
    }
  };

  /// Implementation strategy for no-payload enums, in other words, 'C-like'
  /// enums where none of the cases have data.
  class NoPayloadEnumImplStrategyBase
    : public SingleScalarTypeInfo<NoPayloadEnumImplStrategyBase,
                                  EnumImplStrategy>
  {
  protected:
    llvm::IntegerType *getDiscriminatorType() const {
      llvm::StructType *Struct = getStorageType();
      return cast<llvm::IntegerType>(Struct->getElementType(0));
    }

    /// Map the given element to the appropriate value in the
    /// discriminator type.
    llvm::ConstantInt *getDiscriminatorIdxConst(EnumElementDecl *target) const {
      int64_t index = getDiscriminatorIndex(target);
      return llvm::ConstantInt::get(getDiscriminatorType(), index);
    }
    

  public:
    NoPayloadEnumImplStrategyBase(IRGenModule &IGM,
                                  TypeInfoKind tik,
                                  IsFixedSize_t alwaysFixedSize,
                                  unsigned NumElements,
                                  std::vector<Element> &&WithPayload,
                                  std::vector<Element> &&WithNoPayload)
      : SingleScalarTypeInfo(IGM, tik, alwaysFixedSize,
                             NumElements,
                             std::move(WithPayload),
                             std::move(WithNoPayload))
    {
      assert(ElementsWithPayload.empty());
      assert(!ElementsWithNoPayload.empty());
    }

    bool needsPayloadSizeInMetadata() const override { return false; }

    llvm::Value *
    emitGetEnumTag(IRGenFunction &IGF, SILType T, Address enumAddr)
    const override {
      Explosion value;
      loadAsTake(IGF, enumAddr, value);
      return value.claimNext();
    }

    llvm::Value *emitValueCaseTest(IRGenFunction &IGF,
                                   Explosion &value,
                                   EnumElementDecl *Case) const override {
      // True if the discriminator matches the specified element.
      llvm::Value *discriminator = value.claimNext();
      return IGF.Builder.CreateICmpEQ(discriminator,
                                      getDiscriminatorIdxConst(Case));
    }

    
    llvm::Value *emitIndirectCaseTest(IRGenFunction &IGF, SILType T,
                                      Address enumAddr,
                                      EnumElementDecl *Case) const override {
      Explosion value;
      loadAsTake(IGF, enumAddr, value);
      return emitValueCaseTest(IGF, value, Case);
    }
    
    void emitValueSwitch(IRGenFunction &IGF,
                         Explosion &value,
                         ArrayRef<std::pair<EnumElementDecl*,
                                            llvm::BasicBlock*>> dests,
                         llvm::BasicBlock *defaultDest) const override {
      llvm::Value *discriminator = value.claimNext();

      // Create an unreachable block for the default if the original SIL
      // instruction had none.
      bool unreachableDefault = false;
      if (!defaultDest) {
        unreachableDefault = true;
        defaultDest = llvm::BasicBlock::Create(IGF.IGM.getLLVMContext());
      }

      auto *i = IGF.Builder.CreateSwitch(discriminator, defaultDest,
                                         dests.size());
      for (auto &dest : dests)
        i->addCase(getDiscriminatorIdxConst(dest.first), dest.second);

      if (unreachableDefault) {
        IGF.Builder.emitBlock(defaultDest);
        IGF.Builder.CreateUnreachable();
      }
    }

    void emitIndirectSwitch(IRGenFunction &IGF,
                            SILType T,
                            Address addr,
                            ArrayRef<std::pair<EnumElementDecl*,
                                               llvm::BasicBlock*>> dests,
                            llvm::BasicBlock *defaultDest) const override {
      Explosion value;
      loadAsTake(IGF, addr, value);
      emitValueSwitch(IGF, value, dests, defaultDest);
    }

    void emitValueProject(IRGenFunction &IGF,
                          Explosion &in,
                          EnumElementDecl *elt,
                          Explosion &out) const override {
      // All of the cases project an empty explosion.
      in.claim(getExplosionSize());
    }

    void emitValueInjection(IRGenFunction &IGF,
                            EnumElementDecl *elt,
                            Explosion &params,
                            Explosion &out) const override {
      out.add(getDiscriminatorIdxConst(elt));
    }

    void destructiveProjectDataForLoad(IRGenFunction &IGF,
                                       SILType T,
                                       Address enumAddr) const override {
      llvm_unreachable("cannot project data for no-payload cases");
    }

    void storeTag(IRGenFunction &IGF,
                  SILType T,
                  Address enumAddr,
                  EnumElementDecl *Case)
    const override {
      llvm::Value *discriminator = getDiscriminatorIdxConst(Case);
      Address discriminatorAddr
        = IGF.Builder.CreateStructGEP(enumAddr, 0, Size(0));
      IGF.Builder.CreateStore(discriminator, discriminatorAddr);
    }

    void initializeMetadata(IRGenFunction &IGF,
                            llvm::Value *metadata,
                            llvm::Value *vwtable,
                            SILType T) const override {
      // No-payload enums are always fixed-size so never need dynamic value
      // witness table initialization.
    }

    /// \group Required for SingleScalarTypeInfo

    llvm::Type *getScalarType() const {
      return getDiscriminatorType();
    }

    static Address projectScalar(IRGenFunction &IGF, Address addr) {
      return IGF.Builder.CreateStructGEP(addr, 0, Size(0));
    }

    void emitScalarRetain(IRGenFunction &IGF, llvm::Value *value) const {}
    void emitScalarRelease(IRGenFunction &IGF, llvm::Value *value) const {}
    void emitScalarFixLifetime(IRGenFunction &IGF, llvm::Value *value) const {}

    void initializeWithTake(IRGenFunction &IGF, Address dest, Address src,
                            SILType T)
    const override {
      // No-payload enums are always POD, so we can always initialize by
      // primitive copy.
      llvm::Value *val = IGF.Builder.CreateLoad(src);
      IGF.Builder.CreateStore(val, dest);
    }

    static constexpr IsPOD_t IsScalarPOD = IsPOD;

    ClusteredBitVector getTagBitsForPayloads() const override {
      // No tag bits; no-payload enums always use fixed representations.
      return ClusteredBitVector::getConstant(
                    cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits(),
                    false);
    }

    ClusteredBitVector
    getBitPatternForNoPayloadElement(EnumElementDecl *theCase) const override {
      auto bits
        = getBitVectorFromAPInt(getDiscriminatorIdxConst(theCase)->getValue());
      bits.extendWithClearBits(cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits());
      return bits;
    }

    ClusteredBitVector
    getBitMaskForNoPayloadElements() const override {
      // All bits are significant.
      return ClusteredBitVector::getConstant(
                       cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits(),
                       true);
    }
    
    APInt
    getFixedExtraInhabitantMask(IRGenModule &IGM) const override {
      return APInt::getAllOnesValue(cast<FixedTypeInfo>(TI)->getFixedSize()
                                      .getValueInBits());
    }
  };

  /// Implementation strategy for native Swift no-payload enums.
  class NoPayloadEnumImplStrategy final
    : public NoPayloadEnumImplStrategyBase
  {
  public:
    NoPayloadEnumImplStrategy(IRGenModule &IGM,
                              TypeInfoKind tik,
                              IsFixedSize_t alwaysFixedSize,
                              unsigned NumElements,
                              std::vector<Element> &&WithPayload,
                              std::vector<Element> &&WithNoPayload)
      : NoPayloadEnumImplStrategyBase(IGM, tik, alwaysFixedSize,
                                      NumElements,
                                      std::move(WithPayload),
                                      std::move(WithNoPayload))
    {
      assert(ElementsWithPayload.empty());
      assert(!ElementsWithNoPayload.empty());
    }

    TypeInfo *completeEnumTypeLayout(TypeConverter &TC,
                                     SILType Type,
                                     EnumDecl *theEnum,
                                     llvm::StructType *enumTy) override;

    // TODO: Support this function also for other enum implementation strategies.
    int64_t getDiscriminatorIndex(EnumElementDecl *target) const override {
      // The elements are assigned discriminators in declaration order.
      // FIXME: using a linear search here is fairly ridiculous.
      unsigned index = 0;
      for (auto elt : target->getParentEnum()->getAllElements()) {
        if (elt == target) break;
        index++;
      }
      return index;
    }

    // TODO: Support this function also for other enum implementation strategies.
    llvm::Value *emitExtractDiscriminator(IRGenFunction &IGF,
                                          Explosion &value) const override {
      return value.claimNext();
    }

    /// \group Extra inhabitants for no-payload enums.

    // No-payload enums have all values above their greatest discriminator
    // value that fit inside their storage size available as extra inhabitants.

    bool mayHaveExtraInhabitants(IRGenModule &IGM) const override {
      return getFixedExtraInhabitantCount(IGM) > 0;
    }

    unsigned getFixedExtraInhabitantCount(IRGenModule &IGM) const override {
      unsigned bits = cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits();
      assert(bits < 32 && "freakishly huge no-payload enum");
      return (1U << bits) - ElementsWithNoPayload.size();
    }

    APInt getFixedExtraInhabitantValue(IRGenModule &IGM,
                                       unsigned bits,
                                       unsigned index) const override {
      unsigned value = index + ElementsWithNoPayload.size();
      return APInt(bits, value);
    }

    llvm::Value *getExtraInhabitantIndex(IRGenFunction &IGF,
                                         Address src, SILType T)
    const override {
      auto &C = IGF.IGM.getLLVMContext();

      // Load the value.
      auto payloadTy = llvm::IntegerType::get(C,
                      cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits());
      src = IGF.Builder.CreateBitCast(src, payloadTy->getPointerTo());
      llvm::Value *val = IGF.Builder.CreateLoad(src);

      // Convert to i32.
      val = IGF.Builder.CreateZExtOrTrunc(val, IGF.IGM.Int32Ty);

      // Subtract the number of cases.
      val = IGF.Builder.CreateSub(val,
              llvm::ConstantInt::get(IGF.IGM.Int32Ty, ElementsWithNoPayload.size()));

      // If signed less than zero, we have a valid value. Otherwise, we have
      // an extra inhabitant.
      auto valid
        = IGF.Builder.CreateICmpSLT(val,
                                    llvm::ConstantInt::get(IGF.IGM.Int32Ty, 0));
      val = IGF.Builder.CreateSelect(valid,
                        llvm::ConstantInt::getSigned(IGF.IGM.Int32Ty, -1), val);

      return val;
    }

    void storeExtraInhabitant(IRGenFunction &IGF,
                              llvm::Value *index,
                              Address dest, SILType T) const override {
      auto &C = IGF.IGM.getLLVMContext();
      auto payloadTy = llvm::IntegerType::get(C,
                      cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits());
      dest = IGF.Builder.CreateBitCast(dest, payloadTy->getPointerTo());

      index = IGF.Builder.CreateZExtOrTrunc(index, payloadTy);
      index = IGF.Builder.CreateAdd(index,
                llvm::ConstantInt::get(payloadTy, ElementsWithNoPayload.size()));
      IGF.Builder.CreateStore(index, dest);
    }
  };

  /// Implementation strategy for C-compatible enums, where none of the cases
  /// have data but they all have fixed integer associated values.
  class CCompatibleEnumImplStrategy final
    : public NoPayloadEnumImplStrategyBase
  {
  protected:
    int64_t getDiscriminatorIndex(EnumElementDecl *target) const override {
      // The elements are assigned discriminators ABI-compatible with their
      // raw values from C.
      assert(target->hasRawValueExpr()
             && "c-compatible enum elt has no raw value?!");
      auto intExpr = cast<IntegerLiteralExpr>(target->getRawValueExpr());
      auto intType = getDiscriminatorType();

      APInt intValue = IntegerLiteralExpr::getValue(intExpr->getDigitsText(),
                                                    intType->getBitWidth());

      if (intExpr->isNegative())
        intValue = -intValue;

      return intValue.getZExtValue();
    }

  public:
    CCompatibleEnumImplStrategy(IRGenModule &IGM,
                                TypeInfoKind tik,
                                IsFixedSize_t alwaysFixedSize,
                                unsigned NumElements,
                                std::vector<Element> &&WithPayload,
                                std::vector<Element> &&WithNoPayload)
      : NoPayloadEnumImplStrategyBase(IGM, tik, alwaysFixedSize,
                                      NumElements,
                                      std::move(WithPayload),
                                      std::move(WithNoPayload))
    {
      assert(ElementsWithPayload.empty());
      assert(!ElementsWithNoPayload.empty());
    }

    TypeInfo *completeEnumTypeLayout(TypeConverter &TC,
                                     SILType Type,
                                     EnumDecl *theEnum,
                                     llvm::StructType *enumTy) override;

    /// \group Extra inhabitants for C-compatible enums.

    // C-compatible enums have scattered inhabitants. For now, expose no
    // extra inhabitants.

    bool mayHaveExtraInhabitants(IRGenModule &IGM) const override {
      return false;
    }

    unsigned getFixedExtraInhabitantCount(IRGenModule &IGM) const override {
      return 0;
    }

    APInt getFixedExtraInhabitantValue(IRGenModule &IGM,
                                       unsigned bits,
                                       unsigned index) const override {
      llvm_unreachable("no extra inhabitants");
    }

    llvm::Value *getExtraInhabitantIndex(IRGenFunction &IGF,
                                         Address src, SILType T) const override {
      llvm_unreachable("no extra inhabitants");
    }

    void storeExtraInhabitant(IRGenFunction &IGF,
                              llvm::Value *index,
                              Address dest, SILType T) const override {
      llvm_unreachable("no extra inhabitants");
    }

    llvm::Constant *emitCaseNames() const override {
      // C enums have arbitrary values and we don't preserve the mapping
      // between the case and raw value at runtime, so don't emit any
      // case names at all so that reflection can give up in this case.
      return llvm::ConstantPointerNull::get(IGM.Int8PtrTy);
    }
  };

  /// Common base class for enums with one or more cases with data.
  class PayloadEnumImplStrategyBase : public EnumImplStrategy {
  protected:
    EnumPayloadSchema PayloadSchema;
    unsigned PayloadElementCount;
    llvm::IntegerType *extraTagTy = nullptr;

    // The number of payload bits.
    unsigned PayloadBitCount = 0;
    // The number of extra tag bits outside of the payload required to
    // discriminate enum cases.
    unsigned ExtraTagBitCount = ~0u;
    // The number of possible values for the extra tag bits that are used.
    // Log2(NumExtraTagValues - 1) + 1 == ExtraTagBitCount
    unsigned NumExtraTagValues = ~0u;

    void setTaggedEnumBody(IRGenModule &IGM,
                           llvm::StructType *bodyStruct,
                           unsigned payloadBits, unsigned extraTagBits) {
      // LLVM's ABI rules for I.O.U.S. (Integer Of Unusual Size) types is to
      // pad them out as if aligned to the largest native integer type, even
      // inside "packed" structs, so to accurately lay things out, we use
      // i8 arrays for the payload and extra tag bits.
      auto payloadArrayTy = llvm::ArrayType::get(IGM.Int8Ty,
                                                 (payloadBits+7U)/8U);

      SmallVector<llvm::Type*, 2> body;

      // Handle the case when the payload has no storage.
      // This may come up when a generic type with payload is instantiated on an
      // empty type.
      if (payloadBits > 0) {
        body.push_back(payloadArrayTy);
      }

      if (extraTagBits > 0) {
        auto extraTagArrayTy = llvm::ArrayType::get(IGM.Int8Ty,
                                                    (extraTagBits+7U)/8U);
        body.push_back(extraTagArrayTy);
        extraTagTy = llvm::IntegerType::get(IGM.getLLVMContext(),
                                            extraTagBits);
      } else {
        extraTagTy = nullptr;
      }
      bodyStruct->setBody(body, /*isPacked*/true);
    }

  public:
    PayloadEnumImplStrategyBase(IRGenModule &IGM,
                                TypeInfoKind tik,
                                IsFixedSize_t alwaysFixedSize,
                                unsigned NumElements,
                                std::vector<Element> &&WithPayload,
                                std::vector<Element> &&WithNoPayload,
                                EnumPayloadSchema schema)
      : EnumImplStrategy(IGM, tik, alwaysFixedSize,
                         NumElements,
                         std::move(WithPayload),
                         std::move(WithNoPayload)),
                         PayloadSchema(schema),
                         PayloadElementCount(0)
    {
      assert(ElementsWithPayload.size() >= 1);
      if (PayloadSchema) {
        PayloadSchema.forEachType(IGM, [&](llvm::Type *t){
          PayloadElementCount++;
          PayloadBitCount += IGM.DataLayout.getTypeSizeInBits(t);
        });
      } else {
        // The bit count is dynamic.
        PayloadBitCount = ~0u;
      }
    }
    
    ~PayloadEnumImplStrategyBase() {
      if (auto schema = PayloadSchema.getSchema())
        delete schema;
    }

    void getSchema(ExplosionSchema &schema) const override {
      if (TIK < Loadable) {
        schema.add(ExplosionSchema::Element::forAggregate(getStorageType(),
                                                  TI->getBestKnownAlignment()));
        return;
      }
      
      PayloadSchema.forEachType(IGM, [&](llvm::Type *payloadTy) {
        schema.add(ExplosionSchema::Element::forScalar(payloadTy));
      });

      if (ExtraTagBitCount > 0)
        schema.add(ExplosionSchema::Element::forScalar(extraTagTy));
    }

    unsigned getExplosionSize() const override {
      return unsigned(ExtraTagBitCount > 0) + PayloadElementCount;
    }

    Address projectPayload(IRGenFunction &IGF, Address addr) const {
      // The payload is currently always at the address point.
      return addr;
    }

    Address projectExtraTagBits(IRGenFunction &IGF, Address addr) const {
      assert(ExtraTagBitCount > 0 && "does not have extra tag bits");

      if (PayloadElementCount == 0) {
        return IGF.Builder.CreateBitCast(addr, extraTagTy->getPointerTo());
      }

      addr = IGF.Builder.CreateStructGEP(addr, 1, Size(PayloadBitCount/8U));
      return IGF.Builder.CreateBitCast(addr, extraTagTy->getPointerTo());
    }

    void loadForSwitch(IRGenFunction &IGF, Address addr, Explosion &e)
    const {
      assert(TIK >= Fixed);
      auto payload = EnumPayload::load(IGF, projectPayload(IGF, addr),
                                       PayloadSchema);
      payload.explode(IGF.IGM, e);
      if (ExtraTagBitCount > 0)
        e.add(IGF.Builder.CreateLoad(projectExtraTagBits(IGF, addr)));
    }

    void loadAsTake(IRGenFunction &IGF, Address addr, Explosion &e)
    const override {
      assert(TIK >= Loadable);
      loadForSwitch(IGF, addr, e);
    }

    void loadAsCopy(IRGenFunction &IGF, Address addr, Explosion &e)
    const override {
      assert(TIK >= Loadable);
      Explosion tmp;
      loadAsTake(IGF, addr, tmp);
      copy(IGF, tmp, e);
    }

    void assign(IRGenFunction &IGF, Explosion &e, Address addr) const override {
      assert(TIK >= Loadable);
      Explosion old;
      if (!isPOD(ResilienceScope::Component))
        loadAsTake(IGF, addr, old);
      initialize(IGF, e, addr);
      if (!isPOD(ResilienceScope::Component))
        consume(IGF, old);
    }

    void initialize(IRGenFunction &IGF, Explosion &e, Address addr)
    const override {
      assert(TIK >= Loadable);
      auto payload = EnumPayload::fromExplosion(IGF.IGM, e, PayloadSchema);
      payload.store(IGF, projectPayload(IGF, addr));
      if (ExtraTagBitCount > 0)
        IGF.Builder.CreateStore(e.claimNext(), projectExtraTagBits(IGF, addr));
    }

    void reexplode(IRGenFunction &IGF, Explosion &src, Explosion &dest)
    const override {
      assert(TIK >= Loadable);
      dest.add(src.claim(getExplosionSize()));
    }

  protected:
    /// Do a primitive copy of the enum from one address to another.
    void emitPrimitiveCopy(IRGenFunction &IGF, Address dest, Address src,
                           SILType T) const {
      // If the layout is fixed, load and store the fixed-size payload and tag.
      if (TIK >= Fixed) {
        EnumPayload payload;
        llvm::Value *extraTag;
        std::tie(payload, extraTag)
          = emitPrimitiveLoadPayloadAndExtraTag(IGF, src);
        emitPrimitiveStorePayloadAndExtraTag(IGF, dest, payload, extraTag);
        return;
      }

      // Otherwise, do a memcpy of the dynamic size of the type.
      IGF.Builder.CreateMemCpy(dest.getAddress(), src.getAddress(),
                               TI->getSize(IGF, T),
                               std::min(dest.getAlignment().getValue(),
                                        src.getAlignment().getValue()));
    }

    void emitPrimitiveStorePayloadAndExtraTag(IRGenFunction &IGF, Address dest,
                                              const EnumPayload &payload,
                                              llvm::Value *extraTag) const {
      payload.store(IGF, projectPayload(IGF, dest));
      if (ExtraTagBitCount > 0)
        IGF.Builder.CreateStore(extraTag, projectExtraTagBits(IGF, dest));
    }

    std::pair<EnumPayload, llvm::Value*>
    getPayloadAndExtraTagFromExplosion(IRGenFunction &IGF, Explosion &src)
    const {
      auto payload = EnumPayload::fromExplosion(IGF.IGM, src, PayloadSchema);
      llvm::Value *extraTag = ExtraTagBitCount > 0 ? src.claimNext() : nullptr;
      return {payload, extraTag};
    }

    std::pair<EnumPayload, llvm::Value*>
    emitPrimitiveLoadPayloadAndExtraTag(IRGenFunction &IGF, Address addr) const{
      llvm::Value *extraTag = nullptr;
      auto payload = EnumPayload::load(IGF, projectPayload(IGF, addr),
                                       PayloadSchema);
      if (ExtraTagBitCount > 0)
        extraTag = IGF.Builder.CreateLoad(projectExtraTagBits(IGF, addr));
      return {std::move(payload), extraTag};
    }
    
    void packIntoEnumPayload(IRGenFunction &IGF,
                             EnumPayload &outerPayload,
                             Explosion &src,
                             unsigned offset) const override {
      // Pack payload, if any.
      auto payload = EnumPayload::fromExplosion(IGF.IGM, src, PayloadSchema);
      payload.packIntoEnumPayload(IGF, outerPayload, offset);

      // Pack tag bits, if any.
      if (ExtraTagBitCount > 0) {
        unsigned extraTagOffset = PayloadBitCount + offset;

        outerPayload.insertValue(IGF, src.claimNext(), extraTagOffset);
      }
    }

    void unpackFromEnumPayload(IRGenFunction &IGF,
                               const EnumPayload &outerPayload,
                               Explosion &dest,
                               unsigned offset) const override {
      // Unpack our inner payload, if any.
      auto payload
        = EnumPayload::unpackFromEnumPayload(IGF, outerPayload, offset,
                                             PayloadSchema);
      
      payload.explode(IGF.IGM, dest);

      // Unpack our extra tag bits, if any.
      if (ExtraTagBitCount > 0) {
        unsigned extraTagOffset = PayloadBitCount + offset;

        dest.add(outerPayload.extractValue(IGF, extraTagTy, extraTagOffset));
      }
    }
  };

  class SinglePayloadEnumImplStrategy final
    : public PayloadEnumImplStrategyBase
  {
    // The payload size is readily available from the payload metadata; no
    // need to cache it in the enum metadata.
    bool needsPayloadSizeInMetadata() const override {
      return false;
    }
    
    EnumElementDecl *getPayloadElement() const {
      return ElementsWithPayload[0].decl;
    }

    SILType getPayloadType(IRGenModule &IGM, SILType T) const {
      return T.getEnumElementType(ElementsWithPayload[0].decl,
                                  *IGM.SILMod);
    }

    const TypeInfo &getPayloadTypeInfo() const {
      return *ElementsWithPayload[0].ti;
    }
    const FixedTypeInfo &getFixedPayloadTypeInfo() const {
      return cast<FixedTypeInfo>(*ElementsWithPayload[0].ti);
    }
    const LoadableTypeInfo &getLoadablePayloadTypeInfo() const {
      return cast<LoadableTypeInfo>(*ElementsWithPayload[0].ti);
    }

    llvm::Value *emitPayloadMetadataForLayout(IRGenFunction &IGF,
                                     SILType T) const {
      return IGF.emitTypeMetadataRefForLayout(getPayloadType(IGF.IGM, T));
    }

    /// More efficient value semantics implementations for certain enum layouts.
    enum CopyDestroyStrategy {
      /// No special behavior.
      Normal,
      /// The payload is POD, so copying is bitwise, and destruction is a noop.
      POD,
      /// The payload is a single reference-counted value, and we have
      /// a single no-payload case which uses the null extra inhabitant, so
      /// copy and destroy can pass through to retain and release entry
      /// points.
      NullableRefcounted,
    };

    CopyDestroyStrategy CopyDestroyKind;
    ReferenceCounting Refcounting;

    unsigned NumExtraInhabitantTagValues = ~0U;

    static EnumPayloadSchema getPreferredPayloadSchema(Element payloadElement) {
      // TODO: If the payload type info provides a preferred explosion schema,
      // use it. For now, just use a generic word-chunked schema.
      if (auto fixedTI = dyn_cast<FixedTypeInfo>(payloadElement.ti))
        return EnumPayloadSchema(fixedTI->getFixedSize().getValueInBits());
      return EnumPayloadSchema();
    }

  public:
    SinglePayloadEnumImplStrategy(IRGenModule &IGM,
                                  TypeInfoKind tik,
                                  IsFixedSize_t alwaysFixedSize,
                                  unsigned NumElements,
                                  std::vector<Element> &&WithPayload,
                                  std::vector<Element> &&WithNoPayload)
      : PayloadEnumImplStrategyBase(IGM, tik, alwaysFixedSize,
                                    NumElements,
                                    std::move(WithPayload),
                                    std::move(WithNoPayload),
                                getPreferredPayloadSchema(WithPayload.front())),
                                    CopyDestroyKind(Normal),
                                    Refcounting(ReferenceCounting::Native)
    {
      assert(ElementsWithPayload.size() == 1);

      // If the payload is POD, then we can use POD value semantics.
      auto &payloadTI = *ElementsWithPayload[0].ti;
      if (payloadTI.isPOD(ResilienceScope::Component)) {
        CopyDestroyKind = POD;
      // If the payload is a single refcounted pointer and we have a single
      // empty case, then the layout will be a nullable pointer, and we can
      // pass enum values directly into swift_retain/swift_release as-is.
      } else if (tik >= TypeInfoKind::Loadable
          && payloadTI.isSingleRetainablePointer(ResilienceScope::Component,
                                                 &Refcounting)
          && ElementsWithNoPayload.size() == 1
          // FIXME: All single-retainable-pointer types should eventually have
          // extra inhabitants.
          && cast<FixedTypeInfo>(payloadTI)
            .getFixedExtraInhabitantCount(IGM) > 0) {
        CopyDestroyKind = NullableRefcounted;
      }
    }

    /// Return the number of tag values represented with extra
    /// inhabitants in the payload.
    unsigned getNumExtraInhabitantTagValues() const {
      assert(NumExtraInhabitantTagValues != ~0U);
      return NumExtraInhabitantTagValues;
    }

    /// Emit a call into the runtime to get the current enum payload tag; this
    /// is an internal form that returns -1 in the payload case, or an index of
    /// an empty case.
    llvm::Value *
    loadPayloadTag(IRGenFunction &IGF, Address enumAddr, SILType T) const {
      auto payloadMetadata = emitPayloadMetadataForLayout(IGF, T);
      auto numEmptyCases = llvm::ConstantInt::get(IGF.IGM.Int32Ty,
                                                  ElementsWithNoPayload.size());

      auto opaqueAddr = IGF.Builder.CreateBitCast(enumAddr.getAddress(),
                                                  IGF.IGM.OpaquePtrTy);

      return IGF.Builder.CreateCall(
                 IGF.IGM.getGetEnumCaseSinglePayloadFn(),
                 {opaqueAddr, payloadMetadata, numEmptyCases});
    }

    /// Emit a call into the runtime to get the current enum case; this form
    /// is used for reflection where we want the payload cases to start at
    /// zero.
    llvm::Value *
    emitGetEnumTag(IRGenFunction &IGF, SILType T, Address enumAddr)
    const override {
      auto value = loadPayloadTag(IGF, enumAddr, T);
      return IGF.Builder.CreateAdd(value,
                                   llvm::ConstantInt::get(IGF.IGM.Int32Ty, 1));
    }

    /// The payload for a single-payload enum is always placed in front and
    /// will never have interleaved tag bits, so we can just bitcast the enum
    /// address to the payload type for either injection or projection of the
    /// enum.
    Address projectPayloadData(IRGenFunction &IGF, Address addr) const {
      return IGF.Builder.CreateBitCast(addr,
                         getPayloadTypeInfo().getStorageType()->getPointerTo());
    }
    void destructiveProjectDataForLoad(IRGenFunction &IGF,
                                       SILType T,
                                       Address enumAddr) const override {
    }

    TypeInfo *completeEnumTypeLayout(TypeConverter &TC,
                                     SILType Type,
                                     EnumDecl *theEnum,
                                      llvm::StructType *enumTy) override;
  private:
    TypeInfo *completeFixedLayout(TypeConverter &TC,
                                  SILType Type,
                                  EnumDecl *theEnum,
                                  llvm::StructType *enumTy);
    TypeInfo *completeDynamicLayout(TypeConverter &TC,
                                    SILType Type,
                                    EnumDecl *theEnum,
                                    llvm::StructType *enumTy);

  public:
    llvm::Value *
    emitIndirectCaseTest(IRGenFunction &IGF, SILType T,
                         Address enumAddr,
                         EnumElementDecl *Case) const override {
      if (TIK >= Fixed) {
        // Load the fixed-size representation and switch directly.
        Explosion value;
        loadForSwitch(IGF, enumAddr, value);
        return emitValueCaseTest(IGF, value, Case);
      }

      // Just fall back to emitting a switch.
      // FIXME: This could likely be implemented directly.
      auto &C = IGF.IGM.getLLVMContext();
      auto curBlock = IGF.Builder.GetInsertBlock();
      auto caseBlock = llvm::BasicBlock::Create(C);
      auto contBlock = llvm::BasicBlock::Create(C);
      emitIndirectSwitch(IGF, T, enumAddr, {{Case, caseBlock}}, contBlock);
      
      // Emit the case block.
      IGF.Builder.emitBlock(caseBlock);
      IGF.Builder.CreateBr(contBlock);

      // Emit the continuation block and generate a PHI to produce the value.
      IGF.Builder.emitBlock(contBlock);
      auto Phi = IGF.Builder.CreatePHI(IGF.IGM.Int1Ty, 2);
      Phi->addIncoming(IGF.Builder.getInt1(true), caseBlock);
      Phi->addIncoming(IGF.Builder.getInt1(false), curBlock);
      return Phi;
    }

    llvm::Value *
    emitValueCaseTest(IRGenFunction &IGF,
                      Explosion &value,
                      EnumElementDecl *Case) const override {
      // If we're testing for the payload element, we cannot directly check to
      // see whether it is present (in full generality) without doing a switch.
      // Try some easy cases, then bail back to the general case.
      if (Case == getPayloadElement()) {
        // If the Enum only contains two cases, test for the non-payload case
        // and invert the result.
        assert(ElementsWithPayload.size() == 1 && "Should have one payload");
        if (ElementsWithNoPayload.size() == 1) {
          auto *InvertedResult = emitValueCaseTest(IGF, value,
                                                 ElementsWithNoPayload[0].decl);
          return IGF.Builder.CreateNot(InvertedResult);
        }
        
        // Otherwise, just fall back to emitting a switch to decide.  Maybe LLVM
        // will be able to simplify it further.
        auto &C = IGF.IGM.getLLVMContext();
        auto caseBlock = llvm::BasicBlock::Create(C);
        auto contBlock = llvm::BasicBlock::Create(C);
        emitValueSwitch(IGF, value, {{Case, caseBlock}}, contBlock);
        
        // Emit the case block.
        IGF.Builder.emitBlock(caseBlock);
        IGF.Builder.CreateBr(contBlock);
        
        // Emit the continuation block and generate a PHI to produce the value.
        IGF.Builder.emitBlock(contBlock);
        auto Phi = IGF.Builder.CreatePHI(IGF.IGM.Int1Ty, 2);
        Phi->addIncoming(IGF.Builder.getInt1(true), caseBlock);
        for (auto I = llvm::pred_begin(contBlock),
             E = llvm::pred_end(contBlock); I != E; ++I)
          if (*I != caseBlock)
            Phi->addIncoming(IGF.Builder.getInt1(false), *I);
        return Phi;
      }

      assert(Case != getPayloadElement());

      // Destructure the value into its payload + tag bit components, each is
      // optional.
      auto payload = EnumPayload::fromExplosion(IGF.IGM, value, PayloadSchema);

      // If there are extra tag bits, test them first.
      llvm::Value *tagBits = nullptr;
      if (ExtraTagBitCount > 0)
        tagBits = value.claimNext();


      // Non-payload cases use extra inhabitants, if any, or are discriminated
      // by setting the tag bits.
      APInt payloadTag, extraTag;
      std::tie(payloadTag, extraTag) = getNoPayloadCaseValue(Case);

      auto &ti = getFixedPayloadTypeInfo();
      bool hasExtraInhabitants = ti.getFixedExtraInhabitantCount(IGF.IGM) > 0;

      llvm::Value *payloadResult = nullptr;
      if (hasExtraInhabitants)
        payloadResult = payload.emitCompare(IGF,
                                        ti.getFixedExtraInhabitantMask(IGF.IGM),
                                            payloadTag);

      // If any tag bits are present, they must match.
      llvm::Value *tagResult = nullptr;
      if (tagBits) {
        if (ExtraTagBitCount == 1) {
          if (extraTag == 1)
            tagResult = tagBits;
          else
            tagResult = IGF.Builder.CreateNot(tagBits);
        } else {
          tagResult = IGF.Builder.CreateICmpEQ(tagBits,
                    llvm::ConstantInt::get(IGF.IGM.getLLVMContext(), extraTag));
        }
      }
      
      if (tagResult && payloadResult)
        return IGF.Builder.CreateAnd(tagResult, payloadResult);
      if (tagResult)
        return tagResult;
      assert(payloadResult && "No tag or payload?");
      return payloadResult;
    }

    
    void emitValueSwitch(IRGenFunction &IGF,
                         Explosion &value,
                         ArrayRef<std::pair<EnumElementDecl*,
                                            llvm::BasicBlock*>> dests,
                         llvm::BasicBlock *defaultDest) const override {
      auto &C = IGF.IGM.getLLVMContext();

      // Create a map of the destination blocks for quicker lookup.
      llvm::DenseMap<EnumElementDecl*,llvm::BasicBlock*> destMap(dests.begin(),
                                                                  dests.end());
      // Create an unreachable branch for unreachable switch defaults.
      auto *unreachableBB = llvm::BasicBlock::Create(C);

      // If there was no default branch in SIL, use the unreachable branch as
      // the default.
      if (!defaultDest)
        defaultDest = unreachableBB;

      auto blockForCase = [&](EnumElementDecl *theCase) -> llvm::BasicBlock* {
        auto found = destMap.find(theCase);
        if (found == destMap.end())
          return defaultDest;
        else
          return found->second;
      };

      auto payload = EnumPayload::fromExplosion(IGF.IGM, value, PayloadSchema);
      llvm::BasicBlock *payloadDest = blockForCase(getPayloadElement());
      unsigned extraInhabitantCount = getNumExtraInhabitantTagValues();

      auto elements = getPayloadElement()->getParentEnum()->getAllElements();
      auto elti = elements.begin(), eltEnd = elements.end();
      if (*elti == getPayloadElement())
        ++elti;

      // Advance the enum element iterator, skipping the payload case.
      auto nextCase = [&]() -> EnumElementDecl* {
        assert(elti != eltEnd);
        auto result = *elti;
        ++elti;
        if (elti != eltEnd && *elti == getPayloadElement())
          ++elti;
        return result;
      };
      
      // If there are extra tag bits, switch over them first.
      SmallVector<llvm::BasicBlock*, 2> tagBitBlocks;
      if (ExtraTagBitCount > 0) {
        llvm::Value *tagBits = value.claimNext();
        assert(NumExtraTagValues > 1
               && "should have more than two tag values if there are extra "
                  "tag bits!");

        llvm::BasicBlock *zeroDest;
        // If we have extra inhabitants, we need to check for them in the
        // zero-tag case. Otherwise, we switch directly to the payload case.
        if (extraInhabitantCount > 0)
          zeroDest = llvm::BasicBlock::Create(C);
        else
          zeroDest = payloadDest;

        // If there are only two interesting cases, do a cond_br instead of
        // a switch.
        if (ExtraTagBitCount == 1) {
          tagBitBlocks.push_back(zeroDest);
          llvm::BasicBlock *oneDest;
          
          // If there's only one no-payload case, we can jump to it directly.
          if (ElementsWithNoPayload.size() == 1) {
            oneDest = blockForCase(nextCase());
          } else {
            oneDest = llvm::BasicBlock::Create(C);
            tagBitBlocks.push_back(oneDest);
          }
          IGF.Builder.CreateCondBr(tagBits, oneDest, zeroDest);
        } else {
          auto *swi = IGF.Builder.CreateSwitch(tagBits, unreachableBB,
                                               NumExtraTagValues);

          // If we have extra inhabitants, we need to check for them in the
          // zero-tag case. Otherwise, we switch directly to the payload case.
          tagBitBlocks.push_back(zeroDest);
          swi->addCase(llvm::ConstantInt::get(C,APInt(ExtraTagBitCount,0)),
                       zeroDest);
          
          for (unsigned i = 1; i < NumExtraTagValues; ++i) {
            // If there's only one no-payload case, or the payload is empty,
            // we can jump directly to cases without more branching.
            llvm::BasicBlock *bb;
            
            if (ElementsWithNoPayload.size() == 1
                || PayloadBitCount == 0) {
              bb = blockForCase(nextCase());
            } else {
              bb = llvm::BasicBlock::Create(C);
              tagBitBlocks.push_back(bb);
            }
            swi->addCase(llvm::ConstantInt::get(C, APInt(ExtraTagBitCount, i)),
                         bb);
          }
        }

        // Continue by emitting the extra inhabitant dispatch, if any.
        if (extraInhabitantCount > 0)
          IGF.Builder.emitBlock(tagBitBlocks[0]);
      }

      // If there are no extra tag bits, or they're set to zero, then we either
      // have a payload, or an empty case represented using an extra inhabitant.
      // Check the extra inhabitant cases if we have any.
      auto &fpTypeInfo = getFixedPayloadTypeInfo();
      if (extraInhabitantCount > 0) {
        // Switch over the extra inhabitant patterns we used.
        APInt mask = fpTypeInfo.getFixedExtraInhabitantMask(IGF.IGM);
        
        SmallVector<std::pair<APInt, llvm::BasicBlock *>, 4> cases;
        for (auto i = 0U; i < extraInhabitantCount && elti != eltEnd; ++i) {
          cases.push_back({
            fpTypeInfo.getFixedExtraInhabitantValue(IGF.IGM, PayloadBitCount,i),
            blockForCase(nextCase())
          });
        }
        
        payload.emitSwitch(IGF, mask, cases,
                           SwitchDefaultDest(payloadDest, IsNotUnreachable));
      }

      // We should have handled the payload case either in extra inhabitant
      // or in extra tag dispatch by now.
      assert(IGF.Builder.hasPostTerminatorIP() &&
             "did not handle payload case");

      // Handle the cases covered by each tag bit value.
      // If there was only one no-payload case, or the payload is empty, we
      // already branched in the first switch.
      if (PayloadBitCount > 0 && ElementsWithNoPayload.size() > 1) {
        unsigned casesPerTag = PayloadBitCount >= 32
          ? UINT_MAX : 1U << PayloadBitCount;
        for (unsigned i = 1, e = tagBitBlocks.size(); i < e; ++i) {
          assert(elti != eltEnd &&
                 "ran out of cases before running out of extra tags?");
          IGF.Builder.emitBlock(tagBitBlocks[i]);
          
          SmallVector<std::pair<APInt, llvm::BasicBlock *>, 4> cases;
          for (unsigned tag = 0; tag < casesPerTag && elti != eltEnd; ++tag) {
            cases.push_back({APInt(PayloadBitCount, tag),
                             blockForCase(nextCase())});
          }
          
          // FIXME: Provide a mask to only match the bits in the payload
          // whose extra inhabitants differ.
          payload.emitSwitch(IGF, APInt::getAllOnesValue(PayloadBitCount),
                             cases,
                             SwitchDefaultDest(unreachableBB, IsUnreachable));
        }
      }
      
      assert(elti == eltEnd && "did not branch to all cases?!");

      // Delete the unreachable default block if we didn't use it, or emit it
      // if we did.
      if (unreachableBB->use_empty()) {
        delete unreachableBB;
      } else {
        IGF.Builder.emitBlock(unreachableBB);
        IGF.Builder.CreateUnreachable();
      }
    }

    void emitDynamicSwitch(IRGenFunction &IGF,
                           SILType T,
                           Address addr,
                           ArrayRef<std::pair<EnumElementDecl*,
                                              llvm::BasicBlock*>> dests,
                           llvm::BasicBlock *defaultDest) const {
      // Create a map of the destination blocks for quicker lookup.
      llvm::DenseMap<EnumElementDecl*,llvm::BasicBlock*> destMap(dests.begin(),
                                                                  dests.end());

      // If there was no default branch in SIL, use an unreachable branch as
      // the default.
      llvm::BasicBlock *unreachableBB = nullptr;
      if (!defaultDest) {
        unreachableBB = llvm::BasicBlock::Create(IGF.IGM.getLLVMContext());
        defaultDest = unreachableBB;
      }

      // Ask the runtime to find the case index.
      auto caseIndex = loadPayloadTag(IGF, addr, T);

      // Switch on the index.
      auto *swi = IGF.Builder.CreateSwitch(caseIndex, defaultDest);

      // Add the payload case.
      auto payloadCase = destMap.find(getPayloadElement());
      if (payloadCase != destMap.end())
        swi->addCase(llvm::ConstantInt::getSigned(IGF.IGM.Int32Ty, -1),
                     payloadCase->second);

      // Add the empty cases.
      unsigned emptyCaseIndex = 0;
      for (auto &empty : ElementsWithNoPayload) {
        auto emptyCase = destMap.find(empty.decl);
        if (emptyCase != destMap.end())
          swi->addCase(llvm::ConstantInt::get(IGF.IGM.Int32Ty, emptyCaseIndex),
                       emptyCase->second);
        ++emptyCaseIndex;
      }

      // Emit the unreachable block, if any.
      if (unreachableBB) {
        IGF.Builder.emitBlock(unreachableBB);
        IGF.Builder.CreateUnreachable();
      }
    }

    void emitIndirectSwitch(IRGenFunction &IGF,
                            SILType T,
                            Address addr,
                            ArrayRef<std::pair<EnumElementDecl*,
                                               llvm::BasicBlock*>> dests,
                            llvm::BasicBlock *defaultDest) const override {
      if (TIK >= Fixed) {
        // Load the fixed-size representation and switch directly.
        Explosion value;
        loadForSwitch(IGF, addr, value);
        return emitValueSwitch(IGF, value, dests, defaultDest);
      }

      // Use the runtime to dynamically switch.
      emitDynamicSwitch(IGF, T, addr, dests, defaultDest);
    }

    void emitValueProject(IRGenFunction &IGF,
                          Explosion &inEnum,
                          EnumElementDecl *theCase,
                          Explosion &out) const override {
      // Only the payload case has anything to project. The other cases are
      // empty.
      if (theCase != getPayloadElement()) {
        inEnum.claim(getExplosionSize());
        return;
      }

      auto payload = EnumPayload::fromExplosion(IGF.IGM, inEnum, PayloadSchema);
      getLoadablePayloadTypeInfo()
        .unpackFromEnumPayload(IGF, payload, out, 0);
      if (ExtraTagBitCount > 0)
        inEnum.claimNext();
    }

  private:
    // Get the index of an enum element among the non-payload cases.
    unsigned getSimpleElementTagIndex(EnumElementDecl *elt) const {
      assert(elt != getPayloadElement() && "is payload element");
      unsigned i = 0;
      // FIXME: linear search
      for (auto *enumElt : elt->getParentEnum()->getAllElements()) {
        if (elt == enumElt)
          return i;
        if (enumElt != getPayloadElement())
          ++i;
      }
      llvm_unreachable("element was not a member of enum");
    }

    // Get the payload and extra tag (if any) parts of the discriminator for
    // a no-data case.
    std::pair<APInt, APInt>
    getNoPayloadCaseValue(EnumElementDecl *elt) const {
      assert(elt != getPayloadElement());

      unsigned payloadSize
        = getFixedPayloadTypeInfo().getFixedSize().getValueInBits();

      // Non-payload cases use extra inhabitants, if any, or are discriminated
      // by setting the tag bits.
      unsigned tagIndex = getSimpleElementTagIndex(elt);
      unsigned numExtraInhabitants = getNumExtraInhabitantTagValues();
      APInt payload;
      unsigned extraTagValue;
      if (tagIndex < numExtraInhabitants) {
        payload = getFixedPayloadTypeInfo().getFixedExtraInhabitantValue(
                                               IGM, payloadSize, tagIndex);
        extraTagValue = 0;
      } else {
        tagIndex -= numExtraInhabitants;

        // Factor the extra tag value from the payload value.
        unsigned payloadValue;
        if (payloadSize >= 32) {
          payloadValue = tagIndex;
          extraTagValue = 1U;
        } else {
          payloadValue = tagIndex & ((1U << payloadSize) - 1U);
          extraTagValue = (tagIndex >> payloadSize) + 1U;
        }

        if (payloadSize > 0)
          payload = APInt(payloadSize, payloadValue);
      }

      APInt extraTag;
      if (ExtraTagBitCount > 0) {
        extraTag = APInt(ExtraTagBitCount, extraTagValue);
      } else {
        assert(extraTagValue == 0 &&
               "non-zero extra tag value with no tag bits");
      }
      return {payload, extraTag};
    }

  public:
    void emitValueInjection(IRGenFunction &IGF,
                            EnumElementDecl *elt,
                            Explosion &params,
                            Explosion &out) const override {
      // The payload case gets its native representation. If there are extra
      // tag bits, set them to zero.
      if (elt == getPayloadElement()) {
        auto payload = EnumPayload::zero(IGF.IGM, PayloadSchema);
        auto &loadablePayloadTI = getLoadablePayloadTypeInfo();
        loadablePayloadTI.packIntoEnumPayload(IGF, payload, params, 0);
        payload.explode(IGF.IGM, out);
        if (ExtraTagBitCount > 0)
          out.add(getZeroExtraTagConstant(IGF.IGM));
        return;
      }

      // Non-payload cases use extra inhabitants, if any, or are discriminated
      // by setting the tag bits.
      APInt payloadPattern, extraTag;
      std::tie(payloadPattern, extraTag) = getNoPayloadCaseValue(elt);
      auto payload = EnumPayload::fromBitPattern(IGF.IGM, payloadPattern,
                                                 PayloadSchema);
      payload.explode(IGF.IGM, out);
      if (ExtraTagBitCount > 0) {
        out.add(llvm::ConstantInt::get(IGF.IGM.getLLVMContext(), extraTag));
      }
    }

  private:
    /// Emits the test(s) that determine whether the fixed-size enum contains a
    /// payload or an empty case. Emits the basic block for the "true" case and
    /// returns the unemitted basic block for the "false" case.
    llvm::BasicBlock *
    testFixedEnumContainsPayload(IRGenFunction &IGF,
                                 const EnumPayload &payload,
                                 llvm::Value *extraBits) const {
      auto *nonzeroBB = llvm::BasicBlock::Create(IGF.IGM.getLLVMContext());
      // We only need to apply the payload operation if the enum contains a
      // value of the payload case.

      // If we have extra tag bits, they will be zero if we contain a payload.
      if (ExtraTagBitCount > 0) {
        assert(extraBits);
        llvm::Value *isNonzero;
        if (ExtraTagBitCount == 1) {
          isNonzero = extraBits;
        } else {
          llvm::Value *zero = llvm::ConstantInt::get(extraBits->getType(), 0);
          isNonzero = IGF.Builder.CreateICmp(llvm::CmpInst::ICMP_NE,
                                         extraBits, zero);
        }

        auto *zeroBB = llvm::BasicBlock::Create(IGF.IGM.getLLVMContext());
        IGF.Builder.CreateCondBr(isNonzero, nonzeroBB, zeroBB);

        IGF.Builder.emitBlock(zeroBB);
      }

      // If we used extra inhabitants to represent empty case discriminators,
      // weed them out.
      unsigned numExtraInhabitants = getNumExtraInhabitantTagValues();
      if (numExtraInhabitants > 0) {
        unsigned bitWidth =
          getFixedPayloadTypeInfo().getFixedSize().getValueInBits();

        auto *payloadBB = llvm::BasicBlock::Create(IGF.IGM.getLLVMContext());
        
        SmallVector<std::pair<APInt, llvm::BasicBlock*>, 4> cases;
        
        auto elements = getPayloadElement()->getParentEnum()->getAllElements();
        unsigned inhabitant = 0;
        for (auto i = elements.begin(), end = elements.end();
             i != end && inhabitant < numExtraInhabitants;
             ++i, ++inhabitant) {
          auto xi = getFixedPayloadTypeInfo()
            .getFixedExtraInhabitantValue(IGF.IGM, bitWidth, inhabitant);
          cases.push_back({xi, nonzeroBB});
        }
        
        auto mask
          = getFixedPayloadTypeInfo().getFixedExtraInhabitantMask(IGF.IGM);
        payload.emitSwitch(IGF, mask, cases,
                           SwitchDefaultDest(payloadBB, IsNotUnreachable));
        IGF.Builder.emitBlock(payloadBB);
      }

      return nonzeroBB;
    }

    /// Emits the test(s) that determine whether the enum contains a payload
    /// or an empty case. For a fixed-size enum, this does a primitive load
    /// of the representation and calls down to testFixedEnumContainsPayload.
    /// For a dynamic enum, this queries the value witness table of the payload
    /// type. Emits the basic block for the "true" case and
    /// returns the unemitted basic block for the "false" case.
    llvm::BasicBlock *
    testEnumContainsPayload(IRGenFunction &IGF,
                            Address addr,
                            SILType T) const {
      auto &C = IGF.IGM.getLLVMContext();

      if (TIK >= Fixed) {
        EnumPayload payload;
        llvm::Value *extraTag;
        std::tie(payload, extraTag)
          = emitPrimitiveLoadPayloadAndExtraTag(IGF, addr);
        return testFixedEnumContainsPayload(IGF, payload, extraTag);
      }

      auto *payloadBB = llvm::BasicBlock::Create(C);
      auto *noPayloadBB = llvm::BasicBlock::Create(C);

      // Ask the runtime what case we have.
      llvm::Value *which = loadPayloadTag(IGF, addr, T);

      // If it's -1 then we have the payload.
      llvm::Value *hasPayload = IGF.Builder.CreateICmpEQ(which,
                             llvm::ConstantInt::getSigned(IGF.IGM.Int32Ty, -1));
      IGF.Builder.CreateCondBr(hasPayload, payloadBB, noPayloadBB);

      IGF.Builder.emitBlock(payloadBB);
      return noPayloadBB;
    }

    llvm::Type *getRefcountedPtrType(IRGenModule &IGM) const {
      switch (CopyDestroyKind) {
      case NullableRefcounted:
        return IGM.getReferenceType(Refcounting);
      case POD:
      case Normal:
        llvm_unreachable("not a refcounted payload");
      }
    }

    void retainRefcountedPayload(IRGenFunction &IGF,
                                 llvm::Value *ptr) const {
      switch (CopyDestroyKind) {
      case NullableRefcounted:
        IGF.emitScalarRetainCall(ptr, Refcounting);
        return;
      case POD:
      case Normal:
        llvm_unreachable("not a refcounted payload");
      }
    }

    void fixLifetimeOfRefcountedPayload(IRGenFunction &IGF,
                                        llvm::Value *ptr) const {
      switch (CopyDestroyKind) {
      case NullableRefcounted:
        IGF.emitFixLifetime(ptr);
        return;
      case POD:
      case Normal:
        llvm_unreachable("not a refcounted payload");
      }
    }

    void releaseRefcountedPayload(IRGenFunction &IGF,
                                  llvm::Value *ptr) const {
      switch (CopyDestroyKind) {
      case NullableRefcounted:
        IGF.emitScalarRelease(ptr, Refcounting);
        return;
      case POD:
      case Normal:
        llvm_unreachable("not a refcounted payload");
      }
    }

  public:
    void copy(IRGenFunction &IGF, Explosion &src, Explosion &dest)
    const override {
      assert(TIK >= Loadable);

      switch (CopyDestroyKind) {
      case POD:
        reexplode(IGF, src, dest);
        return;

      case Normal: {
        // Copy the payload, if we have it.
        EnumPayload payload; llvm::Value *extraTag;
        std::tie(payload, extraTag)
          = getPayloadAndExtraTagFromExplosion(IGF, src);

        llvm::BasicBlock *endBB = testFixedEnumContainsPayload(IGF, payload, extraTag);

        if (PayloadBitCount > 0) {
          Explosion payloadValue;
          Explosion payloadCopy;
          auto &loadableTI = getLoadablePayloadTypeInfo();
          loadableTI.unpackFromEnumPayload(IGF, payload, payloadValue, 0);
          loadableTI.copy(IGF, payloadValue, payloadCopy);
          payloadCopy.claimAll(); // FIXME: repack if not bit-identical
        }

        IGF.Builder.CreateBr(endBB);
        IGF.Builder.emitBlock(endBB);

        // Copy to the new explosion.
        payload.explode(IGF.IGM, dest);
        if (extraTag) dest.add(extraTag);
        return;
      }

      case NullableRefcounted: {
        // Bitcast to swift.refcounted*, and retain the pointer.
        llvm::Value *val = src.claimNext();
        llvm::Value *ptr = IGF.Builder.CreateBitOrPointerCast(val,
                                                getRefcountedPtrType(IGF.IGM));
        retainRefcountedPayload(IGF, ptr);
        dest.add(val);
        return;
      }
      }
    }

    void consume(IRGenFunction &IGF, Explosion &src) const override {
      assert(TIK >= Loadable);

      switch (CopyDestroyKind) {
      case POD:
        src.claim(getExplosionSize());
        return;

      case Normal: {
        // Check that we have a payload.
        EnumPayload payload; llvm::Value *extraTag;
        std::tie(payload, extraTag)
          = getPayloadAndExtraTagFromExplosion(IGF, src);

        llvm::BasicBlock *endBB
          = testFixedEnumContainsPayload(IGF, payload, extraTag);

        // If we did, consume it.
        if (PayloadBitCount > 0) {
          Explosion payloadValue;
          auto &loadableTI = getLoadablePayloadTypeInfo();
          loadableTI.unpackFromEnumPayload(IGF, payload, payloadValue, 0);
          loadableTI.consume(IGF, payloadValue);
        }

        IGF.Builder.CreateBr(endBB);
        IGF.Builder.emitBlock(endBB);
        return;
      }

      case NullableRefcounted: {
        // Bitcast to swift.refcounted*, and hand to swift_release.
        llvm::Value *val = src.claimNext();
        llvm::Value *ptr = IGF.Builder.CreateBitOrPointerCast(val,
                                                getRefcountedPtrType(IGF.IGM));
        releaseRefcountedPayload(IGF, ptr);
        return;
      }
      }

    }

    void fixLifetime(IRGenFunction &IGF, Explosion &src) const override {
      assert(TIK >= Loadable);

      switch (CopyDestroyKind) {
      case POD:
        src.claim(getExplosionSize());
        return;

      case Normal: {
        // Check that we have a payload.
        EnumPayload payload; llvm::Value *extraTag;
        std::tie(payload, extraTag)
          = getPayloadAndExtraTagFromExplosion(IGF, src);

        llvm::BasicBlock *endBB
          = testFixedEnumContainsPayload(IGF, payload, extraTag);

        // If we did, consume it.
        if (PayloadBitCount > 0) {
          Explosion payloadValue;
          auto &loadableTI = getLoadablePayloadTypeInfo();
          loadableTI.unpackFromEnumPayload(IGF, payload, payloadValue, 0);
          loadableTI.fixLifetime(IGF, payloadValue);
        }

        IGF.Builder.CreateBr(endBB);
        IGF.Builder.emitBlock(endBB);
        return;
      }

      case NullableRefcounted: {
        // Bitcast to swift.refcounted*, and hand to swift_release.
        llvm::Value *val = src.claimNext();
        llvm::Value *ptr = IGF.Builder.CreateIntToPtr(val,
                                                getRefcountedPtrType(IGF.IGM));
        fixLifetimeOfRefcountedPayload(IGF, ptr);
        return;
      }
      }

    }

    void destroy(IRGenFunction &IGF, Address addr, SILType T) const override {
      switch (CopyDestroyKind) {
      case POD:
        return;

      case Normal: {
        // Check that there is a payload at the address.
        llvm::BasicBlock *endBB = testEnumContainsPayload(IGF, addr, T);

        // If there is, project and destroy it.
        Address payloadAddr = projectPayloadData(IGF, addr);
        getPayloadTypeInfo().destroy(IGF, payloadAddr,
                                     getPayloadType(IGF.IGM, T));

        IGF.Builder.CreateBr(endBB);
        IGF.Builder.emitBlock(endBB);
        return;
      }

      case NullableRefcounted: {
        // Load the value as swift.refcounted, then hand to swift_release.
        addr = IGF.Builder.CreateBitCast(addr,
                                 getRefcountedPtrType(IGF.IGM)->getPointerTo());
        llvm::Value *ptr = IGF.Builder.CreateLoad(addr);
        releaseRefcountedPayload(IGF, ptr);
        return;
      }
      }
    }

    llvm::Value *loadRefcountedPtr(IRGenFunction &IGF, SourceLoc loc,
                                   Address addr) const override {
      // There is no need to bitcast from the enum address. Loading from the
      // reference type emits a bitcast to the proper reference type first.
      return cast<LoadableTypeInfo>(getPayloadTypeInfo()).loadRefcountedPtr(
        IGF, loc, addr).getValue();
    }
  private:
    llvm::ConstantInt *getZeroExtraTagConstant(IRGenModule &IGM) const {
      assert(TIK >= Fixed && "not fixed layout");
      assert(ExtraTagBitCount > 0 && "no extra tag bits?!");
      return llvm::ConstantInt::get(IGM.getLLVMContext(),
                                    APInt(ExtraTagBitCount, 0));
    }

    /// Initialize the extra tag bits, if any, to zero to indicate a payload.
    void emitInitializeExtraTagBitsForPayload(IRGenFunction &IGF,
                                              Address dest,
                                              SILType T) const {
      if (TIK >= Fixed) {
        // We statically know whether we have extra tag bits.
        // Store zero directly to the fixed-layout extra tag field.
        if (ExtraTagBitCount > 0) {
          auto *zeroTag = getZeroExtraTagConstant(IGF.IGM);
          IGF.Builder.CreateStore(zeroTag, projectExtraTagBits(IGF, dest));
        }
        return;
      }

      // Ask the runtime to store the tag.
      llvm::Value *opaqueAddr = IGF.Builder.CreateBitCast(dest.getAddress(),
                                                          IGF.IGM.OpaquePtrTy);
      llvm::Value *metadata = emitPayloadMetadataForLayout(IGF, T);
      IGF.Builder.CreateCall(IGF.IGM.getStoreEnumTagSinglePayloadFn(),
                             {opaqueAddr, metadata,
                              llvm::ConstantInt::getSigned(IGF.IGM.Int32Ty, -1),
                              llvm::ConstantInt::get(IGF.IGM.Int32Ty,
                                                 ElementsWithNoPayload.size())});
    }

    /// Emit an reassignment sequence from an enum at one address to another.
    void emitIndirectAssign(IRGenFunction &IGF,
       Address dest, Address src, SILType T,
       IsTake_t isTake)
    const {
      auto &C = IGF.IGM.getLLVMContext();
      auto PayloadT = getPayloadType(IGF.IGM, T);

      switch (CopyDestroyKind) {
      case POD:
        return emitPrimitiveCopy(IGF, dest, src, T);

      case Normal: {
        llvm::BasicBlock *endBB = llvm::BasicBlock::Create(C);

        Address destData = projectPayloadData(IGF, dest);
        Address srcData = projectPayloadData(IGF, src);
        // See whether the current value at the destination has a payload.

        llvm::BasicBlock *noDestPayloadBB
          = testEnumContainsPayload(IGF, dest, T);

        // Here, the destination has a payload. Now see if the source also has
        // one.
        llvm::BasicBlock *destNoSrcPayloadBB
          = testEnumContainsPayload(IGF, src, T);

        // Here, both source and destination have payloads. Do the reassignment
        // of the payload in-place.
        if (isTake)
          getPayloadTypeInfo().assignWithTake(IGF, destData, srcData, PayloadT);
        else
          getPayloadTypeInfo().assignWithCopy(IGF, destData, srcData, PayloadT);
        IGF.Builder.CreateBr(endBB);

        // If the destination has a payload but the source doesn't, we can destroy
        // the payload and primitive-store the new no-payload value.
        IGF.Builder.emitBlock(destNoSrcPayloadBB);
        getPayloadTypeInfo().destroy(IGF, destData, PayloadT);
        emitPrimitiveCopy(IGF, dest, src, T);
        IGF.Builder.CreateBr(endBB);

        // Now, if the destination has no payload, check if the source has one.
        IGF.Builder.emitBlock(noDestPayloadBB);
        llvm::BasicBlock *noDestNoSrcPayloadBB
          = testEnumContainsPayload(IGF, src, T);

        // Here, the source has a payload but the destination doesn't. We can
        // copy-initialize the source over the destination, then primitive-store
        // the zero extra tag (if any).
        if (isTake)
          getPayloadTypeInfo().initializeWithTake(IGF, destData, srcData, PayloadT);
        else
          getPayloadTypeInfo().initializeWithCopy(IGF, destData, srcData, PayloadT);
        emitInitializeExtraTagBitsForPayload(IGF, dest, T);
        IGF.Builder.CreateBr(endBB);

        // If neither destination nor source have payloads, we can just primitive-
        // store the new empty-case value.
        IGF.Builder.emitBlock(noDestNoSrcPayloadBB);
        emitPrimitiveCopy(IGF, dest, src, T);
        IGF.Builder.CreateBr(endBB);

        IGF.Builder.emitBlock(endBB);
        return;
      }

      case NullableRefcounted: {
        // Do the assignment as for a refcounted pointer.
        auto refCountedTy = getRefcountedPtrType(IGF.IGM);
        Address destAddr = IGF.Builder.CreateBitCast(dest,
                                                 refCountedTy->getPointerTo());
        Address srcAddr = IGF.Builder.CreateBitCast(src,
                                                refCountedTy->getPointerTo());
        // Load the old pointer at the destination.
        llvm::Value *oldPtr = IGF.Builder.CreateLoad(destAddr);
        // Store the new pointer.
        llvm::Value *srcPtr = IGF.Builder.CreateLoad(srcAddr);
        if (!isTake)
          retainRefcountedPayload(IGF, srcPtr);
        IGF.Builder.CreateStore(srcPtr, destAddr);
        // Release the old value.
        releaseRefcountedPayload(IGF, oldPtr);
        return;
      }
      }

    }

    /// Emit an initialization sequence, initializing an enum at one address
    /// with another at a different address.
    void emitIndirectInitialize(IRGenFunction &IGF,
                                Address dest, Address src, SILType T,
                                IsTake_t isTake)
    const {
      auto &C = IGF.IGM.getLLVMContext();

      switch (CopyDestroyKind) {
      case POD:
        return emitPrimitiveCopy(IGF, dest, src, T);

      case Normal: {
        llvm::BasicBlock *endBB = llvm::BasicBlock::Create(C);

        Address destData = projectPayloadData(IGF, dest);
        Address srcData = projectPayloadData(IGF, src);

        // See whether the source value has a payload.
        llvm::BasicBlock *noSrcPayloadBB
          = testEnumContainsPayload(IGF, src, T);

        // Here, the source value has a payload. Initialize the destination with
        // it, and set the extra tag if any to zero.
        if (isTake)
          getPayloadTypeInfo().initializeWithTake(IGF, destData, srcData,
                                                  getPayloadType(IGF.IGM, T));
        else
          getPayloadTypeInfo().initializeWithCopy(IGF, destData, srcData,
                                                  getPayloadType(IGF.IGM, T));
        emitInitializeExtraTagBitsForPayload(IGF, dest, T);
        IGF.Builder.CreateBr(endBB);

        // If the source value has no payload, we can primitive-store the
        // empty-case value.
        IGF.Builder.emitBlock(noSrcPayloadBB);
        emitPrimitiveCopy(IGF, dest, src, T);
        IGF.Builder.CreateBr(endBB);

        IGF.Builder.emitBlock(endBB);
        return;
      }

      case NullableRefcounted: {
        auto refCountedTy = getRefcountedPtrType(IGF.IGM);

        // Do the initialization as for a refcounted pointer.
        Address destAddr = IGF.Builder.CreateBitCast(dest,
                                                 refCountedTy->getPointerTo());
        Address srcAddr = IGF.Builder.CreateBitCast(src,
                                                 refCountedTy->getPointerTo());

        llvm::Value *srcPtr = IGF.Builder.CreateLoad(srcAddr);
        if (!isTake)
          retainRefcountedPayload(IGF, srcPtr);
        IGF.Builder.CreateStore(srcPtr, destAddr);
        return;
      }
      }
    }

  public:
    void assignWithCopy(IRGenFunction &IGF, Address dest, Address src,
                        SILType T)
    const override {
      emitIndirectAssign(IGF, dest, src, T, IsNotTake);
    }

    void assignWithTake(IRGenFunction &IGF, Address dest, Address src,
                        SILType T)
    const override {
      emitIndirectAssign(IGF, dest, src, T, IsTake);
    }

    void initializeWithCopy(IRGenFunction &IGF, Address dest, Address src,
                            SILType T)
    const override {
      emitIndirectInitialize(IGF, dest, src, T, IsNotTake);
    }

    void initializeWithTake(IRGenFunction &IGF, Address dest, Address src,
                            SILType T)
    const override {
      emitIndirectInitialize(IGF, dest, src, T, IsTake);
    }

    void storeTag(IRGenFunction &IGF,
                  SILType T,
                  Address enumAddr,
                  EnumElementDecl *Case) const override {
      if (TIK < Fixed) {
        // If the enum isn't fixed-layout, get the runtime to do this for us.
        llvm::Value *payload = emitPayloadMetadataForLayout(IGF, T);
        llvm::Value *caseIndex;
        if (Case == getPayloadElement()) {
          caseIndex = llvm::ConstantInt::getSigned(IGF.IGM.Int32Ty, -1);
        } else {
          auto found = std::find_if(ElementsWithNoPayload.begin(),
                                    ElementsWithNoPayload.end(),
                                    [&](Element a) { return a.decl == Case; });
          assert(found != ElementsWithNoPayload.end() &&
                 "case not in enum?!");
          unsigned caseIndexVal = found - ElementsWithNoPayload.begin();
          caseIndex = llvm::ConstantInt::get(IGF.IGM.Int32Ty, caseIndexVal);
        }

        llvm::Value *numEmptyCases = llvm::ConstantInt::get(IGF.IGM.Int32Ty,
                                                  ElementsWithNoPayload.size());

        llvm::Value *opaqueAddr
          = IGF.Builder.CreateBitCast(enumAddr.getAddress(),
                                      IGF.IGM.OpaquePtrTy);

        IGF.Builder.CreateCall(IGF.IGM.getStoreEnumTagSinglePayloadFn(),
                               {opaqueAddr, payload, caseIndex, numEmptyCases});

        return;
      }

      if (Case == getPayloadElement()) {
        // The data occupies the entire payload. If we have extra tag bits,
        // zero them out.
        if (ExtraTagBitCount > 0)
          IGF.Builder.CreateStore(getZeroExtraTagConstant(IGF.IGM),
                                  projectExtraTagBits(IGF, enumAddr));
        return;
      }

      // Store the discriminator for the no-payload case.
      APInt payloadValue, extraTag;
      std::tie(payloadValue, extraTag) = getNoPayloadCaseValue(Case);
      auto &C = IGF.IGM.getLLVMContext();
      auto payload = EnumPayload::fromBitPattern(IGF.IGM, payloadValue,
                                                 PayloadSchema);
      payload.store(IGF, projectPayload(IGF, enumAddr));
      if (ExtraTagBitCount > 0)
        IGF.Builder.CreateStore(llvm::ConstantInt::get(C, extraTag),
                                projectExtraTagBits(IGF, enumAddr));
    }

    void initializeMetadata(IRGenFunction &IGF,
                            llvm::Value *metadata,
                            llvm::Value *vwtable,
                            SILType T) const override {
      // Fixed-size enums don't need dynamic witness table initialization.
      if (TIK >= Fixed) return;

      // Ask the runtime to do our layout using the payload metadata and number
      // of empty cases.
      auto payloadTy = T.getEnumElementType(ElementsWithPayload[0].decl,
                                            *IGM.SILMod);
      auto payloadLayout = IGF.emitTypeLayoutRef(payloadTy);
      auto emptyCasesVal = llvm::ConstantInt::get(IGF.IGM.Int32Ty,
                                                  ElementsWithNoPayload.size());

      IGF.Builder.CreateCall(
                    IGF.IGM.getInitEnumValueWitnessTableSinglePayloadFn(),
                    {vwtable, payloadLayout, emptyCasesVal});
    }

    /// \group Extra inhabitants

    // Extra inhabitants from the payload that we didn't use for our empty cases
    // are available to outer enums.
    // FIXME: If we spilled extra tag bits, we could offer spare bits from the
    // tag.

    bool mayHaveExtraInhabitants(IRGenModule &IGM) const override {
      if (TIK >= Fixed)
        return getFixedExtraInhabitantCount(IGM) > 0;

      return getPayloadTypeInfo().mayHaveExtraInhabitants(IGM);
    }

    unsigned getFixedExtraInhabitantCount(IRGenModule &IGM) const override {
      return getFixedPayloadTypeInfo().getFixedExtraInhabitantCount(IGM)
               - getNumExtraInhabitantTagValues();
    }

    APInt
    getFixedExtraInhabitantValue(IRGenModule &IGM,
                                 unsigned bits,
                                 unsigned index) const override {
      return getFixedPayloadTypeInfo()
        .getFixedExtraInhabitantValue(IGM, bits,
                                      index + getNumExtraInhabitantTagValues());
    }

    llvm::Value *
    getExtraInhabitantIndex(IRGenFunction &IGF,
                            Address src, SILType T) const override {
      auto payload = projectPayloadData(IGF, src);
      llvm::Value *index
        = getPayloadTypeInfo().getExtraInhabitantIndex(IGF, payload,
                                                   getPayloadType(IGF.IGM, T));

      // Offset the payload extra inhabitant index by the number of inhabitants
      // we used. If less than zero, it's a valid value of the enum type.
      index = IGF.Builder.CreateSub(index,
         llvm::ConstantInt::get(IGF.IGM.Int32Ty, ElementsWithNoPayload.size()));
      auto valid = IGF.Builder.CreateICmpSLT(index,
                                   llvm::ConstantInt::get(IGF.IGM.Int32Ty, 0));
      index = IGF.Builder.CreateSelect(valid,
                              llvm::ConstantInt::getSigned(IGF.IGM.Int32Ty, -1),
                              index);
      return index;
    }

    void storeExtraInhabitant(IRGenFunction &IGF,
                              llvm::Value *index,
                              Address dest, SILType T) const override {
      // Offset the index to skip the extra inhabitants we used.
      index = IGF.Builder.CreateAdd(index,
          llvm::ConstantInt::get(IGF.IGM.Int32Ty, ElementsWithNoPayload.size()));

      auto payload = projectPayloadData(IGF, dest);
      getPayloadTypeInfo().storeExtraInhabitant(IGF, index, payload,
                                                   getPayloadType(IGF.IGM, T));
    }
    
    APInt
    getFixedExtraInhabitantMask(IRGenModule &IGM) const override {
      auto &payloadTI = getFixedPayloadTypeInfo();
      unsigned totalSize
        = cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits();
      if (payloadTI.isKnownEmpty())
        return APInt::getAllOnesValue(totalSize);
      auto baseMask =
        getFixedPayloadTypeInfo().getFixedExtraInhabitantMask(IGM);
      
      if (baseMask.getBitWidth() < totalSize)
        baseMask = baseMask.zext(totalSize)
         | APInt::getHighBitsSet(totalSize, totalSize - baseMask.getBitWidth());
      
      return baseMask;
    }

    ClusteredBitVector
    getBitPatternForNoPayloadElement(EnumElementDecl *theCase) const override {
      APInt payloadPart, extraPart;
      std::tie(payloadPart, extraPart) = getNoPayloadCaseValue(theCase);
      ClusteredBitVector bits;

      if (PayloadBitCount > 0)
        bits = getBitVectorFromAPInt(payloadPart);

      unsigned totalSize
        = cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits();
      if (ExtraTagBitCount > 0) {
        ClusteredBitVector extraBits = getBitVectorFromAPInt(extraPart,
                                                             bits.size());
        bits.extendWithClearBits(totalSize);
        extraBits.extendWithClearBits(totalSize);
        bits |= extraBits;
      } else {
        assert(totalSize == bits.size());
      }
      return bits;
    }

    ClusteredBitVector
    getBitMaskForNoPayloadElements() const override {
      // Use the extra inhabitants mask from the payload.
      auto &payloadTI = getFixedPayloadTypeInfo();
      ClusteredBitVector extraInhabitantsMask;
      
      if (!payloadTI.isKnownEmpty())
        extraInhabitantsMask =
          getBitVectorFromAPInt(payloadTI.getFixedExtraInhabitantMask(IGM));
      // Extend to include the extra tag bits, which are always significant.
      unsigned totalSize
        = cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits();
      extraInhabitantsMask.extendWithSetBits(totalSize);
      return extraInhabitantsMask;
    }

    ClusteredBitVector getTagBitsForPayloads() const override {
      // We only have tag bits if we spilled extra bits.
      ClusteredBitVector result;
      unsigned payloadSize
        = getFixedPayloadTypeInfo().getFixedSize().getValueInBits();
      result.appendClearBits(payloadSize);

      unsigned totalSize
        = cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits();

      if (ExtraTagBitCount) {
        result.appendSetBits(ExtraTagBitCount);
        result.extendWithClearBits(totalSize);
      } else {
        assert(payloadSize == totalSize);
      }
      return result;
    }
  };

  class MultiPayloadEnumImplStrategy final
    : public PayloadEnumImplStrategyBase
  {
    // The spare bits shared by all payloads, if any.
    // Invariant: The size of the bit vector is the size of the payload in bits,
    // rounded up to a byte boundary.
    SpareBitVector CommonSpareBits;

    // The common spare bits actually used for a tag in the payload area.
    SpareBitVector PayloadTagBits;

    // The number of tag values used for no-payload cases.
    unsigned NumEmptyElementTags = ~0u;

    /// More efficient value semantics implementations for certain enum layouts.
    enum CopyDestroyStrategy {
      /// No special behavior.
      Normal,
      /// The payloads are all POD, so copying is bitwise, and destruction is a
      /// noop.
      POD,
      /// The payloads are all bitwise-takable, but have no other special
      /// shared layout.
      BitwiseTakable,
      /// The payloads are all reference-counted values, and there is at
      /// most one no-payload case with the tagged-zero representation. Copy
      /// and destroy can just mask out the tag bits and pass the result to
      /// retain and release entry points.
      /// This implies BitwiseTakable.
      TaggedRefcounted,
    };

    CopyDestroyStrategy CopyDestroyKind;
    ReferenceCounting Refcounting;

    static EnumPayloadSchema getPayloadSchema(ArrayRef<Element> payloads) {
      // TODO: We might be able to form a nicer schema if the payload elements
      // share a schema. For now just use a generic schema.
      unsigned maxBitSize = 0;
      for (auto payload : payloads) {
        auto fixedTI = dyn_cast<FixedTypeInfo>(payload.ti);
        if (!fixedTI)
          return EnumPayloadSchema();
        maxBitSize = std::max(maxBitSize,
                            unsigned(fixedTI->getFixedSize().getValueInBits()));
      }
      return EnumPayloadSchema(maxBitSize);
    }

  public:
    MultiPayloadEnumImplStrategy(IRGenModule &IGM,
                                 TypeInfoKind tik,
                                 IsFixedSize_t alwaysFixedSize,
                                 unsigned NumElements,
                                 std::vector<Element> &&WithPayload,
                                 std::vector<Element> &&WithNoPayload)
      : PayloadEnumImplStrategyBase(IGM, tik, alwaysFixedSize,
                                    NumElements,
                                    std::move(WithPayload),
                                    std::move(WithNoPayload),
                                    getPayloadSchema(WithPayload)),
                                    CopyDestroyKind(Normal)
    {
      assert(ElementsWithPayload.size() > 1);

      // Check the payloads to see if we can take advantage of common layout to
      // optimize our value semantics.
      bool allPOD = true;
      bool allBitwiseTakable = true;
      bool allSingleRefcount = true;
      bool haveRefcounting = false;
      for (auto &elt : ElementsWithPayload) {
        if (!elt.ti->isPOD(ResilienceScope::Component))
          allPOD = false;
        if (!elt.ti->isBitwiseTakable(ResilienceScope::Component))
          allBitwiseTakable = false;

        // refcounting is only set in the else branches
        ReferenceCounting refcounting;
        if (!elt.ti->isSingleRetainablePointer(ResilienceScope::Component,
                                               &refcounting)) {
          allSingleRefcount = false;
        } else if (haveRefcounting) {
          // Different payloads have different reference counting styles.
          if (refcounting != Refcounting) {
            // Fall back to unknown entry points if the Objective-C runtime is
            // available.
            Refcounting = ReferenceCounting::Unknown;
            // Otherwise, use value witnesses.
            if (!IGM.ObjCInterop)
              allSingleRefcount = false;
          }
        } else {
          Refcounting = refcounting;
          haveRefcounting = true;
        }
      }

      if (allPOD) {
        assert(!allSingleRefcount && "pod *and* refcounted?!");
        CopyDestroyKind = POD;
      // FIXME: Memory corruption issues arise when enabling this for mixed
      // Swift/ObjC enums.
      } else if (allSingleRefcount
                 && ElementsWithNoPayload.size() <= 1) {
        CopyDestroyKind = TaggedRefcounted;
      } else if (allBitwiseTakable) {
        CopyDestroyKind = BitwiseTakable;
      }
    }

    bool needsPayloadSizeInMetadata() const override {
      // For dynamic multi-payload enums, it would be expensive to recalculate
      // the payload area size from all of the cases, so cache it in the
      // metadata. For fixed-layout cases this isn't necessary (except for
      // reflection, but it's OK if reflection is a little slow).
      return TIK < Fixed;
    }

    TypeInfo *completeEnumTypeLayout(TypeConverter &TC,
                                     SILType Type,
                                     EnumDecl *theEnum,
                                     llvm::StructType *enumTy) override;

  private:
    TypeInfo *completeFixedLayout(TypeConverter &TC,
                                  SILType Type,
                                  EnumDecl *theEnum,
                                  llvm::StructType *enumTy);
    TypeInfo *completeDynamicLayout(TypeConverter &TC,
                                    SILType Type,
                                    EnumDecl *theEnum,
                                    llvm::StructType *enumTy);

    unsigned getNumCaseBits() const {
      return CommonSpareBits.size() - CommonSpareBits.count();
    }

    /// The number of empty cases representable by each tag value.
    /// Equal to the size of the payload minus the spare bits used for tags.
    unsigned getNumCasesPerTag() const {
      unsigned numCaseBits = getNumCaseBits();
      return numCaseBits >= 32
        ? 0x80000000 : 1 << numCaseBits;
    }

    /// Extract the payload-discriminating tag from a payload and optional
    /// extra tag value.
    llvm::Value *extractPayloadTag(IRGenFunction &IGF,
                                   const EnumPayload &payload,
                                   llvm::Value *extraTagBits) const {
      unsigned numSpareBits = PayloadTagBits.count();
      llvm::Value *tag = nullptr;
      unsigned numTagBits = numSpareBits + ExtraTagBitCount;

      // Get the tag bits from spare bits, if any.
      if (numSpareBits > 0) {
        tag = payload.emitGatherSpareBits(IGF, PayloadTagBits, 0, numTagBits);
      }

      // Get the extra tag bits, if any.
      if (ExtraTagBitCount > 0) {
        assert(extraTagBits);
        if (!tag) {
          return extraTagBits;
        } else {
          extraTagBits = IGF.Builder.CreateZExt(extraTagBits, tag->getType());
          extraTagBits = IGF.Builder.CreateShl(extraTagBits,
                                               numTagBits - ExtraTagBitCount);
          return IGF.Builder.CreateOr(tag, extraTagBits);
        }
      }
      assert(!extraTagBits);
      return tag;
    }

    llvm::Type *getRefcountedPtrType(IRGenModule &IGM) const {
      switch (CopyDestroyKind) {
      case TaggedRefcounted:
        return IGM.getReferenceType(Refcounting);
      case POD:
      case BitwiseTakable:
      case Normal:
        llvm_unreachable("not a refcounted payload");
      }
    }

    void retainRefcountedPayload(IRGenFunction &IGF,
                                 llvm::Value *ptr) const {
      switch (CopyDestroyKind) {
      case TaggedRefcounted:
        IGF.emitScalarRetainCall(ptr, Refcounting);
        return;
      case POD:
      case BitwiseTakable:
      case Normal:
        llvm_unreachable("not a refcounted payload");
      }
    }

    void fixLifetimeOfRefcountedPayload(IRGenFunction &IGF,
                                        llvm::Value *ptr) const {
      switch (CopyDestroyKind) {
      case TaggedRefcounted:
        IGF.emitFixLifetime(ptr);
        return;
      case POD:
      case BitwiseTakable:
      case Normal:
        llvm_unreachable("not a refcounted payload");
      }
    }

    void releaseRefcountedPayload(IRGenFunction &IGF,
                                  llvm::Value *ptr) const {
      switch (CopyDestroyKind) {
      case TaggedRefcounted:
        IGF.emitScalarRelease(ptr, Refcounting);
        return;
      case POD:
      case BitwiseTakable:
      case Normal:
        llvm_unreachable("not a refcounted payload");
      }
    }

    APInt getEmptyCasePayload(IRGenModule &IGM,
                              unsigned tagIndex,
                              unsigned idx) const {
      // The payload may be empty.
      if (CommonSpareBits.empty())
        return APInt();
      
      APInt v = interleaveSpareBits(IGM, PayloadTagBits,
                                    PayloadTagBits.size(),
                                    tagIndex, 0);
      v |= interleaveSpareBits(IGM, CommonSpareBits,
                               CommonSpareBits.size(),
                               0, idx);
      return v;
    }
    
    struct DestructuredLoadableEnum {
      EnumPayload payload;
      llvm::Value *extraTagBits;
    };
    DestructuredLoadableEnum
    destructureLoadableEnum(IRGenFunction &IGF, Explosion &src) const {
      auto payload = EnumPayload::fromExplosion(IGF.IGM, src, PayloadSchema);
      llvm::Value *extraTagBits
        = ExtraTagBitCount > 0 ? src.claimNext() : nullptr;
      
      return {payload, extraTagBits};
    }
    
    struct DestructuredAndTaggedLoadableEnum {
      EnumPayload payload;
      llvm::Value *extraTagBits, *tag;
    };
    DestructuredAndTaggedLoadableEnum
    destructureAndTagLoadableEnum(IRGenFunction &IGF, Explosion &src) const {
      auto destructured = destructureLoadableEnum(IGF, src);
      
      llvm::Value *tag = extractPayloadTag(IGF, destructured.payload,
                                           destructured.extraTagBits);

      return {destructured.payload, destructured.extraTagBits, tag};
    }
    
    llvm::Value *
    loadDynamicTag(IRGenFunction &IGF, Address addr, SILType T) const {
      addr = IGF.Builder.CreateBitCast(addr, IGF.IGM.OpaquePtrTy);
      auto metadata = IGF.emitTypeMetadataRef(T.getSwiftRValueType());
      auto call = IGF.Builder.CreateCall(IGF.IGM.getGetEnumCaseMultiPayloadFn(),
                                         {addr.getAddress(), metadata});
      call->setDoesNotThrow();
      call->addAttribute(llvm::AttributeSet::FunctionIndex,
                         llvm::Attribute::ReadOnly);
      
      return call;
    }
    
    llvm::Value *
    loadPayloadTag(IRGenFunction &IGF, Address addr, SILType T) const {
      if (TIK >= Fixed) {
        // Load the fixed-size representation and derive the tags.
        EnumPayload payload; llvm::Value *extraTagBits;
        std::tie(payload, extraTagBits)
          = emitPrimitiveLoadPayloadAndExtraTag(IGF, addr);
        return extractPayloadTag(IGF, payload, extraTagBits);
      }
      
      // Otherwise, ask the runtime to extract the dynamically-placed tag.
      return loadDynamicTag(IGF, addr, T);
    }

  public:

    llvm::Value *
    emitGetEnumTag(IRGenFunction &IGF, SILType T, Address addr)
    const override {
      if (TIK < Fixed) {
        // Ask the runtime to extract the dynamically-placed tag.
        return loadDynamicTag(IGF, addr, T);
      }

      // For fixed-size enums, the currently inhabited case is a function of
      // both the payload tag and the payload value.
      //
      // Low-numbered payload tags correspond to payload cases. No payload
      // cases are represented with the remaining payload tags.

      // Load the fixed-size representation and derive the tags.
      EnumPayload payload; llvm::Value *extraTagBits;
      std::tie(payload, extraTagBits)
        = emitPrimitiveLoadPayloadAndExtraTag(IGF, addr);

      // Load the payload tag.
      llvm::Value *tagValue = extractPayloadTag(IGF, payload, extraTagBits);

      // If we don't have any no-payload cases, we are done -- the payload tag
      // alone is enough to distinguish between all cases.
      if (ElementsWithNoPayload.empty())
        return tagValue;

      tagValue = IGF.Builder.CreateZExtOrTrunc(tagValue, IGF.IGM.Int32Ty);

      // To distinguish between non-payload cases, load the payload value and
      // strip off the spare bits.
      auto OccupiedBits = CommonSpareBits;
      OccupiedBits.flipAll();

      llvm::Value *payloadValue = payload.emitGatherSpareBits(
          IGF, OccupiedBits, 0, 32);

      llvm::Constant *payloadCases =
          llvm::ConstantInt::get(IGF.IGM.Int32Ty, ElementsWithPayload.size());

      llvm::Value *currentCase;

      unsigned numCaseBits = getNumCaseBits();
      if (numCaseBits >= 32 ||
          getNumCasesPerTag() >= ElementsWithNoPayload.size()) {
        // All no-payload cases have the same payload tag, so we can just use
        // the payload value to distinguish between no-payload cases.
        currentCase = payloadValue;
      } else {
        // The no-payload cases are distributed between multiple payload tags;
        // combine the payload tag with the payload value.

        // First, subtract number of payload cases from the payload tag to get
        // the most significant bits of the current case.
        currentCase = IGF.Builder.CreateSub(tagValue, payloadCases);

        // Now, shift these bits into place.
        llvm::Constant *numCaseBitsVal =
            llvm::ConstantInt::get(IGF.IGM.Int32Ty, numCaseBits);
        currentCase = IGF.Builder.CreateShl(currentCase, numCaseBitsVal);

        // Add the payload value to the shifted payload tag.
        currentCase = IGF.Builder.CreateOr(currentCase, payloadValue);
      }

      // Now, we have the index of a no-payload case. Add the number of payload
      // cases back, to get an index of a case.
      currentCase = IGF.Builder.CreateAdd(currentCase, payloadCases);

      // Test if this is a payload or no-payload case.
      llvm::Value *match = IGF.Builder.CreateICmpUGE(tagValue, payloadCases);

      // Return one of the two values we computed based on the above.
      return IGF.Builder.CreateSelect(match, currentCase, tagValue);
    }

    llvm::Value *
    emitIndirectCaseTest(IRGenFunction &IGF, SILType T,
                         Address enumAddr,
                         EnumElementDecl *Case) const override {
      if (TIK >= Fixed) {
        // Load the fixed-size representation and switch directly.
        Explosion value;
        loadForSwitch(IGF, enumAddr, value);
        return emitValueCaseTest(IGF, value, Case);
      }
      
      // Use the runtime to dynamically switch.
      auto tag = loadDynamicTag(IGF, enumAddr, T);
      unsigned tagIndex = getTagIndex(Case);
      llvm::Value *expectedTag
        = llvm::ConstantInt::get(IGF.IGM.Int32Ty, tagIndex);
      return IGF.Builder.CreateICmpEQ(tag, expectedTag);
    }
    
    llvm::Value *
    emitValueCaseTest(IRGenFunction &IGF, Explosion &value,
                      EnumElementDecl *Case) const override {
      auto &C = IGF.IGM.getLLVMContext();
      auto parts = destructureAndTagLoadableEnum(IGF, value);
      unsigned numTagBits
        = cast<llvm::IntegerType>(parts.tag->getType())->getBitWidth();

      // Cases with payloads are numbered consecutively, and only required
      // testing the tag. Scan until we find the right one.
      unsigned tagIndex = 0;
      for (auto &payloadCasePair : ElementsWithPayload) {
        if (payloadCasePair.decl == Case) {
          llvm::Value *caseValue
            = llvm::ConstantInt::get(C, APInt(numTagBits,tagIndex));
          return IGF.Builder.CreateICmpEQ(parts.tag, caseValue);
        }
        ++tagIndex;
      }
      // Elements without payloads are numbered after the payload elts.
      // Multiple empty elements are packed into the payload for each tag
      // value.
      unsigned casesPerTag = getNumCasesPerTag();

      auto elti = ElementsWithNoPayload.begin(),
      eltEnd = ElementsWithNoPayload.end();

      llvm::Value *tagValue = nullptr;
      APInt payloadValue;
      for (unsigned i = 0; i < NumEmptyElementTags; ++i) {
        assert(elti != eltEnd &&
               "ran out of cases before running out of extra tags?");

        // Look through the cases for this tag.
        for (unsigned idx = 0; idx < casesPerTag && elti != eltEnd; ++idx) {
          if (elti->decl == Case) {
            tagValue = llvm::ConstantInt::get(C, APInt(numTagBits,tagIndex));
            payloadValue = getEmptyCasePayload(IGF.IGM, tagIndex, idx);
            goto found_empty_case;
          }
          ++elti;
        }
        ++tagIndex;
      }

      llvm_unreachable("Didn't find case decl");
      
    found_empty_case:
      llvm::Value *match = IGF.Builder.CreateICmpEQ(parts.tag, tagValue);
      if (CommonSpareBits.size() > 0) {
        auto payloadMatch = parts.payload
          .emitCompare(IGF, APInt::getAllOnesValue(CommonSpareBits.size()),
                       payloadValue);
        match = IGF.Builder.CreateAnd(match, payloadMatch);
      }
      return match;
    }

    void emitValueSwitch(IRGenFunction &IGF,
                         Explosion &value,
                         ArrayRef<std::pair<EnumElementDecl*,
                                            llvm::BasicBlock*>> dests,
                         llvm::BasicBlock *defaultDest) const override {
      auto &C = IGF.IGM.getLLVMContext();

      // Create a map of the destination blocks for quicker lookup.
      llvm::DenseMap<EnumElementDecl*,llvm::BasicBlock*> destMap(dests.begin(),
                                                                 dests.end());

      // Create an unreachable branch for unreachable switch defaults.
      auto *unreachableBB = llvm::BasicBlock::Create(C);

      // If there was no default branch in SIL, use the unreachable branch as
      // the default.
      if (!defaultDest)
        defaultDest = unreachableBB;

      auto blockForCase = [&](EnumElementDecl *theCase) -> llvm::BasicBlock* {
        auto found = destMap.find(theCase);
        if (found == destMap.end())
          return defaultDest;
        else
          return found->second;
      };

      auto parts = destructureAndTagLoadableEnum(IGF, value);

      // Extract and switch on the tag bits.
      unsigned numTagBits
        = cast<llvm::IntegerType>(parts.tag->getType())->getBitWidth();
      
      auto *tagSwitch = IGF.Builder.CreateSwitch(parts.tag, unreachableBB,
                             ElementsWithPayload.size() + NumEmptyElementTags);

      // Switch over the tag bits for payload cases.
      unsigned tagIndex = 0;
      for (auto &payloadCasePair : ElementsWithPayload) {
        EnumElementDecl *payloadCase = payloadCasePair.decl;
        tagSwitch->addCase(llvm::ConstantInt::get(C,APInt(numTagBits,tagIndex)),
                           blockForCase(payloadCase));
        ++tagIndex;
      }

      // Switch over the no-payload cases.
      unsigned casesPerTag = getNumCasesPerTag();

      auto elti = ElementsWithNoPayload.begin(),
           eltEnd = ElementsWithNoPayload.end();

      for (unsigned i = 0; i < NumEmptyElementTags; ++i) {
        assert(elti != eltEnd &&
               "ran out of cases before running out of extra tags?");
        
        auto tagVal = llvm::ConstantInt::get(C, APInt(numTagBits, tagIndex));
        
        // If the payload is empty, there's only one case per tag.
        if (CommonSpareBits.size() == 0) {
          tagSwitch->addCase(tagVal, blockForCase(elti->decl));
        
          ++elti;
          ++tagIndex;
          continue;
        }
        
        auto *tagBB = llvm::BasicBlock::Create(C);
        tagSwitch->addCase(tagVal, tagBB);

        // Switch over the cases for this tag.
        IGF.Builder.emitBlock(tagBB);
        SmallVector<std::pair<APInt, llvm::BasicBlock *>, 4> cases;
        
        for (unsigned idx = 0; idx < casesPerTag && elti != eltEnd; ++idx) {
          auto val = getEmptyCasePayload(IGF.IGM, tagIndex, idx);
          cases.push_back({val, blockForCase(elti->decl)});
          ++elti;
        }

        parts.payload.emitSwitch(IGF, APInt::getAllOnesValue(PayloadBitCount),
                               cases,
                               SwitchDefaultDest(unreachableBB, IsUnreachable));
        ++tagIndex;
      }

      // Delete the unreachable default block if we didn't use it, or emit it
      // if we did.
      if (unreachableBB->use_empty()) {
        delete unreachableBB;
      } else {
        IGF.Builder.emitBlock(unreachableBB);
        IGF.Builder.CreateUnreachable();
      }
    }
    
  private:
    void emitDynamicSwitch(IRGenFunction &IGF,
                           SILType T,
                           Address addr,
                           ArrayRef<std::pair<EnumElementDecl*,
                                              llvm::BasicBlock*>> dests,
                           llvm::BasicBlock *defaultDest) const {
      // Ask the runtime to derive the tag index.
      auto tag = loadDynamicTag(IGF, addr, T);
      
      // Switch on the tag value.
      
      // Create a map of the destination blocks for quicker lookup.
      llvm::DenseMap<EnumElementDecl*,llvm::BasicBlock*> destMap(dests.begin(),
                                                                 dests.end());

      // Create an unreachable branch for unreachable switch defaults.
      auto &C = IGF.IGM.getLLVMContext();
      auto *unreachableBB = llvm::BasicBlock::Create(C);

      // If there was no default branch in SIL, use the unreachable branch as
      // the default.
      if (!defaultDest)
        defaultDest = unreachableBB;

      auto blockForCase = [&](EnumElementDecl *theCase) -> llvm::BasicBlock* {
        auto found = destMap.find(theCase);
        if (found == destMap.end())
          return defaultDest;
        else
          return found->second;
      };

      auto *tagSwitch = IGF.Builder.CreateSwitch(tag, unreachableBB,
                                                 NumElements);

      unsigned tagIndex = 0;
      
      // Payload tags come first.
      for (auto &elt : ElementsWithPayload) {
        auto tagVal = llvm::ConstantInt::get(IGF.IGM.Int32Ty, tagIndex);
        tagSwitch->addCase(tagVal, blockForCase(elt.decl));
        ++tagIndex;
      }
      
      // Next come empty tags.
      for (auto &elt : ElementsWithNoPayload) {
        auto tagVal = llvm::ConstantInt::get(IGF.IGM.Int32Ty, tagIndex);
        tagSwitch->addCase(tagVal, blockForCase(elt.decl));
        ++tagIndex;
      }

      assert(tagIndex == NumElements);

      // Delete the unreachable default block if we didn't use it, or emit it
      // if we did.
      if (unreachableBB->use_empty()) {
        delete unreachableBB;
      } else {
        IGF.Builder.emitBlock(unreachableBB);
        IGF.Builder.CreateUnreachable();
      }
    }
  
  public:
    void emitIndirectSwitch(IRGenFunction &IGF,
                            SILType T,
                            Address addr,
                            ArrayRef<std::pair<EnumElementDecl*,
                                               llvm::BasicBlock*>> dests,
                            llvm::BasicBlock *defaultDest) const override {
      if (TIK >= Fixed) {
        // Load the fixed-size representation and switch directly.
        Explosion value;
        loadForSwitch(IGF, addr, value);
        return emitValueSwitch(IGF, value, dests, defaultDest);
      }

      // Use the runtime to dynamically switch.
      return emitDynamicSwitch(IGF, T, addr, dests, defaultDest);
    }

  private:
    void projectPayloadValue(IRGenFunction &IGF,
                             EnumPayload payload,
                             unsigned payloadTag,
                             const LoadableTypeInfo &payloadTI,
                             Explosion &out) const {
      // If the payload is empty, so is the explosion.
      if (CommonSpareBits.size() == 0)
        return;
      
      // If we have spare bits, we have to mask out any set tag bits packed
      // there.
      if (PayloadTagBits.any()) {
        unsigned spareBitCount = PayloadTagBits.count();
        if (spareBitCount < 32)
          payloadTag &= (1U << spareBitCount) - 1U;
        if (payloadTag != 0) {
          APInt mask = ~PayloadTagBits.asAPInt();
          payload.emitApplyAndMask(IGF, mask);
        }
      }

      // Unpack the payload.
      payloadTI.unpackFromEnumPayload(IGF, payload, out, 0);
    }

  public:
    void emitValueProject(IRGenFunction &IGF,
                          Explosion &inValue,
                          EnumElementDecl *theCase,
                          Explosion &out) const override {
      auto foundPayload = std::find_if(ElementsWithPayload.begin(),
                                       ElementsWithPayload.end(),
             [&](const Element &e) { return e.decl == theCase; });

      // Non-payload cases project to an empty explosion.
      if (foundPayload == ElementsWithPayload.end()) {
        inValue.claim(getExplosionSize());
        return;
      }

      auto parts = destructureLoadableEnum(IGF, inValue);

      // Unpack the payload.
      projectPayloadValue(IGF, parts.payload,
                          foundPayload - ElementsWithPayload.begin(),
                     cast<LoadableTypeInfo>(*foundPayload->ti), out);
    }

    void packIntoEnumPayload(IRGenFunction &IGF,
                             EnumPayload &outerPayload,
                             Explosion &src,
                             unsigned offset) const override {
      auto innerPayload = EnumPayload::fromExplosion(IGF.IGM, src,
                                                     PayloadSchema);
      // Pack the payload, if any.
      innerPayload.packIntoEnumPayload(IGF, outerPayload, offset);
      // Pack the extra bits, if any.
      if (ExtraTagBitCount > 0)
        outerPayload.insertValue(IGF, src.claimNext(),
                                 CommonSpareBits.size() + offset);
    }

    void unpackFromEnumPayload(IRGenFunction &IGF,
                               const EnumPayload &outerPayload,
                               Explosion &dest,
                               unsigned offset) const override {
      // Unpack the payload.
      auto inner
        = EnumPayload::unpackFromEnumPayload(IGF, outerPayload, offset,
                                             PayloadSchema);
      inner.explode(IGF.IGM, dest);
      // Unpack the extra bits, if any.
      if (ExtraTagBitCount > 0)
        dest.add(outerPayload.extractValue(IGF, extraTagTy,
                                           CommonSpareBits.size() + offset));
    }

  private:
    void emitPayloadInjection(IRGenFunction &IGF,
                              const FixedTypeInfo &payloadTI,
                              Explosion &params, Explosion &out,
                              unsigned tag) const {
      // Pack the payload.
      auto &loadablePayloadTI = cast<LoadableTypeInfo>(payloadTI); // FIXME
      
      auto payload = EnumPayload::zero(IGF.IGM, PayloadSchema);
      loadablePayloadTI.packIntoEnumPayload(IGF, payload, params, 0);

      // If we have spare bits, pack tag bits into them.
      unsigned numSpareBits = PayloadTagBits.count();
      if (numSpareBits > 0) {
        APInt tagMaskVal
          = interleaveSpareBits(IGF.IGM, PayloadTagBits,
                                PayloadTagBits.size(), tag, 0);
        payload.emitApplyOrMask(IGF, tagMaskVal);
      }

      payload.explode(IGF.IGM, out);

      // If we have extra tag bits, pack the remaining tag bits into them.
      if (ExtraTagBitCount > 0) {
        tag >>= numSpareBits;
        auto extra = llvm::ConstantInt::get(IGF.IGM.getLLVMContext(),
                                            APInt(ExtraTagBitCount, tag));
        out.add(extra);
      }
    }

    std::pair<APInt, APInt>
    getNoPayloadCaseValue(unsigned index) const {
      // Figure out the tag and payload for the empty case.
      unsigned numCaseBits = getNumCaseBits();
      unsigned tag, tagIndex;
      if (numCaseBits >= 32) {
        tag = ElementsWithPayload.size();
        tagIndex = index;
      } else {
        tag = (index >> numCaseBits) + ElementsWithPayload.size();
        tagIndex = index & ((1 << numCaseBits) - 1);
      }

      APInt payload;
      APInt extraTag;
      unsigned numSpareBits = CommonSpareBits.count();
      if (numSpareBits > 0) {
        // If we have spare bits, pack tag bits into them.
        payload = getEmptyCasePayload(IGM, tag, tagIndex);
      } else if (CommonSpareBits.size() > 0) {
        // Otherwise the payload is just the index.
        payload = APInt(CommonSpareBits.size(), tagIndex);
      }

      // If we have extra tag bits, pack the remaining tag bits into them.
      if (ExtraTagBitCount > 0) {
        tag >>= numSpareBits;
        extraTag = APInt(ExtraTagBitCount, tag);
      }
      return {payload, extraTag};
    }

    void emitNoPayloadInjection(IRGenFunction &IGF, Explosion &out,
                                unsigned index) const {
      APInt payloadVal, extraTag;
      std::tie(payloadVal, extraTag) = getNoPayloadCaseValue(index);
      
      auto payload = EnumPayload::fromBitPattern(IGF.IGM, payloadVal,
                                                 PayloadSchema);
      payload.explode(IGF.IGM, out);
      if (ExtraTagBitCount > 0) {
        out.add(llvm::ConstantInt::get(IGF.IGM.getLLVMContext(), extraTag));
      }
    }

    void forNontrivialPayloads(IRGenFunction &IGF, llvm::Value *tag,
               std::function<void (unsigned, EnumImplStrategy::Element)> f)
    const {
      auto *endBB = llvm::BasicBlock::Create(IGF.IGM.getLLVMContext());

      auto *swi = IGF.Builder.CreateSwitch(tag, endBB);
      auto *tagTy = cast<llvm::IntegerType>(tag->getType());

      // Handle nontrivial tags.
      unsigned tagIndex = 0;
      for (auto &payloadCasePair : ElementsWithPayload) {
        auto &payloadTI = *payloadCasePair.ti;

        // Trivial payloads don't need any work.
        if (payloadTI.isPOD(ResilienceScope::Component)) {
          ++tagIndex;
          continue;
        }

        // Unpack and handle nontrivial payloads.
        auto *caseBB = llvm::BasicBlock::Create(IGF.IGM.getLLVMContext());
        swi->addCase(llvm::ConstantInt::get(tagTy, tagIndex), caseBB);

        IGF.Builder.emitBlock(caseBB);
        f(tagIndex, payloadCasePair);
        IGF.Builder.CreateBr(endBB);

        ++tagIndex;
      }

      IGF.Builder.emitBlock(endBB);
    }

    void maskTagBitsFromPayload(IRGenFunction &IGF,
                                EnumPayload &payload) const {
      if (PayloadTagBits.none())
        return;

      APInt mask = ~PayloadTagBits.asAPInt();
      payload.emitApplyAndMask(IGF, mask);
    }
    
  public:
    void emitValueInjection(IRGenFunction &IGF,
                            EnumElementDecl *elt,
                            Explosion &params,
                            Explosion &out) const override {
      // See whether this is a payload or empty case we're emitting.
      auto payloadI = std::find_if(ElementsWithPayload.begin(),
                                   ElementsWithPayload.end(),
                               [&](const Element &e) { return e.decl == elt; });
      if (payloadI != ElementsWithPayload.end())
        return emitPayloadInjection(IGF, cast<FixedTypeInfo>(*payloadI->ti),
                                    params, out,
                                    payloadI - ElementsWithPayload.begin());

      auto emptyI = std::find_if(ElementsWithNoPayload.begin(),
                                 ElementsWithNoPayload.end(),
                               [&](const Element &e) { return e.decl == elt; });
      assert(emptyI != ElementsWithNoPayload.end() && "case not in enum");
      emitNoPayloadInjection(IGF, out, emptyI - ElementsWithNoPayload.begin());
    }

    void copy(IRGenFunction &IGF, Explosion &src, Explosion &dest)
    const override {
      assert(TIK >= Loadable);

      switch (CopyDestroyKind) {
      case POD:
        reexplode(IGF, src, dest);
        return;

      case BitwiseTakable:
      case Normal: {
        auto parts = destructureAndTagLoadableEnum(IGF, src);
        
        forNontrivialPayloads(IGF, parts.tag,
          [&](unsigned tagIndex, EnumImplStrategy::Element elt) {
            auto &lti = cast<LoadableTypeInfo>(*elt.ti);
            Explosion value;
            projectPayloadValue(IGF, parts.payload, tagIndex, lti, value);

            Explosion tmp;
            lti.copy(IGF, value, tmp);
            tmp.claimAll(); // FIXME: repack if not bit-identical
          });

        parts.payload.explode(IGF.IGM, dest);
        if (parts.extraTagBits)
          dest.add(parts.extraTagBits);
        return;
      }

      case TaggedRefcounted: {
        auto parts = destructureLoadableEnum(IGF, src);

        // Hold onto the original payload, so we can pass it on as the copy.
        auto origPayload = parts.payload;

        // Mask the tag bits out of the payload, if any.
        maskTagBitsFromPayload(IGF, parts.payload);

        // Retain the pointer.
        auto ptr = parts.payload.extractValue(IGF,
                                          getRefcountedPtrType(IGF.IGM), 0);
        retainRefcountedPayload(IGF, ptr);

        origPayload.explode(IGF.IGM, dest);
        if (parts.extraTagBits)
          dest.add(parts.extraTagBits);
        return;
      }
      }

    }

    void consume(IRGenFunction &IGF, Explosion &src) const override {
      assert(TIK >= Loadable);

      switch (CopyDestroyKind) {
      case POD:
        src.claim(getExplosionSize());
        return;

      case BitwiseTakable:
      case Normal: {
        auto parts = destructureAndTagLoadableEnum(IGF, src);

        forNontrivialPayloads(IGF, parts.tag,
          [&](unsigned tagIndex, EnumImplStrategy::Element elt) {
            auto &lti = cast<LoadableTypeInfo>(*elt.ti);
            Explosion value;
            projectPayloadValue(IGF, parts.payload, tagIndex, lti, value);

            lti.consume(IGF, value);
          });
        return;
      }

      case TaggedRefcounted: {
        auto parts = destructureLoadableEnum(IGF, src);
        // Mask the tag bits out of the payload, if any.
        maskTagBitsFromPayload(IGF, parts.payload);
        
        // Release the pointer.
        auto ptr = parts.payload.extractValue(IGF,
                                          getRefcountedPtrType(IGF.IGM), 0);
        releaseRefcountedPayload(IGF, ptr);
        return;
      }
      }
    }

    void fixLifetime(IRGenFunction &IGF, Explosion &src) const override {
      assert(TIK >= Loadable);

      switch (CopyDestroyKind) {
      case POD:
        src.claim(getExplosionSize());
        return;

      case BitwiseTakable:
      case Normal: {
        auto parts = destructureAndTagLoadableEnum(IGF, src);

        forNontrivialPayloads(IGF, parts.tag,
          [&](unsigned tagIndex, EnumImplStrategy::Element elt) {
            auto &lti = cast<LoadableTypeInfo>(*elt.ti);
            Explosion value;
            projectPayloadValue(IGF, parts.payload, tagIndex, lti, value);

            lti.fixLifetime(IGF, value);
          });
        return;
      }

      case TaggedRefcounted: {
        auto parts = destructureLoadableEnum(IGF, src);
        // Mask the tag bits out of the payload, if any.
        maskTagBitsFromPayload(IGF, parts.payload);
        
        // Fix the pointer.
        auto ptr = parts.payload.extractValue(IGF,
                                          getRefcountedPtrType(IGF.IGM), 0);
        fixLifetimeOfRefcountedPayload(IGF, ptr);
        return;
      }
      }
    }

  private:
    /// Emit a reassignment sequence from an enum at one address to another.
    void emitIndirectAssign(IRGenFunction &IGF,
                            Address dest, Address src, SILType T,
                            IsTake_t isTake) const {
      auto &C = IGF.IGM.getLLVMContext();

      switch (CopyDestroyKind) {
      case POD:
        return emitPrimitiveCopy(IGF, dest, src, T);

      case BitwiseTakable:
      case TaggedRefcounted:
      case Normal: {
        // If the enum is loadable, it's better to do this directly using
        // values, so we don't need to RMW tag bits in place.
        if (TI->isLoadable()) {
          Explosion tmpSrc, tmpOld;
          if (isTake)
            loadAsTake(IGF, src, tmpSrc);
          else
            loadAsCopy(IGF, src, tmpSrc);

          loadAsTake(IGF, dest, tmpOld);
          initialize(IGF, tmpSrc, dest);
          consume(IGF, tmpOld);
          return;
        }

        auto *endBB = llvm::BasicBlock::Create(C);

        // Check whether the source and destination alias.
        llvm::Value *alias = IGF.Builder.CreateICmpEQ(dest.getAddress(),
                                                      src.getAddress());
        auto *noAliasBB = llvm::BasicBlock::Create(C);
        IGF.Builder.CreateCondBr(alias, endBB, noAliasBB);
        IGF.Builder.emitBlock(noAliasBB);

        // Destroy the old value.
        destroy(IGF, dest, T);

        // Reinitialize with the new value.
        emitIndirectInitialize(IGF, dest, src, T, isTake);

        IGF.Builder.CreateBr(endBB);
        IGF.Builder.emitBlock(endBB);
        return;
      }
      }
    }

    void emitIndirectInitialize(IRGenFunction &IGF,
                                Address dest, Address src,
                                SILType T,
                                IsTake_t isTake) const{
      auto &C = IGF.IGM.getLLVMContext();

      switch (CopyDestroyKind) {
      case POD:
        return emitPrimitiveCopy(IGF, dest, src, T);

      case BitwiseTakable:
      case TaggedRefcounted:
        // Takes can be done by primitive copy in these case.
        if (isTake)
          return emitPrimitiveCopy(IGF, dest, src, T);
        SWIFT_FALLTHROUGH;
        
      case Normal: {
        // If the enum is loadable, do this directly using values, since we
        // have to strip spare bits from the payload.
        if (TI->isLoadable()) {
          Explosion tmpSrc;
          if (isTake)
            loadAsTake(IGF, src, tmpSrc);
          else
            loadAsCopy(IGF, src, tmpSrc);
          initialize(IGF, tmpSrc, dest);
          return;
        }

        // If the enum is address-only, we better not have any spare bits,
        // otherwise we have no way of copying the original payload without
        // destructively modifying it in place.
        assert(PayloadTagBits.none() &&
               "address-only multi-payload enum layout cannot use spare bits");

        llvm::Value *tag = loadPayloadTag(IGF, src, T);

        auto *endBB = llvm::BasicBlock::Create(C);

        /// Switch out nontrivial payloads.
        auto *trivialBB = llvm::BasicBlock::Create(C);
        auto *swi = IGF.Builder.CreateSwitch(tag, trivialBB);
        auto *tagTy = cast<llvm::IntegerType>(tag->getType());

        unsigned tagIndex = 0;
        for (auto &payloadCasePair : ElementsWithPayload) {
          SILType PayloadT = T.getEnumElementType(payloadCasePair.decl,
                                                  *IGF.IGM.SILMod);
          auto &payloadTI = *payloadCasePair.ti;
          // Trivial and, in the case of a take, bitwise-takable payloads,
          // can all share the default path.
          if (payloadTI.isPOD(ResilienceScope::Component)
              || (isTake && payloadTI.isBitwiseTakable(ResilienceScope::Component))) {
            ++tagIndex;
            continue;
          }

          // For nontrivial payloads, we need to copy/take the payload using its
          // value semantics.
          auto *caseBB = llvm::BasicBlock::Create(C);
          swi->addCase(llvm::ConstantInt::get(tagTy, tagIndex), caseBB);
          IGF.Builder.emitBlock(caseBB);

          // Do the take/copy of the payload.
          Address srcData = IGF.Builder.CreateBitCast(src,
                                  payloadTI.getStorageType()->getPointerTo());
          Address destData = IGF.Builder.CreateBitCast(dest,
                                    payloadTI.getStorageType()->getPointerTo());

          if (isTake)
            payloadTI.initializeWithTake(IGF, destData, srcData, PayloadT);
          else
            payloadTI.initializeWithCopy(IGF, destData, srcData, PayloadT);

          // Plant spare bit tag bits, if any, into the new value.
          storePayloadTag(IGF, dest, tagIndex, T);
          IGF.Builder.CreateBr(endBB);

          ++tagIndex;
        }

        // For trivial payloads (including no-payload cases), we can just
        // primitive-copy to the destination.
        IGF.Builder.emitBlock(trivialBB);
        emitPrimitiveCopy(IGF, dest, src, T);
        IGF.Builder.CreateBr(endBB);

        IGF.Builder.emitBlock(endBB);
      }
      }
    }

  public:
    void assignWithCopy(IRGenFunction &IGF, Address dest, Address src,
                        SILType T)
    const override {
      emitIndirectAssign(IGF, dest, src, T, IsNotTake);
    }

    void assignWithTake(IRGenFunction &IGF, Address dest, Address src,
                        SILType T)
    const override {
      emitIndirectAssign(IGF, dest, src, T, IsTake);
    }

    void initializeWithCopy(IRGenFunction &IGF, Address dest, Address src,
                            SILType T)
    const override {
      emitIndirectInitialize(IGF, dest, src, T, IsNotTake);
    }

    void initializeWithTake(IRGenFunction &IGF, Address dest, Address src,
                            SILType T)
    const override {
      emitIndirectInitialize(IGF, dest, src, T, IsTake);
    }

    void destroy(IRGenFunction &IGF, Address addr, SILType T)
    const override {
      switch (CopyDestroyKind) {
      case POD:
        return;

      case BitwiseTakable:
      case Normal:
      case TaggedRefcounted: {
        // If loadable, it's better to do this directly to the value than
        // in place, so we don't need to RMW out the tag bits in memory.
        if (TI->isLoadable()) {
          Explosion tmp;
          loadAsTake(IGF, addr, tmp);
          consume(IGF, tmp);
          return;
        }

        auto tag = loadPayloadTag(IGF, addr, T);

        forNontrivialPayloads(IGF, tag,
          [&](unsigned tagIndex, EnumImplStrategy::Element elt) {
            // Clear tag bits out of the payload area, if any.
            destructiveProjectDataForLoad(IGF, T, addr);
            // Destroy the data.
            Address dataAddr = IGF.Builder.CreateBitCast(addr,
                                      elt.ti->getStorageType()->getPointerTo());
            SILType payloadT = T.getEnumElementType(elt.decl, *IGF.IGM.SILMod);
            elt.ti->destroy(IGF, dataAddr, payloadT);
          });
        return;
      }
      }
    }

  private:
    void storePayloadTag(IRGenFunction &IGF,
                         Address enumAddr, unsigned index,
                         SILType T) const {
      // Use the runtime to initialize dynamic cases.
      if (TIK < Fixed) {
        return storeDynamicTag(IGF, enumAddr, index, T);
      }

      // If the tag has spare bits, we need to mask them into the
      // payload area.
      unsigned numSpareBits = PayloadTagBits.count();
      if (numSpareBits > 0) {
        unsigned spareTagBits = numSpareBits >= 32
          ? index : index & ((1U << numSpareBits) - 1U);

        // Mask the spare bits into the payload area.
        Address payloadAddr = projectPayload(IGF, enumAddr);
        auto payload = EnumPayload::load(IGF, payloadAddr, PayloadSchema);
        
        auto spareBitMask = ~PayloadTagBits.asAPInt();
        APInt tagBitMask
          = interleaveSpareBits(IGF.IGM, PayloadTagBits, PayloadTagBits.size(),
                                spareTagBits, 0);

        payload.emitApplyAndMask(IGF, spareBitMask);
        payload.emitApplyOrMask(IGF, tagBitMask);
        payload.store(IGF, payloadAddr);
      }

      // Initialize the extra tag bits, if we have them.
      if (ExtraTagBitCount > 0) {
        unsigned extraTagBits = index >> numSpareBits;
        auto *extraTagValue = llvm::ConstantInt::get(IGF.IGM.getLLVMContext(),
                                        APInt(ExtraTagBitCount, extraTagBits));
        IGF.Builder.CreateStore(extraTagValue,
                                projectExtraTagBits(IGF, enumAddr));
      }
    }

    void storeNoPayloadTag(IRGenFunction &IGF, Address enumAddr,
                           unsigned index, SILType T) const {
      // Use the runtime to initialize dynamic cases.
      if (TIK < Fixed) {
        // Dynamic case indexes start after the payload cases.
        return storeDynamicTag(IGF, enumAddr,
                               index + ElementsWithPayload.size(), T);
      }

      // We can just primitive-store the representation for the empty case.
      APInt payloadValue, extraTag;
      std::tie(payloadValue, extraTag) = getNoPayloadCaseValue(index);
      
      auto payload = EnumPayload::fromBitPattern(IGF.IGM, payloadValue,
                                                 PayloadSchema);
      payload.store(IGF, projectPayload(IGF, enumAddr));
      if (ExtraTagBitCount > 0) {
        IGF.Builder.CreateStore(
                    llvm::ConstantInt::get(IGF.IGM.getLLVMContext(), extraTag),
                    projectExtraTagBits(IGF, enumAddr));
      }
    }
    
    void storeDynamicTag(IRGenFunction &IGF, Address enumAddr, unsigned index,
                         SILType T) const {
      // Invoke the runtime to store the tag.
      enumAddr = IGF.Builder.CreateBitCast(enumAddr, IGF.IGM.OpaquePtrTy);
      auto indexVal = llvm::ConstantInt::get(IGF.IGM.Int32Ty, index);
      auto metadata = IGF.emitTypeMetadataRef(T.getSwiftRValueType());
      
      auto call = IGF.Builder.CreateCall(
                                     IGF.IGM.getStoreEnumTagMultiPayloadFn(),
                                     {enumAddr.getAddress(), metadata, indexVal});
      call->setDoesNotThrow();
    }

  public:

    void storeTag(IRGenFunction &IGF,
                  SILType T,
                  Address enumAddr,
                  EnumElementDecl *Case) const override {
      // See whether this is a payload or empty case we're emitting.
      auto payloadI = std::find_if(ElementsWithPayload.begin(),
                                   ElementsWithPayload.end(),
                               [&](const Element &e) { return e.decl == Case; });
      if (payloadI != ElementsWithPayload.end()) {
        unsigned index = payloadI - ElementsWithPayload.begin();

        return storePayloadTag(IGF, enumAddr, index, T);
      }

      auto emptyI = std::find_if(ElementsWithNoPayload.begin(),
                                 ElementsWithNoPayload.end(),
                               [&](const Element &e) { return e.decl == Case; });
      assert(emptyI != ElementsWithNoPayload.end() && "case not in enum");
      unsigned index = emptyI - ElementsWithNoPayload.begin();
      
      // Use the runtime to store a dynamic tag. Empty tag values always follow
      // the payloads.
      storeNoPayloadTag(IGF, enumAddr, index, T);
    }

    /// Clear any tag bits stored in the payload area of the given address.
    void destructiveProjectDataForLoad(IRGenFunction &IGF,
                                       SILType T,
                                       Address enumAddr) const override {
      // If the case has non-zero tag bits stored in spare bits, we need to
      // mask them out before the data can be read.
      unsigned numSpareBits = PayloadTagBits.count();
      if (numSpareBits > 0) {
        Address payloadAddr = projectPayload(IGF, enumAddr);
        auto payload = EnumPayload::load(IGF, payloadAddr, PayloadSchema);
        auto spareBitMask = ~PayloadTagBits.asAPInt();
        payload.emitApplyAndMask(IGF, spareBitMask);
        payload.store(IGF, payloadAddr);
      }
    }

    llvm::Value *emitPayloadLayoutArray(IRGenFunction &IGF, SILType T) const {
      auto numPayloads = ElementsWithPayload.size();
      auto metadataBufferTy = llvm::ArrayType::get(IGF.IGM.Int8PtrPtrTy,
                                                   numPayloads);
      auto metadataBuffer = IGF.createAlloca(metadataBufferTy,
                                             IGF.IGM.getPointerAlignment(),
                                             "payload_types");
      llvm::Value *firstAddr;
      for (unsigned i = 0; i < numPayloads; ++i) {
        auto &elt = ElementsWithPayload[i];
        Address eltAddr = IGF.Builder.CreateStructGEP(metadataBuffer, i,
                                                  IGF.IGM.getPointerSize() * i);
        if (i == 0) firstAddr = eltAddr.getAddress();
        
        auto payloadTy = T.getEnumElementType(elt.decl, *IGF.IGM.SILMod);
        
        auto metadata = IGF.emitTypeLayoutRef(payloadTy);
        
        IGF.Builder.CreateStore(metadata, eltAddr);
      }
      
      return firstAddr;
    }

    void initializeMetadata(IRGenFunction &IGF,
                            llvm::Value *metadata,
                            llvm::Value *vwtable,
                            SILType T) const override {
      // Fixed-size enums don't need dynamic metadata initialization.
      if (TIK >= Fixed) return;
      
      // Ask the runtime to set up the metadata record for a dynamic enum.
      auto payloadLayoutArray = emitPayloadLayoutArray(IGF, T);
      auto numPayloadsVal = llvm::ConstantInt::get(IGF.IGM.SizeTy,
                                                   ElementsWithPayload.size());

      IGF.Builder.CreateCall(IGF.IGM.getInitEnumMetadataMultiPayloadFn(),
                             {vwtable, metadata, numPayloadsVal,
                              payloadLayoutArray});
    }

    /// \group Extra inhabitants

    // TODO

    bool mayHaveExtraInhabitants(IRGenModule &) const override { return false; }

    llvm::Value *getExtraInhabitantIndex(IRGenFunction &IGF,
                                         Address src,
                                         SILType T) const override {
      llvm_unreachable("extra inhabitants for multi-payload enums not implemented");
    }

    void storeExtraInhabitant(IRGenFunction &IGF,
                              llvm::Value *index,
                              Address dest,
                              SILType T) const override {
      llvm_unreachable("extra inhabitants for multi-payload enums not implemented");
    }
    
    APInt
    getFixedExtraInhabitantMask(IRGenModule &IGM) const override {
      // TODO may not always be all-ones
      return APInt::getAllOnesValue(
                      cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits());
    }
    
    unsigned getFixedExtraInhabitantCount(IRGenModule &IGM) const override {
      return 0;
    }

    APInt
    getFixedExtraInhabitantValue(IRGenModule &IGM,
                                 unsigned bits,
                                 unsigned index) const override {
      llvm_unreachable("extra inhabitants for multi-payload enums not implemented");
    }

    ClusteredBitVector
    getBitPatternForNoPayloadElement(EnumElementDecl *theCase) const override {
      assert(TIK >= Fixed);

      APInt payloadPart, extraPart;

      auto emptyI = std::find_if(ElementsWithNoPayload.begin(),
                                 ElementsWithNoPayload.end(),
                           [&](const Element &e) { return e.decl == theCase; });
      assert(emptyI != ElementsWithNoPayload.end() && "case not in enum");

      unsigned index = emptyI - ElementsWithNoPayload.begin();

      std::tie(payloadPart, extraPart) = getNoPayloadCaseValue(index);
      ClusteredBitVector bits;
      
      if (CommonSpareBits.size() > 0)
        bits = getBitVectorFromAPInt(payloadPart);

      unsigned totalSize
        = cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits();
      if (ExtraTagBitCount > 0) {
        ClusteredBitVector extraBits =
          getBitVectorFromAPInt(extraPart, bits.size());
        bits.extendWithClearBits(totalSize);
        extraBits.extendWithClearBits(totalSize);
        bits |= extraBits;
      } else {
        assert(totalSize == bits.size());
      }
      return bits;
    }

    ClusteredBitVector
    getBitMaskForNoPayloadElements() const override {
      assert(TIK >= Fixed);

      // All bits are significant.
      // TODO: They don't have to be.
      return ClusteredBitVector::getConstant(
                       cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits(),
                       true);
    }

    ClusteredBitVector getTagBitsForPayloads() const override {
      assert(TIK >= Fixed);
      
      ClusteredBitVector result = PayloadTagBits;

      unsigned totalSize
        = cast<FixedTypeInfo>(TI)->getFixedSize().getValueInBits();

      if (ExtraTagBitCount) {
        result.appendSetBits(ExtraTagBitCount);
        result.extendWithClearBits(totalSize);
      } else {
        assert(PayloadTagBits.size() == totalSize);
      }
      return result;
    }
  };

  class ResilientEnumImplStrategy final
    : public EnumImplStrategy
  {
  public:
    ResilientEnumImplStrategy(IRGenModule &IGM,
                              unsigned NumElements,
                              std::vector<Element> &&WithPayload,
                              std::vector<Element> &&WithNoPayload)
      : EnumImplStrategy(IGM, Opaque, IsFixedSize,
                         NumElements,
                         std::move(WithPayload),
                         std::move(WithNoPayload))
    { }

    TypeInfo *completeEnumTypeLayout(TypeConverter &TC,
                                     SILType Type,
                                     EnumDecl *theEnum,
                                     llvm::StructType *enumTy) override;

    void destructiveProjectDataForLoad(IRGenFunction &IGF,
                                       SILType T,
                                       Address enumAddr) const override {
      emitDestructiveProjectEnumDataCall(IGF, T, enumAddr.getAddress());
    }

    void storeTag(IRGenFunction &IGF,
                  SILType T,
                  Address enumAddr,
                  EnumElementDecl *Case) const override {
      llvm_unreachable("resilient enums cannot be constructed directly");
    }

    llvm::Value *
    emitIndirectCaseTest(IRGenFunction &IGF, SILType T,
                         Address enumAddr,
                         EnumElementDecl *Case) const override {
      llvm::Value *tag = emitGetEnumTagCall(IGF, T, enumAddr.getAddress());
      llvm::Value *expectedTag = llvm::ConstantInt::get(IGF.IGM.Int32Ty,
                                                        getTagIndex(Case));
      return IGF.Builder.CreateICmpEQ(tag, expectedTag);
    }

    void emitIndirectSwitch(IRGenFunction &IGF,
                            SILType T,
                            Address enumAddr,
                            ArrayRef<std::pair<EnumElementDecl*,
                                               llvm::BasicBlock*>> dests,
                            llvm::BasicBlock *defaultDest) const override {
      // Switch on the tag value.
      llvm::Value *tag = emitGetEnumTagCall(IGF, T, enumAddr.getAddress());

      // Create a map of the destination blocks for quicker lookup.
      llvm::DenseMap<EnumElementDecl*,llvm::BasicBlock*> destMap(dests.begin(),
                                                                 dests.end());

      // Create an unreachable branch for unreachable switch defaults.
      auto &C = IGF.IGM.getLLVMContext();
      auto *unreachableBB = llvm::BasicBlock::Create(C);

      // If there was no default branch in SIL, use the unreachable branch as
      // the default.
      if (!defaultDest)
        defaultDest = unreachableBB;

      auto *tagSwitch = IGF.Builder.CreateSwitch(tag, defaultDest, NumElements);

      unsigned tagIndex = 0;
      
      // Payload tags come first.
      for (auto &elt : ElementsWithPayload) {
        auto found = destMap.find(elt.decl);
        if (found != destMap.end()) {
          auto tagVal = llvm::ConstantInt::get(IGF.IGM.Int32Ty, tagIndex);
          tagSwitch->addCase(tagVal, found->second);
        }
        ++tagIndex;
      }

      // Next come empty tags.
      for (auto &elt : ElementsWithNoPayload) {
        auto found = destMap.find(elt.decl);
        if (found != destMap.end()) {
          auto tagVal = llvm::ConstantInt::get(IGF.IGM.Int32Ty, tagIndex);
          tagSwitch->addCase(tagVal, found->second);
        }
        ++tagIndex;
      }

      assert(tagIndex == NumElements);

      // Delete the unreachable default block if we didn't use it, or emit it
      // if we did.
      if (unreachableBB->use_empty()) {
        delete unreachableBB;
      } else {
        IGF.Builder.emitBlock(unreachableBB);
        IGF.Builder.CreateUnreachable();
      }
    }

    void assignWithCopy(IRGenFunction &IGF, Address dest, Address src,
                        SILType T)
    const override {
      emitAssignWithCopyCall(IGF, T,
                             dest.getAddress(), src.getAddress());
    }

    void assignWithTake(IRGenFunction &IGF, Address dest, Address src,
                        SILType T)
    const override {
      emitAssignWithTakeCall(IGF, T,
                             dest.getAddress(), src.getAddress());
    }

    void initializeWithCopy(IRGenFunction &IGF, Address dest, Address src,
                            SILType T)
    const override {
      emitInitializeWithCopyCall(IGF, T,
                                 dest.getAddress(), src.getAddress());
    }

    void initializeWithTake(IRGenFunction &IGF, Address dest, Address src,
                            SILType T)
    const override {
      emitInitializeWithTakeCall(IGF, T,
                                 dest.getAddress(), src.getAddress());
    }

    void destroy(IRGenFunction &IGF, Address addr, SILType T)
    const override {
      emitDestroyCall(IGF, T, addr.getAddress());
    }

    // \group Operations for loadable enums

    ClusteredBitVector
    getTagBitsForPayloads() const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    ClusteredBitVector
    getBitPatternForNoPayloadElement(EnumElementDecl *theCase)
    const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    ClusteredBitVector
    getBitMaskForNoPayloadElements() const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void initializeFromParams(IRGenFunction &IGF, Explosion &params,
                              Address dest, SILType T) const override {
      llvm_unreachable("resilient enums are always indirect");
    }
  
    void emitValueInjection(IRGenFunction &IGF,
                            EnumElementDecl *elt,
                            Explosion &params,
                            Explosion &out) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    llvm::Value *
    emitValueCaseTest(IRGenFunction &IGF, Explosion &value,
                      EnumElementDecl *Case) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void emitValueSwitch(IRGenFunction &IGF,
                         Explosion &value,
                         ArrayRef<std::pair<EnumElementDecl*,
                                            llvm::BasicBlock*>> dests,
                         llvm::BasicBlock *defaultDest) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void emitValueProject(IRGenFunction &IGF,
                          Explosion &inValue,
                          EnumElementDecl *theCase,
                          Explosion &out) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void getSchema(ExplosionSchema &schema) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    unsigned getExplosionSize() const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void loadAsCopy(IRGenFunction &IGF, Address addr,
                            Explosion &e) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void loadAsTake(IRGenFunction &IGF, Address addr,
                            Explosion &e) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void assign(IRGenFunction &IGF, Explosion &e,
                        Address addr) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void initialize(IRGenFunction &IGF, Explosion &e,
                            Address addr) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void reexplode(IRGenFunction &IGF, Explosion &src,
                           Explosion &dest) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void copy(IRGenFunction &IGF, Explosion &src, Explosion &dest)
    const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void consume(IRGenFunction &IGF, Explosion &src) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void fixLifetime(IRGenFunction &IGF, Explosion &src) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void packIntoEnumPayload(IRGenFunction &IGF,
                             EnumPayload &outerPayload,
                             Explosion &src,
                             unsigned offset) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    void unpackFromEnumPayload(IRGenFunction &IGF,
                               const EnumPayload &outerPayload,
                               Explosion &dest,
                               unsigned offset) const override {
      llvm_unreachable("resilient enums are always indirect");
    }

    /// \group Operations for emitting type metadata

    llvm::Value *
    emitGetEnumTag(IRGenFunction &IGF, SILType T, Address addr)
    const override {
      llvm_unreachable("resilient enums cannot be defined");
    }
    
    bool needsPayloadSizeInMetadata() const override {
      llvm_unreachable("resilient enums cannot be defined");
    }

    void initializeMetadata(IRGenFunction &IGF,
                            llvm::Value *metadata,
                            llvm::Value *vwtable,
                            SILType T) const override {
      llvm_unreachable("resilient enums cannot be defined");
    }

    /// \group Extra inhabitants

    bool mayHaveExtraInhabitants(IRGenModule &) const override {
      return true;
    }

    llvm::Value *getExtraInhabitantIndex(IRGenFunction &IGF,
                                         Address src,
                                         SILType T) const override {
      return emitGetExtraInhabitantIndexCall(IGF, T, src.getAddress());
    }

    void storeExtraInhabitant(IRGenFunction &IGF,
                              llvm::Value *index,
                              Address dest,
                              SILType T) const override {
      emitStoreExtraInhabitantCall(IGF, T, index, dest.getAddress());
    }

    APInt
    getFixedExtraInhabitantMask(IRGenModule &IGM) const override {
      llvm_unreachable("resilient enum is not fixed size");
    }
    
    unsigned getFixedExtraInhabitantCount(IRGenModule &IGM) const override {
      llvm_unreachable("resilient enum is not fixed size");
    }

    APInt
    getFixedExtraInhabitantValue(IRGenModule &IGM,
                                 unsigned bits,
                                 unsigned index) const override {
      llvm_unreachable("resilient enum is not fixed size");
    }
  };
} // end anonymous namespace

EnumImplStrategy *EnumImplStrategy::get(TypeConverter &TC,
                                        SILType type,
                                        EnumDecl *theEnum)
{
  unsigned numElements = 0;
  TypeInfoKind tik = Loadable;
  IsFixedSize_t alwaysFixedSize = IsFixedSize;
  std::vector<Element> elementsWithPayload;
  std::vector<Element> elementsWithNoPayload;

  for (auto elt : theEnum->getAllElements()) {
    numElements++;

    // Compute whether this gives us an apparent payload or dynamic layout.
    // Note that we do *not* apply substitutions from a bound generic instance
    // yet. We want all instances of a generic enum to share an implementation
    // strategy. If the abstract layout of the enum is dependent on generic
    // parameters, then we additionally need to constrain any layout
    // optimizations we perform to things that are reproducible by the runtime.
    Type origArgType = elt->getArgumentType();
    if (origArgType.isNull()) {
      elementsWithNoPayload.push_back({elt, nullptr, nullptr});
      continue;
    }

    // If the payload is indirect, we can use the NativeObject type metadata
    // without recurring. The box won't affect loadability or fixed-ness.
    if (elt->isIndirect() || theEnum->isIndirect()) {
      auto *nativeTI = &TC.getNativeObjectTypeInfo();
      elementsWithPayload.push_back({elt, nativeTI, nativeTI});
      continue;
    }

    auto origArgLoweredTy = TC.IGM.SILMod->Types.getLoweredType(origArgType);
    const TypeInfo *origArgTI
      = TC.tryGetCompleteTypeInfo(origArgLoweredTy.getSwiftRValueType());
    assert(origArgTI && "didn't complete type info?!");

    // If the unsubstituted argument contains a generic parameter type, or if
    // the substituted argument is not universally fixed-size, we need to
    // constrain our layout optimizations to what the runtime can reproduce.
    if (!origArgTI->isFixedSize(ResilienceScope::Universal))
      alwaysFixedSize = IsNotFixedSize;

    auto loadableOrigArgTI = dyn_cast<LoadableTypeInfo>(origArgTI);
    if (loadableOrigArgTI && loadableOrigArgTI->isKnownEmpty()) {
      elementsWithNoPayload.push_back({elt, nullptr, nullptr});
    } else {
      // *Now* apply the substitutions and get the type info for the instance's
      // payload type, since we know this case carries an apparent payload in
      // the generic case.
      SILType fieldTy = type.getEnumElementType(elt, *TC.IGM.SILMod);
      auto *substArgTI = &TC.IGM.getTypeInfo(fieldTy);

      elementsWithPayload.push_back({elt, substArgTI, origArgTI});
      if (!substArgTI->isFixedSize())
        tik = Opaque;
      else if (!substArgTI->isLoadable() && tik > Fixed)
        tik = Fixed;

      // If the substituted argument contains a type that is not universally
      // fixed-size, we need to constrain our layout optimizations to what
      // the runtime can reproduce.
      if (!substArgTI->isFixedSize(ResilienceScope::Universal))
        alwaysFixedSize = IsNotFixedSize;
    }
  }


  assert(numElements == elementsWithPayload.size()
           + elementsWithNoPayload.size()
         && "not all elements accounted for");

  // Resilient enums are manipulated as opaque values, except we still
  // make the following assumptions:
  // 1) Physical case indices won't change
  // 2) The indirect-ness of cases won't change
  // 3) Payload types won't change in a non-resilient way
  if (TC.IGM.isResilient(theEnum, ResilienceScope::Component)) {
    return new ResilientEnumImplStrategy(TC.IGM,
                                         numElements,
                                         std::move(elementsWithPayload),
                                         std::move(elementsWithNoPayload));
  }

  // Enums imported from Clang or marked with @objc use C-compatible layout.
  if (theEnum->hasClangNode() || theEnum->isObjC()) {
    assert(elementsWithPayload.size() == 0 && "C enum with payload?!");
    assert(alwaysFixedSize == IsFixedSize && "C enum with resilient payload?!");
    return new CCompatibleEnumImplStrategy(TC.IGM, tik, alwaysFixedSize,
                                           numElements,
                                           std::move(elementsWithPayload),
                                           std::move(elementsWithNoPayload));
  }

  if (numElements <= 1)
    return new SingletonEnumImplStrategy(TC.IGM, tik, alwaysFixedSize,
                                         numElements,
                                         std::move(elementsWithPayload),
                                         std::move(elementsWithNoPayload));
  if (elementsWithPayload.size() > 1)
    return new MultiPayloadEnumImplStrategy(TC.IGM, tik, alwaysFixedSize,
                                            numElements,
                                            std::move(elementsWithPayload),
                                            std::move(elementsWithNoPayload));
  if (elementsWithPayload.size() == 1)
    return new SinglePayloadEnumImplStrategy(TC.IGM, tik, alwaysFixedSize,
                                             numElements,
                                             std::move(elementsWithPayload),
                                             std::move(elementsWithNoPayload));

  return new NoPayloadEnumImplStrategy(TC.IGM, tik, alwaysFixedSize,
                                       numElements,
                                       std::move(elementsWithPayload),
                                       std::move(elementsWithNoPayload));
}

namespace {
  /// Common base template for enum type infos.
  template<typename BaseTypeInfo>
  class EnumTypeInfoBase : public BaseTypeInfo {
  public:
    EnumImplStrategy &Strategy;

    template<typename...AA>
    EnumTypeInfoBase(EnumImplStrategy &strategy, AA &&...args)
      : BaseTypeInfo(std::forward<AA>(args)...), Strategy(strategy) {}

    llvm::StructType *getStorageType() const {
      return cast<llvm::StructType>(TypeInfo::getStorageType());
    }

    /// \group Methods delegated to the EnumImplStrategy

    void getSchema(ExplosionSchema &s) const override {
      return Strategy.getSchema(s);
    }
    void destroy(IRGenFunction &IGF, Address addr, SILType T) const override {
      return Strategy.destroy(IGF, addr, T);
    }
    void initializeFromParams(IRGenFunction &IGF, Explosion &params,
                              Address dest, SILType T) const override {
      return Strategy.initializeFromParams(IGF, params, dest, T);
    }
    void initializeWithCopy(IRGenFunction &IGF, Address dest,
                            Address src, SILType T) const override {
      return Strategy.initializeWithCopy(IGF, dest, src, T);
    }
    void initializeWithTake(IRGenFunction &IGF, Address dest,
                            Address src, SILType T) const override {
      return Strategy.initializeWithTake(IGF, dest, src, T);
    }
    void assignWithCopy(IRGenFunction &IGF, Address dest,
                        Address src, SILType T) const override {
      return Strategy.assignWithCopy(IGF, dest, src, T);
    }
    void assignWithTake(IRGenFunction &IGF, Address dest,
                        Address src, SILType T) const override {
      return Strategy.assignWithTake(IGF, dest, src, T);
    }
    void initializeMetadata(IRGenFunction &IGF,
                            llvm::Value *metadata,
                            llvm::Value *vwtable,
                            SILType T) const override {
      return Strategy.initializeMetadata(IGF, metadata, vwtable, T);
    }
    bool mayHaveExtraInhabitants(IRGenModule &IGM) const override {
      return Strategy.mayHaveExtraInhabitants(IGM);
    }
    llvm::Value *getExtraInhabitantIndex(IRGenFunction &IGF,
                                         Address src,
                                         SILType T) const override {
      return Strategy.getExtraInhabitantIndex(IGF, src, T);
    }
    void storeExtraInhabitant(IRGenFunction &IGF,
                              llvm::Value *index,
                              Address dest,
                              SILType T) const override {
      return Strategy.storeExtraInhabitant(IGF, index, dest, T);
    }
  };

  /// TypeInfo for fixed-layout, address-only enum types.
  class FixedEnumTypeInfo : public EnumTypeInfoBase<FixedTypeInfo> {
  public:
    FixedEnumTypeInfo(EnumImplStrategy &strategy,
                      llvm::StructType *T, Size S, SpareBitVector SB,
                      Alignment A, IsPOD_t isPOD, IsBitwiseTakable_t isBT,
                      IsFixedSize_t alwaysFixedSize)
      : EnumTypeInfoBase(strategy, T, S, std::move(SB), A, isPOD, isBT,
                         alwaysFixedSize) {}

    /// \group Methods delegated to the EnumImplStrategy

    unsigned getFixedExtraInhabitantCount(IRGenModule &IGM) const override {
      return Strategy.getFixedExtraInhabitantCount(IGM);
    }

    APInt getFixedExtraInhabitantValue(IRGenModule &IGM,
                                       unsigned bits,
                                       unsigned index)
    const override {
      return Strategy.getFixedExtraInhabitantValue(IGM, bits, index);
    }
  };

  /// TypeInfo for loadable enum types.
  class LoadableEnumTypeInfo : public EnumTypeInfoBase<LoadableTypeInfo> {
  public:
    // FIXME: Derive spare bits from element layout.
    LoadableEnumTypeInfo(EnumImplStrategy &strategy,
                         llvm::StructType *T, Size S, SpareBitVector SB,
                         Alignment A, IsPOD_t isPOD,
                         IsFixedSize_t alwaysFixedSize)
      : EnumTypeInfoBase(strategy, T, S, std::move(SB), A, isPOD,
                         alwaysFixedSize) {}

    unsigned getExplosionSize() const override {
      return Strategy.getExplosionSize();
    }
    void loadAsCopy(IRGenFunction &IGF, Address addr,
                    Explosion &e) const override {
      return Strategy.loadAsCopy(IGF, addr, e);
    }
    void loadAsTake(IRGenFunction &IGF, Address addr,
                    Explosion &e) const override {
      return Strategy.loadAsTake(IGF, addr, e);
    }
    void assign(IRGenFunction &IGF, Explosion &e,
                Address addr) const override {
      return Strategy.assign(IGF, e, addr);
    }
    void initialize(IRGenFunction &IGF, Explosion &e,
                    Address addr) const override {
      return Strategy.initialize(IGF, e, addr);
    }
    void reexplode(IRGenFunction &IGF, Explosion &src,
                   Explosion &dest) const override {
      return Strategy.reexplode(IGF, src, dest);
    }
    void copy(IRGenFunction &IGF, Explosion &src,
              Explosion &dest) const override {
      return Strategy.copy(IGF, src, dest);
    }
    void consume(IRGenFunction &IGF, Explosion &src) const override {
      return Strategy.consume(IGF, src);
    }
    void fixLifetime(IRGenFunction &IGF, Explosion &src) const override {
      return Strategy.fixLifetime(IGF, src);
    }
    void packIntoEnumPayload(IRGenFunction &IGF,
                             EnumPayload &payload,
                             Explosion &in,
                             unsigned offset) const override {
      return Strategy.packIntoEnumPayload(IGF, payload, in, offset);
    }
    void unpackFromEnumPayload(IRGenFunction &IGF,
                               const EnumPayload &payload,
                               Explosion &dest,
                               unsigned offset) const override {
      return Strategy.unpackFromEnumPayload(IGF, payload, dest, offset);
    }
    unsigned getFixedExtraInhabitantCount(IRGenModule &IGM) const override {
      return Strategy.getFixedExtraInhabitantCount(IGM);
    }

    APInt getFixedExtraInhabitantValue(IRGenModule &IGM,
                                       unsigned bits,
                                       unsigned index)
    const override {
      return Strategy.getFixedExtraInhabitantValue(IGM, bits, index);
    }
    
    APInt getFixedExtraInhabitantMask(IRGenModule &IGM) const override {
      return Strategy.getFixedExtraInhabitantMask(IGM);
    }
    LoadedRef loadRefcountedPtr(IRGenFunction &IGF,
                                SourceLoc loc, Address addr) const override {
      return LoadedRef(Strategy.loadRefcountedPtr(IGF, loc, addr), false);
    }
  };

  /// TypeInfo for dynamically-sized enum types.
  class NonFixedEnumTypeInfo
    : public EnumTypeInfoBase<WitnessSizedTypeInfo<NonFixedEnumTypeInfo>>
  {
  public:
    NonFixedEnumTypeInfo(EnumImplStrategy &strategy,
                         llvm::Type *irTy,
                         Alignment align,
                         IsPOD_t pod,
                         IsBitwiseTakable_t bt)
      : EnumTypeInfoBase(strategy, irTy, align, pod, bt) {}
  };

  /// TypeInfo for dynamically-sized enum types.
  class ResilientEnumTypeInfo
    : public EnumTypeInfoBase<ResilientTypeInfo<ResilientEnumTypeInfo>>
  {
  public:
    ResilientEnumTypeInfo(EnumImplStrategy &strategy,
                          llvm::Type *irTy)
      : EnumTypeInfoBase(strategy, irTy) {}
  };
} // end anonymous namespace

const EnumImplStrategy &
irgen::getEnumImplStrategy(IRGenModule &IGM, SILType ty) {
  assert(ty.getEnumOrBoundGenericEnum() && "not an enum");
  auto *ti = &IGM.getTypeInfo(ty);
  if (auto *loadableTI = dyn_cast<LoadableTypeInfo>(ti))
    return loadableTI->as<LoadableEnumTypeInfo>().Strategy;
  if (auto *fti = dyn_cast<FixedTypeInfo>(ti))
    return fti->as<FixedEnumTypeInfo>().Strategy;
  return ti->as<NonFixedEnumTypeInfo>().Strategy;
}

const EnumImplStrategy &
irgen::getEnumImplStrategy(IRGenModule &IGM, CanType ty) {
  // Nominal types are always preserved through SIL lowering.
  return getEnumImplStrategy(IGM, SILType::getPrimitiveAddressType(ty));
}

TypeInfo *
EnumImplStrategy::getFixedEnumTypeInfo(llvm::StructType *T, Size S,
                                       SpareBitVector SB,
                                       Alignment A, IsPOD_t isPOD,
                                       IsBitwiseTakable_t isBT) {
  TypeInfo *mutableTI;
  switch (TIK) {
  case Opaque:
    llvm_unreachable("not valid");
  case Fixed:
    mutableTI = new FixedEnumTypeInfo(*this, T, S, std::move(SB), A, isPOD, isBT,
                                      AlwaysFixedSize);
    break;
  case Loadable:
    assert(isBT && "loadable enum not bitwise takable?!");
    mutableTI = new LoadableEnumTypeInfo(*this, T, S, std::move(SB), A, isPOD,
                                         AlwaysFixedSize);
    break;
  }
  TI = mutableTI;
  return mutableTI;
}

TypeInfo *
SingletonEnumImplStrategy::completeEnumTypeLayout(TypeConverter &TC,
                                                  SILType Type,
                                                  EnumDecl *theEnum,
                                                  llvm::StructType *enumTy) {
  if (ElementsWithPayload.empty()) {
    enumTy->setBody(ArrayRef<llvm::Type*>{}, /*isPacked*/ true);
    Alignment alignment(1);
    applyLayoutAttributes(TC.IGM, Type.getSwiftRValueType(), /*fixed*/true,
                          alignment);
    return registerEnumTypeInfo(new LoadableEnumTypeInfo(*this, enumTy,
                                                         Size(0), {},
                                                         alignment,
                                                         IsPOD,
                                                         AlwaysFixedSize));
  } else {
    const TypeInfo &eltTI = *getSingleton();

    // Use the singleton element's storage type if fixed-size.
    if (eltTI.isFixedSize()) {
      llvm::Type *body[] = { eltTI.StorageType };
      enumTy->setBody(body, /*isPacked*/ true);
    } else {
      enumTy->setBody(ArrayRef<llvm::Type*>{}, /*isPacked*/ true);
    }

    if (TIK <= Opaque) {
      auto alignment = eltTI.getBestKnownAlignment();
      applyLayoutAttributes(TC.IGM, Type.getSwiftRValueType(), /*fixed*/false,
                            alignment);
      return registerEnumTypeInfo(new NonFixedEnumTypeInfo(*this, enumTy,
                             alignment,
                             eltTI.isPOD(ResilienceScope::Component),
                             eltTI.isBitwiseTakable(ResilienceScope::Component)));
    } else {
      auto &fixedEltTI = cast<FixedTypeInfo>(eltTI);
      auto alignment = fixedEltTI.getFixedAlignment();
      applyLayoutAttributes(TC.IGM, Type.getSwiftRValueType(), /*fixed*/true,
                            alignment);

      return getFixedEnumTypeInfo(enumTy,
                        fixedEltTI.getFixedSize(),
                        fixedEltTI.getSpareBits(),
                        alignment,
                        fixedEltTI.isPOD(ResilienceScope::Component),
                        fixedEltTI.isBitwiseTakable(ResilienceScope::Component));
    }
  }
}

TypeInfo *
NoPayloadEnumImplStrategy::completeEnumTypeLayout(TypeConverter &TC,
                                                  SILType Type,
                                                  EnumDecl *theEnum,
                                                  llvm::StructType *enumTy) {
  // Since there are no payloads, we need just enough bits to hold a
  // discriminator.
  unsigned tagBits = llvm::Log2_32(ElementsWithNoPayload.size() - 1) + 1;
  auto tagTy = llvm::IntegerType::get(TC.IGM.getLLVMContext(), tagBits);
  // Round the physical size up to the next power of two.
  unsigned tagBytes = (tagBits + 7U)/8U;
  if (!llvm::isPowerOf2_32(tagBytes))
    tagBytes = llvm::NextPowerOf2(tagBytes);
  Size tagSize(tagBytes);

  llvm::Type *body[] = { tagTy };
  enumTy->setBody(body, /*isPacked*/true);

  // Unused tag bits in the physical size can be used as spare bits.
  // TODO: We can use all values greater than the largest discriminator as
  // extra inhabitants, not just those made available by spare bits.
  SpareBitVector spareBits;
  spareBits.appendClearBits(tagBits);
  spareBits.extendWithSetBits(tagSize.getValueInBits());

  Alignment alignment(tagBytes);
  applyLayoutAttributes(TC.IGM, Type.getSwiftRValueType(), /*fixed*/true,
                        alignment);

  return registerEnumTypeInfo(new LoadableEnumTypeInfo(*this,
                                   enumTy, tagSize, std::move(spareBits),
                                   alignment, IsPOD, AlwaysFixedSize));
}

TypeInfo *
CCompatibleEnumImplStrategy::completeEnumTypeLayout(TypeConverter &TC,
                                                    SILType Type,
                                                    EnumDecl *theEnum,
                                                    llvm::StructType *enumTy){
  // The type should have come from Clang or be @objc,
  // and should have a raw type.
  assert((theEnum->hasClangNode() || theEnum->isObjC())
         && "c-compatible enum didn't come from clang!");
  assert(theEnum->hasRawType()
         && "c-compatible enum doesn't have raw type!");
  assert(!theEnum->getDeclaredTypeInContext()->is<BoundGenericType>()
         && "c-compatible enum is generic!");

  // The raw type should be a C integer type, which should have a single
  // scalar representation as a Swift struct. We'll use that same
  // representation type for the enum so that it's ABI-compatible.
  auto &rawTI = TC.getCompleteTypeInfo(
                                   theEnum->getRawType()->getCanonicalType());
  auto &rawFixedTI = cast<FixedTypeInfo>(rawTI);
  assert(rawFixedTI.isPOD(ResilienceScope::Component)
         && "c-compatible raw type isn't POD?!");
  ExplosionSchema rawSchema = rawTI.getSchema();
  assert(rawSchema.size() == 1
         && "c-compatible raw type has non-single-scalar representation?!");
  assert(rawSchema.begin()[0].isScalar()
         && "c-compatible raw type has non-single-scalar representation?!");
  llvm::Type *tagTy = rawSchema.begin()[0].getScalarType();

  llvm::Type *body[] = { tagTy };
  enumTy->setBody(body, /*isPacked*/ false);

  auto alignment = rawFixedTI.getFixedAlignment();
  applyLayoutAttributes(TC.IGM, Type.getSwiftRValueType(), /*fixed*/true,
                        alignment);

  assert(!TC.IGM.isResilient(theEnum, ResilienceScope::Universal) &&
         "C-compatible enums cannot be resilient");

  return registerEnumTypeInfo(new LoadableEnumTypeInfo(*this, enumTy,
                                               rawFixedTI.getFixedSize(),
                                               rawFixedTI.getSpareBits(),
                                               alignment,
                                               IsPOD,
                                               IsFixedSize));
}

TypeInfo *SinglePayloadEnumImplStrategy::completeFixedLayout(
                                    TypeConverter &TC,
                                    SILType Type,
                                    EnumDecl *theEnum,
                                    llvm::StructType *enumTy) {
  // See whether the payload case's type has extra inhabitants.
  unsigned fixedExtraInhabitants = 0;
  unsigned numTags = ElementsWithNoPayload.size();

  auto &payloadTI = getFixedPayloadTypeInfo();
  fixedExtraInhabitants = payloadTI.getFixedExtraInhabitantCount(TC.IGM);

  // Determine how many tag bits we need. Given N extra inhabitants, we
  // represent the first N tags using those inhabitants. For additional tags,
  // we use discriminator bit(s) to inhabit the full bit size of the payload.
  NumExtraInhabitantTagValues = std::min(numTags, fixedExtraInhabitants);

  unsigned tagsWithoutInhabitants = numTags - NumExtraInhabitantTagValues;
  if (tagsWithoutInhabitants == 0) {
    ExtraTagBitCount = 0;
    NumExtraTagValues = 0;
  // If the payload size is greater than 32 bits, the calculation would
  // overflow, but one tag bit should suffice. if you have more than 2^32
  // enum discriminators you have other problems.
  } else if (payloadTI.getFixedSize().getValue() >= 4) {
    ExtraTagBitCount = 1;
    NumExtraTagValues = 2;
  } else {
    unsigned tagsPerTagBitValue =
      1 << payloadTI.getFixedSize().getValueInBits();
    NumExtraTagValues
      = (tagsWithoutInhabitants+(tagsPerTagBitValue-1))/tagsPerTagBitValue+1;
    ExtraTagBitCount = llvm::Log2_32(NumExtraTagValues-1) + 1;
  }

  // Create the body type.
  setTaggedEnumBody(TC.IGM, enumTy,
                    payloadTI.getFixedSize().getValueInBits(),
                    ExtraTagBitCount);

  // The enum has the alignment of the payload. The size includes the added
  // tag bits.
  auto sizeWithTag = payloadTI.getFixedSize().getValue();
  unsigned extraTagByteCount = (ExtraTagBitCount+7U)/8U;
  sizeWithTag += extraTagByteCount;

  // FIXME: We don't have enough semantic understanding of extra inhabitant
  // sets to be able to reason about how many spare bits from the payload type
  // we can forward. If we spilled tag bits, however, we can offer the unused
  // bits we have in that byte.
  SpareBitVector spareBits;
  spareBits.appendClearBits(payloadTI.getFixedSize().getValueInBits());
  if (ExtraTagBitCount > 0) {
    spareBits.appendClearBits(ExtraTagBitCount);
    spareBits.appendSetBits(extraTagByteCount * 8 - ExtraTagBitCount);
  }
  
  auto alignment = payloadTI.getFixedAlignment();
  applyLayoutAttributes(TC.IGM, Type.getSwiftRValueType(), /*fixed*/true,
                        alignment);
  
  return getFixedEnumTypeInfo(enumTy, Size(sizeWithTag), std::move(spareBits),
                      alignment,
                      payloadTI.isPOD(ResilienceScope::Component),
                      payloadTI.isBitwiseTakable(ResilienceScope::Component));
}

TypeInfo *SinglePayloadEnumImplStrategy::completeDynamicLayout(
                                                TypeConverter &TC,
                                                SILType Type,
                                                EnumDecl *theEnum,
                                                llvm::StructType *enumTy) {
  // The body is runtime-dependent, so we can't put anything useful here
  // statically.
  enumTy->setBody(ArrayRef<llvm::Type*>{}, /*isPacked*/true);

  // Layout has to be done when the value witness table is instantiated,
  // during initializeMetadata.
  auto &payloadTI = getPayloadTypeInfo();
  auto alignment = payloadTI.getBestKnownAlignment();
  
  applyLayoutAttributes(TC.IGM, Type.getSwiftRValueType(), /*fixed*/false,
                        alignment);
  
  return registerEnumTypeInfo(new NonFixedEnumTypeInfo(*this, enumTy,
         alignment,
         payloadTI.isPOD(ResilienceScope::Component),
         payloadTI.isBitwiseTakable(ResilienceScope::Component)));
}

TypeInfo *
SinglePayloadEnumImplStrategy::completeEnumTypeLayout(TypeConverter &TC,
                                                      SILType type,
                                                      EnumDecl *theEnum,
                                                      llvm::StructType *enumTy) {
  if (TIK >= Fixed)
    return completeFixedLayout(TC, type, theEnum, enumTy);
  return completeDynamicLayout(TC, type, theEnum, enumTy);
}

TypeInfo *
MultiPayloadEnumImplStrategy::completeFixedLayout(TypeConverter &TC,
                                                  SILType Type,
                                                  EnumDecl *theEnum,
                                                  llvm::StructType *enumTy) {
  // We need tags for each of the payload types, which we may be able to form
  // using spare bits, plus a minimal number of tags with which we can
  // represent the empty cases.
  unsigned numPayloadTags = ElementsWithPayload.size();
  unsigned numEmptyElements = ElementsWithNoPayload.size();

  // See if the payload types have any spare bits in common.
  // At the end of the loop CommonSpareBits.size() will be the size (in bits)
  // of the largest payload.
  CommonSpareBits = {};
  Alignment worstAlignment(1);
  IsPOD_t isPOD = IsPOD;
  IsBitwiseTakable_t isBT = IsBitwiseTakable;
  for (auto &elt : ElementsWithPayload) {
    auto &fixedPayloadTI = cast<FixedTypeInfo>(*elt.ti);
    if (fixedPayloadTI.getFixedAlignment() > worstAlignment)
      worstAlignment = fixedPayloadTI.getFixedAlignment();
    if (!fixedPayloadTI.isPOD(ResilienceScope::Component))
      isPOD = IsNotPOD;
    if (!fixedPayloadTI.isBitwiseTakable(ResilienceScope::Component))
      isBT = IsNotBitwiseTakable;

    unsigned payloadBits = fixedPayloadTI.getFixedSize().getValueInBits();
    
    // See what spare bits from the payload we can use for layout optimization.

    // The runtime currently does not track spare bits, so we can't use them
    // if the type is layout-dependent. (Even when the runtime does, it will
    // likely only track a subset of the spare bits.)
    if (!AlwaysFixedSize || TIK < Loadable) {
      if (CommonSpareBits.size() < payloadBits)
        CommonSpareBits.extendWithClearBits(payloadBits);
      continue;
    }

    // Otherwise, all unsubstituted payload types are fixed-size and
    // we have no constraints on what spare bits we can use.

    // We might still have a dependently typed payload though, namely a
    // class-bound archetype. These do not have any spare bits because
    // they can contain Obj-C tagged pointers. To handle this case
    // correctly, we get spare bits from the unsubstituted type.
    auto &fixedOrigTI = cast<FixedTypeInfo>(*elt.origTI);
    fixedOrigTI.applyFixedSpareBitsMask(CommonSpareBits);
  }

  unsigned commonSpareBitCount = CommonSpareBits.count();
  unsigned usedBitCount = CommonSpareBits.size() - commonSpareBitCount;

  // Determine how many tags we need to accommodate the empty cases, if any.
  if (ElementsWithNoPayload.empty()) {
    NumEmptyElementTags = 0;
  } else {
    // We can store tags for the empty elements using the inhabited bits with
    // their own tag(s).
    if (usedBitCount >= 32) {
      NumEmptyElementTags = 1;
    } else {
      unsigned emptyElementsPerTag = 1 << usedBitCount;
      NumEmptyElementTags
        = (numEmptyElements + (emptyElementsPerTag-1))/emptyElementsPerTag;
    }
  }

  unsigned numTags = numPayloadTags + NumEmptyElementTags;
  unsigned numTagBits = llvm::Log2_32(numTags-1) + 1;
  ExtraTagBitCount = numTagBits <= commonSpareBitCount
    ? 0 : numTagBits - commonSpareBitCount;
  NumExtraTagValues = numTags >> commonSpareBitCount;

  // Create the type. We need enough bits to store the largest payload plus
  // extra tag bits we need.
  setTaggedEnumBody(TC.IGM, enumTy,
                     CommonSpareBits.size(),
                     ExtraTagBitCount);

  // The enum has the worst alignment of its payloads. The size includes the
  // added tag bits.
  auto sizeWithTag = (CommonSpareBits.size() + 7U)/8U;
  unsigned extraTagByteCount = (ExtraTagBitCount+7U)/8U;
  sizeWithTag += extraTagByteCount;

  SpareBitVector spareBits;

  // Determine the bits we're going to use for the tag.
  assert(PayloadTagBits.empty());

  // The easiest case is if we're going to use all of the available
  // payload tag bits (plus potentially some extra bits), because we
  // can just straight-up use CommonSpareBits as that bitset.
  if (numTagBits >= commonSpareBitCount) {
    PayloadTagBits = CommonSpareBits;

    // We're using all of the common spare bits as tag bits, so none
    // of them are spare; nor are the extra tag bits.
    spareBits.appendClearBits(CommonSpareBits.size() + ExtraTagBitCount);

    // The remaining bits in the extra tag bytes are spare.
    spareBits.appendSetBits(extraTagByteCount * 8 - ExtraTagBitCount);

  // Otherwise, we need to construct a new bitset that doesn't
  // include the bits we aren't using.
  } else {
    assert(ExtraTagBitCount == 0
           && "spilled extra tag bits with spare bits available?!");
    PayloadTagBits =
      ClusteredBitVector::getConstant(CommonSpareBits.size(), false);

    // Start the spare bit set using all the common spare bits.
    spareBits = CommonSpareBits;

    // Mark the bits we'll use as occupied in both bitsets.
    // We take bits starting from the most significant.
    unsigned remainingTagBits = numTagBits;
    for (unsigned bit = CommonSpareBits.size() - 1; true; --bit) {
      if (!CommonSpareBits[bit]) {
        assert(bit > 0 && "ran out of spare bits?!");
        continue;
      }

      // Use this bit as a payload tag bit.
      PayloadTagBits.setBit(bit);

      // A bit used as a payload tag bit is not a spare bit.
      spareBits.clearBit(bit);

      if (--remainingTagBits == 0) break;
      assert(bit > 0 && "ran out of spare bits?!");
    }
    assert(PayloadTagBits.count() == numTagBits);
  }
  
  applyLayoutAttributes(TC.IGM, Type.getSwiftRValueType(), /*fixed*/ true,
                        worstAlignment);

  return getFixedEnumTypeInfo(enumTy, Size(sizeWithTag), std::move(spareBits),
                              worstAlignment, isPOD, isBT);
}


TypeInfo *MultiPayloadEnumImplStrategy::completeDynamicLayout(
                                                TypeConverter &TC,
                                                SILType Type,
                                                EnumDecl *theEnum,
                                                llvm::StructType *enumTy) {
  // The body is runtime-dependent, so we can't put anything useful here
  // statically.
  enumTy->setBody(ArrayRef<llvm::Type*>{}, /*isPacked*/true);

  // Layout has to be done when the value witness table is instantiated,
  // during initializeMetadata. We can at least glean the best available
  // static information from the payloads.
  Alignment alignment(1);
  IsPOD_t pod = IsPOD;
  IsBitwiseTakable_t bt = IsBitwiseTakable;
  for (auto &element : ElementsWithPayload) {
    auto &payloadTI = *element.ti;
    alignment = std::max(alignment, payloadTI.getBestKnownAlignment());
    pod &= payloadTI.isPOD(ResilienceScope::Component);
    bt &= payloadTI.isBitwiseTakable(ResilienceScope::Component);
  }
  
  applyLayoutAttributes(TC.IGM, Type.getSwiftRValueType(), /*fixed*/false,
                        alignment);
  
  return registerEnumTypeInfo(new NonFixedEnumTypeInfo(*this, enumTy,
                                                       alignment, pod, bt));
}

TypeInfo *
MultiPayloadEnumImplStrategy::completeEnumTypeLayout(TypeConverter &TC,
                                                     SILType Type,
                                                     EnumDecl *theEnum,
                                                     llvm::StructType *enumTy) {
  if (TIK >= Fixed)
    return completeFixedLayout(TC, Type, theEnum, enumTy);
  
  return completeDynamicLayout(TC, Type, theEnum, enumTy);
}

TypeInfo *
ResilientEnumImplStrategy::completeEnumTypeLayout(TypeConverter &TC,
                                                  SILType Type,
                                                  EnumDecl *theEnum,
                                                  llvm::StructType *enumTy) {
  return registerEnumTypeInfo(new ResilientEnumTypeInfo(*this, enumTy));
}

const TypeInfo *TypeConverter::convertEnumType(TypeBase *key, CanType type,
                                               EnumDecl *theEnum) {
  llvm::StructType *storageType;

  // Resilient enum types lower down to the same opaque type.
  if (IGM.isResilient(theEnum, ResilienceScope::Component))
    storageType = cast<llvm::StructType>(IGM.OpaquePtrTy->getElementType());
  else
    storageType = IGM.createNominalType(theEnum);

  // Create a forward declaration for that type.
  addForwardDecl(key, storageType);
  
  SILType loweredTy = SILType::getPrimitiveAddressType(type);

  // Determine the implementation strategy.
  EnumImplStrategy *strategy = EnumImplStrategy::get(*this, loweredTy, theEnum);

  // Create the TI.
  auto *ti = strategy->completeEnumTypeLayout(*this, loweredTy,
                                              theEnum, storageType);
  // Assert that the layout query functions for fixed-layout enums work, for
  // LLDB's sake.
#ifndef NDEBUG
  auto displayBitMask = [&](const SpareBitVector &v) {
    for (unsigned i = v.size(); i-- > 0;) {
      llvm::dbgs() << (v[i] ? '1' : '0');
      if (i % 8 == 0 && i != 0)
        llvm::dbgs() << '_';
    }
    llvm::dbgs() << '\n';
  };

  if (auto fixedTI = dyn_cast<FixedTypeInfo>(ti)) {
    DEBUG(llvm::dbgs() << "Layout for enum ";
          type->print(llvm::dbgs());
          llvm::dbgs() << ":\n";);

    SpareBitVector spareBits;
    fixedTI->applyFixedSpareBitsMask(spareBits);

    auto bitMask = strategy->getBitMaskForNoPayloadElements();
    assert(bitMask.size() == fixedTI->getFixedSize().getValueInBits());
    DEBUG(llvm::dbgs() << "  no-payload mask:\t";
          displayBitMask(bitMask));
    DEBUG(llvm::dbgs() << "  spare bits mask:\t";
          displayBitMask(spareBits));

    for (auto &elt : strategy->getElementsWithNoPayload()) {
      auto bitPattern = strategy->getBitPatternForNoPayloadElement(elt.decl);
      assert(bitPattern.size() == fixedTI->getFixedSize().getValueInBits());
      DEBUG(llvm::dbgs() << "  no-payload case " << elt.decl->getName().str()
                         << ":\t";
            displayBitMask(bitPattern));

      auto maskedBitPattern = bitPattern;
      maskedBitPattern &= spareBits;
      assert(maskedBitPattern.none() && "no-payload case occupies spare bits?!");
    }
    auto tagBits = strategy->getTagBitsForPayloads();
    assert(tagBits.count() >= 32
            || (1U << tagBits.count())
               >= strategy->getElementsWithPayload().size());
    DEBUG(llvm::dbgs() << "  payload tag bits:\t";
          displayBitMask(tagBits));

    tagBits &= spareBits;
    assert(tagBits.none() && "tag bits overlap spare bits?!");
  }
#endif
  return ti;
}

void IRGenModule::emitEnumDecl(EnumDecl *theEnum) {
  emitEnumMetadata(*this, theEnum);
  emitNestedTypeDecls(theEnum->getMembers());
}

void irgen::emitSwitchAddressOnlyEnumDispatch(IRGenFunction &IGF,
                                  SILType enumTy,
                                  Address enumAddr,
                                  ArrayRef<std::pair<EnumElementDecl *,
                                                     llvm::BasicBlock *>> dests,
                                  llvm::BasicBlock *defaultDest) {
  auto &strategy = getEnumImplStrategy(IGF.IGM, enumTy);
  strategy.emitIndirectSwitch(IGF, enumTy,
                              enumAddr, dests, defaultDest);
}

void irgen::emitInjectLoadableEnum(IRGenFunction &IGF, SILType enumTy,
                                    EnumElementDecl *theCase,
                                    Explosion &data,
                                    Explosion &out) {
  getEnumImplStrategy(IGF.IGM, enumTy)
    .emitValueInjection(IGF, theCase, data, out);
}

void irgen::emitProjectLoadableEnum(IRGenFunction &IGF, SILType enumTy,
                                     Explosion &inEnumValue,
                                     EnumElementDecl *theCase,
                                     Explosion &out) {
  getEnumImplStrategy(IGF.IGM, enumTy)
    .emitValueProject(IGF, inEnumValue, theCase, out);
}

Address irgen::emitProjectEnumAddressForStore(IRGenFunction &IGF,
                                               SILType enumTy,
                                               Address enumAddr,
                                               EnumElementDecl *theCase) {
  return getEnumImplStrategy(IGF.IGM, enumTy)
    .projectDataForStore(IGF, theCase, enumAddr);
}

Address irgen::emitDestructiveProjectEnumAddressForLoad(IRGenFunction &IGF,
                                                   SILType enumTy,
                                                   Address enumAddr,
                                                   EnumElementDecl *theCase) {
  return getEnumImplStrategy(IGF.IGM, enumTy)
    .destructiveProjectDataForLoad(IGF, enumTy, enumAddr, theCase);
}

void irgen::emitStoreEnumTagToAddress(IRGenFunction &IGF,
                                       SILType enumTy,
                                       Address enumAddr,
                                       EnumElementDecl *theCase) {
  getEnumImplStrategy(IGF.IGM, enumTy)
    .storeTag(IGF, enumTy, enumAddr, theCase);
}

/// Gather spare bits into the low bits of a smaller integer value.
llvm::Value *irgen::emitGatherSpareBits(IRGenFunction &IGF,
                                        const SpareBitVector &spareBitMask,
                                        llvm::Value *spareBits,
                                        unsigned resultLowBit,
                                        unsigned resultBitWidth) {
  auto destTy
    = llvm::IntegerType::get(IGF.IGM.getLLVMContext(), resultBitWidth);
  unsigned usedBits = resultLowBit;
  llvm::Value *result = nullptr;

  auto spareBitEnumeration = spareBitMask.enumerateSetBits();
  for (auto optSpareBit = spareBitEnumeration.findNext();
       optSpareBit.hasValue() && usedBits < resultBitWidth;
       optSpareBit = spareBitEnumeration.findNext()) {
    unsigned u = optSpareBit.getValue();
    assert(u >= (usedBits - resultLowBit) &&
           "used more bits than we've processed?!");

    // Shift the bits into place.
    llvm::Value *newBits;
    if (u > usedBits)
      newBits = IGF.Builder.CreateLShr(spareBits, u - usedBits);
    else if (u < usedBits)
      newBits = IGF.Builder.CreateShl(spareBits, usedBits - u);
    else
      newBits = spareBits;
    newBits = IGF.Builder.CreateZExtOrTrunc(newBits, destTy);

    // See how many consecutive bits we have.
    unsigned numBits = 1;
    ++u;
    // We don't need more bits than the size of the result.
    unsigned maxBits = resultBitWidth - usedBits;
    for (unsigned e = spareBitMask.size();
         u < e && numBits < maxBits && spareBitMask[u];
         ++u) {
      ++numBits;
      (void) spareBitEnumeration.findNext();
    }

    // Mask out the selected bits.
    auto val = APInt::getAllOnesValue(numBits);
    if (numBits < resultBitWidth)
      val = val.zext(resultBitWidth);
    val = val.shl(usedBits);
    auto *mask = llvm::ConstantInt::get(IGF.IGM.getLLVMContext(), val);
    newBits = IGF.Builder.CreateAnd(newBits, mask);

    // Accumulate the result.
    if (result)
      result = IGF.Builder.CreateOr(result, newBits);
    else
      result = newBits;

    usedBits += numBits;
  }

  return result;
}

/// Scatter spare bits from the low bits of an integer value.
llvm::Value *irgen::emitScatterSpareBits(IRGenFunction &IGF,
                                         const SpareBitVector &spareBitMask,
                                         llvm::Value *packedBits,
                                         unsigned packedLowBit) {
  auto destTy
    = llvm::IntegerType::get(IGF.IGM.getLLVMContext(), spareBitMask.size());
  llvm::Value *result = nullptr;
  unsigned usedBits = packedLowBit;

  // Expand the packed bits to the destination type.
  packedBits = IGF.Builder.CreateZExtOrTrunc(packedBits, destTy);

  auto spareBitEnumeration = spareBitMask.enumerateSetBits();
  for (auto nextSpareBit = spareBitEnumeration.findNext();
       nextSpareBit.hasValue();
       nextSpareBit = spareBitEnumeration.findNext()) {
    unsigned u = nextSpareBit.getValue(), startBit = u;
    assert(u >= usedBits - packedLowBit
           && "used more bits than we've processed?!");

    // Shift the selected bits into place.
    llvm::Value *newBits;
    if (u > usedBits)
      newBits = IGF.Builder.CreateShl(packedBits, u - usedBits);
    else if (u < usedBits)
      newBits = IGF.Builder.CreateLShr(packedBits, usedBits - u);
    else
      newBits = packedBits;

    // See how many consecutive bits we have.
    unsigned numBits = 1;
    ++u;
    for (unsigned e = spareBitMask.size(); u < e && spareBitMask[u]; ++u) {
      ++numBits;
      auto nextBit = spareBitEnumeration.findNext(); (void) nextBit;
      assert(nextBit.hasValue());
    }

    // Mask out the selected bits.
    auto val = APInt::getAllOnesValue(numBits);
    if (numBits < spareBitMask.size())
      val = val.zext(spareBitMask.size());
    val = val.shl(startBit);
    auto mask = llvm::ConstantInt::get(IGF.IGM.getLLVMContext(), val);
    newBits = IGF.Builder.CreateAnd(newBits, mask);

    // Accumulate the result.
    if (result)
      result = IGF.Builder.CreateOr(result, newBits);
    else
      result = newBits;

    usedBits += numBits;
  }

  return result;
}

/// Interleave the occupiedValue and spareValue bits, taking a bit from one
/// or the other at each position based on the spareBits mask.
APInt
irgen::interleaveSpareBits(IRGenModule &IGM, const SpareBitVector &spareBits,
                           unsigned bits,
                           unsigned spareValue, unsigned occupiedValue) {
  // FIXME: endianness.
  SmallVector<llvm::integerPart, 2> valueParts;
  valueParts.push_back(0);

  llvm::integerPart valueBit = 1;
  auto advanceValueBit = [&]{
    valueBit <<= 1;
    if (valueBit == 0) {
      valueParts.push_back(0);
      valueBit = 1;
    }
  };

  for (unsigned i = 0, e = spareBits.size();
       (occupiedValue || spareValue) && i < e;
       ++i, advanceValueBit()) {
    if (spareBits[i]) {
      if (spareValue & 1)
        valueParts.back() |= valueBit;
      spareValue >>= 1;
    } else {
      if (occupiedValue & 1)
        valueParts.back() |= valueBit;
      occupiedValue >>= 1;
    }
  }
  
  // Create the value.
  return llvm::APInt(bits, valueParts);
}

static void setAlignmentBits(SpareBitVector &v, Alignment align) {
  auto value = align.getValue() >> 1;
  for (unsigned i = 0; value; ++i, value >>= 1) {
    v.setBit(i);
  }
}

const SpareBitVector &
IRGenModule::getHeapObjectSpareBits() const {
  if (!HeapPointerSpareBits) {
    // Start with the spare bit mask for all pointers.
    HeapPointerSpareBits = TargetInfo.PointerSpareBits;

    // Low bits are made available by heap object alignment.
    setAlignmentBits(*HeapPointerSpareBits, TargetInfo.HeapObjectAlignment);
  }
  return *HeapPointerSpareBits;
}

const SpareBitVector &
IRGenModule::getFunctionPointerSpareBits() const {
  return TargetInfo.FunctionPointerSpareBits;
}
