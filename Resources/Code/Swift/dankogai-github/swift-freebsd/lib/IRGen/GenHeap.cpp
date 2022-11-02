//===--- GenHeap.cpp - Layout of heap objects and their metadata ----------===//
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
//  This file implements routines for arbitrary Swift-native heap objects,
//  such as layout and reference-counting.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/ErrorHandling.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Intrinsics.h"

#include "swift/Basic/Fallthrough.h"
#include "swift/Basic/SourceLoc.h"
#include "swift/ABI/MetadataValues.h"

#include "Explosion.h"
#include "GenProto.h"
#include "GenType.h"
#include "IRGenDebugInfo.h"
#include "IRGenFunction.h"
#include "IRGenModule.h"
#include "HeapTypeInfo.h"
#include "IndirectTypeInfo.h"
#include "UnownedTypeInfo.h"
#include "WeakTypeInfo.h"

#include "GenHeap.h"

using namespace swift;
using namespace irgen;

/// Produce a constant to place in a metatype's isa field
/// corresponding to the given metadata kind.
static llvm::ConstantInt *getMetadataKind(IRGenModule &IGM,
                                          MetadataKind kind) {
  return llvm::ConstantInt::get(IGM.MetadataKindTy, uint8_t(kind));
}

/// Perform the layout required for a heap object.
HeapLayout::HeapLayout(IRGenModule &IGM, LayoutStrategy strategy,
                       ArrayRef<SILType> fieldTypes,
                       ArrayRef<const TypeInfo *> fieldTypeInfos,
                       llvm::StructType *typeToFill,
                       NecessaryBindings &&bindings)
  : StructLayout(IGM, CanType(), LayoutKind::HeapObject, strategy,
                 fieldTypeInfos, typeToFill),
    ElementTypes(fieldTypes.begin(), fieldTypes.end()),
    Bindings(std::move(bindings))
{
#ifndef NDEBUG
  assert(fieldTypeInfos.size() == fieldTypes.size()
         && "type infos don't match types");
  if (!Bindings.empty()) {
    assert(fieldTypeInfos.size() >= 1 && "no field for bindings");
    auto fixedBindingsField = dyn_cast<FixedTypeInfo>(fieldTypeInfos[0]);
    assert(fixedBindingsField
           && "bindings field is not fixed size");
    assert(fixedBindingsField->getFixedSize()
               == Bindings.getBufferSize(IGM)
           && fixedBindingsField->getFixedAlignment()
               == IGM.getPointerAlignment()
           && "bindings field doesn't fit bindings");
  }
#endif
}

HeapNonFixedOffsets::HeapNonFixedOffsets(IRGenFunction &IGF,
                                         const HeapLayout &layout) {
  if (!layout.isFixedLayout()) {
    // Calculate all the non-fixed layouts.
    // TODO: We could be lazier about this.
    llvm::Value *offset = nullptr;
    llvm::Value *totalAlign = llvm::ConstantInt::get(IGF.IGM.SizeTy,
                                         layout.getAlignment().getMaskValue());
    for (unsigned i : indices(layout.getElements())) {
      auto &elt = layout.getElement(i);
      auto eltTy = layout.getElementTypes()[i];
      switch (elt.getKind()) {
      case ElementLayout::Kind::InitialNonFixedSize:
        // Factor the non-fixed-size field's alignment into the total alignment.
        totalAlign = IGF.Builder.CreateOr(totalAlign,
                                    elt.getType().getAlignmentMask(IGF, eltTy));
        SWIFT_FALLTHROUGH;
      case ElementLayout::Kind::Empty:
      case ElementLayout::Kind::Fixed:
        // Don't need to dynamically calculate this offset.
        Offsets.push_back(nullptr);
        break;
      
      case ElementLayout::Kind::NonFixed:
        // Start calculating non-fixed offsets from the end of the first fixed
        // field.
        assert(i > 0 && "shouldn't begin with a non-fixed field");
        auto &prevElt = layout.getElement(i-1);
        auto prevType = layout.getElementTypes()[i-1];
        // Start calculating offsets from the last fixed-offset field.
        if (!offset) {
          Size lastFixedOffset = layout.getElement(i-1).getByteOffset();
          if (auto *fixedType = dyn_cast<FixedTypeInfo>(&prevElt.getType())) {
            // If the last fixed-offset field is also fixed-size, we can
            // statically compute the end of the fixed-offset fields.
            auto fixedEnd = lastFixedOffset + fixedType->getFixedSize();
            offset
              = llvm::ConstantInt::get(IGF.IGM.SizeTy, fixedEnd.getValue());
          } else {
            // Otherwise, we need to add the dynamic size to the fixed start
            // offset.
            offset
              = llvm::ConstantInt::get(IGF.IGM.SizeTy,
                                       lastFixedOffset.getValue());
            offset = IGF.Builder.CreateAdd(offset,
                                     prevElt.getType().getSize(IGF, prevType));
          }
        }
        
        // Round up to alignment to get the offset.
        auto alignMask = elt.getType().getAlignmentMask(IGF, eltTy);
        auto notAlignMask = IGF.Builder.CreateNot(alignMask);
        offset = IGF.Builder.CreateAdd(offset, alignMask);
        offset = IGF.Builder.CreateAnd(offset, notAlignMask);
        
        Offsets.push_back(offset);
        
        // Advance by the field's size to start the next field.
        offset = IGF.Builder.CreateAdd(offset,
                                       elt.getType().getSize(IGF, eltTy));
        totalAlign = IGF.Builder.CreateOr(totalAlign, alignMask);

        break;
      }
    }
    TotalSize = offset;
    TotalAlignMask = totalAlign;
  } else {
    TotalSize = layout.emitSize(IGF.IGM);
    TotalAlignMask = layout.emitAlignMask(IGF.IGM);
  }
}

void irgen::emitDeallocateHeapObject(IRGenFunction &IGF,
                                     llvm::Value *object,
                                     llvm::Value *size,
                                     llvm::Value *alignMask) {
  // FIXME: We should call a fast deallocator for heap objects with
  // known size.
  IGF.Builder.CreateCall(IGF.IGM.getDeallocObjectFn(),
                         {object, size, alignMask});
}

void irgen::emitDeallocateClassInstance(IRGenFunction &IGF,
                                        llvm::Value *object,
                                        llvm::Value *size,
                                        llvm::Value *alignMask) {
  // FIXME: We should call a fast deallocator for heap objects with
  // known size.
  IGF.Builder.CreateCall(IGF.IGM.getDeallocClassInstanceFn(),
                         {object, size, alignMask});
}

