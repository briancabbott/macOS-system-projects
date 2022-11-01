//===-- llvm/Attributes.h - Container for Attributes ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the simple types necessary to represent the
// attributes associated with functions and their calls.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ATTRIBUTES_H
#define LLVM_ATTRIBUTES_H

#include "llvm/Support/MathExtras.h"
#include "llvm/ADT/ArrayRef.h"
#include <cassert>
#include <string>

namespace llvm {

class LLVMContext;
class Type;

namespace Attribute {

/// We use this proxy POD type to allow constructing Attributes constants using
/// initializer lists. Do not use this class directly.
struct AttrConst {
  uint64_t v;
  AttrConst operator | (const AttrConst Attrs) const {
    AttrConst Res = {v | Attrs.v};
    return Res;
  }
  AttrConst operator ~ () const {
    AttrConst Res = {~v};
    return Res;
  }
};

/// Function parameters and results can have attributes to indicate how they
/// should be treated by optimizations and code generation. This enumeration
/// lists the attributes that can be associated with parameters, function
/// results or the function itself.
/// @brief Function attributes.

/// We declare AttrConst objects that will be used throughout the code and also
/// raw uint64_t objects with _i suffix to be used below for other constant
/// declarations. This is done to avoid static CTORs and at the same time to
/// keep type-safety of Attributes.
#define DECLARE_LLVM_ATTRIBUTE(name, value) \
  const uint64_t name##_i = value; \
  const AttrConst name = {value};

DECLARE_LLVM_ATTRIBUTE(None,0)    ///< No attributes have been set
DECLARE_LLVM_ATTRIBUTE(ZExt,1<<0) ///< Zero extended before/after call
DECLARE_LLVM_ATTRIBUTE(SExt,1<<1) ///< Sign extended before/after call
DECLARE_LLVM_ATTRIBUTE(NoReturn,1<<2) ///< Mark the function as not returning
DECLARE_LLVM_ATTRIBUTE(InReg,1<<3) ///< Force argument to be passed in register
DECLARE_LLVM_ATTRIBUTE(StructRet,1<<4) ///< Hidden pointer to structure to return
DECLARE_LLVM_ATTRIBUTE(NoUnwind,1<<5) ///< Function doesn't unwind stack
DECLARE_LLVM_ATTRIBUTE(NoAlias,1<<6) ///< Considered to not alias after call
DECLARE_LLVM_ATTRIBUTE(ByVal,1<<7) ///< Pass structure by value
DECLARE_LLVM_ATTRIBUTE(Nest,1<<8) ///< Nested function static chain
DECLARE_LLVM_ATTRIBUTE(ReadNone,1<<9) ///< Function does not access memory
DECLARE_LLVM_ATTRIBUTE(ReadOnly,1<<10) ///< Function only reads from memory
DECLARE_LLVM_ATTRIBUTE(NoInline,1<<11) ///< inline=never
DECLARE_LLVM_ATTRIBUTE(AlwaysInline,1<<12) ///< inline=always
DECLARE_LLVM_ATTRIBUTE(OptimizeForSize,1<<13) ///< opt_size
DECLARE_LLVM_ATTRIBUTE(StackProtect,1<<14) ///< Stack protection.
DECLARE_LLVM_ATTRIBUTE(StackProtectReq,1<<15) ///< Stack protection required.
DECLARE_LLVM_ATTRIBUTE(Alignment,31<<16) ///< Alignment of parameter (5 bits)
                                     // stored as log2 of alignment with +1 bias
                                     // 0 means unaligned different from align 1
DECLARE_LLVM_ATTRIBUTE(NoCapture,1<<21) ///< Function creates no aliases of pointer
DECLARE_LLVM_ATTRIBUTE(NoRedZone,1<<22) /// disable redzone
DECLARE_LLVM_ATTRIBUTE(NoImplicitFloat,1<<23) /// disable implicit floating point
                                           /// instructions.
DECLARE_LLVM_ATTRIBUTE(Naked,1<<24) ///< Naked function
DECLARE_LLVM_ATTRIBUTE(InlineHint,1<<25) ///< source said inlining was
                                           ///desirable
DECLARE_LLVM_ATTRIBUTE(StackAlignment,7<<26) ///< Alignment of stack for
                                           ///function (3 bits) stored as log2
                                           ///of alignment with +1 bias
                                           ///0 means unaligned (different from
                                           ///alignstack= {1))
DECLARE_LLVM_ATTRIBUTE(ReturnsTwice,1<<29) ///< Function can return twice
DECLARE_LLVM_ATTRIBUTE(UWTable,1<<30) ///< Function must be in a unwind
                                           ///table
DECLARE_LLVM_ATTRIBUTE(NonLazyBind,1U<<31) ///< Function is called early and/or
                                            /// often, so lazy binding isn't
                                            /// worthwhile.
DECLARE_LLVM_ATTRIBUTE(AddressSafety,1ULL<<32) ///< Address safety checking is on.

#undef DECLARE_LLVM_ATTRIBUTE

/// Note that uwtable is about the ABI or the user mandating an entry in the
/// unwind table. The nounwind attribute is about an exception passing by the
/// function.
/// In a theoretical system that uses tables for profiling and sjlj for
/// exceptions, they would be fully independent. In a normal system that
/// uses tables for both, the semantics are:
/// nil                = Needs an entry because an exception might pass by.
/// nounwind           = No need for an entry
/// uwtable            = Needs an entry because the ABI says so and because
///                      an exception might pass by.
/// uwtable + nounwind = Needs an entry because the ABI says so.

/// @brief Attributes that only apply to function parameters.
const AttrConst ParameterOnly = {ByVal_i | Nest_i |
    StructRet_i | NoCapture_i};

/// @brief Attributes that may be applied to the function itself.  These cannot
/// be used on return values or function parameters.
const AttrConst FunctionOnly = {NoReturn_i | NoUnwind_i | ReadNone_i |
  ReadOnly_i | NoInline_i | AlwaysInline_i | OptimizeForSize_i |
  StackProtect_i | StackProtectReq_i | NoRedZone_i | NoImplicitFloat_i |
  Naked_i | InlineHint_i | StackAlignment_i |
  UWTable_i | NonLazyBind_i | ReturnsTwice_i | AddressSafety_i};

/// @brief Parameter attributes that do not apply to vararg call arguments.
const AttrConst VarArgsIncompatible = {StructRet_i};

/// @brief Attributes that are mutually incompatible.
const AttrConst MutuallyIncompatible[5] = {
  {ByVal_i | Nest_i | StructRet_i},
  {ByVal_i | Nest_i | InReg_i },
  {ZExt_i  | SExt_i},
  {ReadNone_i | ReadOnly_i},
  {NoInline_i | AlwaysInline_i}
};

}  // namespace Attribute

/// AttributeImpl - The internal representation of the Attributes class. This is
/// uniquified.
class AttributesImpl;

/// Attributes - A bitset of attributes.
class Attributes {
  // Currently, we need less than 64 bits.
  uint64_t Bits;

