// RUN: %target-swift-frontend -emit-silgen %s | FileCheck %s

// Check SILGen against various FunctionConversionExprs emitted by Sema.

// ==== Representation conversions

// CHECK-LABEL: sil hidden @_TF19function_conversion7cToFuncFcSiSiFSiSi : $@convention(thin) (@convention(c) (Int) -> Int) -> @owned @callee_owned (Int) -> Int
// CHECK:         [[THUNK:%.*]] = function_ref @_TTRXFtCc_dSi_dSi_XFo_dSi_dSi_
// CHECK:         [[FUNC:%.*]] = partial_apply [[THUNK]](%0)
// CHECK:         return [[FUNC]]
func cToFunc(arg: @convention(c) Int -> Int) -> Int -> Int {
  return arg
}

// CHECK-LABEL: sil hidden @_TF19function_conversion8cToBlockFcSiSibSiSi : $@convention(thin) (@convention(c) (Int) -> Int) -> @owned @convention(block) (Int) -> Int
// CHECK:         [[BLOCK_STORAGE:%.*]] = alloc_stack $@block_storage
// CHECK:         [[BLOCK:%.*]] = init_block_storage_header [[BLOCK_STORAGE]]
// CHECK:         [[COPY:%.*]] = copy_block [[BLOCK]] : $@convention(block) (Int) -> Int
// CHECK:         return [[COPY]]
func cToBlock(arg: @convention(c) Int -> Int) -> @convention(block) Int -> Int {
  return arg
}

// ==== Throws variance

// CHECK-LABEL: sil hidden @_TF19function_conversion12funcToThrowsFFT_T_FzT_T_ : $@convention(thin) (@owned @callee_owned () -> ()) -> @owned @callee_owned () -> @error ErrorType
// CHECK:         [[FUNC:%.*]] = convert_function %0 : $@callee_owned () -> () to $@callee_owned () -> @error ErrorType
// CHECK:         return [[FUNC]]
func funcToThrows(x: () -> ()) -> () throws -> () {
  return x
}

// CHECK-LABEL: sil hidden @_TF19function_conversion12thinToThrowsFXfT_T_XfzT_T_ : $@convention(thin) (@convention(thin) () -> ()) -> @convention(thin) () -> @error ErrorType
// CHECK:         [[FUNC:%.*]] = convert_function %0 : $@convention(thin) () -> () to $@convention(thin) () -> @error ErrorType
// CHECK:         return [[FUNC]] : $@convention(thin) () -> @error ErrorType
func thinToThrows(x: @convention(thin) () -> ()) -> @convention(thin) () throws -> () {
  return x
}

// FIXME: triggers an assert because we always do a thin to thick conversion on DeclRefExprs
/*
func thinFunc() {}

func thinToThrows() {
  let _: @convention(thin) () -> () = thinFunc
}
*/

// ==== Class downcasts and upcasts

class Feral {}
class Domesticated : Feral {}

// CHECK-LABEL: sil hidden @_TF19function_conversion12funcToUpcastFFT_CS_12DomesticatedFT_CS_5Feral : $@convention(thin) (@owned @callee_owned () -> @owned Domesticated) -> @owned @callee_owned () -> @owned Feral
// CHECK:         [[FUNC:%.*]] = convert_function %0 : $@callee_owned () -> @owned Domesticated to $@callee_owned () -> @owned Feral
// CHECK:         return [[FUNC]]
func funcToUpcast(x: () -> Domesticated) -> () -> Feral {
  return x
}

// CHECK-LABEL: sil hidden @_TF19function_conversion12funcToUpcastFFCS_5FeralT_FCS_12DomesticatedT_ : $@convention(thin) (@owned @callee_owned (@owned Feral) -> ()) -> @owned @callee_owned (@owned Domesticated) -> ()
// CHECK:         [[FUNC:%.*]] = convert_function %0 : $@callee_owned (@owned Feral) -> () to $@callee_owned (@owned Domesticated) -> () // user: %3
// CHECK:         return [[FUNC]]
func funcToUpcast(x: Feral -> ()) -> Domesticated -> () {
  return x
}