void irgen::emitDeallocatePartialClassInstance(IRGenFunction &IGF,
                                               llvm::Value *object,
                                               llvm::Value *metadata,
                                               llvm::Value *size,
                                               llvm::Value *alignMask) {
  // FIXME: We should call a fast deallocator for heap objects with
  // known size.
  IGF.Builder.CreateCall(IGF.IGM.getDeallocPartialClassInstanceFn(),
                         {object, metadata, size, alignMask});
}

/// Create the destructor function for a layout.
/// TODO: give this some reasonable name and possibly linkage.
static llvm::Function *createDtorFn(IRGenModule &IGM,
                                    const HeapLayout &layout) {
  llvm::Function *fn =
    llvm::Function::Create(IGM.DeallocatingDtorTy,
                           llvm::Function::PrivateLinkage,
                           "objectdestroy", &IGM.Module);
  fn->setAttributes(IGM.constructInitialAttributes());

  IRGenFunction IGF(IGM, fn);
  if (IGM.DebugInfo)
    IGM.DebugInfo->emitArtificialFunction(IGF, fn);

  Address structAddr = layout.emitCastTo(IGF, &*fn->arg_begin());

  // Bind necessary bindings, if we have them.
  if (layout.hasBindings()) {
    // The type metadata bindings should be at a fixed offset, so we can pass
    // None for NonFixedOffsets. If we didn't, we'd have a chicken-egg problem.
    auto bindingsAddr = layout.getElement(0).project(IGF, structAddr, None);
    layout.getBindings().restore(IGF, bindingsAddr);
  }

  // Figure out the non-fixed offsets.
  HeapNonFixedOffsets offsets(IGF, layout);

  // Destroy the fields.
  for (unsigned i : indices(layout.getElements())) {
    auto &field = layout.getElement(i);
    auto fieldTy = layout.getElementTypes()[i];
    if (field.isPOD())
      continue;

    field.getType().destroy(IGF, field.project(IGF, structAddr, offsets),
                            fieldTy);
  }

  emitDeallocateHeapObject(IGF, &*fn->arg_begin(), offsets.getSize(),
                           offsets.getAlignMask());
  IGF.Builder.CreateRetVoid();

  return fn;
}

/// Create the size function for a layout.
/// TODO: give this some reasonable name and possibly linkage.
llvm::Constant *HeapLayout::createSizeFn(IRGenModule &IGM) const {
  llvm::Function *fn =
    llvm::Function::Create(IGM.DeallocatingDtorTy,
                           llvm::Function::PrivateLinkage,
                           "objectsize", &IGM.Module);
  fn->setAttributes(IGM.constructInitialAttributes());

  IRGenFunction IGF(IGM, fn);
  if (IGM.DebugInfo)
    IGM.DebugInfo->emitArtificialFunction(IGF, fn);

  // Ignore the object pointer; we aren't a dynamically-sized array,
  // so it's pointless.

  llvm::Value *size = emitSize(IGM);
  IGF.Builder.CreateRet(size);

  return fn;
}

static llvm::Constant *buildPrivateMetadata(IRGenModule &IGM,
                                            const HeapLayout &layout,
                                            llvm::Constant *dtorFn,
                                            MetadataKind kind) {
  // Build the fields of the private metadata.
  SmallVector<llvm::Constant*, 4> fields;
  fields.push_back(dtorFn);
  fields.push_back(llvm::ConstantPointerNull::get(IGM.WitnessTablePtrTy));
  fields.push_back(llvm::ConstantStruct::get(IGM.TypeMetadataStructTy,
                                             getMetadataKind(IGM, kind)));
  // Figure out the offset to the first element, which is necessary to be able
  // to polymorphically project as a generic box.
  auto elements = layout.getElements();
  Size offset;
  if (!elements.empty()
      && elements[0].getKind() == ElementLayout::Kind::Fixed)
    offset = elements[0].getByteOffset();
  else
    offset = Size(0);
  fields.push_back(llvm::ConstantInt::get(IGM.Int32Ty, offset.getValue()));

  llvm::Constant *init =
    llvm::ConstantStruct::get(IGM.FullBoxMetadataStructTy, fields);

  llvm::GlobalVariable *var =
    new llvm::GlobalVariable(IGM.Module, IGM.FullBoxMetadataStructTy,
                             /*constant*/ true,
                             llvm::GlobalVariable::PrivateLinkage, init,
                             "metadata");

  llvm::Constant *indices[] = {
    llvm::ConstantInt::get(IGM.Int32Ty, 0),
    llvm::ConstantInt::get(IGM.Int32Ty, 2)
  };
  return llvm::ConstantExpr::getInBoundsGetElementPtr(
      /*Ty=*/nullptr, var, indices);
}

llvm::Constant *HeapLayout::getPrivateMetadata(IRGenModule &IGM) const {
  if (!privateMetadata)
    privateMetadata = buildPrivateMetadata(IGM, *this, createDtorFn(IGM, *this),
                                           MetadataKind::HeapLocalVariable);
  return privateMetadata;
}

llvm::Value *IRGenFunction::emitUnmanagedAlloc(const HeapLayout &layout,
                                               const llvm::Twine &name,
                                           const HeapNonFixedOffsets *offsets) {
  llvm::Value *metadata = layout.getPrivateMetadata(IGM);
  llvm::Value *size, *alignMask;
  if (offsets) {
    size = offsets->getSize();
    alignMask = offsets->getAlignMask();
  } else {
    size = layout.emitSize(IGM);
    alignMask = layout.emitAlignMask(IGM);
  }

  return emitAllocObjectCall(metadata, size, alignMask, name);
}

namespace {
  class BuiltinNativeObjectTypeInfo
    : public HeapTypeInfo<BuiltinNativeObjectTypeInfo> {
  public:
    BuiltinNativeObjectTypeInfo(llvm::PointerType *storage,
                                 Size size, SpareBitVector spareBits,
                                 Alignment align)
    : HeapTypeInfo(storage, size, spareBits, align) {}

    /// Builtin.NativeObject uses Swift native reference-counting.
    ReferenceCounting getReferenceCounting() const {
      return ReferenceCounting::Native;
    }
  };
}

const LoadableTypeInfo *TypeConverter::convertBuiltinNativeObject() {
  return new BuiltinNativeObjectTypeInfo(IGM.RefCountedPtrTy,
                                      IGM.getPointerSize(),
                                      IGM.getHeapObjectSpareBits(),
                                      IGM.getPointerAlignment());
}

