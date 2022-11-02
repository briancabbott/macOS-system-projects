// RUN: %target-swift-frontend -emit-silgen %s | FileCheck %s

func takeFn<T>(f : T -> T?) {}
func liftOptional(x : Int) -> Int? { return x }

func test0() {
  takeFn(liftOptional)
}
// CHECK:    sil hidden @_TF10reabstract5test0FT_T_ : $@convention(thin) () -> () {
// CHECK:      [[T0:%.*]] = function_ref @_TF10reabstract6takeFn
//   Emit a generalized reference to liftOptional.
//   TODO: just emit a globalized thunk
// CHECK-NEXT: reabstract.liftOptional
// CHECK-NEXT: [[T1:%.*]] = function_ref @_TF10reabstract12liftOptional
// CHECK-NEXT: [[T2:%.*]] = thin_to_thick_function [[T1]]
// CHECK-NEXT: reabstraction thunk
// CHECK-NEXT: [[T3:%.*]] = function_ref [[THUNK:@.*]] :
// CHECK-NEXT: [[T4:%.*]] = partial_apply [[T3]]([[T2]])
// CHECK-NEXT: apply [[T0]]<Int>([[T4]])
// CHECK-NEXT: tuple ()
// CHECK-NEXT: return

// CHECK:    sil shared [transparent] [reabstraction_thunk] [[THUNK]] : $@convention(thin) (@out Optional<Int>, @in Int, @owned @callee_owned (Int) -> Optional<Int>) -> () {
// CHECK:      [[T0:%.*]] = load %1 : $*Int
// CHECK-NEXT: [[T1:%.*]] = apply %2([[T0]])
// CHECK-NEXT: store [[T1]] to %0
// CHECK-NEXT: tuple ()
// CHECK-NEXT: return

// CHECK-LABEL: sil hidden @_TF10reabstract10testThrowsFP_T_
// CHECK:         function_ref @_TTRXFo_iT__iT__XFo__dT__
// CHECK:         function_ref @_TTRXFo_iT__iT_zoPs9ErrorType__XFo__dT_zoPS___
func testThrows(x: Any) {
  _ = x as? () -> ()
  _ = x as? () throws -> ()
}

// Make sure that we preserve inout-ness when lowering types with maximum
// abstraction level -- <rdar://problem/21329377>
class C {}

struct Box<T> {
  let t: T
}

func notFun(inout c: C, i: Int) {}

func testInoutOpaque(c: C, i: Int) {
  var c = c
  let box = Box(t: notFun)
  box.t(&c, i: i)
}

// CHECK-LABEL: sil hidden @_TF10reabstract15testInoutOpaqueFTCS_1C1iSi_T_
// CHECK:         function_ref @_TF10reabstract6notFunFTRCS_1C1iSi_T_
// CHECK:         thin_to_thick_function
// CHECK:         function_ref @_TTRXFo_lC10reabstract1CdSi_dT__XFo_lS0_iSi_iT__
// CHECK:         partial_apply
// CHECK:         store
// CHECK:         load
// CHECK:         function_ref @_TTRXFo_lC10reabstract1CiSi_iT__XFo_lS0_dSi_dT__
// CHECK:         partial_apply
// CHECK:         apply

// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_lC10reabstract1CdSi_dT__XFo_lS0_iSi_iT__ : $@convention(thin) (@out (), @inout C, @in Int, @owned @callee_owned (@inout C, Int) -> ()) -> () {
// CHECK-LABEL: sil shared [transparent] [reabstraction_thunk] @_TTRXFo_lC10reabstract1CiSi_iT__XFo_lS0_dSi_dT__ : $@convention(thin) (@inout C, Int, @owned @callee_owned (@out (), @inout C, @in Int) -> ()) -> () {