  explicit Attributes(AttributesImpl *A);
public:
  Attributes() : Bits(0) {}
  explicit Attributes(uint64_t Val) : Bits(Val) {}
  /*implicit*/ Attributes(Attribute::AttrConst Val) : Bits(Val.v) {}

  class Builder {
    friend class Attributes;
    uint64_t Bits;
  public:
    Builder() : Bits(0) {}
    Builder(const Attributes &A) : Bits(A.Bits) {}

    void addZExtAttr() {
      Bits |= Attribute::ZExt_i;
    }
    void addSExtAttr() {
      Bits |= Attribute::SExt_i;
    }
    void addNoReturnAttr() {
      Bits |= Attribute::NoReturn_i;
    }
    void addInRegAttr() {
      Bits |= Attribute::InReg_i;
    }
    void addStructRetAttr() {
      Bits |= Attribute::StructRet_i;
    }
    void addNoUnwindAttr() {
      Bits |= Attribute::NoUnwind_i;
    }
    void addNoAliasAttr() {
      Bits |= Attribute::NoAlias_i;
    }
    void addByValAttr() {
      Bits |= Attribute::ByVal_i;
    }
    void addNestAttr() {
      Bits |= Attribute::Nest_i;
    }
    void addReadNoneAttr() {
      Bits |= Attribute::ReadNone_i;
    }
    void addReadOnlyAttr() {
      Bits |= Attribute::ReadOnly_i;
    }
    void addNoInlineAttr() {
      Bits |= Attribute::NoInline_i;
    }
    void addAlwaysInlineAttr() {
      Bits |= Attribute::AlwaysInline_i;
    }
    void addOptimizeForSizeAttr() {
      Bits |= Attribute::OptimizeForSize_i;
    }
    void addStackProtectAttr() {
      Bits |= Attribute::StackProtect_i;
    }
    void addStackProtectReqAttr() {
      Bits |= Attribute::StackProtectReq_i;
    }
    void addNoCaptureAttr() {
      Bits |= Attribute::NoCapture_i;
    }
    void addNoRedZoneAttr() {
      Bits |= Attribute::NoRedZone_i;
    }
    void addNoImplicitFloatAttr() {
      Bits |= Attribute::NoImplicitFloat_i;
    }
    void addNakedAttr() {
      Bits |= Attribute::Naked_i;
    }
    void addInlineHintAttr() {
      Bits |= Attribute::InlineHint_i;
    }
    void addReturnsTwiceAttr() {
      Bits |= Attribute::ReturnsTwice_i;
    }
    void addUWTableAttr() {
      Bits |= Attribute::UWTable_i;
    }
    void addNonLazyBindAttr() {
      Bits |= Attribute::NonLazyBind_i;
    }
    void addAddressSafetyAttr() {
      Bits |= Attribute::AddressSafety_i;
    }
    void addAlignmentAttr(unsigned Align) {
      if (Align == 0) return;
      assert(isPowerOf2_32(Align) && "Alignment must be a power of two.");
      assert(Align <= 0x40000000 && "Alignment too large.");
      Bits |= (Log2_32(Align) + 1) << 16;
    }
    void addStackAlignmentAttr(unsigned Align) {
      // Default alignment, allow the target to define how to align it.
      if (Align == 0) return;

      assert(isPowerOf2_32(Align) && "Alignment must be a power of two.");
      assert(Align <= 0x100 && "Alignment too large.");
      Bits |= (Log2_32(Align) + 1) << 26;
    }
  };

