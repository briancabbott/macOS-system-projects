//===--- SILInstruction.cpp - Instructions for SIL code -------------------===//
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
// This file defines the high-level SILInstruction classes used for  SIL code.
//
//===----------------------------------------------------------------------===//

#include "swift/SIL/SILInstruction.h"
#include "swift/Basic/type_traits.h"
#include "swift/Basic/Unicode.h"
#include "swift/SIL/SILBuilder.h"
#include "swift/SIL/SILCloner.h"
#include "swift/SIL/SILVisitor.h"
#include "swift/AST/AST.h"
#include "swift/Basic/AssertImplements.h"
#include "swift/ClangImporter/ClangModule.h"
#include "swift/SIL/SILModule.h"
#include "llvm/ADT/APInt.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/ErrorHandling.h"

using namespace swift;
using namespace Lowering;

//===----------------------------------------------------------------------===//
// SILInstruction Subclasses
//===----------------------------------------------------------------------===//

// alloc_stack always returns two results: Builtin.RawPointer & LValue[EltTy]
static SILTypeList *getAllocStackType(SILType eltTy, SILFunction &F) {
  SILType resTys[] = {
    eltTy.getLocalStorageType(),
    eltTy.getAddressType()
  };

  return F.getModule().getSILTypeList(resTys);
}

AllocStackInst::AllocStackInst(SILDebugLocation *Loc, SILType elementType,
                               SILFunction &F, unsigned ArgNo)
    : AllocationInst(ValueKind::AllocStackInst, Loc,
                     getAllocStackType(elementType, F)), VarInfo(ArgNo) {}

/// getDecl - Return the underlying variable declaration associated with this
/// allocation, or null if this is a temporary allocation.
VarDecl *AllocStackInst::getDecl() const {
  return getLoc().getAsASTNode<VarDecl>();
}

AllocRefInst::AllocRefInst(SILDebugLocation *Loc, SILType elementType,
                           SILFunction &F, bool objc, bool canBeOnStack)
    : AllocationInst(ValueKind::AllocRefInst, Loc, elementType),
      StackPromotable(canBeOnStack), ObjC(objc) {}

// alloc_box returns two results: Builtin.NativeObject & LValue[EltTy]
static SILTypeList *getAllocBoxType(SILType EltTy, SILFunction &F) {
  SILType boxTy = SILType::getPrimitiveObjectType(
                                   SILBoxType::get(EltTy.getSwiftRValueType()));

  SILType ResTys[] = {
    boxTy,
    EltTy.getAddressType()
  };

  return F.getModule().getSILTypeList(ResTys);
}

AllocBoxInst::AllocBoxInst(SILDebugLocation *Loc, SILType ElementType,
                           SILFunction &F, unsigned ArgNo)
    : AllocationInst(ValueKind::AllocBoxInst, Loc,
                     getAllocBoxType(ElementType, F)),
      VarInfo(ArgNo) {}

/// getDecl - Return the underlying variable declaration associated with this
/// allocation, or null if this is a temporary allocation.
VarDecl *AllocBoxInst::getDecl() const {
  return getLoc().getAsASTNode<VarDecl>();
}

VarDecl *DebugValueInst::getDecl() const {
  return getLoc().getAsASTNode<VarDecl>();
}
VarDecl *DebugValueAddrInst::getDecl() const {
  return getLoc().getAsASTNode<VarDecl>();
}

static SILTypeList *getAllocExistentialBoxType(SILType ExistTy,
                                               SILType ConcreteTy,
                                               SILFunction &F) {
  SILType Tys[] = {
    ExistTy.getObjectType(),
    ConcreteTy.getAddressType(),
  };
  return F.getModule().getSILTypeList(Tys);
}

AllocExistentialBoxInst::AllocExistentialBoxInst(
    SILDebugLocation *Loc, SILType ExistentialType, CanType ConcreteType,
    SILType ConcreteLoweredType, ArrayRef<ProtocolConformance *> Conformances,
    SILFunction *Parent)
    : AllocationInst(ValueKind::AllocExistentialBoxInst, Loc,
                     getAllocExistentialBoxType(ExistentialType,
                                                ConcreteLoweredType, *Parent)),
      ConcreteType(ConcreteType), Conformances(Conformances) {}

static void declareWitnessTable(SILModule &Mod,
                                ProtocolConformance *C) {
  if (!C) return;
  if (!Mod.lookUpWitnessTable(C, false).first)
    Mod.createWitnessTableDeclaration(C,
        TypeConverter::getLinkageForProtocolConformance(
                                                  C->getRootNormalConformance(),
                                                  NotForDefinition));
}

AllocExistentialBoxInst *AllocExistentialBoxInst::create(
    SILDebugLocation *Loc, SILType ExistentialType, CanType ConcreteType,
    SILType ConcreteLoweredType, ArrayRef<ProtocolConformance *> Conformances,
    SILFunction *F) {
  SILModule &Mod = F->getModule();
  void *Buffer = Mod.allocate(sizeof(AllocExistentialBoxInst),
                              alignof(AllocExistentialBoxInst));
  for (ProtocolConformance *C : Conformances)
    declareWitnessTable(Mod, C);
  return ::new (Buffer) AllocExistentialBoxInst(Loc,
                                                ExistentialType,
                                                ConcreteType,
                                                ConcreteLoweredType,
                                                Conformances, F);
}

BuiltinInst *BuiltinInst::create(SILDebugLocation *Loc, Identifier Name,
                                 SILType ReturnType,
                                 ArrayRef<Substitution> Substitutions,
                                 ArrayRef<SILValue> Args,
                                 SILFunction &F) {
  void *Buffer = F.getModule().allocate(
                              sizeof(BuiltinInst)
                                + decltype(Operands)::getExtraSize(Args.size())
                                + sizeof(Substitution) * Substitutions.size(),
                              alignof(BuiltinInst));
  return ::new (Buffer) BuiltinInst(Loc, Name, ReturnType, Substitutions,
                                    Args);
}

BuiltinInst::BuiltinInst(SILDebugLocation *Loc, Identifier Name,
                         SILType ReturnType, ArrayRef<Substitution> Subs,
                         ArrayRef<SILValue> Args)
    : SILInstruction(ValueKind::BuiltinInst, Loc, ReturnType), Name(Name),
      NumSubstitutions(Subs.size()), Operands(this, Args) {
  static_assert(IsTriviallyCopyable<Substitution>::value,
                "assuming Substitution is trivially copyable");
  memcpy(getSubstitutionsStorage(), Subs.begin(),
         sizeof(Substitution) * Subs.size());
}