namespace {
  /// A type implementation for an @unowned(unsafe) reference to an
  /// object.
  class UnmanagedReferenceTypeInfo
    : public PODSingleScalarTypeInfo<UnmanagedReferenceTypeInfo,
                                     LoadableTypeInfo> {
  public:
    UnmanagedReferenceTypeInfo(llvm::Type *type,
                               const SpareBitVector &spareBits,
                               Size size, Alignment alignment)
      : PODSingleScalarTypeInfo(type, size, spareBits, alignment) {}
  
    // Unmanaged types have the same spare bits as managed heap objects.
    
    bool mayHaveExtraInhabitants(IRGenModule &IGM) const override {
      return true;
    }

    unsigned getFixedExtraInhabitantCount(IRGenModule &IGM) const override {
      return getHeapObjectExtraInhabitantCount(IGM);
    }

    APInt getFixedExtraInhabitantValue(IRGenModule &IGM,
                                       unsigned bits,
                                       unsigned index) const override {
      return getHeapObjectFixedExtraInhabitantValue(IGM, bits, index, 0);
    }

    llvm::Value *getExtraInhabitantIndex(IRGenFunction &IGF, Address src,
                                         SILType T)
    const override {
      return getHeapObjectExtraInhabitantIndex(IGF, src);
    }

    void storeExtraInhabitant(IRGenFunction &IGF, llvm::Value *index,
                              Address dest, SILType T) const override {
      return storeHeapObjectExtraInhabitant(IGF, index, dest);
    }
  };
}

const LoadableTypeInfo *
TypeConverter::createUnmanagedStorageType(llvm::Type *valueType) {
  return new UnmanagedReferenceTypeInfo(valueType,
                                        IGM.getHeapObjectSpareBits(),
                                        IGM.getPointerSize(),
                                        IGM.getPointerAlignment());
}

namespace {
  /// A type implementation for an [unowned] reference to an object
  /// with a known-Swift reference count.
  class SwiftUnownedReferenceTypeInfo
    : public SingleScalarTypeInfo<SwiftUnownedReferenceTypeInfo,
                                  UnownedTypeInfo> {
  public:
    SwiftUnownedReferenceTypeInfo(llvm::Type *type,
                                  const SpareBitVector &spareBits,
                                  Size size, Alignment alignment)
      : SingleScalarTypeInfo(type, size, spareBits, alignment) {}

    enum { IsScalarPOD = false };

    void emitScalarRetain(IRGenFunction &IGF, llvm::Value *value) const {
      IGF.emitUnownedRetain(value);
    }

    void emitScalarRelease(IRGenFunction &IGF, llvm::Value *value) const {
      IGF.emitUnownedRelease(value);
    }

    void emitScalarFixLifetime(IRGenFunction &IGF, llvm::Value *value) const {
      IGF.emitFixLifetime(value);
    }
    
    // Unowned types have the same spare bits as strong heap object refs.
    
    bool mayHaveExtraInhabitants(IRGenModule &IGM) const override {
      return true;
    }

    unsigned getFixedExtraInhabitantCount(IRGenModule &IGM) const override {
      return getHeapObjectExtraInhabitantCount(IGM);
    }

    APInt getFixedExtraInhabitantValue(IRGenModule &IGM,
                                       unsigned bits,
                                       unsigned index) const override {
      return getHeapObjectFixedExtraInhabitantValue(IGM, bits, index, 0);
    }

    llvm::Value *getExtraInhabitantIndex(IRGenFunction &IGF, Address src,
                                         SILType T)
    const override {
      return getHeapObjectExtraInhabitantIndex(IGF, src);
    }

    void storeExtraInhabitant(IRGenFunction &IGF, llvm::Value *index,
                              Address dest, SILType T) const override {
      return storeHeapObjectExtraInhabitant(IGF, index, dest);
    }
  };

  /// A type implementation for a [weak] reference to an object
  /// with a known-Swift reference count.
  class SwiftWeakReferenceTypeInfo
    : public IndirectTypeInfo<SwiftWeakReferenceTypeInfo,
                              WeakTypeInfo> {
    llvm::Type *ValueType;
  public:
    SwiftWeakReferenceTypeInfo(llvm::Type *valueType,
                               llvm::Type *weakType,
                               Size size, Alignment alignment,
                               SpareBitVector &&spareBits)
      : IndirectTypeInfo(weakType, size, alignment, std::move(spareBits)),
        ValueType(valueType) {}

    void initializeWithCopy(IRGenFunction &IGF, Address destAddr,
                            Address srcAddr, SILType T) const override {
      IGF.emitWeakCopyInit(destAddr, srcAddr);
    }

    void initializeWithTake(IRGenFunction &IGF, Address destAddr,
                            Address srcAddr, SILType T) const override {
      IGF.emitWeakTakeInit(destAddr, srcAddr);
    }

    void assignWithCopy(IRGenFunction &IGF, Address destAddr,
                        Address srcAddr, SILType T) const override {
      IGF.emitWeakCopyAssign(destAddr, srcAddr);
    }

    void assignWithTake(IRGenFunction &IGF, Address destAddr,
                        Address srcAddr, SILType T) const override {
      IGF.emitWeakTakeAssign(destAddr, srcAddr);
    }

    void destroy(IRGenFunction &IGF, Address addr, SILType T) const override {
      IGF.emitWeakDestroy(addr);
    }

    llvm::Type *getOptionalIntType() const {
      return llvm::IntegerType::get(ValueType->getContext(),
                                    getFixedSize().getValueInBits());
    }

    void weakLoadStrong(IRGenFunction &IGF, Address addr,
                        Explosion &out) const override {
      auto value = IGF.emitWeakLoadStrong(addr, ValueType);
      // The optional will be lowered to an integer type the size of the word.
      out.add(IGF.Builder.CreatePtrToInt(value, getOptionalIntType()));
    }

    void weakTakeStrong(IRGenFunction &IGF, Address addr,
                        Explosion &out) const override {
      auto value = IGF.emitWeakTakeStrong(addr, ValueType);
      // The optional will be lowered to an integer type the size of the word.
      out.add(IGF.Builder.CreatePtrToInt(value, getOptionalIntType()));
    }

    void weakInit(IRGenFunction &IGF, Explosion &in,
                  Address dest) const override {
      llvm::Value *value = in.claimNext();
      // The optional will be lowered to an integer type the size of the word.
      assert(value->getType() == getOptionalIntType());
      value = IGF.Builder.CreateIntToPtr(value, ValueType);
      IGF.emitWeakInit(value, dest);
    }

    void weakAssign(IRGenFunction &IGF, Explosion &in,
                    Address dest) const override {
      llvm::Value *value = in.claimNext();
      // The optional will be lowered to an integer type the size of the word.
      assert(value->getType() == getOptionalIntType());
      value = IGF.Builder.CreateIntToPtr(value, ValueType);
      IGF.emitWeakAssign(value, dest);
    }
  };
}