  /// get - Return a uniquified Attributes object. This takes the uniquified
  /// value from the Builder and wraps it in the Attributes class.
  static Attributes get(LLVMContext &Context, Builder &B);

  // Attribute query methods.
  // FIXME: StackAlignment & Alignment attributes have no predicate methods.
  bool hasAttributes() const {
    return Bits != 0;
  }
  bool hasAttributes(const Attributes &A) const {
    return Bits & A.Bits;
  }

  bool hasZExtAttr() const {
    return Bits & Attribute::ZExt_i;
  }
  bool hasSExtAttr() const {
    return Bits & Attribute::SExt_i;
  }
  bool hasNoReturnAttr() const {
    return Bits & Attribute::NoReturn_i;
  }
  bool hasInRegAttr() const {
    return Bits & Attribute::InReg_i;
  }
  bool hasStructRetAttr() const {
    return Bits & Attribute::StructRet_i;
  }
  bool hasNoUnwindAttr() const {
    return Bits & Attribute::NoUnwind_i;
  }
  bool hasNoAliasAttr() const {
    return Bits & Attribute::NoAlias_i;
  }
  bool hasByValAttr() const {
    return Bits & Attribute::ByVal_i;
  }
  bool hasNestAttr() const {
    return Bits & Attribute::Nest_i;
  }
  bool hasReadNoneAttr() const {
    return Bits & Attribute::ReadNone_i;
  }
  bool hasReadOnlyAttr() const {
    return Bits & Attribute::ReadOnly_i;
  }
  bool hasNoInlineAttr() const {
    return Bits & Attribute::NoInline_i;
  }
  bool hasAlwaysInlineAttr() const {
    return Bits & Attribute::AlwaysInline_i;
  }
  bool hasOptimizeForSizeAttr() const {
    return Bits & Attribute::OptimizeForSize_i;
  }
  bool hasStackProtectAttr() const {
    return Bits & Attribute::StackProtect_i;
  }
  bool hasStackProtectReqAttr() const {
    return Bits & Attribute::StackProtectReq_i;
  }
  bool hasAlignmentAttr() const {
    return Bits & Attribute::Alignment_i;
  }
  bool hasNoCaptureAttr() const {
    return Bits & Attribute::NoCapture_i;
  }
  bool hasNoRedZoneAttr() const {
    return Bits & Attribute::NoRedZone_i;
  }
  bool hasNoImplicitFloatAttr() const {
    return Bits & Attribute::NoImplicitFloat_i;
  }
  bool hasNakedAttr() const {
    return Bits & Attribute::Naked_i;
  }
  bool hasInlineHintAttr() const {
    return Bits & Attribute::InlineHint_i;
  }
  bool hasReturnsTwiceAttr() const {
    return Bits & Attribute::ReturnsTwice_i;
  }
  bool hasStackAlignmentAttr() const {
    return Bits & Attribute::StackAlignment_i;
  }
  bool hasUWTableAttr() const {
    return Bits & Attribute::UWTable_i;
  }
  bool hasNonLazyBindAttr() const {
    return Bits & Attribute::NonLazyBind_i;
  }
  bool hasAddressSafetyAttr() const {
    return Bits & Attribute::AddressSafety_i;
  }