ApplyInst::ApplyInst(SILDebugLocation *Loc, SILValue Callee,
                     SILType SubstCalleeTy, SILType Result,
                     ArrayRef<Substitution> Subs, ArrayRef<SILValue> Args,
                     bool isNonThrowing)
    : ApplyInstBase(ValueKind::ApplyInst, Loc, Callee, SubstCalleeTy, Subs,
                    Args, Result) {
  setNonThrowing(isNonThrowing);
}

ApplyInst *ApplyInst::create(SILDebugLocation *Loc, SILValue Callee,
                             SILType SubstCalleeTy, SILType Result,
                             ArrayRef<Substitution> Subs,
                             ArrayRef<SILValue> Args, bool isNonThrowing,
                             SILFunction &F) {
  void *Buffer = allocate(F, Subs, Args);
  return ::new(Buffer) ApplyInst(Loc, Callee, SubstCalleeTy,
                                 Result, Subs, Args, isNonThrowing);
}

bool swift::doesApplyCalleeHaveSemantics(SILValue callee, StringRef semantics) {
  if (auto *FRI = dyn_cast<FunctionRefInst>(callee))
    if (auto *F = FRI->getReferencedFunction())
      return F->hasSemanticsString(semantics);
  return false;
}

void *swift::allocateApplyInst(SILFunction &F, size_t size, size_t alignment) {
  return F.getModule().allocate(size, alignment);
}

PartialApplyInst::PartialApplyInst(SILDebugLocation *Loc, SILValue Callee,
                                   SILType SubstCalleeTy,
                                   ArrayRef<Substitution> Subs,
                                   ArrayRef<SILValue> Args, SILType ClosureType)
    // FIXME: the callee should have a lowered SIL function type, and
    // PartialApplyInst
    // should derive the type of its result by partially applying the callee's
    // type.
    : ApplyInstBase(ValueKind::PartialApplyInst, Loc, Callee, SubstCalleeTy,
                    Subs, Args, ClosureType) {}

PartialApplyInst *
PartialApplyInst::create(SILDebugLocation *Loc, SILValue Callee,
                         SILType SubstCalleeTy, ArrayRef<Substitution> Subs,
                         ArrayRef<SILValue> Args, SILType ClosureType,
                         SILFunction &F) {
  void *Buffer = allocate(F, Subs, Args);
  return ::new(Buffer) PartialApplyInst(Loc, Callee, SubstCalleeTy,
                                        Subs, Args, ClosureType);
}

TryApplyInstBase::TryApplyInstBase(ValueKind valueKind, SILDebugLocation *Loc,
                                   SILBasicBlock *normalBB,
                                   SILBasicBlock *errorBB)
    : TermInst(valueKind, Loc), DestBBs{{this, normalBB}, {this, errorBB}} {}

TryApplyInst::TryApplyInst(SILDebugLocation *Loc, SILValue callee,
                           SILType substCalleeTy, ArrayRef<Substitution> subs,
                           ArrayRef<SILValue> args, SILBasicBlock *normalBB,
                           SILBasicBlock *errorBB)
    : ApplyInstBase(ValueKind::TryApplyInst, Loc, callee, substCalleeTy, subs,
                    args, normalBB, errorBB) {}

TryApplyInst *TryApplyInst::create(SILDebugLocation *Loc, SILValue callee,
                                   SILType substCalleeTy,
                                   ArrayRef<Substitution> subs,
                                   ArrayRef<SILValue> args,
                                   SILBasicBlock *normalBB,
                                   SILBasicBlock *errorBB, SILFunction &F) {
  void *buffer = allocate(F, subs, args);
  return ::new (buffer)
      TryApplyInst(Loc, callee, substCalleeTy, subs, args, normalBB, errorBB);
}

FunctionRefInst::FunctionRefInst(SILDebugLocation *Loc, SILFunction *F)
    : LiteralInst(ValueKind::FunctionRefInst, Loc, F->getLoweredType()),
      Function(F) {
  F->incrementRefCount();
}

FunctionRefInst::~FunctionRefInst() {
  if (Function)
    Function->decrementRefCount();
}

void FunctionRefInst::dropReferencedFunction() {
  if (Function)
    Function->decrementRefCount();
  Function = nullptr;
}

GlobalAddrInst::GlobalAddrInst(SILDebugLocation *Loc, SILGlobalVariable *Global)
    : LiteralInst(ValueKind::GlobalAddrInst, Loc,
                  Global->getLoweredType().getAddressType()),
      Global(Global) {}

GlobalAddrInst::GlobalAddrInst(SILDebugLocation *Loc, SILType Ty)
    : LiteralInst(ValueKind::GlobalAddrInst, Loc, Ty), Global(nullptr) {}

const IntrinsicInfo &BuiltinInst::getIntrinsicInfo() const {
  return getModule().getIntrinsicInfo(getName());
}

const BuiltinInfo &BuiltinInst::getBuiltinInfo() const {
  return getModule().getBuiltinInfo(getName());
}

static unsigned getWordsForBitWidth(unsigned bits) {
  return (bits + llvm::integerPartWidth - 1)/llvm::integerPartWidth;
}

template<typename INST>
static void *allocateLiteralInstWithTextSize(SILFunction &F, unsigned length) {
  return F.getModule().allocate(sizeof(INST) + length, alignof(INST));
}

template<typename INST>
static void *allocateLiteralInstWithBitSize(SILFunction &F, unsigned bits) {
  unsigned words = getWordsForBitWidth(bits);
  return F.getModule().allocate(sizeof(INST) + sizeof(llvm::integerPart)*words,
                                alignof(INST));
}

IntegerLiteralInst::IntegerLiteralInst(SILDebugLocation *Loc, SILType Ty,
                                       const llvm::APInt &Value)
    : LiteralInst(ValueKind::IntegerLiteralInst, Loc, Ty),
      numBits(Value.getBitWidth()) {
  memcpy(this + 1, Value.getRawData(),
         Value.getNumWords() * sizeof(llvm::integerPart));
}

IntegerLiteralInst *IntegerLiteralInst::create(SILDebugLocation *Loc,
                                               SILType Ty, const APInt &Value,
                                               SILFunction &B) {
  auto intTy = Ty.castTo<BuiltinIntegerType>();
  assert(intTy->getGreatestWidth() == Value.getBitWidth() &&
         "IntegerLiteralInst APInt value's bit width doesn't match type");
  (void)intTy;

  void *buf = allocateLiteralInstWithBitSize<IntegerLiteralInst>(B,
                                                          Value.getBitWidth());
  return ::new (buf) IntegerLiteralInst(Loc, Ty, Value);
}

IntegerLiteralInst *IntegerLiteralInst::create(SILDebugLocation *Loc,
                                               SILType Ty, intmax_t Value,
                                               SILFunction &B) {
  auto intTy = Ty.castTo<BuiltinIntegerType>();
  return create(Loc, Ty,
                APInt(intTy->getGreatestWidth(), Value), B);
}