const UnownedTypeInfo *
TypeConverter::createSwiftUnownedStorageType(llvm::Type *valueType) {
  return new SwiftUnownedReferenceTypeInfo(valueType,
                                           IGM.getHeapObjectSpareBits(),
                                           IGM.getPointerSize(),
                                           IGM.getPointerAlignment());
}

const WeakTypeInfo *
TypeConverter::createSwiftWeakStorageType(llvm::Type *valueType) {
  return new SwiftWeakReferenceTypeInfo(valueType,
                                    IGM.WeakReferencePtrTy->getElementType(),
                                        IGM.getWeakReferenceSize(),
                                        IGM.getWeakReferenceAlignment(),
                                        IGM.getWeakReferenceSpareBits());
}

SpareBitVector IRGenModule::getWeakReferenceSpareBits() const {
  // The runtime needs to be able to freely manipulate live weak
  // references without worrying about us mucking around with their
  // bits, so weak references are completely opaque.
  return SpareBitVector::getConstant(getWeakReferenceSize().getValueInBits(),
                                     false);
}

namespace {
  /// A type implementation for an [unowned] reference to an object
  /// that is not necessarily a Swift object.
  class UnknownUnownedReferenceTypeInfo :
      public SingleScalarTypeInfo<UnknownUnownedReferenceTypeInfo,
                                  UnownedTypeInfo> {
  public:
    UnknownUnownedReferenceTypeInfo(llvm::Type *type,
                                    const SpareBitVector &spareBits,
                                    Size size, Alignment alignment)
      : SingleScalarTypeInfo(type, size, spareBits, alignment) {}

    enum { IsScalarPOD = false };

    void emitScalarRetain(IRGenFunction &IGF, llvm::Value *value) const {
      IGF.emitUnknownUnownedRetain(value);
    }

    void emitScalarRelease(IRGenFunction &IGF, llvm::Value *value) const {
      IGF.emitUnknownUnownedRelease(value);
    }

    void emitScalarFixLifetime(IRGenFunction &IGF, llvm::Value *value) const {
      IGF.emitFixLifetime(value);
    }
    
    // Unowned types have the same spare bits as strong unknown object refs.
    
    bool mayHaveExtraInhabitants(IRGenModule &IGM) const override {
      return true;
    }

    unsigned getFixedExtraInhabitantCount(IRGenModule &IGM) const override {
      return getHeapObjectExtraInhabitantCount(IGM);
    }

    APInt getFixedExtraInhabitantValue(IRGenModule &IGM,
                                       unsigned bits,
                                       unsigned index) const override {
      return getHeapObjectFixedExtraInhabitantValue(IGM, bits, index, 0);
    }

    llvm::Value *getExtraInhabitantIndex(IRGenFunction &IGF, Address src,
                                         SILType T)
    const override {
      return getHeapObjectExtraInhabitantIndex(IGF, src);
    }

    void storeExtraInhabitant(IRGenFunction &IGF, llvm::Value *index,
                              Address dest, SILType T) const override {
      return storeHeapObjectExtraInhabitant(IGF, index, dest);
    }
  };

  /// A type implementation for a [weak] reference to an object
  /// that is not necessarily a Swift object.
  class UnknownWeakReferenceTypeInfo :
      public IndirectTypeInfo<UnknownWeakReferenceTypeInfo,
                              WeakTypeInfo> {
    /// We need to separately store the value type because we always
    /// use the same type to store the weak reference struct.
    llvm::Type *ValueType;
  public:
    UnknownWeakReferenceTypeInfo(llvm::Type *valueType,
                                 llvm::Type *weakType,
                                 Size size, Alignment alignment,
                                 SpareBitVector &&spareBits)
      : IndirectTypeInfo(weakType, size, alignment, std::move(spareBits)),
        ValueType(valueType) {}

    void initializeWithCopy(IRGenFunction &IGF, Address destAddr,
                            Address srcAddr, SILType T) const override {
      IGF.emitUnknownWeakCopyInit(destAddr, srcAddr);
    }

    void initializeWithTake(IRGenFunction &IGF, Address destAddr,
                            Address srcAddr, SILType T) const override {
      IGF.emitUnknownWeakTakeInit(destAddr, srcAddr);
    }

    void assignWithCopy(IRGenFunction &IGF, Address destAddr,
                        Address srcAddr, SILType T) const override {
      IGF.emitUnknownWeakCopyAssign(destAddr, srcAddr);
    }

    void assignWithTake(IRGenFunction &IGF, Address destAddr,
                        Address srcAddr, SILType T) const override {
      IGF.emitUnknownWeakTakeAssign(destAddr, srcAddr);
    }

    void destroy(IRGenFunction &IGF, Address addr, SILType T) const override {
      IGF.emitUnknownWeakDestroy(addr);
    }
                                
    llvm::Type *getOptionalIntType() const {
      return llvm::IntegerType::get(ValueType->getContext(),
                                    getFixedSize().getValueInBits());
    }

    void weakLoadStrong(IRGenFunction &IGF, Address addr,
                        Explosion &out) const override {
      auto value = IGF.emitUnknownWeakLoadStrong(addr, ValueType);
      // The optional will be lowered to an integer type the size of the word.
      out.add(IGF.Builder.CreatePtrToInt(value, getOptionalIntType()));
    }

    void weakTakeStrong(IRGenFunction &IGF, Address addr,
                        Explosion &out) const override {
      auto value = IGF.emitUnknownWeakTakeStrong(addr, ValueType);
      // The optional will be lowered to an integer type the size of the word.
      out.add(IGF.Builder.CreatePtrToInt(value, getOptionalIntType()));
    }

    void weakInit(IRGenFunction &IGF, Explosion &in,
                  Address dest) const override {
      llvm::Value *value = in.claimNext();
      // The optional will be lowered to an integer type the size of the word.
      assert(value->getType() == getOptionalIntType());
      value = IGF.Builder.CreateIntToPtr(value, ValueType);
      IGF.emitUnknownWeakInit(value, dest);
    }

    void weakAssign(IRGenFunction &IGF, Explosion &in,
                    Address dest) const override {
      llvm::Value *value = in.claimNext();
      // The optional will be lowered to an integer type the size of the word.
      assert(value->getType() == getOptionalIntType());
      value = IGF.Builder.CreateIntToPtr(value, ValueType);
      IGF.emitUnknownWeakAssign(value, dest);
    }
  };
}

const UnownedTypeInfo *
TypeConverter::createUnknownUnownedStorageType(llvm::Type *valueType) {
  return new UnknownUnownedReferenceTypeInfo(valueType,
                                             IGM.getHeapObjectSpareBits(),
                                             IGM.getPointerSize(),
                                             IGM.getPointerAlignment());
}

