// RUN: %target-swift-frontend -parse-stdlib -enable-experimental-move-only -module-name Swift -enable-sil-opaque-values -parse-as-library -emit-sil -Onone %s | %FileCheck %s

// Like opaque_values_Onone.swift but for code that needs to be compiled with 
// -parse-stdlib.

precedencegroup AssignmentPrecedence { assignment: true }
precedencegroup CastingPrecedence {}

public protocol _ObjectiveCBridgeable {}

public protocol _ExpressibleByBuiltinBooleanLiteral {
  init(_builtinBooleanLiteral value: Builtin.Int1)
}
struct Bool : _ExpressibleByBuiltinBooleanLiteral {
  var _value: Builtin.Int1
  @_silgen_name("Bool_init_noargs")
  init()
  init(_ v: Builtin.Int1) { self._value = v }
  init(_ value: Bool) { self = value }
  init(_builtinBooleanLiteral value: Builtin.Int1) {
    self._value = value
  }
}

@_silgen_name("typeof")
@_semantics("typechecker.type(of:)")
public func type<T, Metatype>(of value: T) -> Metatype

class X {}
func consume(_ x : __owned X) {}

func foo(@_noImplicitCopy _ x: __owned X) {
  consume(_copy(x))
  consume(x)
}

// CHECK-LABEL: sil [transparent] [_semantics "lifetimemanagement.copy"] @_copy : {{.*}} {
// CHECK:       {{bb[0-9]+}}([[OUT_ADDR:%[^,]+]] : $*T, [[IN_ADDR:%[^,]+]] : $*T):
// CHECK:         [[TMP_ADDR:%[^,]+]] = alloc_stack $T
// CHECK:         copy_addr [[IN_ADDR]] to [init] [[TMP_ADDR]] : $*T
// CHECK:         [[REGISTER_5:%[^,]+]] = builtin "copy"<T>([[OUT_ADDR]] : $*T, [[TMP_ADDR]] : $*T) : $()
// CHECK:         dealloc_stack [[TMP_ADDR]] : $*T
// CHECK:         return {{%[^,]+}} : $()
// CHECK-LABEL: } // end sil function '_copy'
@_transparent
@_semantics("lifetimemanagement.copy")
@_silgen_name("_copy")
public func _copy<T>(_ value: T) -> T {
  #if $BuiltinCopy
    Builtin.copy(value)
  #else
    value
  #endif
}

// CHECK-LABEL: sil hidden @getRawPointer : {{.*}} {
// CHECK:       {{bb[0-9]+}}([[ADDR:%[^,]+]] : $*T):
// CHECK:         [[PTR:%[^,]+]] = address_to_pointer [stack_protection] [[ADDR]]
// CHECK:         return [[PTR]]
// CHECK-LABEL: } // end sil function 'getRawPointer'
@_silgen_name("getRawPointer")
func getRawPointer<T>(to value: T) -> Builtin.RawPointer {
  return Builtin.addressOfBorrow(value)
}

// CHECK-LABEL: sil hidden @getUnprotectedRawPointer : {{.*}} {
// CHECK:       {{bb[0-9]+}}([[ADDR:%[^,]+]] : $*T):
// CHECK:         [[PTR:%[^,]+]] = address_to_pointer [[ADDR]]
// CHECK:         return [[PTR]]
// CHECK-LABEL: } // end sil function 'getUnprotectedRawPointer'
@_silgen_name("getUnprotectedRawPointer")
func getUnprotectedRawPointer<T>(to value: T) -> Builtin.RawPointer {
  return Builtin.unprotectedAddressOfBorrow(value)
}

// CHECK-LABEL: sil hidden @getBridgeObject : {{.*}} {
// CHECK:         [[OBJECT_ADDR:%[^,]+]] = unchecked_addr_cast {{%[^,]+}} : $*T to $*Builtin.BridgeObject
// CHECK:         [[OBJECT:%[^,]+]] = load [[OBJECT_ADDR]]
// CHECK:         return [[OBJECT]]
// CHECK-LABEL: } // end sil function 'getBridgeObject'
@_silgen_name("getBridgeObject")
func toObject<T>(_ object: inout T) -> Builtin.BridgeObject {
  Builtin.reinterpretCast(object)
}

// CHECK-LABEL: sil hidden @getAnotherType : {{.*}} {
// CHECK:       {{bb[0-9]+}}([[RETADDR:%[^,]+]] : $*U, {{%[^,]+}} : $*T, {{%[^,]+}} : $@thick U.Type):
// CHECK:         [[OTHER_TY_ADDR:%[^,]+]] = unchecked_addr_cast {{%[^,]+}} : $*T to $*U
// CHECK:         copy_addr [[OTHER_TY_ADDR]] to [init] [[RETADDR]] : $*U
// CHECK-LABEL: } // end sil function 'getAnotherType'
@_silgen_name("getAnotherType")
func getAnotherType<T, U>(_ object: inout T, to ty: U.Type) -> U {
  Builtin.reinterpretCast(object)
}

@_silgen_name("isOfTypeOfAnyObjectType")
func isOfTypeOfAnyObjectType(fromAny any: Any) -> Bool {
  type(of: any) is Builtin.AnyObject.Type
}