// ==== Optionals

struct Trivial {
  let n: Int8
}

class C {
  let n: Int8

  init(n: Int8) {
    self.n = n
  }
}

struct Loadable {
  let c: C

  var n: Int8 {
    return c.n
  }

  init(n: Int8) {
    c = C(n: n)
  }
}

struct AddrOnly {
  let a: Any

  var n: Int8 {
    return a as! Int8
  }

  init(n: Int8) {
    a = n
  }
}

// CHECK-LABEL: sil hidden @_TF19function_conversion19convOptionalTrivialFFGSqVS_7Trivial_S0_T_
func convOptionalTrivial(t1: Trivial? -> Trivial) {
// CHECK:         function_ref @_TTRXFo_dGSqV19function_conversion7Trivial__dS0__XFo_dS0__dGSqS0___
// CHECK:         partial_apply
  let _: Trivial -> Trivial? = t1

// CHECK:         function_ref @_TTRXFo_dGSqV19function_conversion7Trivial__dS0__XFo_dGSQS0___dGSqS0___
// CHECK:         partial_apply
  let _: Trivial! -> Trivial? = t1
}

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_dGSqV19function_conversion7Trivial__dS0__XFo_dS0__dGSqS0___ : $@convention(thin) (Trivial, @owned @callee_owned (Optional<Trivial>) -> Trivial) -> Optional<Trivial>
// CHECK:         enum $Optional<Trivial>
// CHECK-NEXT:    apply %1(%2)
// CHECK-NEXT:    enum $Optional<Trivial>
// CHECK-NEXT:    return

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_dGSqV19function_conversion7Trivial__dS0__XFo_dGSQS0___dGSqS0___ : $@convention(thin) (ImplicitlyUnwrappedOptional<Trivial>, @owned @callee_owned (Optional<Trivial>) -> Trivial) -> Optional<Trivial>
// CHECK:         unchecked_trivial_bit_cast %0 : $ImplicitlyUnwrappedOptional<Trivial> to $Optional<Trivial>
// CHECK-NEXT:    apply %1(%2)
// CHECK-NEXT:    enum $Optional<Trivial>
// CHECK-NEXT:    return

// CHECK-LABEL: sil hidden @_TF19function_conversion20convOptionalLoadableFFGSqVS_8Loadable_S0_T_
func convOptionalLoadable(l1: Loadable? -> Loadable) {
// CHECK:         function_ref @_TTRXFo_oGSqV19function_conversion8Loadable__oS0__XFo_oS0__oGSqS0___
// CHECK:         partial_apply
  let _: Loadable -> Loadable? = l1

// CHECK:         function_ref @_TTRXFo_oGSqV19function_conversion8Loadable__oS0__XFo_oGSQS0___oGSqS0___
// CHECK:         partial_apply
  let _: Loadable! -> Loadable? = l1
}

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_oGSqV19function_conversion8Loadable__oS0__XFo_oGSQS0___oGSqS0___ : $@convention(thin) (@owned ImplicitlyUnwrappedOptional<Loadable>, @owned @callee_owned (@owned Optional<Loadable>) -> @owned Loadable) -> @owned Optional<Loadable>
// CHECK:         unchecked_bitwise_cast %0 : $ImplicitlyUnwrappedOptional<Loadable> to $Optional<Loadable>
// CHECK-NEXT:    apply %1(%2)
// CHECK-NEXT:    enum $Optional<Loadable>
// CHECK-NEXT:    return