const WeakTypeInfo *
TypeConverter::createUnknownWeakStorageType(llvm::Type *valueType) {
  return new UnknownWeakReferenceTypeInfo(valueType,
                                      IGM.WeakReferencePtrTy->getElementType(),
                                          IGM.getWeakReferenceSize(),
                                          IGM.getWeakReferenceAlignment(),
                                          IGM.getWeakReferenceSpareBits());
}

/// Does the given value superficially not require reference-counting?
static bool doesNotRequireRefCounting(llvm::Value *value) {
  // Constants never require reference-counting.
  return isa<llvm::Constant>(value);
}

static llvm::FunctionType *getTypeOfFunction(llvm::Constant *fn) {
  return cast<llvm::FunctionType>(fn->getType()->getPointerElementType());
}

/// Emit a unary call to perform a ref-counting operation.
///
/// \param fn - expected signature 'void (T)'
static void emitUnaryRefCountCall(IRGenFunction &IGF,
                                  llvm::Constant *fn,
                                  llvm::Value *value) {
  // Instead of casting the input, we cast the function type.
  // This tends to produce less IR, but might be evil.
  if (value->getType() != getTypeOfFunction(fn)->getParamType(0)) {
    llvm::FunctionType *fnType =
      llvm::FunctionType::get(IGF.IGM.VoidTy, value->getType(), false);
    fn = llvm::ConstantExpr::getBitCast(fn, fnType->getPointerTo());
  }
  
  // Emit the call.
  llvm::CallInst *call = IGF.Builder.CreateCall(fn, value);
  call->setCallingConv(IGF.IGM.RuntimeCC);
  call->setDoesNotThrow();  
}

/// Emit a copy-like call to perform a ref-counting operation.
///
/// \param fn - expected signature 'void (T, T)'
static void emitCopyLikeCall(IRGenFunction &IGF,
                             llvm::Constant *fn,
                             llvm::Value *dest,
                             llvm::Value *src) {
  assert(dest->getType() == src->getType() &&
         "type mismatch in binary refcounting operation");

  // Instead of casting the inputs, we cast the function type.
  // This tends to produce less IR, but might be evil.
  if (dest->getType() != getTypeOfFunction(fn)->getParamType(0)) {
    llvm::Type *paramTypes[] = { dest->getType(), dest->getType() };
    llvm::FunctionType *fnType =
      llvm::FunctionType::get(IGF.IGM.VoidTy, paramTypes, false);
    fn = llvm::ConstantExpr::getBitCast(fn, fnType->getPointerTo());
  }
  
  // Emit the call.
  llvm::CallInst *call = IGF.Builder.CreateCall(fn, {dest, src});
  call->setCallingConv(IGF.IGM.RuntimeCC);
  call->setDoesNotThrow();  
}

/// Emit a call to a function with a loadWeak-like signature.
///
/// \param fn - expected signature 'T (Weak*)'
static llvm::Value *emitLoadWeakLikeCall(IRGenFunction &IGF,
                                         llvm::Constant *fn,
                                         llvm::Value *addr,
                                         llvm::Type *resultType) {
  assert(addr->getType() == IGF.IGM.WeakReferencePtrTy &&
         "address is not of a weak reference");

  // Instead of casting the output, we cast the function type.
  // This tends to produce less IR, but might be evil.
  if (resultType != getTypeOfFunction(fn)->getReturnType()) {
    llvm::Type *paramTypes[] = { addr->getType() };
    llvm::FunctionType *fnType =
      llvm::FunctionType::get(resultType, paramTypes, false);
    fn = llvm::ConstantExpr::getBitCast(fn, fnType->getPointerTo());
  }

  // Emit the call.
  llvm::CallInst *call = IGF.Builder.CreateCall(fn, addr);
  call->setCallingConv(IGF.IGM.RuntimeCC);
  call->setDoesNotThrow();

  return call;
}

/// Emit a call to a function with a storeWeak-like signature.
///
/// \param fn - expected signature 'void (Weak*, T)'
static void emitStoreWeakLikeCall(IRGenFunction &IGF,
                                  llvm::Constant *fn,
                                  llvm::Value *addr,
                                  llvm::Value *value) {
  assert(addr->getType() == IGF.IGM.WeakReferencePtrTy &&
         "address is not of a weak reference");

  // Instead of casting the inputs, we cast the function type.
  // This tends to produce less IR, but might be evil.
  if (value->getType() != getTypeOfFunction(fn)->getParamType(1)) {
    llvm::Type *paramTypes[] = { addr->getType(), value->getType() };
    llvm::FunctionType *fnType =
      llvm::FunctionType::get(IGF.IGM.VoidTy, paramTypes, false);
    fn = llvm::ConstantExpr::getBitCast(fn, fnType->getPointerTo());
  }

  // Emit the call.
  llvm::CallInst *call = IGF.Builder.CreateCall(fn, {addr, value});
  call->setCallingConv(IGF.IGM.RuntimeCC);
  call->setDoesNotThrow();
}

/// Emit a call to swift_retain.  In general, you should not be using
/// this routine; instead you should use emitRetain, which properly
/// balances the retain.
void IRGenFunction::emitRetainCall(llvm::Value *value) {
  // Make sure the input pointer is the right type.
  if (value->getType() != IGM.RefCountedPtrTy)
    value = Builder.CreateBitCast(value, IGM.RefCountedPtrTy);
  
  // Emit the call.
  llvm::CallInst *call = Builder.CreateCall(IGM.getRetainFn(), value);
  call->setCallingConv(IGM.RuntimeCC);
  call->setDoesNotThrow();
}

/// Emit a retain of a value.  This is usually not required because
/// values in explosions are typically "live", i.e. have a +1 owned by
/// the explosion.
void IRGenFunction::emitRetain(llvm::Value *value, Explosion &out) {
  if (doesNotRequireRefCounting(value)) {
    out.add(value);
    return;
  }

  emitRetainCall(value);
  out.add(value);
}

/// Emit a load of a live value from the given retaining variable.
void IRGenFunction::emitLoadAndRetain(Address address, Explosion &out) {
  llvm::Value *value = Builder.CreateLoad(address);
  emitRetainCall(value);
  out.add(value);
}

/// Emit a store of a live value to the given retaining variable.
void IRGenFunction::emitAssignRetained(llvm::Value *newValue, Address address) {
  // Pull the old value out of the address.
  llvm::Value *oldValue = Builder.CreateLoad(address);

  // We assume the new value is already retained.
  Builder.CreateStore(newValue, address);

  // Release the old value.
  emitRelease(oldValue);
}

