// RUN: %target-swift-frontend -emit-silgen %s -disable-objc-attr-requires-foundation-module | FileCheck %s

infix operator <~> {}

func archetype_method<T: X>(x x: T, y: T) -> T {
  var x = x
  var y = y
  return x.selfTypes(x: y)
}
// CHECK-LABEL: sil hidden @_TF9witnesses16archetype_method{{.*}} : $@convention(thin) <T where T : X> (@out T, @in T, @in T) -> () {
// CHECK:         [[METHOD:%.*]] = witness_method $T, #X.selfTypes!1 : $@convention(witness_method) <τ_0_0 where τ_0_0 : X> (@out τ_0_0, @in τ_0_0, @inout τ_0_0) -> ()
// CHECK:         apply [[METHOD]]<T>({{%.*}}, {{%.*}}, {{%.*}}) : $@convention(witness_method) <τ_0_0 where τ_0_0 : X> (@out τ_0_0, @in τ_0_0, @inout τ_0_0) -> ()
// CHECK:       }

func archetype_generic_method<T: X>(x x: T, y: Loadable) -> Loadable {
  var x = x
  return x.generic(x: y)
}
// CHECK-LABEL: sil hidden @_TF9witnesses24archetype_generic_method{{.*}} : $@convention(thin) <T where T : X> (@in T, Loadable) -> Loadable {
// CHECK:         [[METHOD:%.*]] = witness_method $T, #X.generic!1 : $@convention(witness_method) <τ_0_0 where τ_0_0 : X><τ_1_0> (@out τ_1_0, @in τ_1_0, @inout τ_0_0) -> ()
// CHECK:         apply [[METHOD]]<T, Loadable>({{%.*}}, {{%.*}}, {{%.*}}) : $@convention(witness_method) <τ_0_0 where τ_0_0 : X><τ_1_0> (@out τ_1_0, @in τ_1_0, @inout τ_0_0) -> ()
// CHECK:       }

// CHECK-LABEL: sil hidden @_TF9witnesses32archetype_associated_type_method{{.*}} : $@convention(thin) <T where T : WithAssocType> (@out T, @in T, @in T.AssocType) -> ()
// CHECK:         apply %{{[0-9]+}}<T, T.AssocType>
func archetype_associated_type_method<T: WithAssocType>(x x: T, y: T.AssocType) -> T {
  return x.useAssocType(x: y)
}

protocol StaticMethod { static func staticMethod() }

// CHECK-LABEL: sil hidden @_TF9witnesses23archetype_static_method{{.*}} : $@convention(thin) <T where T : StaticMethod> (@in T) -> ()
func archetype_static_method<T: StaticMethod>(x x: T) {
  // CHECK: [[METHOD:%.*]] = witness_method $T, #StaticMethod.staticMethod!1 : $@convention(witness_method) <τ_0_0 where τ_0_0 : StaticMethod> (@thick τ_0_0.Type) -> ()
  // CHECK: apply [[METHOD]]<T>
  T.staticMethod()
}

protocol Existentiable {
  func foo() -> Loadable
  func generic<T>() -> T
}

func protocol_method(x x: Existentiable) -> Loadable {
  return x.foo()
}
// CHECK-LABEL: sil hidden @_TF9witnesses15protocol_methodFT1xPS_13Existentiable__VS_8Loadable : $@convention(thin) (@in Existentiable) -> Loadable {
// CHECK:         [[METHOD:%.*]] = witness_method $[[OPENED:@opened(.*) Existentiable]], #Existentiable.foo!1
// CHECK:         apply [[METHOD]]<[[OPENED]]>({{%.*}})
// CHECK:       }

func protocol_generic_method(x x: Existentiable) -> Loadable {
  return x.generic()
}
// CHECK-LABEL: sil hidden @_TF9witnesses23protocol_generic_methodFT1xPS_13Existentiable__VS_8Loadable : $@convention(thin) (@in Existentiable) -> Loadable {
// CHECK:         [[METHOD:%.*]] = witness_method $[[OPENED:@opened(.*) Existentiable]], #Existentiable.generic!1
// CHECK:         apply [[METHOD]]<[[OPENED]], Loadable>({{%.*}}, {{%.*}})
// CHECK:       }

@objc protocol ObjCAble {
  func foo()
}

// CHECK-LABEL: sil hidden @_TF9witnesses20protocol_objc_methodFT1xPS_8ObjCAble__T_ : $@convention(thin) (@owned ObjCAble) -> ()
// CHECK:         witness_method [volatile] $@opened({{.*}}) ObjCAble, #ObjCAble.foo!1.foreign
func protocol_objc_method(x x: ObjCAble) {
  x.foo()
}

struct Loadable {}
protocol AddrOnly {}
protocol Classes : class {}

