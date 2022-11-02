// RUN: %target-swift-frontend -emit-silgen %s | FileCheck %s

protocol P {
  typealias A

  func f(x: A)
}
struct Foo<T>: P {
  typealias A = T.Type

  func f(t: T.Type) {}
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWurGV25dependent_member_lowering3Foox_S_1PS_FS1_1{{.*}} : $@convention(witness_method) <T> (@in @thick T.Type, @in_guaranteed Foo<T>) -> ()
  // CHECK:       bb0(%0 : $*@thick T.Type, %1 : $*Foo<T>):
}
struct Bar<T>: P {
  typealias A = Int -> T

  func f(t: Int -> T) {}
  // CHECK-LABEL: sil hidden [transparent] [thunk] @_TTWurGV25dependent_member_lowering3Barx_S_1PS_FS1_1{{.*}} : $@convention(witness_method) <T> (@in @callee_owned (@out T, @in Int) -> (), @in_guaranteed Bar<T>) -> ()
  // CHECK:       bb0(%0 : $*@callee_owned (@out T, @in Int) -> (), %1 : $*Bar<T>):
}