// CHECK-LABEL: sil hidden @_TF19function_conversion20convOptionalAddrOnlyFFGSqVS_8AddrOnly_S0_T_
func convOptionalAddrOnly(a1: AddrOnly? -> AddrOnly) {
// CHECK:         function_ref @_TTRXFo_iGSqV19function_conversion8AddrOnly__iS0__XFo_iGSqS0___iGSqS0___
// CHECK:         partial_apply
  let _: AddrOnly? -> AddrOnly? = a1

// CHECK:         function_ref @_TTRXFo_iGSqV19function_conversion8AddrOnly__iS0__XFo_iGSQS0___iGSqS0___
// CHECK:         partial_apply
  let _: AddrOnly! -> AddrOnly? = a1
}

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_iGSqV19function_conversion8AddrOnly__iS0__XFo_iGSqS0___iGSqS0___ : $@convention(thin) (@out Optional<AddrOnly>, @in Optional<AddrOnly>, @owned @callee_owned (@out AddrOnly, @in Optional<AddrOnly>) -> ()) -> ()
// CHECK:         alloc_stack $AddrOnly
// CHECK-NEXT:    apply %2(%3#1, %1)
// CHECK-NEXT:    init_enum_data_addr %0 : $*Optional<AddrOnly>
// CHECK-NEXT:    copy_addr [take] {{.*}} to [initialization] {{.*}} : $*AddrOnly
// CHECK-NEXT:    inject_enum_addr %0 : $*Optional<AddrOnly>
// CHECK-NEXT:    dealloc_stack {{.*}} : $*@local_storage AddrOnly
// CHECK-NEXT:    return

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_iGSqV19function_conversion8AddrOnly__iS0__XFo_iGSQS0___iGSqS0___ : $@convention(thin) (@out Optional<AddrOnly>, @in ImplicitlyUnwrappedOptional<AddrOnly>, @owned @callee_owned (@out AddrOnly, @in Optional<AddrOnly>) -> ()) -> ()
// CHECK:         alloc_stack $Optional<AddrOnly>
// CHECK-NEXT:    unchecked_addr_cast %1 : $*ImplicitlyUnwrappedOptional<AddrOnly> to $*Optional<AddrOnly>
// CHECK-NEXT:    copy_addr [take] {{.*}} to [initialization] {{.*}} : $*Optional<AddrOnly>
// CHECK-NEXT:    alloc_stack $AddrOnly
// CHECK-NEXT:    apply %2(%6#1, %3#1)
// CHECK-NEXT:    init_enum_data_addr %0 : $*Optional<AddrOnly>
// CHECK-NEXT:    copy_addr [take] {{.*}} to [initialization] {{.*}} : $*AddrOnly
// CHECK-NEXT:    inject_enum_addr %0 : $*Optional<AddrOnly>
// CHECK-NEXT:    dealloc_stack {{.*}} : $*@local_storage AddrOnly
// CHECK-NEXT:    dealloc_stack {{.*}} : $*@local_storage Optional<AddrOnly>
// CHECK-NEXT:    return

// ==== Existentials

protocol Q {
  var n: Int8 { get }
}

protocol P : Q {}

extension Trivial : P {}
extension Loadable : P {}
extension AddrOnly : P {}