protocol X {
  mutating
  func selfTypes(x x: Self) -> Self
  mutating
  func loadable(x x: Loadable) -> Loadable
  mutating
  func addrOnly(x x: AddrOnly) -> AddrOnly
  mutating
  func generic<A>(x x: A) -> A
  mutating
  func classes<A2: Classes>(x x: A2) -> A2
  func <~>(x: Self, y: Self) -> Self
}
protocol Y {}

protocol WithAssocType {
  typealias AssocType
  func useAssocType(x x: AssocType) -> Self
}

protocol ClassBounded : class {
  func selfTypes(x x: Self) -> Self
}

struct ConformingStruct : X {
  mutating
  func selfTypes(x x: ConformingStruct) -> ConformingStruct { return x }
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses16ConformingStructS_1XS_FS1_9selfTypes{{.*}} : $@convention(witness_method) (@out ConformingStruct, @in ConformingStruct, @inout ConformingStruct) -> () {
  // CHECK:       bb0(%0 : $*ConformingStruct, %1 : $*ConformingStruct, %2 : $*ConformingStruct):
  // CHECK-NEXT:    %3 = load %1 : $*ConformingStruct
  // CHECK-NEXT:    // function_ref
  // CHECK-NEXT:    %4 = function_ref @_TFV9witnesses16ConformingStruct9selfTypes{{.*}} : $@convention(method) (ConformingStruct, @inout ConformingStruct) -> ConformingStruct
  // CHECK-NEXT:    %5 = apply %4(%3, %2) : $@convention(method) (ConformingStruct, @inout ConformingStruct) -> ConformingStruct
  // CHECK-NEXT:    store %5 to %0 : $*ConformingStruct
  // CHECK-NEXT:    %7 = tuple ()
  // CHECK-NEXT:    return %7 : $()
  // CHECK-NEXT:  }
  
  mutating
  func loadable(x x: Loadable) -> Loadable { return x }
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses16ConformingStructS_1XS_FS1_8loadable{{.*}} : $@convention(witness_method) (Loadable, @inout ConformingStruct) -> Loadable {
  // CHECK:       bb0(%0 : $Loadable, %1 : $*ConformingStruct):
  // CHECK-NEXT:    // function_ref
  // CHECK-NEXT:    %2 = function_ref @_TFV9witnesses16ConformingStruct8loadable{{.*}} : $@convention(method) (Loadable, @inout ConformingStruct) -> Loadable
  // CHECK-NEXT:    %3 = apply %2(%0, %1) : $@convention(method) (Loadable, @inout ConformingStruct) -> Loadable
  // CHECK-NEXT:    return %3 : $Loadable
  // CHECK-NEXT:  }
  
  mutating
  func addrOnly(x x: AddrOnly) -> AddrOnly { return x }
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses16ConformingStructS_1XS_FS1_8addrOnly{{.*}} : $@convention(witness_method) (@out AddrOnly, @in AddrOnly, @inout ConformingStruct) -> () {
  // CHECK:       bb0(%0 : $*AddrOnly, %1 : $*AddrOnly, %2 : $*ConformingStruct):
  // CHECK-NEXT:    // function_ref
  // CHECK-NEXT:    %3 = function_ref @_TFV9witnesses16ConformingStruct8addrOnly{{.*}} : $@convention(method) (@out AddrOnly, @in AddrOnly, @inout ConformingStruct) -> ()
  // CHECK-NEXT:    %4 = apply %3(%0, %1, %2) : $@convention(method) (@out AddrOnly, @in AddrOnly, @inout ConformingStruct) -> ()
  // CHECK-NEXT:    return %4 : $()
  // CHECK-NEXT:  }
  
  mutating
  func generic<C>(x x: C) -> C { return x }
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses16ConformingStructS_1XS_FS1_7generic{{.*}} : $@convention(witness_method) <A> (@out A, @in A, @inout ConformingStruct) -> () {
  // CHECK:       bb0(%0 : $*A, %1 : $*A, %2 : $*ConformingStruct):
  // CHECK-NEXT:    // function_ref
  // CHECK-NEXT:    %3 = function_ref @_TFV9witnesses16ConformingStruct7generic{{.*}} : $@convention(method) <τ_0_0> (@out τ_0_0, @in τ_0_0, @inout ConformingStruct) -> ()
  // CHECK-NEXT:    %4 = apply %3<A>(%0, %1, %2) : $@convention(method) <τ_0_0> (@out τ_0_0, @in τ_0_0, @inout ConformingStruct) -> ()
  // CHECK-NEXT:    return %4 : $()
  // CHECK-NEXT:  }
  mutating
  func classes<C2: Classes>(x x: C2) -> C2 { return x }
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses16ConformingStructS_1XS_FS1_7classes{{.*}} : $@convention(witness_method) <A2 where A2 : Classes> (@owned A2, @inout ConformingStruct) -> @owned A2 {
  // CHECK:       bb0(%0 : $A2, %1 : $*ConformingStruct):
  // CHECK-NEXT:    // function_ref
  // CHECK-NEXT:    %2 = function_ref @_TFV9witnesses16ConformingStruct7classes{{.*}} : $@convention(method) <τ_0_0 where τ_0_0 : Classes> (@owned τ_0_0, @inout ConformingStruct) -> @owned τ_0_0
  // CHECK-NEXT:    %3 = apply %2<A2>(%0, %1) : $@convention(method) <τ_0_0 where τ_0_0 : Classes> (@owned τ_0_0, @inout ConformingStruct) -> @owned τ_0_0
  // CHECK-NEXT:    return %3 : $A2
  // CHECK-NEXT:  }
}
func <~>(x: ConformingStruct, y: ConformingStruct) -> ConformingStruct { return x }
// CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses16ConformingStructS_1XS_ZFS1_oi3ltg{{.*}} : $@convention(witness_method) (@out ConformingStruct, @in ConformingStruct, @in ConformingStruct, @thick ConformingStruct.Type) -> () {
// CHECK:       bb0(%0 : $*ConformingStruct, %1 : $*ConformingStruct, %2 : $*ConformingStruct, %3 : $@thick ConformingStruct.Type):
// CHECK-NEXT:    %4 = load %1 : $*ConformingStruct
// CHECK-NEXT:    %5 = load %2 : $*ConformingStruct
// CHECK-NEXT:    // function_ref
// CHECK-NEXT:    %6 = function_ref @_TZF9witnessesoi3ltgFTVS_16ConformingStructS0__S0_ : $@convention(thin) (ConformingStruct, ConformingStruct) -> ConformingStruct
// CHECK-NEXT:    %7 = apply %6(%4, %5) : $@convention(thin) (ConformingStruct, ConformingStruct) -> ConformingStruct
// CHECK-NEXT:    store %7 to %0 : $*ConformingStruct
// CHECK-NEXT:    %9 = tuple ()
// CHECK-NEXT:    return %9 : $()
// CHECK-NEXT:  }

final class ConformingClass : X {
  func selfTypes(x x: ConformingClass) -> ConformingClass { return x }
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses15ConformingClassS_1XS_FS1_9selfTypes{{.*}} : $@convention(witness_method) (@out ConformingClass, @in ConformingClass, @inout ConformingClass) -> () {
  // CHECK:       bb0(%0 : $*ConformingClass, %1 : $*ConformingClass, %2 : $*ConformingClass):
  // -- load and retain 'self' from inout witness 'self' parameter
  // CHECK-NEXT:    %3 = load %2 : $*ConformingClass
  // CHECK-NEXT:    strong_retain %3 : $ConformingClass
  // CHECK-NEXT:    %5 = load %1 : $*ConformingClass
  // CHECK:         %6 = function_ref @_TFC9witnesses15ConformingClass9selfTypes 
  // CHECK-NEXT:    %7 = apply %6(%5, %3) : $@convention(method) (@owned ConformingClass, @guaranteed ConformingClass) -> @owned ConformingClass
  // CHECK-NEXT:    store %7 to %0 : $*ConformingClass
  // CHECK-NEXT:    %9 = tuple ()
  // CHECK-NEXT:    strong_release %3
  // CHECK-NEXT:    return %9 : $()
  // CHECK-NEXT:  }
  func loadable(x x: Loadable) -> Loadable { return x }
  func addrOnly(x x: AddrOnly) -> AddrOnly { return x }
  func generic<D>(x x: D) -> D { return x }
  func classes<D2: Classes>(x x: D2) -> D2 { return x }
}
func <~>(x: ConformingClass, y: ConformingClass) -> ConformingClass { return x }

extension ConformingClass : ClassBounded { }
// CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses15ConformingClassS_12ClassBoundedS_FS1_9selfTypes{{.*}} : $@convention(witness_method) (@owned ConformingClass, @guaranteed ConformingClass) -> @owned ConformingClass {
// CHECK:  bb0([[C0:%.*]] : $ConformingClass, [[C1:%.*]] : $ConformingClass):
// CHECK-NEXT:    strong_retain [[C1]]
// CHECK-NEXT:    function_ref
// CHECK-NEXT:    [[FUN:%.*]] = function_ref @_TFC9witnesses15ConformingClass9selfTypes
// CHECK-NEXT:    [[RESULT:%.*]] = apply [[FUN]]([[C0]], [[C1]]) : $@convention(method) (@owned ConformingClass, @guaranteed ConformingClass) -> @owned ConformingClass
// CHECK-NEXT:    strong_release [[C1]]
// CHECK-NEXT:    return [[RESULT]] : $ConformingClass
// CHECK-NEXT:  }

struct ConformingAOStruct : X {
  var makeMeAO : AddrOnly

  mutating
  func selfTypes(x x: ConformingAOStruct) -> ConformingAOStruct { return x }
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses18ConformingAOStructS_1XS_FS1_9selfTypes{{.*}} : $@convention(witness_method) (@out ConformingAOStruct, @in ConformingAOStruct, @inout ConformingAOStruct) -> () {
  // CHECK:       bb0(%0 : $*ConformingAOStruct, %1 : $*ConformingAOStruct, %2 : $*ConformingAOStruct):
  // CHECK-NEXT:    // function_ref
  // CHECK-NEXT:    %3 = function_ref @_TFV9witnesses18ConformingAOStruct9selfTypes{{.*}} : $@convention(method) (@out ConformingAOStruct, @in ConformingAOStruct, @inout ConformingAOStruct) -> ()
  // CHECK-NEXT:    %4 = apply %3(%0, %1, %2) : $@convention(method) (@out ConformingAOStruct, @in ConformingAOStruct, @inout ConformingAOStruct) -> ()
  // CHECK-NEXT:    return %4 : $()
  // CHECK-NEXT:  }
  func loadable(x x: Loadable) -> Loadable { return x }
  func addrOnly(x x: AddrOnly) -> AddrOnly { return x }
  func generic<D>(x x: D) -> D { return x }
  func classes<D2: Classes>(x x: D2) -> D2 { return x }
}
func <~>(x: ConformingAOStruct, y: ConformingAOStruct) -> ConformingAOStruct { return x }

// TODO: The extra abstraction change in these generic witnesses leads to a
// bunch of redundant temporaries that could be avoided by a more sophisticated
// abstraction difference implementation.

struct ConformsWithMoreGeneric : X, Y {
  mutating
  func selfTypes<E>(x x: E) -> E { return x }
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses23ConformsWithMoreGenericS_1XS_FS1_9selfTypes{{.*}} : $@convention(witness_method) (@out ConformsWithMoreGeneric, @in ConformsWithMoreGeneric, @inout ConformsWithMoreGeneric) -> () {
  // CHECK:       bb0(%0 : $*ConformsWithMoreGeneric, %1 : $*ConformsWithMoreGeneric, %2 : $*ConformsWithMoreGeneric):
  // CHECK-NEXT:    %3 = load %1 : $*ConformsWithMoreGeneric
  // CHECK-NEXT:    // function_ref
  // CHECK-NEXT:    %4 = function_ref @_TFV9witnesses23ConformsWithMoreGeneric9selfTypes{{.*}} : $@convention(method) <τ_0_0> (@out τ_0_0, @in τ_0_0, @inout ConformsWithMoreGeneric) -> ()
  // CHECK-NEXT:    %5 = alloc_stack $ConformsWithMoreGeneric
  // CHECK-NEXT:    store %3 to %5#1 : $*ConformsWithMoreGeneric
  // CHECK-NEXT:    %7 = alloc_stack $ConformsWithMoreGeneric
  // CHECK-NEXT:    %8 = apply %4<ConformsWithMoreGeneric>(%7#1, %5#1, %2) : $@convention(method) <τ_0_0> (@out τ_0_0, @in τ_0_0, @inout ConformsWithMoreGeneric) -> ()
  // CHECK-NEXT:    %9 = load %7#1 : $*ConformsWithMoreGeneric
  // CHECK-NEXT:    store %9 to %0 : $*ConformsWithMoreGeneric
  // CHECK-NEXT:    %11 = tuple ()
  // CHECK-NEXT:    dealloc_stack %7#0 : $*@local_storage ConformsWithMoreGeneric
  // CHECK-NEXT:    dealloc_stack %5#0 : $*@local_storage ConformsWithMoreGeneric
  // CHECK-NEXT:    return %11 : $()
  // CHECK-NEXT:  }
  func loadable<F>(x x: F) -> F { return x }
  mutating
  func addrOnly<G>(x x: G) -> G { return x }
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses23ConformsWithMoreGenericS_1XS_FS1_8addrOnly{{.*}} : $@convention(witness_method) (@out AddrOnly, @in AddrOnly, @inout ConformsWithMoreGeneric) -> () {
  // CHECK:       bb0(%0 : $*AddrOnly, %1 : $*AddrOnly, %2 : $*ConformsWithMoreGeneric):
  // CHECK-NEXT:    // function_ref
  // CHECK-NEXT:    %3 = function_ref @_TFV9witnesses23ConformsWithMoreGeneric8addrOnly{{.*}} : $@convention(method) <τ_0_0> (@out τ_0_0, @in τ_0_0, @inout ConformsWithMoreGeneric) -> ()
  // CHECK-NEXT:    %4 = apply %3<AddrOnly>(%0, %1, %2) : $@convention(method) <τ_0_0> (@out τ_0_0, @in τ_0_0, @inout ConformsWithMoreGeneric) -> ()
  // CHECK-NEXT:    return %4 : $()
  // CHECK-NEXT:  }

  mutating
  func generic<H>(x x: H) -> H { return x }
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses23ConformsWithMoreGenericS_1XS_FS1_7generic{{.*}} : $@convention(witness_method) <A> (@out A, @in A, @inout ConformsWithMoreGeneric) -> () {
  // CHECK:       bb0(%0 : $*A, %1 : $*A, %2 : $*ConformsWithMoreGeneric):
  // CHECK-NEXT:    // function_ref
  // CHECK-NEXT:    %3 = function_ref @_TFV9witnesses23ConformsWithMoreGeneric7generic{{.*}} : $@convention(method) <τ_0_0> (@out τ_0_0, @in τ_0_0, @inout ConformsWithMoreGeneric) -> ()
  // CHECK-NEXT:    %4 = apply %3<A>(%0, %1, %2) : $@convention(method) <τ_0_0> (@out τ_0_0, @in τ_0_0, @inout ConformsWithMoreGeneric) -> ()
  // CHECK-NEXT:    return %4 : $()
  // CHECK-NEXT:  }

  mutating
  func classes<I>(x x: I) -> I { return x }
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses23ConformsWithMoreGenericS_1XS_FS1_7classes{{.*}} : $@convention(witness_method) <A2 where A2 : Classes> (@owned A2, @inout ConformsWithMoreGeneric) -> @owned A2 {
  // CHECK:       bb0(%0 : $A2, %1 : $*ConformsWithMoreGeneric):
  // CHECK-NEXT:    // function_ref witnesses.ConformsWithMoreGeneric.classes
  // CHECK-NEXT:    %2 = function_ref @_TFV9witnesses23ConformsWithMoreGeneric7classes{{.*}} : $@convention(method) <τ_0_0> (@out τ_0_0, @in τ_0_0, @inout ConformsWithMoreGeneric) -> ()
  // CHECK-NEXT:    %3 = alloc_stack $A2
  // CHECK-NEXT:    store %0 to %3#1 : $*A2
  // CHECK-NEXT:    %5 = alloc_stack $A2
  // CHECK-NEXT:    %6 = apply %2<A2>(%5#1, %3#1, %1) : $@convention(method) <τ_0_0> (@out τ_0_0, @in τ_0_0, @inout ConformsWithMoreGeneric) -> ()
  // CHECK-NEXT:    %7 = load %5#1 : $*A2
  // CHECK-NEXT:    dealloc_stack %5#0 : $*@local_storage A2
  // CHECK-NEXT:    dealloc_stack %3#0 : $*@local_storage A2
  // CHECK-NEXT:    return %7 : $A2
  // CHECK-NEXT:  }
}
func <~> <J: Y, K: Y>(x: J, y: K) -> K { return y }
// CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses23ConformsWithMoreGenericS_1XS_ZFS1_oi3ltg{{.*}} : $@convention(witness_method) (@out ConformsWithMoreGeneric, @in ConformsWithMoreGeneric, @in ConformsWithMoreGeneric, @thick ConformsWithMoreGeneric.Type) -> () {
// CHECK:       bb0(%0 : $*ConformsWithMoreGeneric, %1 : $*ConformsWithMoreGeneric, %2 : $*ConformsWithMoreGeneric, %3 : $@thick ConformsWithMoreGeneric.Type):
// CHECK-NEXT:    %4 = load %1 : $*ConformsWithMoreGeneric
// CHECK-NEXT:    %5 = load %2 : $*ConformsWithMoreGeneric
// CHECK-NEXT:    // function_ref
// CHECK-NEXT:    %6 = function_ref @_TZF9witnessesoi3ltg{{.*}} : $@convention(thin) <τ_0_0, τ_0_1 where τ_0_0 : Y, τ_0_1 : Y> (@out τ_0_1, @in τ_0_0, @in τ_0_1) -> ()
// CHECK-NEXT:    %7 = alloc_stack $ConformsWithMoreGeneric
// CHECK-NEXT:    store %4 to %7#1 : $*ConformsWithMoreGeneric
// CHECK-NEXT:    %9 = alloc_stack $ConformsWithMoreGeneric
// CHECK-NEXT:    store %5 to %9#1 : $*ConformsWithMoreGeneric
// CHECK-NEXT:    %11 = alloc_stack $ConformsWithMoreGeneric
// CHECK-NEXT:    %12 = apply %6<ConformsWithMoreGeneric, ConformsWithMoreGeneric>(%11#1, %7#1, %9#1) : $@convention(thin) <τ_0_0, τ_0_1 where τ_0_0 : Y, τ_0_1 : Y> (@out τ_0_1, @in τ_0_0, @in τ_0_1) -> ()
// CHECK-NEXT:    %13 = load %11#1 : $*ConformsWithMoreGeneric
// CHECK-NEXT:    store %13 to %0 : $*ConformsWithMoreGeneric
// CHECK-NEXT:    %15 = tuple ()
// CHECK-NEXT:    dealloc_stack %11#0 : $*@local_storage ConformsWithMoreGeneric
// CHECK-NEXT:    dealloc_stack %9#0 : $*@local_storage ConformsWithMoreGeneric
// CHECK-NEXT:    dealloc_stack %7#0 : $*@local_storage ConformsWithMoreGeneric
// CHECK-NEXT:    return %15 : $()
// CHECK-NEXT:  }

protocol LabeledRequirement {
  func method(x x: Loadable)
}

struct UnlabeledWitness : LabeledRequirement {
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses16UnlabeledWitnessS_18LabeledRequirementS_FS1_6method{{.*}} : $@convention(witness_method) (Loadable, @in_guaranteed UnlabeledWitness) -> ()
  func method(x _: Loadable) {}
}

protocol LabeledSelfRequirement {
  func method(x x: Self)
}

struct UnlabeledSelfWitness : LabeledSelfRequirement {
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses20UnlabeledSelfWitnessS_22LabeledSelfRequirementS_FS1_6method{{.*}} : $@convention(witness_method) (@in UnlabeledSelfWitness, @in_guaranteed UnlabeledSelfWitness) -> ()
  func method(x _: UnlabeledSelfWitness) {}
}

protocol UnlabeledRequirement {
  func method(x _: Loadable)
}

struct LabeledWitness : UnlabeledRequirement {
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses14LabeledWitnessS_20UnlabeledRequirementS_FS1_6method{{.*}} : $@convention(witness_method) (Loadable, @in_guaranteed LabeledWitness) -> ()
  func method(x x: Loadable) {}
}

protocol UnlabeledSelfRequirement {
  func method(_: Self)
}

struct LabeledSelfWitness : UnlabeledSelfRequirement {
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses18LabeledSelfWitnessS_24UnlabeledSelfRequirementS_FS1_6method{{.*}} : $@convention(witness_method) (@in LabeledSelfWitness, @in_guaranteed LabeledSelfWitness) -> ()
  func method(x: LabeledSelfWitness) {}
}

protocol ReadOnlyRequirement {
  var prop: String { get }
  static var prop: String { get }
}

struct ImmutableModel: ReadOnlyRequirement {
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses14ImmutableModelS_19ReadOnlyRequirementS_FS1_g4propSS : $@convention(witness_method) (@in_guaranteed ImmutableModel) -> @owned String
  let prop: String = "a"
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses14ImmutableModelS_19ReadOnlyRequirementS_ZFS1_g4propSS : $@convention(witness_method) (@thick ImmutableModel.Type) -> @owned String
  static let prop: String = "b"
}

protocol FailableRequirement {
  init?(foo: Int)
}

protocol NonFailableRefinement: FailableRequirement {
  init(foo: Int)
}

protocol IUOFailableRequirement {
  init!(foo: Int)
}

struct NonFailableModel: FailableRequirement, NonFailableRefinement, IUOFailableRequirement {
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses16NonFailableModelS_19FailableRequirementS_FS1_C{{.*}} : $@convention(witness_method) (@out Optional<NonFailableModel>, Int, @thick NonFailableModel.Type) -> ()
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses16NonFailableModelS_21NonFailableRefinementS_FS1_C{{.*}} : $@convention(witness_method) (@out NonFailableModel, Int, @thick NonFailableModel.Type) -> ()
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses16NonFailableModelS_22IUOFailableRequirementS_FS1_C{{.*}} : $@convention(witness_method) (@out ImplicitlyUnwrappedOptional<NonFailableModel>, Int, @thick NonFailableModel.Type) -> ()
  init(foo: Int) {}
}

struct FailableModel: FailableRequirement, IUOFailableRequirement {
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses13FailableModelS_19FailableRequirementS_FS1_C{{.*}}

  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses13FailableModelS_22IUOFailableRequirementS_FS1_C{{.*}}
  // CHECK: bb0([[SELF:%[0-9]+]] : $*ImplicitlyUnwrappedOptional<FailableModel>, [[FOO:%[0-9]+]] : $Int, [[META:%[0-9]+]] : $@thick FailableModel.Type):
  // CHECK: [[FN:%.*]] = function_ref @_TFV9witnesses13FailableModelC{{.*}}
  // CHECK: [[INNER:%.*]] = apply [[FN]](
  // CHECK: [[OUTER:%.*]] = unchecked_trivial_bit_cast [[INNER]] : $Optional<FailableModel> to $ImplicitlyUnwrappedOptional<FailableModel>
  // CHECK: store [[OUTER]] to %0
  // CHECK: return
  init?(foo: Int) {}
}

struct IUOFailableModel : NonFailableRefinement, IUOFailableRequirement {
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWV9witnesses16IUOFailableModelS_21NonFailableRefinementS_FS1_C{{.*}}
  // CHECK: bb0([[SELF:%[0-9]+]] : $*IUOFailableModel, [[FOO:%[0-9]+]] : $Int, [[META:%[0-9]+]] : $@thick IUOFailableModel.Type):
  // CHECK:   [[META:%[0-9]+]] = metatype $@thin IUOFailableModel.Type
  // CHECK:   [[INIT:%[0-9]+]] = function_ref @_TFV9witnesses16IUOFailableModelC{{.*}} : $@convention(thin) (Int, @thin IUOFailableModel.Type) -> ImplicitlyUnwrappedOptional<IUOFailableModel>
  // CHECK:   [[IUO_RESULT:%[0-9]+]] = apply [[INIT]]([[FOO]], [[META]]) : $@convention(thin) (Int, @thin IUOFailableModel.Type) -> ImplicitlyUnwrappedOptional<IUOFailableModel>
  // CHECK:   [[IUO_RESULT_TEMP:%[0-9]+]] = alloc_stack $ImplicitlyUnwrappedOptional<IUOFailableModel>
  // CHECK:   store [[IUO_RESULT]] to [[IUO_RESULT_TEMP]]#1 : $*ImplicitlyUnwrappedOptional<IUOFailableModel>
  
  // CHECK:   [[FORCE_FN:%[0-9]+]] = function_ref @_TFs36_getImplicitlyUnwrappedOptionalValue{{.*}} : $@convention(thin) <τ_0_0> (@out τ_0_0, @in ImplicitlyUnwrappedOptional<τ_0_0>) -> ()
  // CHECK:   [[RESULT_TEMP:%[0-9]+]] = alloc_stack $IUOFailableModel
  // CHECK:   apply [[FORCE_FN]]<IUOFailableModel>([[RESULT_TEMP]]#1, [[IUO_RESULT_TEMP]]#1) : $@convention(thin) <τ_0_0> (@out τ_0_0, @in ImplicitlyUnwrappedOptional<τ_0_0>) -> ()
  // CHECK:   [[RESULT:%[0-9]+]] = load [[RESULT_TEMP]]#1 : $*IUOFailableModel
  // CHECK:   store [[RESULT]] to [[SELF]] : $*IUOFailableModel
  // CHECK:   dealloc_stack [[RESULT_TEMP]]#0 : $*@local_storage IUOFailableModel
  // CHECK:   dealloc_stack [[IUO_RESULT_TEMP]]#0 : $*@local_storage ImplicitlyUnwrappedOptional<IUOFailableModel>
  // CHECK:   return
  init!(foo: Int) { return nil }
}

protocol FailableClassRequirement: class {
  init?(foo: Int)
}

protocol NonFailableClassRefinement: FailableClassRequirement {
  init(foo: Int)
}

protocol IUOFailableClassRequirement: class {
  init!(foo: Int)
}

final class NonFailableClassModel: FailableClassRequirement, NonFailableClassRefinement, IUOFailableClassRequirement {
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses21NonFailableClassModelS_24FailableClassRequirementS_FS1_C{{.*}} : $@convention(witness_method) (Int, @thick NonFailableClassModel.Type) -> @owned Optional<NonFailableClassModel>
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses21NonFailableClassModelS_26NonFailableClassRefinementS_FS1_C{{.*}} : $@convention(witness_method) (Int, @thick NonFailableClassModel.Type) -> @owned NonFailableClassModel
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses21NonFailableClassModelS_27IUOFailableClassRequirementS_FS1_C{{.*}} : $@convention(witness_method) (Int, @thick NonFailableClassModel.Type) -> @owned ImplicitlyUnwrappedOptional<NonFailableClassModel>
  init(foo: Int) {}
}

final class FailableClassModel: FailableClassRequirement, IUOFailableClassRequirement {
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses18FailableClassModelS_24FailableClassRequirementS_FS1_C{{.*}}

  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses18FailableClassModelS_27IUOFailableClassRequirementS_FS1_C{{.*}}
  // CHECK: [[FUNC:%.*]] = function_ref @_TFC9witnesses18FailableClassModelC{{.*}}
  // CHECK: [[INNER:%.*]] = apply [[FUNC]](%0, %1)
  // CHECK: [[OUTER:%.*]] = unchecked_ref_cast [[INNER]] : $Optional<FailableClassModel> to $ImplicitlyUnwrappedOptional<FailableClassModel>
  // CHECK: return [[OUTER]] : $ImplicitlyUnwrappedOptional<FailableClassModel>
  init?(foo: Int) {}
}

final class IUOFailableClassModel: NonFailableClassRefinement, IUOFailableClassRequirement {
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses21IUOFailableClassModelS_26NonFailableClassRefinementS_FS1_C{{.*}}
  // CHECK: function_ref @_TFs36_getImplicitlyUnwrappedOptionalValue{{.*}}
  // CHECK: return [[RESULT:%[0-9]+]] : $IUOFailableClassModel

  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses21IUOFailableClassModelS_27IUOFailableClassRequirementS_FS1_C{{.*}}
  init!(foo: Int) {}

  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses21IUOFailableClassModelS_24FailableClassRequirementS_FS1_C{{.*}}
  // CHECK: [[FUNC:%.*]] = function_ref @_TFC9witnesses21IUOFailableClassModelC{{.*}}
  // CHECK: [[INNER:%.*]] = apply [[FUNC]](%0, %1)
  // CHECK: [[OUTER:%.*]] = unchecked_ref_cast [[INNER]] : $ImplicitlyUnwrappedOptional<IUOFailableClassModel> to $Optional<IUOFailableClassModel>
  // CHECK: return [[OUTER]] : $Optional<IUOFailableClassModel>
}

protocol HasAssoc {
  typealias Assoc
}

protocol GenericParameterNameCollisionType {
  func foo<T>(x: T)
  typealias Assoc2
  func bar<T>(x: T -> Assoc2)
}

struct GenericParameterNameCollision<T: HasAssoc> :
    GenericParameterNameCollisionType {

  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTW{{.*}}GenericParameterNameCollision{{.*}}GenericParameterNameCollisionType{{.*}}foo{{.*}} : $@convention(witness_method) <T1 where T1 : HasAssoc><T> (@in T, @in_guaranteed GenericParameterNameCollision<T1>) -> () {
  // CHECK:       bb0(%0 : $*T, %1 : $*GenericParameterNameCollision<T1>):
  // CHECK:         apply {{%.*}}<T1, T1.Assoc, T>
  func foo<U>(x: U) {}

  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTW{{.*}}GenericParameterNameCollision{{.*}}GenericParameterNameCollisionType{{.*}}bar{{.*}} : $@convention(witness_method) <T1 where T1 : HasAssoc><T> (@owned @callee_owned (@out T1.Assoc, @in T) -> (), @in_guaranteed GenericParameterNameCollision<T1>) -> () {
  // CHECK:       bb0(%0 : $@callee_owned (@out T1.Assoc, @in T) -> (), %1 : $*GenericParameterNameCollision<T1>):
  // CHECK:         apply {{%.*}}<T1, T1.Assoc, T>
  func bar<V>(x: V -> T.Assoc) {}
}

protocol PropertyRequirement {
  var width: Int { get set }
  static var height: Int { get set }
  var depth: Int { get set }
}

class PropertyRequirementBase {
  var width: Int = 12
  static var height: Int = 13
}

class PropertyRequirementWitnessFromBase : PropertyRequirementBase, PropertyRequirement {
  var depth: Int = 14

  // Make sure the contravariant return type in materializeForSet works correctly

  // If the witness is in a base class of the conforming class, make sure we have a bit_cast in there:

  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses34PropertyRequirementWitnessFromBaseS_19PropertyRequirementS_FS1_m5widthSi : {{.*}} {
  // CHECK: upcast
  // CHECK-NEXT: [[METH:%.*]] = class_method {{%.*}} : $PropertyRequirementBase, #PropertyRequirementBase.width!materializeForSet.1
  // CHECK-NEXT: [[RES:%.*]] = apply [[METH]]
  // CHECK-NEXT: [[CAR:%.*]] = tuple_extract [[RES]] : $({{.*}}), 0
  // CHECK-NEXT: [[CADR:%.*]] = tuple_extract [[RES]] : $({{.*}}), 1
  // CHECK-NEXT: [[CAST:%.*]] = unchecked_trivial_bit_cast [[CADR]]
  // CHECK-NEXT: [[TUPLE:%.*]] = tuple ([[CAR]] : {{.*}}, [[CAST]] : {{.*}})
  // CHECK-NEXT: strong_release
  // CHECK-NEXT: return [[TUPLE]]

  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses34PropertyRequirementWitnessFromBaseS_19PropertyRequirementS_ZFS1_m6heightSi : {{.*}} {
  // CHECK: [[OBJ:%.*]] = upcast %2 : $@thick PropertyRequirementWitnessFromBase.Type to $@thick PropertyRequirementBase.Type
  // CHECK: [[METH:%.*]] = function_ref @_TZFC9witnesses23PropertyRequirementBasem6heightSi
  // CHECK-NEXT: [[RES:%.*]] = apply [[METH]]
  // CHECK-NEXT: [[CAR:%.*]] = tuple_extract [[RES]] : $({{.*}}), 0
  // CHECK-NEXT: [[CADR:%.*]] = tuple_extract [[RES]] : $({{.*}}), 1
  // CHECK-NEXT: [[CAST:%.*]] = unchecked_trivial_bit_cast [[CADR]]
  // CHECK-NEXT: [[TUPLE:%.*]] = tuple ([[CAR]] : {{.*}}, [[CAST]] : {{.*}})
  // CHECK-NEXT: return [[TUPLE]]

  // Otherwise, we shouldn't need the bit_cast:

  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWC9witnesses34PropertyRequirementWitnessFromBaseS_19PropertyRequirementS_FS1_m5depthSi
  // CHECK: [[METH:%.*]] = class_method {{%.*}} : $PropertyRequirementWitnessFromBase, #PropertyRequirementWitnessFromBase.depth!materializeForSet.1
  // CHECK-NEXT: [[RES:%.*]] = apply [[METH]]
  // CHECK-NEXT: strong_release
  // CHECK-NEXT: return [[RES]]
}
