//===--- GenericSignature.h - Generic Signature AST -------------*- C++ -*-===//
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
// This file defines the GenericSignature class and its related classes.
//
//===----------------------------------------------------------------------===//

#ifndef SWIFT_AST_GENERIC_SIGNATURE_H
#define SWIFT_AST_GENERIC_SIGNATURE_H

#include "swift/AST/Requirement.h"
#include "swift/AST/Substitution.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/FoldingSet.h"

namespace swift {

class ArchetypeBuilder;

/// Iterator that walks the generic parameter types declared in a generic
/// signature and their dependent members.
class GenericSignatureWitnessIterator {
  ArrayRef<Requirement> p;
  
public:
  GenericSignatureWitnessIterator() = default;
  GenericSignatureWitnessIterator(ArrayRef<Requirement> p)
    : p(p)
  {
    assert(p.empty() || p.front().getKind() == RequirementKind::WitnessMarker);
  }
  
  GenericSignatureWitnessIterator &operator++() {
    do {
      p = p.slice(1);
    } while (!p.empty()
             && p.front().getKind() != RequirementKind::WitnessMarker);
    return *this;
  }
  
  GenericSignatureWitnessIterator operator++(int) {
    auto copy = *this;
    ++(*this);
    return copy;
  }
  
  Type operator*() const {
    assert(p.front().getKind() == RequirementKind::WitnessMarker);
    return p.front().getFirstType();
  }
  
  Type operator->() const {
    assert(p.front().getKind() == RequirementKind::WitnessMarker);
    return p.front().getFirstType();
  }
  
  bool operator==(const GenericSignatureWitnessIterator &o) {
    return p.data() == o.p.data() && p.size() == o.p.size();
  }
  
  bool operator!=(const GenericSignatureWitnessIterator &o) {
    return p.data() != o.p.data() || p.size() != o.p.size();
  }
  
  static GenericSignatureWitnessIterator emptyRange() {
    return GenericSignatureWitnessIterator();
  }
  
  // Allow the witness iterator to be used with a ranged for.
  GenericSignatureWitnessIterator begin() const {
    return *this;
  }
  GenericSignatureWitnessIterator end() const {
    return GenericSignatureWitnessIterator({p.end(), p.end()});
  }
};

/// Describes the generic signature of a particular declaration, including
/// both the generic type parameters and the requirements placed on those
/// generic parameters.
class GenericSignature : public llvm::FoldingSetNode {
  unsigned NumGenericParams;
  unsigned NumRequirements;

  // Make vanilla new/delete illegal.
  void *operator new(size_t Bytes) = delete;
  void operator delete(void *Data) = delete;

  /// Retrieve a mutable version of the generic parameters.
  MutableArrayRef<GenericTypeParamType *> getGenericParamsBuffer() {
    return { reinterpret_cast<GenericTypeParamType **>(this + 1),
             NumGenericParams };
  }

  /// Retrieve a mutable verison of the requirements.
  MutableArrayRef<Requirement> getRequirementsBuffer() {
    void *genericParams = getGenericParamsBuffer().end();
    return { reinterpret_cast<Requirement *>(genericParams),
      NumRequirements };
  }

  GenericSignature(ArrayRef<GenericTypeParamType *> params,
                   ArrayRef<Requirement> requirements,
                   bool isKnownCanonical);

  mutable llvm::PointerUnion<GenericSignature *, ASTContext *>
    CanonicalSignatureOrASTContext;
  
  static ASTContext &getASTContext(ArrayRef<GenericTypeParamType *> params,
                                   ArrayRef<Requirement> requirements);

  /// Retrieve the archetype builder for the given generic signature.
  ArchetypeBuilder *getArchetypeBuilder(ModuleDecl &mod);

public:
  /// Create a new generic signature with the given type parameters and
  /// requirements.
  static GenericSignature *get(ArrayRef<GenericTypeParamType *> params,
                               ArrayRef<Requirement> requirements,
                               bool isKnownCanonical = false);

