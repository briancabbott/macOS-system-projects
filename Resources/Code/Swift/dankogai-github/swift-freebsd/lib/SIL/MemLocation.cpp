//===------------------------- MemLocation.cpp ----------------------------===//
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

#define DEBUG_TYPE "sil-memlocation"
#include "swift/SIL/MemLocation.h"
#include "llvm/Support/Debug.h"

using namespace swift;

//===----------------------------------------------------------------------===//
//                              Utility Functions
//===----------------------------------------------------------------------===//

static inline void removeMemLocations(MemLocationValueMap &Values,
                                      MemLocationList &FirstLevel) {
  for (auto &X : FirstLevel)
    Values.erase(X);
}

//===----------------------------------------------------------------------===//
//                              SILValue Projection
//===----------------------------------------------------------------------===//

void SILValueProjection::print() const {
  llvm::outs() << Base;
  llvm::outs() << Path.getValue();
}

//===----------------------------------------------------------------------===//
//                              Load Store Value
//===----------------------------------------------------------------------===//

SILValue LoadStoreValue::createExtract(SILValue Base,
                                       Optional<ProjectionPath> &Path,
                                       SILInstruction *Inst) {
  // If we found a projection path, but there are no projections, then the two
  // loads must be the same, return PrevLI.
  if (!Path || Path->empty())
    return Base;

  // Ok, at this point we know that we can construct our aggregate projections
  // from our list of address projections.
  SILValue LastExtract = Base;
  SILBuilder Builder(Inst);

  // Construct the path!
  for (auto PI = Path->rbegin(), PE = Path->rend(); PI != PE; ++PI) {
    LastExtract =
        PI->createValueProjection(Builder, Inst->getLoc(), LastExtract).get();
    continue;
  }
  // Return the last extract we created.
  return LastExtract;
}

//===----------------------------------------------------------------------===//
//                                  Memory Location
//===----------------------------------------------------------------------===//

void MemLocation::initialize(SILValue Dest) {
  Base = getUnderlyingObject(Dest);
  Path = ProjectionPath::getAddrProjectionPath(Base, Dest);
}

bool MemLocation::isMustAliasMemLocation(const MemLocation &RHS,
                                         AliasAnalysis *AA) {
  // If the bases are not must-alias, the locations may not alias.
  if (!AA->isMustAlias(Base, RHS.getBase()))
    return false;
  // If projection paths are different, then the locations can not alias.
  if (!hasIdenticalProjectionPath(RHS))
    return false;
  return true;
}

bool MemLocation::isMayAliasMemLocation(const MemLocation &RHS,
                                        AliasAnalysis *AA) {
  // If the bases do not alias, then the locations can not alias.
  if (AA->isNoAlias(Base, RHS.getBase()))
    return false;
  // If one projection path is a prefix of another, then the locations
  // could alias.
  if (hasNonEmptySymmetricPathDifference(RHS))
    return false;
  return true;
}

MemLocation MemLocation::createMemLocation(SILValue Base, ProjectionPath &P1,
                                           ProjectionPath &P2) {
  ProjectionPath T;
  T.append(P1);
  T.append(P2);
  return MemLocation(Base, T);
}

void MemLocation::getFirstLevelMemLocations(MemLocationList &Locs,
                                            SILModule *Mod) {
  SILType Ty = getType();
  llvm::SmallVector<Projection, 8> Out;
  Projection::getFirstLevelAddrProjections(Ty, *Mod, Out);
  for (auto &X : Out) {
    ProjectionPath P;
    P.append(X);
    P.append(Path.getValue());
    Locs.push_back(MemLocation(Base, P));
  }
}