// CHECK-LABEL: sil hidden @_TF19function_conversion22convExistentialTrivialFTFPS_1Q_VS_7Trivial2t3FGSqPS0___S1__T_
func convExistentialTrivial(t2: Q -> Trivial, t3: Q? -> Trivial) {
// CHECK:         function_ref @_TTRXFo_iP19function_conversion1Q__dVS_7Trivial_XFo_dS1__iPS_1P__
// CHECK:         partial_apply
  let _: Trivial -> P = t2

// CHECK:         function_ref @_TTRXFo_iGSqP19function_conversion1Q___dVS_7Trivial_XFo_dGSqS1___iPS_1P__
// CHECK:         partial_apply
  let _: Trivial? -> P = t3

// CHECK:         function_ref @_TTRXFo_iP19function_conversion1Q__dVS_7Trivial_XFo_iPS_1P__iPS2___
// CHECK:         partial_apply
  let _: P -> P = t2
}

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_iP19function_conversion1Q__dVS_7Trivial_XFo_dS1__iPS_1P__ : $@convention(thin) (@out P, Trivial, @owned @callee_owned (@in Q) -> Trivial) -> ()
// CHECK:         alloc_stack $Q
// CHECK-NEXT:    init_existential_addr
// CHECK-NEXT:    store
// CHECK-NEXT:    apply
// CHECK-NEXT:    init_existential_addr
// CHECK-NEXT:    store
// CHECK:         return

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_iGSqP19function_conversion1Q___dVS_7Trivial_XFo_dGSqS1___iPS_1P__
// CHECK:         select_enum
// CHECK:         cond_br
// CHECK: bb1:
// CHECK:         unchecked_enum_data
// CHECK:         init_existential_addr
// CHECK:         init_enum_data_addr
// CHECK:         copy_addr
// CHECK:         inject_enum_addr
// CHECK: bb2:
// CHECK:         inject_enum_addr
// CHECK: bb3:
// CHECK:         apply
// CHECK:         init_existential_addr
// CHECK:         store
// CHECK:         return

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_iP19function_conversion1Q__dVS_7Trivial_XFo_iPS_1P__iPS2___ : $@convention(thin) (@out P, @in P, @owned @callee_owned (@in Q) -> Trivial) -> ()
// CHECK:         alloc_stack $Q
// CHECK-NEXT:    open_existential_addr %1 : $*P
// CHECK-NEXT:    init_existential_addr %3#1 : $*Q
// CHECK-NEXT:    copy_addr [take] {{.*}} to [initialization] {{.*}}
// CHECK-NEXT:    apply
// CHECK-NEXT:    init_existential_addr
// CHECK-NEXT:    store
// CHECK:         deinit_existential_addr
// CHECK:         return

// ==== Existential metatypes

// CHECK-LABEL: sil hidden @_TF19function_conversion23convExistentialMetatypeFFGSqPMPS_1Q__MVS_7TrivialT_
func convExistentialMetatype(em: Q.Type? -> Trivial.Type) {
// CHECK:         function_ref @_TTRXFo_dGSqPMP19function_conversion1Q___dXMtVS_7Trivial_XFo_dXMtS1__dXPMTPS_1P__
// CHECK:         partial_apply
  let _: Trivial.Type -> P.Type = em

// CHECK:         function_ref @_TTRXFo_dGSqPMP19function_conversion1Q___dXMtVS_7Trivial_XFo_dGSqMS1___dXPMTPS_1P__
// CHECK:         partial_apply
  let _: Trivial.Type? -> P.Type = em

// CHECK:         function_ref @_TTRXFo_dGSqPMP19function_conversion1Q___dXMtVS_7Trivial_XFo_dXPMTPS_1P__dXPMTPS2___
// CHECK:         partial_apply
  let _: P.Type -> P.Type = em
}

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_dGSqPMP19function_conversion1Q___dXMtVS_7Trivial_XFo_dXMtS1__dXPMTPS_1P__ : $@convention(thin) (@thin Trivial.Type, @owned @callee_owned (Optional<Q.Type>) -> @thin Trivial.Type) -> @thick P.Type
// CHECK:         metatype $@thick Trivial.Type
// CHECK-NEXT:    init_existential_metatype %2 : $@thick Trivial.Type, $@thick Q.Type
// CHECK-NEXT:    enum $Optional<Q.Type>
// CHECK-NEXT:    apply
// CHECK-NEXT:    metatype $@thick Trivial.Type
// CHECK-NEXT:    init_existential_metatype {{.*}} : $@thick Trivial.Type, $@thick P.Type
// CHECK-NEXT:    return

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_dGSqPMP19function_conversion1Q___dXMtVS_7Trivial_XFo_dGSqMS1___dXPMTPS_1P__ : $@convention(thin) (Optional<Trivial.Type>, @owned @callee_owned (Optional<Q.Type>) -> @thin Trivial.Type) -> @thick P.Type
// CHECK:         select_enum %0 : $Optional<Trivial.Type>
// CHECK-NEXT:    cond_br
// CHECK: bb1:
// CHECK-NEXT:    unchecked_enum_data %0 : $Optional<Trivial.Type>
// CHECK-NEXT:    metatype $@thin Trivial.Type
// CHECK-NEXT:    metatype $@thick Trivial.Type
// CHECK-NEXT:    init_existential_metatype {{.*}} : $@thick Trivial.Type, $@thick Q.Type
// CHECK-NEXT:    enum $Optional<Q.Type>
// CHECK: bb2:
// CHECK-NEXT:    enum $Optional<Q.Type>
// CHECK: bb3({{.*}}):
// CHECK-NEXT:    apply
// CHECK-NEXT:    metatype $@thick Trivial.Type
// CHECK-NEXT:    init_existential_metatype {{.*}} : $@thick Trivial.Type, $@thick P.Type
// CHECK-NEXT:    return

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_dGSqPMP19function_conversion1Q___dXMtVS_7Trivial_XFo_dXPMTPS_1P__dXPMTPS2___ : $@convention(thin) (@thick P.Type, @owned @callee_owned (Optional<Q.Type>) -> @thin Trivial.Type) -> @thick P.Type
// CHECK:         open_existential_metatype %0 : $@thick P.Type to $@thick (@opened({{.*}}) P).Type
// CHECK-NEXT:    init_existential_metatype %2 : $@thick (@opened({{.*}}) P).Type, $@thick Q.Type
// CHECK-NEXT:    enum $Optional<Q.Type>
// CHECK-NEXT:    apply
// CHECK-NEXT:    metatype $@thick Trivial.Type
// CHECK-NEXT:    init_existential_metatype {{.*}} : $@thick Trivial.Type, $@thick P.Type
// CHECK-NEXT:    return