  /// This returns the alignment field of an attribute as a byte alignment
  /// value.
  unsigned getAlignment() const {
    if (!hasAlignmentAttr())
      return 0;
    return 1U << (((Bits & Attribute::Alignment_i) >> 16) - 1);
  }

  /// This returns the stack alignment field of an attribute as a byte alignment
  /// value.
  unsigned getStackAlignment() const {
    if (!hasStackAlignmentAttr())
      return 0;
    return 1U << (((Bits & Attribute::StackAlignment_i) >> 26) - 1);
  }

  // This is a "safe bool() operator".
  operator const void *() const { return Bits ? this : 0; }
  bool isEmptyOrSingleton() const { return (Bits & (Bits - 1)) == 0; }
  bool operator == (const Attributes &Attrs) const {
    return Bits == Attrs.Bits;
  }
  bool operator != (const Attributes &Attrs) const {
    return Bits != Attrs.Bits;
  }
  Attributes operator | (const Attributes &Attrs) const {
    return Attributes(Bits | Attrs.Bits);
  }
  Attributes operator & (const Attributes &Attrs) const {
    return Attributes(Bits & Attrs.Bits);
  }
  Attributes operator ^ (const Attributes &Attrs) const {
    return Attributes(Bits ^ Attrs.Bits);
  }
  Attributes &operator |= (const Attributes &Attrs) {
    Bits |= Attrs.Bits;
    return *this;
  }
  Attributes &operator &= (const Attributes &Attrs) {
    Bits &= Attrs.Bits;
    return *this;
  }
  Attributes operator ~ () const { return Attributes(~Bits); }
  uint64_t Raw() const { return Bits; }

  /// This turns an int alignment (a power of 2, normally) into the form used
  /// internally in Attributes.
  static Attributes constructAlignmentFromInt(unsigned i) {
    // Default alignment, allow the target to define how to align it.
    if (i == 0)
      return Attribute::None;

    assert(isPowerOf2_32(i) && "Alignment must be a power of two.");
    assert(i <= 0x40000000 && "Alignment too large.");
    return Attributes((Log2_32(i)+1) << 16);
  }