IntegerLiteralInst *IntegerLiteralInst::create(IntegerLiteralExpr *E,
                                               SILDebugLocation *Loc,
                                               SILFunction &F) {
  return create(
      Loc, SILType::getBuiltinIntegerType(
               E->getType()->castTo<BuiltinIntegerType>()->getGreatestWidth(),
               F.getASTContext()),
      E->getValue(), F);
}

/// getValue - Return the APInt for the underlying integer literal.
APInt IntegerLiteralInst::getValue() const {
  return APInt(numBits,
               {reinterpret_cast<const llvm::integerPart *>(this + 1),
                 getWordsForBitWidth(numBits)});
}

FloatLiteralInst::FloatLiteralInst(SILDebugLocation *Loc, SILType Ty,
                                   const APInt &Bits)
    : LiteralInst(ValueKind::FloatLiteralInst, Loc, Ty),
      numBits(Bits.getBitWidth()) {
  memcpy(this + 1, Bits.getRawData(),
         Bits.getNumWords() * sizeof(llvm::integerPart));
}

FloatLiteralInst *FloatLiteralInst::create(SILDebugLocation *Loc, SILType Ty,
                                           const APFloat &Value,
                                           SILFunction &B) {
  auto floatTy = Ty.castTo<BuiltinFloatType>();
  assert(&floatTy->getAPFloatSemantics() == &Value.getSemantics() &&
         "FloatLiteralInst value's APFloat semantics do not match type");
  (void)floatTy;

  APInt Bits = Value.bitcastToAPInt();

  void *buf = allocateLiteralInstWithBitSize<FloatLiteralInst>(B,
                                                            Bits.getBitWidth());
  return ::new (buf) FloatLiteralInst(Loc, Ty, Bits);
}

FloatLiteralInst *FloatLiteralInst::create(FloatLiteralExpr *E,
                                           SILDebugLocation *Loc,
                                           SILFunction &F) {
  return create(Loc,
                // Builtin floating-point types are always valid SIL types.
                SILType::getBuiltinFloatType(
                    E->getType()->castTo<BuiltinFloatType>()->getFPKind(),
                    F.getASTContext()),
                E->getValue(), F);
}

APInt FloatLiteralInst::getBits() const {
  return APInt(numBits,
               {reinterpret_cast<const llvm::integerPart *>(this + 1),
                 getWordsForBitWidth(numBits)});
}

APFloat FloatLiteralInst::getValue() const {
  return APFloat(getType().castTo<BuiltinFloatType>()->getAPFloatSemantics(),
                 getBits());
}

StringLiteralInst::StringLiteralInst(SILDebugLocation *Loc, StringRef Text,
                                     Encoding encoding, SILType Ty)
    : LiteralInst(ValueKind::StringLiteralInst, Loc, Ty), Length(Text.size()),
      TheEncoding(encoding) {
  memcpy(this + 1, Text.data(), Text.size());
}

StringLiteralInst *StringLiteralInst::create(SILDebugLocation *Loc,
                                             StringRef text, Encoding encoding,
                                             SILFunction &F) {
  void *buf
    = allocateLiteralInstWithTextSize<StringLiteralInst>(F, text.size());

  auto Ty = SILType::getRawPointerType(F.getModule().getASTContext());
  return ::new (buf) StringLiteralInst(Loc, text, encoding, Ty);
}

uint64_t StringLiteralInst::getCodeUnitCount() {
  if (TheEncoding == Encoding::UTF16)
    return unicode::getUTF16Length(getValue());
  return Length;
}

StoreInst::StoreInst(SILDebugLocation *Loc, SILValue Src, SILValue Dest)
    : SILInstruction(ValueKind::StoreInst, Loc), Operands(this, Src, Dest) {}

AssignInst::AssignInst(SILDebugLocation *Loc, SILValue Src, SILValue Dest)
    : SILInstruction(ValueKind::AssignInst, Loc), Operands(this, Src, Dest) {}

MarkFunctionEscapeInst *
MarkFunctionEscapeInst::create(SILDebugLocation *Loc,
                               ArrayRef<SILValue> Elements, SILFunction &F) {
  void *Buffer = F.getModule().allocate(sizeof(MarkFunctionEscapeInst) +
                              decltype(Operands)::getExtraSize(Elements.size()),
                                        alignof(MarkFunctionEscapeInst));
  return ::new(Buffer) MarkFunctionEscapeInst(Loc, Elements);
}

MarkFunctionEscapeInst::MarkFunctionEscapeInst(SILDebugLocation *Loc,
                                               ArrayRef<SILValue> Elems)
    : SILInstruction(ValueKind::MarkFunctionEscapeInst, Loc),
      Operands(this, Elems) {}

static SILType getPinResultType(SILType operandType) {
  return SILType::getPrimitiveObjectType(
    OptionalType::get(operandType.getSwiftRValueType())->getCanonicalType());
}

StrongPinInst::StrongPinInst(SILDebugLocation *Loc, SILValue operand)
    : UnaryInstructionBase(Loc, operand, getPinResultType(operand.getType())) {}

StoreWeakInst::StoreWeakInst(SILDebugLocation *Loc, SILValue value,
                             SILValue dest, IsInitialization_t isInit)
    : SILInstruction(ValueKind::StoreWeakInst, Loc),
      Operands(this, value, dest), IsInitializationOfDest(isInit) {}

CopyAddrInst::CopyAddrInst(SILDebugLocation *Loc, SILValue SrcLValue,
                           SILValue DestLValue, IsTake_t isTakeOfSrc,
                           IsInitialization_t isInitializationOfDest)
    : SILInstruction(ValueKind::CopyAddrInst, Loc), IsTakeOfSrc(isTakeOfSrc),
      IsInitializationOfDest(isInitializationOfDest),
      Operands(this, SrcLValue, DestLValue) {}

UncheckedRefCastAddrInst::UncheckedRefCastAddrInst(SILDebugLocation *Loc,
                                                   SILValue src,
                                                   CanType srcType,
                                                   SILValue dest,
                                                   CanType targetType)
    : SILInstruction(ValueKind::UncheckedRefCastAddrInst, Loc),
      Operands(this, src, dest), SourceType(srcType), TargetType(targetType) {}

UnconditionalCheckedCastAddrInst::UnconditionalCheckedCastAddrInst(
    SILDebugLocation *Loc, CastConsumptionKind consumption, SILValue src,
    CanType srcType, SILValue dest, CanType targetType)
    : SILInstruction(ValueKind::UnconditionalCheckedCastAddrInst, Loc),
      Operands(this, src, dest), ConsumptionKind(consumption),
      SourceType(srcType), TargetType(targetType) {}

