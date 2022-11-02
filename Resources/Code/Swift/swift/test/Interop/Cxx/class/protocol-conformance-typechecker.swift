// Tests that a C++ class can conform to a Swift protocol.

// RUN: %target-typecheck-verify-swift -I %S/Inputs -enable-experimental-cxx-interop

import ProtocolConformance

protocol HasReturn42 {
  mutating func return42() -> CInt // expected-note {{requires function 'return42()'}}
}

extension ConformsToProtocol : HasReturn42 {}

extension DoesNotConformToProtocol : HasReturn42 {} // expected-error {{'DoesNotConformToProtocol' does not conform to protocol}}


protocol HasReturnNullable {
  mutating func returnPointer() -> UnsafePointer<Int32>?
}

// HasReturnNullable's returnNullable returns an implicitly unwrapped optional:
//   mutating func returnPointer() -> UnsafePointer<Int32>!
extension ReturnsNullableValue: HasReturnNullable {}

protocol HasReturnNonNull {
  mutating func returnPointer() -> UnsafePointer<Int32>
}

extension ReturnsNonNullValue: HasReturnNonNull {}


protocol Invertable {
  static prefix func !(obj: Self) -> Self
}

extension HasOperatorExclaim: Invertable {}

extension HasOperatorEqualEqual: Equatable {}
