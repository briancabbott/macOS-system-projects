//===--- GenArchetype.h - Swift IR generation for archetypes ----*- C++ -*-===//
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
//  This file provides the private interface to the archetype emission code.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_IRGEN_GENARCHETYPE_H
#define SWIFT_IRGEN_GENARCHETYPE_H

#include "swift/AST/Types.h"

namespace llvm {
  class Value;
}

namespace swift {
  class ProtocolDecl;
  class SILType;

namespace irgen {
  class Address;
  class IRGenFunction;

  /// Emit a witness table reference.
  llvm::Value *emitWitnessTableRef(IRGenFunction &IGF,
                                   CanArchetypeType archetype,
                                   ProtocolDecl *protocol);

  /// Emit a dynamic metatype lookup for the given archetype.
  llvm::Value *emitDynamicTypeOfOpaqueArchetype(IRGenFunction &IGF,
                                                Address archetypeAddr,
                                                SILType archetypeType);
  
  
} // end namespace irgen
} // end namespace swift

#endif