StructInst *StructInst::create(SILDebugLocation *Loc, SILType Ty,
                               ArrayRef<SILValue> Elements, SILFunction &F) {
  void *Buffer = F.getModule().allocate(sizeof(StructInst) +
                            decltype(Operands)::getExtraSize(Elements.size()),
                            alignof(StructInst));
  return ::new(Buffer) StructInst(Loc, Ty, Elements);
}

StructInst::StructInst(SILDebugLocation *Loc, SILType Ty,
                       ArrayRef<SILValue> Elems)
    : SILInstruction(ValueKind::StructInst, Loc, Ty), Operands(this, Elems) {
  assert(!Ty.getStructOrBoundGenericStruct()->hasUnreferenceableStorage());
}

TupleInst *TupleInst::create(SILDebugLocation *Loc, SILType Ty,
                             ArrayRef<SILValue> Elements, SILFunction &F) {
  void *Buffer = F.getModule().allocate(sizeof(TupleInst) +
                            decltype(Operands)::getExtraSize(Elements.size()),
                            alignof(TupleInst));
  return ::new(Buffer) TupleInst(Loc, Ty, Elements);
}

TupleInst::TupleInst(SILDebugLocation *Loc, SILType Ty,
                     ArrayRef<SILValue> Elems)
    : SILInstruction(ValueKind::TupleInst, Loc, Ty), Operands(this, Elems) {}

MetatypeInst::MetatypeInst(SILDebugLocation *Loc, SILType Metatype)
    : SILInstruction(ValueKind::MetatypeInst, Loc, Metatype) {}

bool TupleExtractInst::isTrivialEltOfOneRCIDTuple() const {
  SILModule &Mod = getModule();

  // If we are not trivial, bail.
  if (!getType().isTrivial(Mod))
    return false;

  // If the elt we are extracting is trivial, we can not have any non trivial
  // fields.
  if (getOperand().getType().isTrivial(Mod))
    return false;

  // Ok, now we know that our tuple has non-trivial fields. Make sure that our
  // parent tuple has only one non-trivial field.
  bool FoundNonTrivialField = false;
  SILType OpTy = getOperand().getType();
  unsigned FieldNo = getFieldNo();

  // For each element index of the tuple...
  for (unsigned i = 0, e = getNumTupleElts(); i != e; ++i) {
    // If the element index is the one we are extracting, skip it...
    if (i == FieldNo)
      continue;

    // Otherwise check if we have a non-trivial type. If we don't have one,
    // continue.
    if (OpTy.getTupleElementType(i).isTrivial(Mod))
      continue;

    // Ok, this type is non-trivial. If we have not seen a non-trivial field
    // yet, set the FoundNonTrivialField flag.
    if (!FoundNonTrivialField) {
      FoundNonTrivialField = true;
      continue;
    }

    // If we have seen a field and thus the FoundNonTrivialField flag is set,
    // return false.
    return false;
  }

  // We found only one trivial field.
  assert(FoundNonTrivialField && "Tuple is non-trivial, but does not have a "
                                 "non-trivial element?!");
  return true;
}

bool TupleExtractInst::isEltOnlyNonTrivialElt() const {
  SILModule &Mod = getModule();

  // If the elt we are extracting is trivial, we can not be a non-trivial
  // field... return false.
  if (getType().isTrivial(Mod))
    return false;

  // Ok, we know that the elt we are extracting is non-trivial. Make sure that
  // we have no other non-trivial elts.
  SILType OpTy = getOperand().getType();
  unsigned FieldNo = getFieldNo();

  // For each element index of the tuple...
  for (unsigned i = 0, e = getNumTupleElts(); i != e; ++i) {
    // If the element index is the one we are extracting, skip it...
    if (i == FieldNo)
      continue;

    // Otherwise check if we have a non-trivial type. If we don't have one,
    // continue.
    if (OpTy.getTupleElementType(i).isTrivial(Mod))
      continue;

    // If we do have a non-trivial type, return false. We have multiple
    // non-trivial types violating our condition.
    return false;
  }

  // We checked every other elt of the tuple and did not find any
  // non-trivial elt except for ourselves. Return true.
  return true;
}

bool StructExtractInst::isTrivialFieldOfOneRCIDStruct() const {
  SILModule &Mod = getModule();

  // If we are not trivial, bail.
  if (!getType().isTrivial(Mod))
    return false;

  SILType StructTy = getOperand().getType();

  // If the elt we are extracting is trivial, we can not have any non trivial
  // fields.
  if (StructTy.isTrivial(Mod))
    return false;

  // Ok, now we know that our tuple has non-trivial fields. Make sure that our
  // parent tuple has only one non-trivial field.
  bool FoundNonTrivialField = false;

  // For each element index of the tuple...
  for (VarDecl *D : getStructDecl()->getStoredProperties()) {
    // If the field is the one we are extracting, skip it...
    if (Field == D)
      continue;

    // Otherwise check if we have a non-trivial type. If we don't have one,
    // continue.
    if (StructTy.getFieldType(D, Mod).isTrivial(Mod))
      continue;

    // Ok, this type is non-trivial. If we have not seen a non-trivial field
    // yet, set the FoundNonTrivialField flag.
    if (!FoundNonTrivialField) {
      FoundNonTrivialField = true;
      continue;
    }

    // If we have seen a field and thus the FoundNonTrivialField flag is set,
    // return false.
    return false;
  }

  // We found only one trivial field.
  assert(FoundNonTrivialField && "Struct is non-trivial, but does not have a "
                                 "non-trivial field?!");
  return true;
}

/// Return true if we are extracting the only non-trivial field of out parent
/// struct. This implies that a ref count operation on the aggregate is
/// equivalent to a ref count operation on this field.
bool StructExtractInst::isFieldOnlyNonTrivialField() const {
  SILModule &Mod = getModule();

  // If the field we are extracting is trivial, we can not be a non-trivial
  // field... return false.
  if (getType().isTrivial(Mod))
    return false;

  SILType StructTy = getOperand().getType();

  // Ok, we are visiting a non-trivial field. Then for every stored field...
  for (VarDecl *D : getStructDecl()->getStoredProperties()) {
    // If we are visiting our own field continue.
    if (Field == D)
      continue;

    // Ok, we have a field that is not equal to the field we are
    // extracting. If that field is trivial, we do not care about
    // it... continue.
    if (StructTy.getFieldType(D, Mod).isTrivial(Mod))
      continue;

    // We have found a non trivial member that is not the member we are
    // extracting, fail.
    return false;
  }

  // We checked every other field of the struct and did not find any
  // non-trivial fields except for ourselves. Return true.
  return true;
}

//===----------------------------------------------------------------------===//
// Instructions representing terminators
//===----------------------------------------------------------------------===//