// ==== Class metatype upcasts

class Parent {}
class Child : Parent {}

// Note: we add a Trivial => Trivial? conversion here to force a thunk
// to be generated

// CHECK-LABEL: sil hidden @_TF19function_conversion18convUpcastMetatypeFTFTMCS_6ParentGSqVS_7Trivial__MCS_5Child2c5FTGSqMS0__GSqS1___MS2__T_
func convUpcastMetatype(c4: (Parent.Type, Trivial?) -> Child.Type,
                        c5: (Parent.Type?, Trivial?) -> Child.Type) {
// CHECK:         function_ref @_TTRXFo_dXMTC19function_conversion6ParentdGSqVS_7Trivial__dXMTCS_5Child_XFo_dXMTS2_dS1__dXMTS0__
// CHECK:         partial_apply
  let _: (Child.Type, Trivial) -> Parent.Type = c4

// CHECK:         function_ref @_TTRXFo_dGSqMC19function_conversion6Parent_dGSqVS_7Trivial__dXMTCS_5Child_XFo_dXMTS2_dS1__dXMTS0__
// CHECK:         partial_apply
  let _: (Child.Type, Trivial) -> Parent.Type = c5

// CHECK:         function_ref @_TTRXFo_dGSqMC19function_conversion6Parent_dGSqVS_7Trivial__dXMTCS_5Child_XFo_dGSqMS2__dS1__dGSqMS0___
// CHECK:         partial_apply
  let _: (Child.Type?, Trivial) -> Parent.Type? = c5
}

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_dXMTC19function_conversion6ParentdGSqVS_7Trivial__dXMTCS_5Child_XFo_dXMTS2_dS1__dXMTS0__ : $@convention(thin) (@thick Child.Type, Trivial, @owned @callee_owned (@thick Parent.Type, Optional<Trivial>) -> @thick Child.Type) -> @thick Parent.Type
// CHECK:         upcast %0 : $@thick Child.Type to $@thick Parent.Type
// CHECK:         apply
// CHECK:         upcast {{.*}} : $@thick Child.Type to $@thick Parent.Type
// CHECK:         return

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_dGSqMC19function_conversion6Parent_dGSqVS_7Trivial__dXMTCS_5Child_XFo_dXMTS2_dS1__dXMTS0__ : $@convention(thin) (@thick Child.Type, Trivial, @owned @callee_owned (Optional<Parent.Type>, Optional<Trivial>) -> @thick Child.Type) -> @thick Parent.Type
// CHECK:         upcast %0 : $@thick Child.Type to $@thick Parent.Type
// CHECK:         enum $Optional<Parent.Type>
// CHECK:         apply
// CHECK:         upcast {{.*}} : $@thick Child.Type to $@thick Parent.Type
// CHECK:         return

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_dGSqMC19function_conversion6Parent_dGSqVS_7Trivial__dXMTCS_5Child_XFo_dGSqMS2__dS1__dGSqMS0___ : $@convention(thin) (Optional<Child.Type>, Trivial, @owned @callee_owned (Optional<Parent.Type>, Optional<Trivial>) -> @thick Child.Type) -> Optional<Parent.Type>
// CHECK:         unchecked_trivial_bit_cast %0 : $Optional<Child.Type> to $Optional<Parent.Type>
// CHECK:         apply
// CHECK:         upcast {{.*}} : $@thick Child.Type to $@thick Parent.Type
// CHECK:         enum $Optional<Parent.Type>
// CHECK:         return