/// Emit an initialize of a live value to the given retaining variable.
void IRGenFunction::emitInitializeRetained(llvm::Value *newValue,
                                           Address address) {
  // We assume the new value is already retained.
  Builder.CreateStore(newValue, address);
}

/// Emit a release of a live value with the given refcounting implementation.
void IRGenFunction::emitScalarRelease(llvm::Value *value,
                                      ReferenceCounting refcounting) {
  switch (refcounting) {
  case ReferenceCounting::Native:
    return emitRelease(value);
  case ReferenceCounting::ObjC:
    return emitObjCRelease(value);
  case ReferenceCounting::Block:
    return emitBlockRelease(value);
  case ReferenceCounting::Unknown:
    return emitUnknownRelease(value);
  case ReferenceCounting::Bridge:
    return emitBridgeRelease(value);
  case ReferenceCounting::Error:
    return emitErrorRelease(value);
  }
}

void IRGenFunction::emitScalarRetainCall(llvm::Value *value,
                                         ReferenceCounting refcounting) {
  switch (refcounting) {
  case ReferenceCounting::Native:
    emitRetainCall(value);
    return;
  case ReferenceCounting::Bridge:
    emitBridgeRetainCall(value);
    return;
  case ReferenceCounting::ObjC:
    emitObjCRetainCall(value);
    return;
  case ReferenceCounting::Block:
    emitBlockCopyCall(value);
    return;
  case ReferenceCounting::Unknown:
    emitUnknownRetainCall(value);
    return;
  case ReferenceCounting::Error:
    emitErrorRetainCall(value);
    return;
  }
}

llvm::Type *IRGenModule::getReferenceType(ReferenceCounting refcounting) {
  switch (refcounting) {
  case ReferenceCounting::Native:
    return RefCountedPtrTy;
  case ReferenceCounting::Bridge:
    return BridgeObjectPtrTy;
  case ReferenceCounting::ObjC:
    return ObjCPtrTy;
  case ReferenceCounting::Block:
    return ObjCBlockPtrTy;
  case ReferenceCounting::Unknown:
    return UnknownRefCountedPtrTy;
  case ReferenceCounting::Error:
    return ErrorPtrTy;
  }
}

/// Emit a release of a live value.
void IRGenFunction::emitRelease(llvm::Value *value) {
  if (doesNotRequireRefCounting(value)) return;
  emitUnaryRefCountCall(*this, IGM.getReleaseFn(), value);
}

/// Fix the lifetime of a live value. This communicates to the LLVM level ARC
/// optimizer not to touch this value.
void IRGenFunction::emitFixLifetime(llvm::Value *value) {
  if (doesNotRequireRefCounting(value)) return;
  emitUnaryRefCountCall(*this, IGM.getFixLifetimeFn(), value);
}

llvm::Value *IRGenFunction::emitUnknownRetainCall(llvm::Value *value) {
  emitUnaryRefCountCall(*this, IGM.getUnknownRetainFn(), value);
  return value;
}

void IRGenFunction::emitUnknownRelease(llvm::Value *value) {
  emitUnaryRefCountCall(*this, IGM.getUnknownReleaseFn(), value);
}

llvm::Value *IRGenFunction::emitBridgeRetainCall(llvm::Value *value) {
  emitUnaryRefCountCall(*this, IGM.getBridgeObjectRetainFn(), value);
  return value;
}

void IRGenFunction::emitBridgeRelease(llvm::Value *value) {
  emitUnaryRefCountCall(*this, IGM.getBridgeObjectReleaseFn(), value);
}

void IRGenFunction::emitErrorRetain(llvm::Value *value) {
  emitUnaryRefCountCall(*this, IGM.getErrorRetainFn(), value);
}

llvm::Value *IRGenFunction::emitErrorRetainCall(llvm::Value *value) {
  emitUnaryRefCountCall(*this, IGM.getErrorRetainFn(), value);
  return value;
}

void IRGenFunction::emitErrorRelease(llvm::Value *value) {
  emitUnaryRefCountCall(*this, IGM.getErrorReleaseFn(), value);
}

llvm::Value *IRGenFunction::emitTryPin(llvm::Value *value) {
  llvm::CallInst *call = Builder.CreateCall(IGM.getTryPinFn(), value);
  call->setCallingConv(IGM.RuntimeCC);
  call->setDoesNotThrow();

  // Builtin.NativeObject? has representation i32/i64.
  llvm::Value *handle = Builder.CreatePtrToInt(call, IGM.IntPtrTy);
  return handle;
}

void IRGenFunction::emitUnpin(llvm::Value *value) {
  // Builtin.NativeObject? has representation i32/i64.
  value = Builder.CreateIntToPtr(value, IGM.RefCountedPtrTy);

  llvm::CallInst *call = Builder.CreateCall(IGM.getUnpinFn(), value);
  call->setCallingConv(IGM.RuntimeCC);
  call->setDoesNotThrow();
}

llvm::Value *IRGenFunction::emitLoadNativeRefcountedPtr(Address addr) {
  Address src =
    Builder.CreateBitCast(addr, IGM.RefCountedPtrTy->getPointerTo());
  return Builder.CreateLoad(src);
}

llvm::Value *IRGenFunction::emitLoadUnknownRefcountedPtr(Address addr) {
  Address src =
    Builder.CreateBitCast(addr, IGM.UnknownRefCountedPtrTy->getPointerTo());
  return Builder.CreateLoad(src);
}

llvm::Value *IRGenFunction::emitLoadBridgeRefcountedPtr(Address addr) {
  Address src =
    Builder.CreateBitCast(addr, IGM.BridgeObjectPtrTy->getPointerTo());
  return Builder.CreateLoad(src);
}

llvm::Value *IRGenFunction::
emitIsUniqueCall(llvm::Value *value, SourceLoc loc, bool isNonNull,
                 bool checkPinned) {
  llvm::Constant *fn;
  if (value->getType() == IGM.RefCountedPtrTy) {
    if (checkPinned) {
      if (isNonNull)
        fn = IGM.getIsUniquelyReferencedOrPinned_nonNull_nativeFn();
      else
        fn = IGM.getIsUniquelyReferencedOrPinned_nativeFn();
    }
    else {
      if (isNonNull)
        fn = IGM.getIsUniquelyReferenced_nonNull_nativeFn();
      else
        fn = IGM.getIsUniquelyReferenced_nativeFn();
    }
  } else if (value->getType() == IGM.UnknownRefCountedPtrTy) {
    if (checkPinned) {
      if (!isNonNull)
        unimplemented(loc, "optional objc ref");

      fn = IGM.getIsUniquelyReferencedOrPinnedNonObjC_nonNullFn();
    }
    else {
      if (isNonNull)
        fn = IGM.getIsUniquelyReferencedNonObjC_nonNullFn();
      else
        fn = IGM.getIsUniquelyReferencedNonObjCFn();
    }
  } else if (value->getType() == IGM.BridgeObjectPtrTy) {
    if (!isNonNull)
      unimplemented(loc, "optional bridge ref");

    if (checkPinned)
      fn = IGM.getIsUniquelyReferencedOrPinnedNonObjC_nonNull_bridgeObjectFn();
    else
      fn = IGM.getIsUniquelyReferencedNonObjC_nonNull_bridgeObjectFn();
  } else {
    llvm_unreachable("Unexpected LLVM type for a refcounted pointer.");
  }
  llvm::CallInst *call = Builder.CreateCall(fn, value);
  call->setCallingConv(IGM.RuntimeCC);
  call->setDoesNotThrow();
  return call;
}