TermInst::SuccessorListTy TermInst::getSuccessors() {
  #define TERMINATOR(TYPE, PARENT, EFFECT, RELEASING) \
    if (auto I = dyn_cast<TYPE>(this)) \
      return I->getSuccessors();
  #include "swift/SIL/SILNodes.def"

  llvm_unreachable("not a terminator?!");
}

BranchInst::BranchInst(SILDebugLocation *Loc, SILBasicBlock *DestBB,
                       ArrayRef<SILValue> Args)
    : TermInst(ValueKind::BranchInst, Loc), DestBB(this, DestBB),
      Operands(this, Args) {}

BranchInst *BranchInst::create(SILDebugLocation *Loc, SILBasicBlock *DestBB,
                               SILFunction &F) {
  return create(Loc, DestBB, {}, F);
}

BranchInst *BranchInst::create(SILDebugLocation *Loc,
                               SILBasicBlock *DestBB, ArrayRef<SILValue> Args,
                               SILFunction &F) {
  void *Buffer = F.getModule().allocate(sizeof(BranchInst) +
                              decltype(Operands)::getExtraSize(Args.size()),
                            alignof(BranchInst));
  return ::new (Buffer) BranchInst(Loc, DestBB, Args);
}

CondBranchInst::CondBranchInst(SILDebugLocation *Loc, SILValue Condition,
                               SILBasicBlock *TrueBB, SILBasicBlock *FalseBB,
                               ArrayRef<SILValue> Args, unsigned NumTrue,
                               unsigned NumFalse)
    : TermInst(ValueKind::CondBranchInst, Loc),
      DestBBs{{this, TrueBB}, {this, FalseBB}}, NumTrueArgs(NumTrue),
      NumFalseArgs(NumFalse), Operands(this, Args, Condition) {
  assert(Args.size() == (NumTrueArgs + NumFalseArgs) &&
         "Invalid number of args");
  assert(TrueBB != FalseBB && "Identical destinations");
}

CondBranchInst *CondBranchInst::create(SILDebugLocation *Loc,
                                       SILValue Condition,
                                       SILBasicBlock *TrueBB,
                                       SILBasicBlock *FalseBB, SILFunction &F) {
  return create(Loc, Condition, TrueBB, {}, FalseBB, {}, F);
}

CondBranchInst *
CondBranchInst::create(SILDebugLocation *Loc, SILValue Condition,
                       SILBasicBlock *TrueBB, ArrayRef<SILValue> TrueArgs,
                       SILBasicBlock *FalseBB, ArrayRef<SILValue> FalseArgs,
                       SILFunction &F) {
  SmallVector<SILValue, 4> Args;
  Args.append(TrueArgs.begin(), TrueArgs.end());
  Args.append(FalseArgs.begin(), FalseArgs.end());

  void *Buffer = F.getModule().allocate(sizeof(CondBranchInst) +
                              decltype(Operands)::getExtraSize(Args.size()),
                            alignof(CondBranchInst));
  return ::new (Buffer) CondBranchInst(Loc, Condition, TrueBB, FalseBB, Args,
                                       TrueArgs.size(), FalseArgs.size());
}

OperandValueArrayRef CondBranchInst::getTrueArgs() const {
  return Operands.asValueArray().slice(1, NumTrueArgs);
}

OperandValueArrayRef CondBranchInst::getFalseArgs() const {
  return Operands.asValueArray().slice(1 + NumTrueArgs, NumFalseArgs);
}

SILValue
CondBranchInst::getArgForDestBB(SILBasicBlock *DestBB, SILArgument *A) {
  // If TrueBB and FalseBB equal, we can not find an arg for this DestBB so
  // return an empty SILValue.
  if (getTrueBB() == getFalseBB()) {
    assert(DestBB == getTrueBB() && "DestBB is not a target of this cond_br");
    return SILValue();
  }

  unsigned i = A->getIndex();

  if (DestBB == getTrueBB())
    return Operands[1 + i].get();

  assert(DestBB == getFalseBB()
         && "By process of elimination BB must be false BB");
  return Operands[1 + NumTrueArgs + i].get();
}

ArrayRef<Operand> CondBranchInst::getTrueOperands() const {
  if (NumTrueArgs == 0)
    return ArrayRef<Operand>();
  return ArrayRef<Operand>(&Operands[1], NumTrueArgs);
}

MutableArrayRef<Operand> CondBranchInst::getTrueOperands() {
  if (NumTrueArgs == 0)
    return MutableArrayRef<Operand>();
  return MutableArrayRef<Operand>(&Operands[1], NumTrueArgs);
}

ArrayRef<Operand> CondBranchInst::getFalseOperands() const {
  if (NumFalseArgs == 0)
    return ArrayRef<Operand>();
  return ArrayRef<Operand>(&Operands[1+NumTrueArgs], NumFalseArgs);
}

MutableArrayRef<Operand> CondBranchInst::getFalseOperands() {
  if (NumFalseArgs == 0)
    return MutableArrayRef<Operand>();
  return MutableArrayRef<Operand>(&Operands[1+NumTrueArgs], NumFalseArgs);
}

void CondBranchInst::swapSuccessors() {
  // Swap our destinations.
  SILBasicBlock *First = DestBBs[0].getBB();
  DestBBs[0] = DestBBs[1].getBB();
  DestBBs[1] = First;

  // If we don't have any arguments return.
  if (!NumTrueArgs && !NumFalseArgs)
    return;

  // Otherwise swap our true and false arguments.
  MutableArrayRef<Operand> Ops = getAllOperands();
  llvm::SmallVector<SILValue, 4> TrueOps;
  for (SILValue V : getTrueArgs())
    TrueOps.push_back(V);

  auto FalseArgs = getFalseArgs();
  for (unsigned i = 0, e = NumFalseArgs; i < e; ++i) {
    Ops[1+i].set(FalseArgs[i]);
  }

  for (unsigned i = 0, e = NumTrueArgs; i < e; ++i) {
    Ops[1+i+NumFalseArgs].set(TrueOps[i]);
  }

  // Finally swap the number of arguments that we have.
  std::swap(NumTrueArgs, NumFalseArgs);
}

