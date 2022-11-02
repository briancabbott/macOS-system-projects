// RUN: %target-swift-emit-sil -parse-as-library -module-name back_deploy %s -target %target-cpu-apple-macosx10.50 -verify
// RUN: %target-swift-emit-silgen -parse-as-library -module-name back_deploy %s | %FileCheck %s
// RUN: %target-swift-emit-silgen -parse-as-library -module-name back_deploy %s -target %target-cpu-apple-macosx10.50 | %FileCheck %s
// RUN: %target-swift-emit-silgen -parse-as-library -module-name back_deploy %s -target %target-cpu-apple-macosx10.60 | %FileCheck %s

// REQUIRES: OS=macosx

@available(macOS 10.50, *)
public struct TopLevelStruct {
  // -- Fallback definition for TopLevelStruct.property.getter
  // CHECK-LABEL: sil non_abi [serialized] [ossa] @$s11back_deploy14TopLevelStructV8propertyACvgTwB : $@convention(method) (TopLevelStruct) -> TopLevelStruct
  // CHECK: bb0([[SELF:%.*]] : $TopLevelStruct):
  // CHECK:   return [[SELF]] : $TopLevelStruct

  // -- Back deployment thunk for TopLevelStruct.property.getter
  // CHECK-LABEL: sil non_abi [serialized] [thunk] [ossa] @$s11back_deploy14TopLevelStructV8propertyACvgTwb : $@convention(method) (TopLevelStruct) -> TopLevelStruct
  // CHECK: bb0([[BB0_ARG:%.*]] : $TopLevelStruct):
  // CHECK:   [[MAJOR:%.*]] = integer_literal $Builtin.Word, 10
  // CHECK:   [[MINOR:%.*]] = integer_literal $Builtin.Word, 52
  // CHECK:   [[PATCH:%.*]] = integer_literal $Builtin.Word, 0
  // CHECK:   [[OSVFN:%.*]] = function_ref @$ss26_stdlib_isOSVersionAtLeastyBi1_Bw_BwBwtF : $@convention(thin) (Builtin.Word, Builtin.Word, Builtin.Word) -> Builtin.Int1
  // CHECK:   [[AVAIL:%.*]] = apply [[OSVFN]]([[MAJOR]], [[MINOR]], [[PATCH]]) : $@convention(thin) (Builtin.Word, Builtin.Word, Builtin.Word) -> Builtin.Int1
  // CHECK:   cond_br [[AVAIL]], [[AVAIL_BB:bb[0-9]+]], [[UNAVAIL_BB:bb[0-9]+]]
  //
  // CHECK: [[UNAVAIL_BB]]:
  // CHECK:   [[FALLBACKFN:%.*]] = function_ref @$s11back_deploy14TopLevelStructV8propertyACvgTwB : $@convention(method) (TopLevelStruct) -> TopLevelStruct
  // CHECK:   [[FALLBACKRES:%.*]] = apply [[FALLBACKFN]]([[BB0_ARG]]) : $@convention(method) (TopLevelStruct) -> TopLevelStruct
  // CHECK:   br [[RETURN_BB:bb[0-9]+]]([[FALLBACKRES]] : $TopLevelStruct)
  //
  // CHECK: [[AVAIL_BB]]:
  // CHECK:   [[ORIGFN:%.*]] = function_ref @$s11back_deploy14TopLevelStructV8propertyACvg : $@convention(method) (TopLevelStruct) -> TopLevelStruct
  // CHECK:   [[ORIGRES:%.*]] = apply [[ORIGFN]]([[BB0_ARG]]) : $@convention(method) (TopLevelStruct) -> TopLevelStruct
  // CHECK:   br [[RETURN_BB]]([[ORIGRES]] : $TopLevelStruct)
  //
  // CHECK: [[RETURN_BB]]([[RETURN_BB_ARG:%.*]] : $TopLevelStruct)
  // CHECK:   return [[RETURN_BB_ARG]] : $TopLevelStruct

  // -- Original definition of TopLevelStruct.property.getter
  // CHECK-LABEL: sil [available 10.52] [ossa] @$s11back_deploy14TopLevelStructV8propertyACvg : $@convention(method) (TopLevelStruct) -> TopLevelStruct
  @available(macOS 10.51, *)
  @_backDeploy(before: macOS 10.52)
  public var property: TopLevelStruct { self }
}

// CHECK-LABEL: sil hidden [available 10.51] [ossa] @$s11back_deploy6calleryyAA14TopLevelStructVF : $@convention(thin) (TopLevelStruct) -> ()
// CHECK: bb0([[STRUCT_ARG:%.*]] : $TopLevelStruct):
@available(macOS 10.51, *)
func caller(_ s: TopLevelStruct) {
  // -- Verify the thunk is called
  // CHECK: {{%.*}} = function_ref @$s11back_deploy14TopLevelStructV8propertyACvgTwb : $@convention(method) (TopLevelStruct) -> TopLevelStruct
  _ = s.property
}