namespace {
/// Basic layout and common operations for box types.
class BoxTypeInfo : public HeapTypeInfo<BoxTypeInfo> {
public:
  BoxTypeInfo(IRGenModule &IGM)
    : HeapTypeInfo(IGM.RefCountedPtrTy, IGM.getPointerSize(),
                   IGM.getHeapObjectSpareBits(), IGM.getPointerAlignment())
  {}

  ReferenceCounting getReferenceCounting() const {
    // Boxes are always native-refcounted.
    return ReferenceCounting::Native;
  }

  /// Allocate a box of the given type.
  virtual OwnedAddress
  allocate(IRGenFunction &IGF, SILType boxedType,
           const llvm::Twine &name) const = 0;

  /// Deallocate an uninitialized box.
  virtual void
  deallocate(IRGenFunction &IGF, llvm::Value *box, SILType boxedType) const = 0;

  /// Project the address of the contained value from a box.
  virtual Address
  project(IRGenFunction &IGF, llvm::Value *box, SILType boxedType) const = 0;
};

/// Common implementation for empty box type info.
class EmptyBoxTypeInfo final : public BoxTypeInfo {
public:
  EmptyBoxTypeInfo(IRGenModule &IGM) : BoxTypeInfo(IGM) {}

  OwnedAddress
  allocate(IRGenFunction &IGF, SILType boxedType,
           const llvm::Twine &name) const override {
    return OwnedAddress(IGF.getTypeInfo(boxedType).getUndefAddress(),
                        IGF.IGM.RefCountedNull);
  }

  void
  deallocate(IRGenFunction &IGF, llvm::Value *box, SILType boxedType)
  const override {
    /* Nothing to do; the box should be nil. */
  }

  Address
  project(IRGenFunction &IGF, llvm::Value *box, SILType boxedType)
  const override {
    return IGF.getTypeInfo(boxedType).getUndefAddress();
  }
};

/// Common implementation for non-fixed box type info.
class NonFixedBoxTypeInfo final : public BoxTypeInfo {
public:
  NonFixedBoxTypeInfo(IRGenModule &IGM) : BoxTypeInfo(IGM) {}

  OwnedAddress
  allocate(IRGenFunction &IGF, SILType boxedType,
           const llvm::Twine &name) const override {
    auto &ti = IGF.getTypeInfo(boxedType);
    // Use the runtime to allocate a box of the appropriate size.
    auto metadata = IGF.emitTypeMetadataRefForLayout(boxedType);
    llvm::Value *box, *address;
    IGF.emitAllocBoxCall(metadata, box, address);
    address = IGF.Builder.CreateBitCast(address,
                                        ti.getStorageType()->getPointerTo());
    return {ti.getAddressForPointer(address), box};
  }

  void
  deallocate(IRGenFunction &IGF, llvm::Value *box, SILType boxedType)
  const override {
    auto metadata = IGF.emitTypeMetadataRefForLayout(boxedType);
    IGF.emitDeallocBoxCall(box, metadata);
  }

  Address
  project(IRGenFunction &IGF, llvm::Value *box, SILType boxedType)
  const override {
    auto &ti = IGF.getTypeInfo(boxedType);
    auto metadata = IGF.emitTypeMetadataRefForLayout(boxedType);
    llvm::Value *address = IGF.emitProjectBoxCall(box, metadata);
    address = IGF.Builder.CreateBitCast(address,
                                        ti.getStorageType()->getPointerTo());
    return ti.getAddressForPointer(address);
  }
};

/// Base implementation for fixed-sized boxes.
class FixedBoxTypeInfoBase : public BoxTypeInfo {
  HeapLayout layout;

public:
  FixedBoxTypeInfoBase(IRGenModule &IGM, HeapLayout &&layout)
    : BoxTypeInfo(IGM), layout(std::move(layout))
  {}

  OwnedAddress
  allocate(IRGenFunction &IGF, SILType boxedType, const llvm::Twine &name)
  const override {
    // Allocate a new object using the layout.
    llvm::Value *allocation = IGF.emitUnmanagedAlloc(layout, name);
    Address rawAddr = project(IGF, allocation, boxedType);
    return {rawAddr, allocation};
  }

  void
  deallocate(IRGenFunction &IGF, llvm::Value *box, SILType _)
  const override {
    auto size = layout.emitSize(IGF.IGM);
    auto alignMask = layout.emitAlignMask(IGF.IGM);

    emitDeallocateHeapObject(IGF, box, size, alignMask);
  }

  Address
  project(IRGenFunction &IGF, llvm::Value *box, SILType boxedType)
  const override {
    Address rawAddr = layout.emitCastTo(IGF, box);
    rawAddr = layout.getElement(0).project(IGF, rawAddr, None);
    auto &ti = IGF.getTypeInfo(boxedType);
    return IGF.Builder.CreateBitCast(rawAddr,
                                     ti.getStorageType()->getPointerTo());
  }
};

static HeapLayout getHeapLayoutForSingleTypeInfo(IRGenModule &IGM,
                                                 const TypeInfo &ti) {
  return HeapLayout(IGM, LayoutStrategy::Optimal, SILType(), &ti);
}

/// Common implementation for POD boxes of a known stride and alignment.
class PODBoxTypeInfo final : public FixedBoxTypeInfoBase {
public:
  PODBoxTypeInfo(IRGenModule &IGM, Size stride, Alignment alignment)
    : FixedBoxTypeInfoBase(IGM, getHeapLayoutForSingleTypeInfo(IGM,
                             IGM.getOpaqueStorageTypeInfo(stride, alignment))) {
  }
};

/// Common implementation for single-refcounted boxes.
class SingleRefcountedBoxTypeInfo final : public FixedBoxTypeInfoBase {
public:
  SingleRefcountedBoxTypeInfo(IRGenModule &IGM, ReferenceCounting refcounting)
    : FixedBoxTypeInfoBase(IGM, getHeapLayoutForSingleTypeInfo(IGM,
                                   IGM.getReferenceObjectTypeInfo(refcounting)))
  {
  }
};

/// Implementation of a box for a specific type.
class FixedBoxTypeInfo final : public FixedBoxTypeInfoBase {
public:
  FixedBoxTypeInfo(IRGenModule &IGM, SILType T)
    : FixedBoxTypeInfoBase(IGM,
       HeapLayout(IGM, LayoutStrategy::Optimal, T, &IGM.getTypeInfo(T)))
  {}
};

}