SwitchValueInst::SwitchValueInst(SILDebugLocation *Loc, SILValue Operand,
                                 SILBasicBlock *DefaultBB,
                                 ArrayRef<SILValue> Cases,
                                 ArrayRef<SILBasicBlock *> BBs)
    : TermInst(ValueKind::SwitchValueInst, Loc), NumCases(Cases.size()),
      HasDefault(bool(DefaultBB)), Operands(this, Cases, Operand) {

  // Initialize the successor array.
  auto *succs = getSuccessorBuf();
  unsigned OperandBitWidth = 0;

  if (auto OperandTy = Operand.getType().getAs<BuiltinIntegerType>()) {
    OperandBitWidth = OperandTy->getGreatestWidth();
  }

  for (unsigned i = 0, size = Cases.size(); i < size; ++i) {
    if (OperandBitWidth) {
      auto *IL = dyn_cast<IntegerLiteralInst>(Cases[i]);
      assert(IL && "switch_value case value should be of an integer type");
      assert(IL->getValue().getBitWidth() == OperandBitWidth &&
             "switch_value case value is not same bit width as operand");
      (void)IL;
    } else {
      auto *FR = dyn_cast<FunctionRefInst>(Cases[i]);
      if (!FR) {
        if (auto *CF = dyn_cast<ConvertFunctionInst>(Cases[i])) {
          FR = dyn_cast<FunctionRefInst>(CF->getOperand());
        }
      }
      assert(FR && "switch_value case value should be a function reference");
    }
    ::new (succs + i) SILSuccessor(this, BBs[i]);
  }

  if (HasDefault)
    ::new (succs + NumCases) SILSuccessor(this, DefaultBB);
}

SwitchValueInst::~SwitchValueInst() {
  // Destroy the successor records to keep the CFG up to date.
  auto *succs = getSuccessorBuf();
  for (unsigned i = 0, end = NumCases + HasDefault; i < end; ++i) {
    succs[i].~SILSuccessor();
  }
}

SwitchValueInst *SwitchValueInst::create(
    SILDebugLocation *Loc, SILValue Operand, SILBasicBlock *DefaultBB,
    ArrayRef<std::pair<SILValue, SILBasicBlock *>> CaseBBs, SILFunction &F) {
  // Allocate enough room for the instruction with tail-allocated data for all
  // the case values and the SILSuccessor arrays. There are `CaseBBs.size()`
  // SILValues and `CaseBBs.size() + (DefaultBB ? 1 : 0)` successors.
  SmallVector<SILValue, 8> Cases;
  SmallVector<SILBasicBlock *, 8> BBs;
  unsigned numCases = CaseBBs.size();
  unsigned numSuccessors = numCases + (DefaultBB ? 1 : 0);
  for(auto pair: CaseBBs) {
    Cases.push_back(pair.first);
    BBs.push_back(pair.second);
  }
  size_t bufSize = sizeof(SwitchValueInst) +
                   decltype(Operands)::getExtraSize(Cases.size()) +
                   sizeof(SILSuccessor) * numSuccessors;
  void *buf = F.getModule().allocate(bufSize, alignof(SwitchValueInst));
  return ::new (buf) SwitchValueInst(Loc, Operand, DefaultBB, Cases, BBs);
}

SelectValueInst::SelectValueInst(SILDebugLocation *Loc, SILValue Operand,
                                 SILType Type, SILValue DefaultResult,
                                 ArrayRef<SILValue> CaseValuesAndResults)
    : SelectInstBase(ValueKind::SelectValueInst, Loc, Type,
                     CaseValuesAndResults.size() / 2, bool(DefaultResult),
                     CaseValuesAndResults, Operand) {

  unsigned OperandBitWidth = 0;

  if (auto OperandTy = Operand.getType().getAs<BuiltinIntegerType>()) {
    OperandBitWidth = OperandTy->getGreatestWidth();
  }

  for (unsigned i = 0; i < NumCases; ++i) {
    auto *IL = dyn_cast<IntegerLiteralInst>(CaseValuesAndResults[i * 2]);
    assert(IL && "select_value case value should be of an integer type");
    assert(IL->getValue().getBitWidth() == OperandBitWidth &&
           "select_value case value is not same bit width as operand");
    (void)IL;
  }
}

SelectValueInst::~SelectValueInst() {
}

SelectValueInst *
SelectValueInst::create(SILDebugLocation *Loc, SILValue Operand, SILType Type,
                        SILValue DefaultResult,
                        ArrayRef<std::pair<SILValue, SILValue>> CaseValues,
                        SILFunction &F) {
  // Allocate enough room for the instruction with tail-allocated data for all
  // the case values and the SILSuccessor arrays. There are `CaseBBs.size()`
  // SILValuues and `CaseBBs.size() + (DefaultBB ? 1 : 0)` successors.
  SmallVector<SILValue, 8> CaseValuesAndResults;
  for (auto pair : CaseValues) {
    CaseValuesAndResults.push_back(pair.first);
    CaseValuesAndResults.push_back(pair.second);
  }

  if ((bool)DefaultResult)
    CaseValuesAndResults.push_back(DefaultResult);

  size_t bufSize = sizeof(SelectValueInst) + decltype(Operands)::getExtraSize(
                                               CaseValuesAndResults.size());
  void *buf = F.getModule().allocate(bufSize, alignof(SelectValueInst));
  return ::new (buf)
      SelectValueInst(Loc, Operand, Type, DefaultResult, CaseValuesAndResults);
}

static SmallVector<SILValue, 4>
getCaseOperands(ArrayRef<std::pair<EnumElementDecl*, SILValue>> CaseValues,
                SILValue DefaultValue) {
  SmallVector<SILValue, 4> result;

  for (auto &pair : CaseValues)
    result.push_back(pair.second);
  if (DefaultValue)
    result.push_back(DefaultValue);

  return result;
}

SelectEnumInstBase::SelectEnumInstBase(
    ValueKind Kind, SILDebugLocation *Loc, SILValue Operand, SILType Ty,
    SILValue DefaultValue,
    ArrayRef<std::pair<EnumElementDecl *, SILValue>> CaseValues)
    : SelectInstBase(Kind, Loc, Ty, CaseValues.size(), bool(DefaultValue),
                     getCaseOperands(CaseValues, DefaultValue), Operand) {
  // Initialize the case and successor arrays.
  auto *cases = getCaseBuf();
  for (unsigned i = 0, size = CaseValues.size(); i < size; ++i) {
    cases[i] = CaseValues[i].first;
  }
}

template <typename SELECT_ENUM_INST>
SELECT_ENUM_INST *SelectEnumInstBase::createSelectEnum(
    SILDebugLocation *Loc, SILValue Operand, SILType Ty, SILValue DefaultValue,
    ArrayRef<std::pair<EnumElementDecl *, SILValue>> CaseValues,
    SILFunction &F) {
  // Allocate enough room for the instruction with tail-allocated
  // EnumElementDecl and operand arrays. There are `CaseBBs.size()` decls
  // and `CaseBBs.size() + (DefaultBB ? 1 : 0)` values.
  unsigned numCases = CaseValues.size();

  void *buf = F.getModule().allocate(
    sizeof(SELECT_ENUM_INST) + sizeof(EnumElementDecl*) * numCases
     + TailAllocatedOperandList<1>::getExtraSize(numCases + (bool)DefaultValue),
    alignof(SELECT_ENUM_INST));
  return ::new (buf) SELECT_ENUM_INST(Loc,Operand,Ty,DefaultValue,CaseValues);
}