// ==== Function to existential -- make sure we maximally abstract it

// CHECK-LABEL: sil hidden @_TF19function_conversion19convFuncExistentialFFP_FSiSiT_ : $@convention(thin) (@owned @callee_owned (@in protocol<>) -> @owned @callee_owned (Int) -> Int) -> ()
func convFuncExistential(f1: Any -> Int -> Int) {
// CHECK:         function_ref @_TTRXFo_iP__oXFo_dSi_dSi__XFo_oXFo_dSi_dSi__iP__
// CHECK:         partial_apply %3(%0)
  let _: (Int -> Int) -> Any = f1
}

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_iP__oXFo_dSi_dSi__XFo_oXFo_dSi_dSi__iP__ : $@convention(thin) (@out protocol<>, @owned @callee_owned (Int) -> Int, @owned @callee_owned (@in protocol<>) -> @owned @callee_owned (Int) -> Int) -> ()
// CHECK:         alloc_stack $protocol<>
// CHECK:         function_ref @_TTRXFo_dSi_dSi_XFo_iSi_iSi_
// CHECK-NEXT:    partial_apply
// CHECK-NEXT:    init_existential_addr %3#1 : $*protocol<>, $Int -> Int
// CHECK-NEXT:    store
// CHECK-NEXT:    apply
// CHECK:         function_ref @_TTRXFo_dSi_dSi_XFo_iSi_iSi_
// CHECK-NEXT:    partial_apply
// CHECK-NEXT:    init_existential_addr %0 : $*protocol<>, $Int -> Int
// CHECK-NEXT:    store {{.*}} to {{.*}} : $*@callee_owned (@out Int, @in Int) -> ()
// CHECK:         return

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_dSi_dSi_XFo_iSi_iSi_ : $@convention(thin) (@out Int, @in Int, @owned @callee_owned (Int) -> Int) -> ()
// CHECK:         load %1 : $*Int
// CHECK-NEXT:    apply %2(%3)
// CHECK-NEXT:    store {{.*}} to %0
// CHECK:         return

// ==== Class-bound archetype upcast

// CHECK-LABEL: sil hidden @_TF19function_conversion29convClassBoundArchetypeUpcast
func convClassBoundArchetypeUpcast<T : Parent>(f1: Parent -> (T, Trivial)) {
// CHECK:         function_ref @_TTRGRxC19function_conversion6ParentrXFo_oS0__oTxVS_7Trivial__XFo_ox_oTS0_GSqS1____
// CHECK:         partial_apply
  let _: T -> (Parent, Trivial?) = f1
}

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRGRxC19function_conversion6ParentrXFo_oS0__oTxVS_7Trivial__XFo_ox_oTS0_GSqS1____ : $@convention(thin) <T where T : Parent> (@owned T, @owned @callee_owned (@owned Parent) -> @owned (T, Trivial)) -> @owned (Parent, Optional<Trivial>)
// CHECK:         upcast %0 : $T to $Parent
// CHECK-NEXT:    apply
// CHECK-NEXT:    tuple_extract
// CHECK-NEXT:    tuple_extract
// CHECK-NEXT:    upcast {{.*}} : $T to $Parent
// CHECK-NEXT:    enum $Optional<Trivial>
// CHECK-NEXT:    tuple
// CHECK-NEXT:    return