const TypeInfo *TypeConverter::convertBoxType(SILBoxType *T) {
  // We can share a type info for all dynamic-sized heap metadata.
  auto &eltTI = IGM.getTypeInfoForLowered(T->getBoxedType());
  if (!eltTI.isFixedSize()) {
    if (!NonFixedBoxTI)
      NonFixedBoxTI = new NonFixedBoxTypeInfo(IGM);
    return NonFixedBoxTI;
  }

  // For fixed-sized types, we can emit concrete box metadata.
  auto &fixedTI = cast<FixedTypeInfo>(eltTI);

  // For empty types, we don't really need to allocate anything.
  if (fixedTI.isKnownEmpty()) {
    if (!EmptyBoxTI)
      EmptyBoxTI = new EmptyBoxTypeInfo(IGM);
    return EmptyBoxTI;
  }

  // We can share box info for all similarly-shaped POD types.
  if (fixedTI.isPOD(ResilienceScope::Component)) {
    auto stride = fixedTI.getFixedStride();
    auto align = fixedTI.getFixedAlignment();
    auto foundPOD = PODBoxTI.find({stride.getValue(),align.getValue()});
    if (foundPOD == PODBoxTI.end()) {
      auto newPOD = new PODBoxTypeInfo(IGM, stride, align);
      PODBoxTI.insert({{stride.getValue(), align.getValue()}, newPOD});
      return newPOD;
    }

    return foundPOD->second;
  }

  // We can share box info for all single-refcounted types.
  if (fixedTI.isSingleSwiftRetainablePointer(ResilienceScope::Component)) {
    if (!SwiftRetainablePointerBoxTI)
      SwiftRetainablePointerBoxTI
        = new SingleRefcountedBoxTypeInfo(IGM, ReferenceCounting::Native);
    return SwiftRetainablePointerBoxTI;
  }

  // TODO: Other common shapes? Optional-of-Refcounted would be nice.

  // Produce a tailored box metadata for the type.
  return new FixedBoxTypeInfo(IGM, T->getBoxedAddressType());
}

OwnedAddress
irgen::emitAllocateBox(IRGenFunction &IGF, CanSILBoxType boxType,
                       const llvm::Twine &name) {
  auto &boxTI = IGF.getTypeInfoForLowered(boxType).as<BoxTypeInfo>();
  return boxTI.allocate(IGF, boxType->getBoxedAddressType(), name);
}

void irgen::emitDeallocateBox(IRGenFunction &IGF,
                              llvm::Value *box,
                              CanSILBoxType boxType) {
  auto &boxTI = IGF.getTypeInfoForLowered(boxType).as<BoxTypeInfo>();
  return boxTI.deallocate(IGF, box, boxType->getBoxedAddressType());
}

Address irgen::emitProjectBox(IRGenFunction &IGF,
                              llvm::Value *box,
                              CanSILBoxType boxType) {
  auto &boxTI = IGF.getTypeInfoForLowered(boxType).as<BoxTypeInfo>();
  return boxTI.project(IGF, box, boxType->getBoxedAddressType());
}

#define DEFINE_VALUE_OP(ID)                                           \
void IRGenFunction::emit##ID(llvm::Value *value) {                    \
  if (doesNotRequireRefCounting(value)) return;                       \
  emitUnaryRefCountCall(*this, IGM.get##ID##Fn(), value);             \
}
#define DEFINE_ADDR_OP(ID)                                            \
void IRGenFunction::emit##ID(Address addr) {                          \
  emitUnaryRefCountCall(*this, IGM.get##ID##Fn(), addr.getAddress()); \
}
#define DEFINE_COPY_OP(ID)                                            \
void IRGenFunction::emit##ID(Address dest, Address src) {             \
  emitCopyLikeCall(*this, IGM.get##ID##Fn(), dest.getAddress(),       \
                   src.getAddress());                                 \
}
#define DEFINE_LOAD_WEAK_OP(ID)                                       \
llvm::Value *IRGenFunction::emit##ID(Address src, llvm::Type *type) { \
  return emitLoadWeakLikeCall(*this, IGM.get##ID##Fn(),               \
                              src.getAddress(), type);                \
}
#define DEFINE_STORE_WEAK_OP(ID)                                      \
void IRGenFunction::emit##ID(llvm::Value *value, Address src) {       \
  emitStoreWeakLikeCall(*this, IGM.get##ID##Fn(),                     \
                        src.getAddress(), value);                     \
}

DEFINE_VALUE_OP(RetainUnowned)
DEFINE_VALUE_OP(UnownedRelease)
DEFINE_VALUE_OP(UnownedRetain)
DEFINE_LOAD_WEAK_OP(WeakLoadStrong)
DEFINE_LOAD_WEAK_OP(WeakTakeStrong)
DEFINE_STORE_WEAK_OP(WeakInit)
DEFINE_STORE_WEAK_OP(WeakAssign)
DEFINE_ADDR_OP(WeakDestroy)
DEFINE_COPY_OP(WeakCopyInit)
DEFINE_COPY_OP(WeakCopyAssign)
DEFINE_COPY_OP(WeakTakeInit)
DEFINE_COPY_OP(WeakTakeAssign)
DEFINE_VALUE_OP(UnknownRetainUnowned)
DEFINE_VALUE_OP(UnknownUnownedRelease)
DEFINE_VALUE_OP(UnknownUnownedRetain)
DEFINE_LOAD_WEAK_OP(UnknownWeakLoadStrong)
DEFINE_LOAD_WEAK_OP(UnknownWeakTakeStrong)
DEFINE_STORE_WEAK_OP(UnknownWeakInit)
DEFINE_STORE_WEAK_OP(UnknownWeakAssign)
DEFINE_ADDR_OP(UnknownWeakDestroy)
DEFINE_COPY_OP(UnknownWeakCopyInit)
DEFINE_COPY_OP(UnknownWeakCopyAssign)
DEFINE_COPY_OP(UnknownWeakTakeInit)
DEFINE_COPY_OP(UnknownWeakTakeAssign)
