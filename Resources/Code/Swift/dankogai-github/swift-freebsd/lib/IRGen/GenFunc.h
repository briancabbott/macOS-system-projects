//===--- GenFunc.h - Swift IR generation for functions ----------*- C++ -*-===//
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
//  This file provides the private interface to the function and
//  function-type emission code.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_IRGEN_GENFUNC_H
#define SWIFT_IRGEN_GENFUNC_H

#include "CallingConvention.h"

namespace clang {
  template <class> class CanQual;
  class Type;
}

namespace swift {
  class ApplyInst;
  class FuncDecl;
  enum class ResilienceExpansion : unsigned;
  class SILParameterInfo;
  class Substitution;
  class SILType;

namespace irgen {
  class Address;
  class Alignment;
  class Explosion;
  class IRGenFunction;
  class LoadableTypeInfo;
  class TypeInfo;

  /// Should the given self parameter be given the special treatment
  /// for self parameters?
  bool isSelfContextParameter(SILParameterInfo parameter);

  /// Emit a partial application thunk for a function pointer applied to a
  /// partial set of argument values.
  void emitFunctionPartialApplication(IRGenFunction &IGF,
                                      llvm::Value *fnPtr,
                                      llvm::Value *fnContext,
                                      Explosion &args,
                                      ArrayRef<SILParameterInfo> argTypes,
                                      ArrayRef<Substitution> subs,
                                      CanSILFunctionType origType,
                                      CanSILFunctionType substType,
                                      CanSILFunctionType outType,
                                      Explosion &out);
  
  /// Does an ObjC method or C function with the given signature
  /// require an sret indirect result?
  llvm::PointerType *requiresExternalIndirectResult(IRGenModule &IGM,
                                                    CanSILFunctionType fnType);
  
  /// Add function attributes to an attribute set for an indirect return
  /// argument.
  void addIndirectReturnAttributes(IRGenModule &IGM,
                                   llvm::AttributeSet &attrs);

  /// Add function attributes to an attribute set for a byval argument.
  void addByvalArgumentAttributes(IRGenModule &IGM,
                                  llvm::AttributeSet &attrs,
                                  unsigned argIndex,
                                  Alignment align);

  /// Add signext or zeroext attribute set for an argument that needs
  /// extending.
  void addExtendAttribute(IRGenModule &IGM, llvm::AttributeSet &attrs,
                          unsigned index, bool signExtend);
  
  /// Emit a call to a builtin function.
  void emitBuiltinCall(IRGenFunction &IGF, Identifier FnId,
                       SILType resultType,
                       Explosion &args, Explosion &result,
                       ArrayRef<Substitution> substitutions);
  
  /// Project the capture address from on-stack block storage.
  Address projectBlockStorageCapture(IRGenFunction &IGF,
                                     Address storageAddr,
                                     CanSILBlockStorageType storageTy);
  
  /// Emit the block header into a block storage slot.
  void emitBlockHeader(IRGenFunction &IGF,
                       Address storage,
                       CanSILBlockStorageType blockTy,
                       llvm::Function *invokeFunction,
                       CanSILFunctionType invokeTy);

  /// Can a series of values be simply pairwise coerced to (or from) an
  /// explosion schema, or do they need to traffic through memory?
  bool canCoerceToSchema(IRGenModule &IGM,
                         ArrayRef<llvm::Type*> types,
                         const ExplosionSchema &schema);

  void emitClangExpandedParameter(IRGenFunction &IGF,
                                  Explosion &in, Explosion &out,
                                  clang::CanQual<clang::Type> clangType,
                                  SILType swiftType,
                                  const LoadableTypeInfo &swiftTI);
  
  /// Allocate a stack buffer of the appropriate size to bitwise-coerce a value
  /// between two LLVM types.
  Address allocateForCoercion(IRGenFunction &IGF,
                              llvm::Type *fromTy,
                              llvm::Type *toTy,
                              const llvm::Twine &basename);

} // end namespace irgen
} // end namespace swift

#endif