  /// This turns an int stack alignment (which must be a power of 2) into the
  /// form used internally in Attributes.
  static Attributes constructStackAlignmentFromInt(unsigned i) {
    // Default alignment, allow the target to define how to align it.
    if (i == 0)
      return Attribute::None;

    assert(isPowerOf2_32(i) && "Alignment must be a power of two.");
    assert(i <= 0x100 && "Alignment too large.");
    return Attributes((Log2_32(i)+1) << 26);
  }

  /// @brief Which attributes cannot be applied to a type.
  static Attributes typeIncompatible(Type *Ty);

  /// This returns an integer containing an encoding of all the LLVM attributes
  /// found in the given attribute bitset.  Any change to this encoding is a
  /// breaking change to bitcode compatibility.
  static uint64_t encodeLLVMAttributesForBitcode(Attributes Attrs) {
    // FIXME: It doesn't make sense to store the alignment information as an
    // expanded out value, we should store it as a log2 value.  However, we
    // can't just change that here without breaking bitcode compatibility.  If
    // this ever becomes a problem in practice, we should introduce new tag
    // numbers in the bitcode file and have those tags use a more efficiently
    // encoded alignment field.

    // Store the alignment in the bitcode as a 16-bit raw value instead of a
    // 5-bit log2 encoded value. Shift the bits above the alignment up by 11
    // bits.
    uint64_t EncodedAttrs = Attrs.Raw() & 0xffff;
    if (Attrs.hasAlignmentAttr())
      EncodedAttrs |= (1ULL << 16) <<
        (((Attrs.Bits & Attribute::Alignment_i) - 1) >> 16);
    EncodedAttrs |= (Attrs.Raw() & (0xfffULL << 21)) << 11;
    return EncodedAttrs;
  }

  /// This returns an attribute bitset containing the LLVM attributes that have
  /// been decoded from the given integer.  This function must stay in sync with
  /// 'encodeLLVMAttributesForBitcode'.
  static Attributes decodeLLVMAttributesForBitcode(uint64_t EncodedAttrs) {
    // The alignment is stored as a 16-bit raw value from bits 31--16.  We shift
    // the bits above 31 down by 11 bits.
    unsigned Alignment = (EncodedAttrs & (0xffffULL << 16)) >> 16;
    assert((!Alignment || isPowerOf2_32(Alignment)) &&
           "Alignment must be a power of two.");

    Attributes Attrs(EncodedAttrs & 0xffff);
    if (Alignment)
      Attrs |= Attributes::constructAlignmentFromInt(Alignment);
    Attrs |= Attributes((EncodedAttrs & (0xfffULL << 32)) >> 11);
    return Attrs;
  }

  /// The set of Attributes set in Attributes is converted to a string of
  /// equivalent mnemonics. This is, presumably, for writing out the mnemonics
  /// for the assembly writer.
  /// @brief Convert attribute bits to text
  std::string getAsString() const;
};

/// This is just a pair of values to associate a set of attributes
/// with an index.
struct AttributeWithIndex {
  Attributes Attrs;  ///< The attributes that are set, or'd together.
  unsigned Index;    ///< Index of the parameter for which the attributes apply.
                     ///< Index 0 is used for return value attributes.
                     ///< Index ~0U is used for function attributes.

  static AttributeWithIndex get(unsigned Idx, Attributes Attrs) {
    AttributeWithIndex P;
    P.Index = Idx;
    P.Attrs = Attrs;
    return P;
  }
};

//===----------------------------------------------------------------------===//
// AttrListPtr Smart Pointer
//===----------------------------------------------------------------------===//

class AttributeListImpl;

/// AttrListPtr - This class manages the ref count for the opaque
/// AttributeListImpl object and provides accessors for it.
class AttrListPtr {
  /// AttrList - The attributes that we are managing.  This can be null
  /// to represent the empty attributes list.
  AttributeListImpl *AttrList;
public:
  AttrListPtr() : AttrList(0) {}
  AttrListPtr(const AttrListPtr &P);
  const AttrListPtr &operator=(const AttrListPtr &RHS);
  ~AttrListPtr();

