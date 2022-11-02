// RUN: %target-swift-frontend -parse-as-library -emit-silgen -disable-objc-attr-requires-foundation-module %s | FileCheck %s

@objc protocol P1 {
  optional func method(x: Int)

  optional var prop: Int { get }

  optional subscript (i: Int) -> Int { get }
}

// CHECK-LABEL: sil hidden @{{.*}}optionalMethodGeneric{{.*}} : $@convention(thin) <T where T : P1> (@owned T) -> ()
func optionalMethodGeneric<T : P1>(t t : T) {
  var t = t
  // CHECK: bb0([[T:%[0-9]+]] : $T):
  // CHECK: [[TBOX:%[0-9]+]] = alloc_box $T
  // CHECK-NEXT: strong_retain [[T]]
  // CHECK-NEXT: store [[T]] to [[TBOX]]#1 : $*T
  // CHECK-NEXT: [[OPT_BOX:%[0-9]+]] = alloc_box $Optional<Int -> ()>
  // CHECK-NEXT: [[T:%[0-9]+]] = load [[TBOX]]#1 : $*T
  // CHECK-NEXT: strong_retain [[T]] : $T
  // CHECK-NEXT: alloc_stack $Optional<Int -> ()>
  // CHECK-NEXT: dynamic_method_br [[T]] : $T, #P1.method!1.foreign
  var methodRef = t.method
}

// CHECK-LABEL: sil hidden @_TF17protocol_optional23optionalPropertyGeneric{{.*}} : $@convention(thin) <T where T : P1> (@owned T) -> ()
func optionalPropertyGeneric<T : P1>(t t : T) {
  var t = t
  // CHECK: bb0([[T:%[0-9]+]] : $T):
  // CHECK: [[TBOX:%[0-9]+]] = alloc_box $T
  // CHECK: strong_retain [[T]]
  // CHECK-NEXT: store [[T]] to [[TBOX]]#1 : $*T
  // CHECK-NEXT: [[OPT_BOX:%[0-9]+]] = alloc_box $Optional<Int>
  // CHECK-NEXT: [[T:%[0-9]+]] = load [[TBOX]]#1 : $*T
  // CHECK-NEXT: strong_retain [[T]] : $T
  // CHECK-NEXT: alloc_stack $Optional<Int>
  // CHECK-NEXT: dynamic_method_br [[T]] : $T, #P1.prop!getter.1.foreign
  var propertyRef = t.prop
}

// CHECK-LABEL: sil hidden @_TF17protocol_optional24optionalSubscriptGeneric{{.*}} : $@convention(thin) <T where T : P1> (@owned T) -> ()
func optionalSubscriptGeneric<T : P1>(t t : T) {
  var t = t
  // CHECK: bb0([[T:%[0-9]+]] : $T):
  // CHECK: [[TBOX:%[0-9]+]] = alloc_box $T
  // CHECK-NEXT: strong_retain [[T]]
  // CHECK-NEXT: store [[T]] to [[TBOX]]#1 : $*T
  // CHECK-NEXT: [[OPT_BOX:%[0-9]+]] = alloc_box $Optional<Int>
  // CHECK-NEXT: [[T:%[0-9]+]] = load [[TBOX]]#1 : $*T
  // CHECK-NEXT: strong_retain [[T]] : $T
  // CHECK: [[INTCONV:%[0-9]+]] = function_ref @_TFSiC
  // CHECK-NEXT: [[INT64:%[0-9]+]] = metatype $@thin Int.Type
  // CHECK-NEXT: [[FIVELIT:%[0-9]+]] = integer_literal $Builtin.Int2048, 5
  // CHECK-NEXT: [[FIVE:%[0-9]+]] = apply [[INTCONV]]([[FIVELIT]], [[INT64]]) : $@convention(thin) (Builtin.Int2048, @thin Int.Type) -> Int
  // CHECK-NEXT: alloc_stack $Optional<Int>
  // CHECK-NEXT: dynamic_method_br [[T]] : $T, #P1.subscript!getter.1.foreign
  var subscriptRef = t[5]
}