SelectEnumInst *SelectEnumInst::create(
    SILDebugLocation *Loc, SILValue Operand, SILType Type,
    SILValue DefaultValue,
    ArrayRef<std::pair<EnumElementDecl *, SILValue>> CaseValues,
    SILFunction &F) {
  return createSelectEnum<SelectEnumInst>(Loc, Operand, Type, DefaultValue,
                                          CaseValues, F);
}

SelectEnumAddrInst *SelectEnumAddrInst::create(
    SILDebugLocation *Loc, SILValue Operand, SILType Type,
    SILValue DefaultValue,
    ArrayRef<std::pair<EnumElementDecl *, SILValue>> CaseValues,
    SILFunction &F) {
  return createSelectEnum<SelectEnumAddrInst>(Loc, Operand, Type, DefaultValue,
                                              CaseValues, F);
}

SwitchEnumInstBase::SwitchEnumInstBase(
    ValueKind Kind, SILDebugLocation *Loc, SILValue Operand,
    SILBasicBlock *DefaultBB,
    ArrayRef<std::pair<EnumElementDecl *, SILBasicBlock *>> CaseBBs)
    : TermInst(Kind, Loc), Operands(this, Operand), NumCases(CaseBBs.size()),
      HasDefault(bool(DefaultBB)) {
  // Initialize the case and successor arrays.
  auto *cases = getCaseBuf();
  auto *succs = getSuccessorBuf();
  for (unsigned i = 0, size = CaseBBs.size(); i < size; ++i) {
    cases[i] = CaseBBs[i].first;
    ::new (succs + i) SILSuccessor(this, CaseBBs[i].second);
  }

  if (HasDefault)
    ::new (succs + NumCases) SILSuccessor(this, DefaultBB);
}

namespace {
  template <class Inst> EnumElementDecl *
  getUniqueCaseForDefaultValue(Inst *inst, SILValue enumValue) {
    assert(inst->hasDefault() && "doesn't have a default");
    SILType enumType = enumValue.getType();

    if (!enumType.hasFixedLayout(inst->getModule()))
      return nullptr;

    EnumDecl *decl = enumType.getEnumOrBoundGenericEnum();
    assert(decl && "switch_enum operand is not an enum");

    llvm::SmallPtrSet<EnumElementDecl *, 4> unswitchedElts;
    for (auto elt : decl->getAllElements())
      unswitchedElts.insert(elt);

    for (unsigned i = 0, e = inst->getNumCases(); i != e; ++i) {
      auto Entry = inst->getCase(i);
      unswitchedElts.erase(Entry.first);
    }

    if (unswitchedElts.size() == 1)
      return *unswitchedElts.begin();

    return nullptr;
  }
}

NullablePtr<EnumElementDecl> SelectEnumInstBase::getUniqueCaseForDefault() {
  return getUniqueCaseForDefaultValue(this, getEnumOperand());
}

NullablePtr<EnumElementDecl> SelectEnumInstBase::getSingleTrueElement() const {
  auto SEIType = getType().getAs<BuiltinIntegerType>();
  if (!SEIType)
    return nullptr;
  if (SEIType->getWidth() != BuiltinIntegerWidth::fixed(1))
    return nullptr;

  // Try to find a single literal "true" case.
  Optional<EnumElementDecl*> TrueElement;
  for (unsigned i = 0, e = getNumCases(); i < e; ++i) {
    auto casePair = getCase(i);
    if (auto intLit = dyn_cast<IntegerLiteralInst>(casePair.second)) {
      if (intLit->getValue() == APInt(1, 1)) {
        if (!TrueElement)
          TrueElement = casePair.first;
        else
          // Use Optional(nullptr) to represent more than one.
          TrueElement = Optional<EnumElementDecl*>(nullptr);
      }
    }
  }

  if (!TrueElement || !*TrueElement)
    return nullptr;
  return *TrueElement;
}

SwitchEnumInstBase::~SwitchEnumInstBase() {
  // Destroy the successor records to keep the CFG up to date.
  auto *succs = getSuccessorBuf();
  for (unsigned i = 0, end = NumCases + HasDefault; i < end; ++i) {
    succs[i].~SILSuccessor();
  }
}

template <typename SWITCH_ENUM_INST>
SWITCH_ENUM_INST *SwitchEnumInstBase::createSwitchEnum(
    SILDebugLocation *Loc, SILValue Operand, SILBasicBlock *DefaultBB,
    ArrayRef<std::pair<EnumElementDecl *, SILBasicBlock *>> CaseBBs,
    SILFunction &F) {
  // Allocate enough room for the instruction with tail-allocated
  // EnumElementDecl and SILSuccessor arrays. There are `CaseBBs.size()` decls
  // and `CaseBBs.size() + (DefaultBB ? 1 : 0)` successors.
  unsigned numCases = CaseBBs.size();
  unsigned numSuccessors = numCases + (DefaultBB ? 1 : 0);

  void *buf = F.getModule().allocate(sizeof(SWITCH_ENUM_INST)
                                       + sizeof(EnumElementDecl*) * numCases
                                       + sizeof(SILSuccessor) * numSuccessors,
                                     alignof(SWITCH_ENUM_INST));
  return ::new (buf) SWITCH_ENUM_INST(Loc, Operand, DefaultBB, CaseBBs);
}

NullablePtr<EnumElementDecl> SwitchEnumInstBase::getUniqueCaseForDefault() {
  return getUniqueCaseForDefaultValue(this, getOperand());
}

NullablePtr<EnumElementDecl>
SwitchEnumInstBase::getUniqueCaseForDestination(SILBasicBlock *BB) {
  SILValue value = getOperand();
  SILType enumType = value.getType();
  EnumDecl *decl = enumType.getEnumOrBoundGenericEnum();
  assert(decl && "switch_enum operand is not an enum");
  (void)decl;

  EnumElementDecl *D = nullptr;
  for (unsigned i = 0, e = getNumCases(); i != e; ++i) {
    auto Entry = getCase(i);
    if (Entry.second == BB) {
      if (D != nullptr)
        return nullptr;
      D = Entry.first;
    }
  }
  if (!D && hasDefault() && getDefaultBB() == BB) {
    return getUniqueCaseForDefault();
  }
  return D;
}

SwitchEnumInst *SwitchEnumInst::create(
    SILDebugLocation *Loc, SILValue Operand, SILBasicBlock *DefaultBB,
    ArrayRef<std::pair<EnumElementDecl *, SILBasicBlock *>> CaseBBs,
    SILFunction &F) {
  return
    createSwitchEnum<SwitchEnumInst>(Loc, Operand, DefaultBB, CaseBBs, F);
}