void MemLocation::expand(MemLocation &Base, SILModule *Mod,
                         MemLocationList &Locs,
                         TypeExpansionMap &TypeExpansionVault) {
  // To expand a memory location to its indivisible parts, we first get the
  // address projection paths from the accessed type to each indivisible field,
  // i.e. leaf nodes, then we append these projection paths to the Base.
  //
  // NOTE: we get the address projection because the Base memory location is
  // initialized with address projection paths. By keeping it consistent makes
  // it easier to implement the getType function for MemLocation.
  //
  SILType BaseType = Base.getType();
  if (TypeExpansionVault.find(BaseType) == TypeExpansionVault.end()) {
    // There is no cached expansion for this type, build and cache it now.
    ProjectionPathList Paths;
    ProjectionPath::expandTypeIntoLeafProjectionPaths(BaseType, Mod, Paths,
                                                      true);
    for (auto &X : Paths) {
      TypeExpansionVault[Base.getType()].push_back(std::move(X.getValue()));
    }
  }

  // Construct the MemLocation by appending the projection path from the
  // accessed node to the leaf nodes.
  for (auto &X : TypeExpansionVault[Base.getType()]) {
    Locs.push_back(MemLocation::createMemLocation(Base.getBase(), X.getValue(),
                                                  Base.getPath().getValue()));
  }
}

void MemLocation::reduce(MemLocation &Base, SILModule *Mod,
                         MemLocationSet &Locs) {
  // First, construct the MemLocation by appending the projection path from the
  // accessed node to the leaf nodes.
  MemLocationList ALocs;
  ProjectionPathList Paths;
  ProjectionPath::expandTypeIntoLeafProjectionPaths(Base.getType(), Mod, Paths,
                                                    false);
  for (auto &X : Paths) {
    ALocs.push_back(MemLocation::createMemLocation(Base.getBase(), X.getValue(),
                                                   Base.getPath().getValue()));
  }

  // Second, go from leaf nodes to their parents. This guarantees that at the
  // point the parent is processed, its children have been processed already.
  for (auto I = ALocs.rbegin(), E = ALocs.rend(); I != E; ++I) {
    MemLocationList FirstLevel;
    I->getFirstLevelMemLocations(FirstLevel, Mod);
    // Reached the end of the projection tree, this is a leaf node.
    if (FirstLevel.empty())
      continue;

    // If this is a class reference type, we have reached end of the type tree.
    if (I->getType().getClassOrBoundGenericClass())
      continue;

    // This is NOT a leaf node, check whether all its first level children are
    // alive.
    bool Alive = true;
    for (auto &X : FirstLevel) {
      if (Locs.find(X) != Locs.end())
        continue;
      Alive = false;
    }

    // All first level locations are alive, create the new aggregated location.
    if (Alive) {
      for (auto &X : FirstLevel)
        Locs.erase(X);
      Locs.insert(*I);
    }
  }
}

void MemLocation::expandWithValues(MemLocation &Base, SILValue &Val,
                                   SILModule *Mod, MemLocationList &Locs,
                                   LoadStoreValueList &Vals) {
  // To expand a memory location to its indivisible parts, we first get the
  // projection paths from the accessed type to each indivisible field, i.e.
  // leaf nodes, then we append these projection paths to the Base.
  ProjectionPathList Paths;
  ProjectionPath::expandTypeIntoLeafProjectionPaths(Base.getType(), Mod, Paths,
                                                    true);

  // Construct the MemLocation and LoadStoreValues by appending the projection
  // path
  // from the accessed node to the leaf nodes.
  for (auto &X : Paths) {
    Locs.push_back(MemLocation::createMemLocation(Base.getBase(), X.getValue(),
                                                  Base.getPath().getValue()));
    Vals.push_back(LoadStoreValue(Val, X.getValue()));
  }
}