  //===--------------------------------------------------------------------===//
  // Attribute List Construction and Mutation
  //===--------------------------------------------------------------------===//

  /// get - Return a Attributes list with the specified parameters in it.
  static AttrListPtr get(ArrayRef<AttributeWithIndex> Attrs);

  /// addAttr - Add the specified attribute at the specified index to this
  /// attribute list.  Since attribute lists are immutable, this
  /// returns the new list.
  AttrListPtr addAttr(unsigned Idx, Attributes Attrs) const;

  /// removeAttr - Remove the specified attribute at the specified index from
  /// this attribute list.  Since attribute lists are immutable, this
  /// returns the new list.
  AttrListPtr removeAttr(unsigned Idx, Attributes Attrs) const;

  //===--------------------------------------------------------------------===//
  // Attribute List Accessors
  //===--------------------------------------------------------------------===//
  /// getParamAttributes - The attributes for the specified index are
  /// returned.
  Attributes getParamAttributes(unsigned Idx) const {
    assert (Idx && Idx != ~0U && "Invalid parameter index!");
    return getAttributes(Idx);
  }

  /// getRetAttributes - The attributes for the ret value are
  /// returned.
  Attributes getRetAttributes() const {
    return getAttributes(0);
  }

  /// getFnAttributes - The function attributes are returned.
  Attributes getFnAttributes() const {
    return getAttributes(~0U);
  }

  /// paramHasAttr - Return true if the specified parameter index has the
  /// specified attribute set.
  bool paramHasAttr(unsigned Idx, Attributes Attr) const {
    return getAttributes(Idx).hasAttributes(Attr);
  }

  /// getParamAlignment - Return the alignment for the specified function
  /// parameter.
  unsigned getParamAlignment(unsigned Idx) const {
    return getAttributes(Idx).getAlignment();
  }

  /// hasAttrSomewhere - Return true if the specified attribute is set for at
  /// least one parameter or for the return value.
  bool hasAttrSomewhere(Attributes Attr) const;

  /// operator==/!= - Provide equality predicates.
  bool operator==(const AttrListPtr &RHS) const
  { return AttrList == RHS.AttrList; }
  bool operator!=(const AttrListPtr &RHS) const
  { return AttrList != RHS.AttrList; }

  void dump() const;

  //===--------------------------------------------------------------------===//
  // Attribute List Introspection
  //===--------------------------------------------------------------------===//

  /// getRawPointer - Return a raw pointer that uniquely identifies this
  /// attribute list.
  void *getRawPointer() const {
    return AttrList;
  }

  // Attributes are stored as a dense set of slots, where there is one
  // slot for each argument that has an attribute.  This allows walking over the
  // dense set instead of walking the sparse list of attributes.

  /// isEmpty - Return true if there are no attributes.
  ///
  bool isEmpty() const {
    return AttrList == 0;
  }

  /// getNumSlots - Return the number of slots used in this attribute list.
  /// This is the number of arguments that have an attribute set on them
  /// (including the function itself).
  unsigned getNumSlots() const;

  /// getSlot - Return the AttributeWithIndex at the specified slot.  This
  /// holds a index number plus a set of attributes.
  const AttributeWithIndex &getSlot(unsigned Slot) const;

private:
  explicit AttrListPtr(AttributeListImpl *L);

  /// getAttributes - The attributes for the specified index are
  /// returned.  Attributes for the result are denoted with Idx = 0.
  Attributes getAttributes(unsigned Idx) const;

};

} // End llvm namespace

#endif