  /// Create a new generic signature with the given type parameters and
  /// requirements, first canonicalizing the types.
  static CanGenericSignature getCanonical(ArrayRef<GenericTypeParamType *> params,
                                          ArrayRef<Requirement> requirements);

  /// Retrieve the generic parameters.
  ArrayRef<GenericTypeParamType *> getGenericParams() const {
    return { reinterpret_cast<GenericTypeParamType * const *>(this + 1),
             NumGenericParams };
  }

  /// Retrieve the innermost generic parameters.
  ///
  /// Given a generic signature for a nested generic type, produce an
  /// array of the generic parameters for the innermost generic type.
  ArrayRef<GenericTypeParamType *> getInnermostGenericParams() const;

  /// Retrieve the requirements.
  ArrayRef<Requirement> getRequirements() const {
    const void *genericParams = getGenericParams().end();
    return { reinterpret_cast<const Requirement *>(genericParams),
             NumRequirements };
  }

  // Only allow allocation by doing a placement new.
  void *operator new(size_t Bytes, void *Mem) {
    assert(Mem);
    return Mem;
  }
  
  /// Build a substitution map from a vector of Substitutions that correspond to
  /// the generic parameters in this generic signature. The order of primary
  /// archetypes in the substitution vector must match the order of generic
  /// parameters in getGenericParams().
  TypeSubstitutionMap getSubstitutionMap(ArrayRef<Substitution> args) const;
  
  /// Return a range that iterates through first all of the generic parameters
  /// of the signature, followed by all of their recursive member types exposed
  /// through protocol requirements.
  ///
  /// The member types are presented in the
  /// same order as GenericParamList::getAllArchetypes would present for an
  /// equivalent GenericParamList.
  GenericSignatureWitnessIterator getAllDependentTypes() const {
    return GenericSignatureWitnessIterator(getRequirements());
  }

  /// Determines whether this ASTContext is canonical.
  bool isCanonical() const;
  
  ASTContext &getASTContext() const;
  
  /// Canonicalize the components of a generic signature.
  CanGenericSignature getCanonicalSignature() const;
  
  /// Canonicalize a generic signature down to its essential requirements,
  /// for mangling purposes.
  ///
  /// TODO: This is what getCanonicalSignature() ought to do, but currently
  /// cannot due to implementation dependencies on 'getAllDependentTypes'
  /// order matching 'getAllArchetypes' order of a generic param list.
  CanGenericSignature getCanonicalManglingSignature(ModuleDecl &M) const;

  /// Uniquing for the ASTContext.
  void Profile(llvm::FoldingSetNodeID &ID) {
    Profile(ID, getGenericParams(), getRequirements());
  }
  
  /// Determine whether the given dependent type is required to be a class.
  bool requiresClass(Type type, ModuleDecl &mod);

  /// Determine the superclass bound on the given dependent type.
  Type getSuperclassBound(Type type, ModuleDecl &mod);

  /// Determine the set of protocols to which the given dependent type
  /// must conform.
  SmallVector<ProtocolDecl *, 2> getConformsTo(Type type, ModuleDecl &mod);

  /// Determine whether the given dependent type is equal to a concrete type.
  bool isConcreteType(Type type, ModuleDecl &mod);

  static void Profile(llvm::FoldingSetNodeID &ID,
                      ArrayRef<GenericTypeParamType *> genericParams,
                      ArrayRef<Requirement> requirements);
  
  void print(raw_ostream &OS) const;
  void dump() const;
  std::string getAsString() const;
};
  
inline
CanGenericSignature::CanGenericSignature(GenericSignature *Signature)
  : Signature(Signature)
{
  assert(!Signature || Signature->isCanonical());
}
  
inline ArrayRef<CanTypeWrapper<GenericTypeParamType>>
CanGenericSignature::getGenericParams() const{
  ArrayRef<GenericTypeParamType*> params = Signature->getGenericParams();
  auto base = reinterpret_cast<const CanTypeWrapper<GenericTypeParamType>*>(
                                                                params.data());
  return {base, params.size()};
}

} // end namespace swift

#endif // SWIFT_AST_GENERIC_SIGNATURE_H
