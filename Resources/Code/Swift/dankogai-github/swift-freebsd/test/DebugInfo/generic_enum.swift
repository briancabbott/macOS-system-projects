// RUN: %target-swift-frontend %s -emit-ir -g -o - | FileCheck %s

func markUsed<T>(t: T) {}

enum TrivialGeneric<T, U> {
  case x(T, U)
}

func unwrapTrivialGeneric<T, U>(tg: TrivialGeneric<T, U>) -> (T, U) {
  switch tg {
  case .x(let t, let u):
    return (t, u)
  }
}

func wrapTrivialGeneric<T, U>(t: T, u: U) -> TrivialGeneric<T, U> {
  return .x(t, u)
}
// CHECK-DAG: !DIGlobalVariable(name: "tg",{{.*}} line: [[@LINE+2]],{{.*}} type: !"_TtGO12generic_enum14TrivialGenericVs5Int64SS_",{{.*}} isLocal: false, isDefinition: true
// CHECK-DAG: !DICompositeType(tag: DW_TAG_union_type, name: "TrivialGeneric", {{.*}}identifier: "_TtGO12generic_enum14TrivialGenericVs5Int64SS_"
var tg : TrivialGeneric<Int64, String> = .x(23, "skidoo")
switch tg {
case .x(let t, let u):
  markUsed("\(t) \(u)")
}