SwitchEnumAddrInst *SwitchEnumAddrInst::create(
    SILDebugLocation *Loc, SILValue Operand, SILBasicBlock *DefaultBB,
    ArrayRef<std::pair<EnumElementDecl *, SILBasicBlock *>> CaseBBs,
    SILFunction &F) {
  return createSwitchEnum<SwitchEnumAddrInst>
    (Loc, Operand, DefaultBB, CaseBBs, F);
}

DynamicMethodBranchInst::DynamicMethodBranchInst(SILDebugLocation *Loc,
                                                 SILValue Operand,
                                                 SILDeclRef Member,
                                                 SILBasicBlock *HasMethodBB,
                                                 SILBasicBlock *NoMethodBB)
  : TermInst(ValueKind::DynamicMethodBranchInst, Loc),
    Member(Member),
    DestBBs{{this, HasMethodBB}, {this, NoMethodBB}},
    Operands(this, Operand)
{
}

DynamicMethodBranchInst *
DynamicMethodBranchInst::create(SILDebugLocation *Loc, SILValue Operand,
                                SILDeclRef Member, SILBasicBlock *HasMethodBB,
                                SILBasicBlock *NoMethodBB, SILFunction &F) {
  void *Buffer = F.getModule().allocate(sizeof(DynamicMethodBranchInst),
                                        alignof(DynamicMethodBranchInst));
  return ::new (Buffer)
      DynamicMethodBranchInst(Loc, Operand, Member, HasMethodBB, NoMethodBB);
}

SILLinkage
TypeConverter::getLinkageForProtocolConformance(const NormalProtocolConformance *C,
                                                ForDefinition_t definition) {
  // If the conformance is imported from Clang, give it shared linkage.
  auto typeDecl = C->getType()->getNominalOrBoundGenericNominal();
  auto typeUnit = typeDecl->getModuleScopeContext();
  if (isa<ClangModuleUnit>(typeUnit)
      && C->getDeclContext()->getParentModule() == typeUnit->getParentModule())
    return SILLinkage::Shared;

  // FIXME: This should be using std::min(protocol's access, type's access).
  switch (C->getProtocol()->getEffectiveAccess()) {
    case Accessibility::Private:
      return (definition ? SILLinkage::Private : SILLinkage::PrivateExternal);

    case Accessibility::Internal:
      return (definition ? SILLinkage::Hidden : SILLinkage::HiddenExternal);

    default:
      return (definition ? SILLinkage::Public : SILLinkage::PublicExternal);
  }
}

/// Create a witness method, creating a witness table declaration if we don't
/// have a witness table for it. Later on if someone wants the real definition,
/// lookUpWitnessTable will deserialize it for us if we can.
///
/// This is following the same model of how we deal with SILFunctions in
/// function_ref. There we always just create a declaration and then later
/// deserialize the actual function definition if we need to.
WitnessMethodInst *
WitnessMethodInst::create(SILDebugLocation *Loc, CanType LookupType,
                          ProtocolConformance *Conformance, SILDeclRef Member,
                          SILType Ty, SILFunction *F,
                          SILValue OpenedExistential, bool Volatile) {
  SILModule &Mod = F->getModule();
  void *Buffer =
      Mod.allocate(sizeof(WitnessMethodInst), alignof(WitnessMethodInst));

  declareWitnessTable(Mod, Conformance);
  return ::new (Buffer) WitnessMethodInst(Loc, LookupType, Conformance, Member,
                                          Ty, OpenedExistential, Volatile);
}

InitExistentialAddrInst *InitExistentialAddrInst::create(
    SILDebugLocation *Loc, SILValue Existential, CanType ConcreteType,
    SILType ConcreteLoweredType, ArrayRef<ProtocolConformance *> Conformances,
    SILFunction *F) {
  SILModule &Mod = F->getModule();
  void *Buffer = Mod.allocate(sizeof(InitExistentialAddrInst),
                              alignof(InitExistentialAddrInst));
  for (ProtocolConformance *C : Conformances)
    declareWitnessTable(Mod, C);
  return ::new (Buffer) InitExistentialAddrInst(Loc, Existential,
                                            ConcreteType,
                                            ConcreteLoweredType,
                                            Conformances);
}

InitExistentialRefInst *
InitExistentialRefInst::create(SILDebugLocation *Loc, SILType ExistentialType,
                               CanType ConcreteType, SILValue Instance,
                               ArrayRef<ProtocolConformance *> Conformances,
                               SILFunction *F) {
  SILModule &Mod = F->getModule();
  void *Buffer = Mod.allocate(sizeof(InitExistentialRefInst),
                              alignof(InitExistentialRefInst));
  for (ProtocolConformance *C : Conformances) {
    if (!C)
      continue;
    if (!Mod.lookUpWitnessTable(C, false).first)
      declareWitnessTable(Mod, C);
  }

  return ::new (Buffer) InitExistentialRefInst(Loc, ExistentialType,
                                               ConcreteType,
                                               Instance,
                                               Conformances);
}

InitExistentialMetatypeInst::InitExistentialMetatypeInst(
    SILDebugLocation *Loc, SILType existentialMetatypeType, SILValue metatype,
    ArrayRef<ProtocolConformance *> conformances)
    : UnaryInstructionBase(Loc, metatype, existentialMetatypeType),
      LastConformance(nullptr) {
  if (conformances.empty())
    return;
  auto **offset = reinterpret_cast<ProtocolConformance **>(this + 1);
  memcpy(offset, &conformances[0],
         conformances.size() * sizeof(ProtocolConformance *));
  LastConformance = &offset[conformances.size() - 1];
}

InitExistentialMetatypeInst *InitExistentialMetatypeInst::create(
    SILDebugLocation *Loc, SILType existentialMetatypeType, SILValue metatype,
    ArrayRef<ProtocolConformance *> conformances, SILFunction *F) {
  SILModule &M = F->getModule();
  unsigned size = sizeof(InitExistentialMetatypeInst);
  size += conformances.size() * sizeof(ProtocolConformance *);

  void *buffer = M.allocate(size, alignof(InitExistentialMetatypeInst));
  for (ProtocolConformance *conformance : conformances)
    if (!M.lookUpWitnessTable(conformance, false).first)
      declareWitnessTable(M, conformance);

  return ::new (buffer) InitExistentialMetatypeInst(
      Loc, existentialMetatypeType, metatype, conformances);
}

ArrayRef<ProtocolConformance *>
InitExistentialMetatypeInst::getConformances() const {
  if (!LastConformance)
    return ArrayRef<ProtocolConformance *>();
  // The first conformance is going to be at *this[1];
  auto **FirstConformance = reinterpret_cast<ProtocolConformance **>(
      const_cast<InitExistentialMetatypeInst *>(this) + 1);
  // Construct the protocol conformance list from the range of our conformances.
  return ArrayRef<ProtocolConformance *>(FirstConformance,
                                         LastConformance.get() + 1);
}