// CHECK-LABEL: sil hidden @_TF19function_conversion37convClassBoundMetatypeArchetypeUpcast
func convClassBoundMetatypeArchetypeUpcast<T : Parent>(f1: Parent.Type -> (T.Type, Trivial)) {
// CHECK:         function_ref @_TTRGRxC19function_conversion6ParentrXFo_dXMTS0__dTXMTxVS_7Trivial__XFo_dXMTx_dTXMTS0_GSqS1____
// CHECK:         partial_apply
  let _: T.Type -> (Parent.Type, Trivial?) = f1
}

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRGRxC19function_conversion6ParentrXFo_dXMTS0__dTXMTxVS_7Trivial__XFo_dXMTx_dTXMTS0_GSqS1____ : $@convention(thin) <T where T : Parent> (@thick T.Type, @owned @callee_owned (@thick Parent.Type) -> (@thick T.Type, Trivial)) -> (@thick Parent.Type, Optional<Trivial>)
// CHECK:         upcast %0 : $@thick T.Type to $@thick Parent.Type
// CHECK-NEXT:    apply
// CHECK-NEXT:    tuple_extract
// CHECK-NEXT:    tuple_extract
// CHECK-NEXT:    upcast {{.*}} : $@thick T.Type to $@thick Parent.Type
// CHECK-NEXT:    enum $Optional<Trivial>
// CHECK-NEXT:    tuple
// CHECK-NEXT:    return

// ==== Make sure we destructure one-element tuples

// CHECK-LABEL: sil hidden @_TF19function_conversion15convTupleScalarFTFPS_1Q_T_2f2FT6parentPS0___T_2f3FT5tupleGSqTSiSi___T__T_
// CHECK:         function_ref @_TTRXFo_iP19function_conversion1Q__dT__XFo_iPS_1P__dT__
// CHECK:         function_ref @_TTRXFo_iP19function_conversion1Q__dT__XFo_iPS_1P__dT__
// CHECK:         function_ref @_TTRXFo_dGSqTSiSi___dT__XFo_dSidSi_dT__

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_iP19function_conversion1Q__dT__XFo_iPS_1P__dT__ : $@convention(thin) (@in P, @owned @callee_owned (@in Q) -> ()) -> ()

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_dGSqTSiSi___dT__XFo_dSidSi_dT__ : $@convention(thin) (Int, Int, @owned @callee_owned (Optional<(Int, Int)>) -> ()) -> ()

func convTupleScalar(f1: Q -> (),
                     f2: (parent: Q) -> (),
                     f3: (tuple: (Int, Int)?) -> ()) {
  let _: (parent: P) -> () = f1
  let _: P -> () = f2
  let _: (Int, Int) -> () = f3
}

// CHECK-LABEL: sil hidden @_TF19function_conversion21convTupleScalarOpaqueurFFt4argsGSax__T_GSqFt4argsGSax__T__
// CHECK:         function_ref @_TTRGrXFo_oGSax__dT__XFo_it4argsGSax___iT__

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRGrXFo_oGSax__dT__XFo_it4argsGSax___iT__ : $@convention(thin) <T> (@out (), @in (args: T...), @owned @callee_owned (@owned Array<T>) -> ()) -> ()

func convTupleScalarOpaque<T>(f: (args: T...) -> ()) -> ((args: T...) -> ())? {
  return f
}