SILValue MemLocation::reduceWithValues(MemLocation &Base, SILModule *Mod,
                                       MemLocationValueMap &Values,
                                       SILInstruction *InsertPt) {
  // Walk bottom up the projection tree, try to reason about how to construct
  // a single SILValue out of all the available values for all the memory
  // locations.
  //
  // First, get a list of all the leaf nodes and intermediate nodes for the
  // Base memory location.
  MemLocationList ALocs;
  ProjectionPathList Paths;
  ProjectionPath::expandTypeIntoLeafProjectionPaths(Base.getType(), Mod, Paths,
                                                    false);
  for (auto &X : Paths) {
    ALocs.push_back(MemLocation::createMemLocation(Base.getBase(), X.getValue(),
                                                   Base.getPath().getValue()));
  }

  // Second, go from leaf nodes to their parents. This guarantees that at the
  // point the parent is processed, its children have been processed already.
  for (auto I = ALocs.rbegin(), E = ALocs.rend(); I != E; ++I) {
    // This is a leaf node, we have a value for it.
    //
    // Reached the end of the projection tree, this is a leaf node.
    MemLocationList FirstLevel;
    I->getFirstLevelMemLocations(FirstLevel, Mod);
    if (FirstLevel.empty())
      continue;

    // If this is a class reference type, we have reached end of the type tree.
    if (I->getType().getClassOrBoundGenericClass())
      continue;

    // This is NOT a leaf node, we need to construct a value for it.
    //
    // If there are more than 1 children and all the children nodes have
    // LoadStoreValues with the same base. we can get away by not extracting
    // value
    // for every single field.
    //
    // Simply create a new node with all the aggregated base value, i.e.
    // stripping off the last level projection.
    //
    bool HasIdenticalValueBase = true;
    auto Iter = FirstLevel.begin();
    LoadStoreValue &FirstVal = Values[*Iter];
    SILValue FirstBase = FirstVal.getBase();
    Iter = std::next(Iter);
    for (auto EndIter = FirstLevel.end(); Iter != EndIter; ++Iter) {
      LoadStoreValue &V = Values[*Iter];
      HasIdenticalValueBase &= (FirstBase == V.getBase());
    }

    if (HasIdenticalValueBase &&
        (FirstLevel.size() > 1 || !FirstVal.hasEmptyProjectionPath())) {
      Values[*I] = FirstVal.stripLastLevelProjection();
      // We have a value for the parent, remove all the values for children.
      removeMemLocations(Values, FirstLevel);
      continue;
    }

    // In 2 cases do we need aggregation.
    //
    // 1. If there is only 1 child and we can not strip off any projections,
    // that means we need to create an aggregation.
    //
    // 2. Children have values from different bases, We need to create
    // extractions and aggregation in this case.
    //
    llvm::SmallVector<SILValue, 8> Vals;
    for (auto &X : FirstLevel) {
      Vals.push_back(Values[X].materialize(InsertPt));
    }
    SILBuilder Builder(InsertPt);
    NullablePtr<swift::SILInstruction> AI =
        Projection::createAggFromFirstLevelProjections(
            Builder, InsertPt->getLoc(), I->getType(), Vals);
    // This is the Value for the current node.
    ProjectionPath P;
    Values[*I] = LoadStoreValue(SILValue(AI.get()), P);
    removeMemLocations(Values, FirstLevel);

    // Keep iterating until we have reach the top-most level of the projection
    // tree.
    // i.e. the memory location represented by the Base.
  }

  assert(Values.size() == 1 && "Should have a single location this point");

  // Finally materialize and return the forwarding SILValue.
  return Values.begin()->second.materialize(InsertPt);
}

void MemLocation::enumerateMemLocation(SILModule *M, SILValue Mem,
                                       std::vector<MemLocation> &LV,
                                       MemLocationIndexMap &BM,
                                       TypeExpansionMap &TV) {
  // Construct a Location to represent the memory written by this instruction.
  MemLocation L(Mem);

  // If we cant figure out the Base or Projection Path for the memory location,
  // simply ignore it for now.
  if (!L.isValid())
    return;

  // Expand the given Mem into individual fields and add them to the
  // locationvault.
  MemLocationList Locs;
  MemLocation::expand(L, M, Locs, TV);
  for (auto &Loc : Locs) {
    BM[Loc] = LV.size();
    LV.push_back(Loc);
  }
}

void MemLocation::enumerateMemLocations(SILFunction &F,
                                        std::vector<MemLocation> &LV,
                                        MemLocationIndexMap &BM,
                                        TypeExpansionMap &TV) {
  // Enumerate all locations accessed by the loads or stores.
  //
  // TODO: process more instructions as we process more instructions in
  // processInstruction.
  //
  SILValue Op;
  for (auto &B : F) {
    for (auto &I : B) {
      if (auto *LI = dyn_cast<LoadInst>(&I)) {
        enumerateMemLocation(&I.getModule(), LI->getOperand(), LV, BM, TV);
      } else if (auto *SI = dyn_cast<StoreInst>(&I)) {
        enumerateMemLocation(&I.getModule(), SI->getDest(), LV, BM, TV);
      }
    }
  }
}
