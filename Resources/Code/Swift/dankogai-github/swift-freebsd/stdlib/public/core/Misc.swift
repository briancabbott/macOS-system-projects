//===----------------------------------------------------------------------===//
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
// Extern C functions
//===----------------------------------------------------------------------===//

// FIXME: Once we have an FFI interface, make these have proper function bodies

@_transparent
@warn_unused_result
public // @testable
func _countLeadingZeros(value: Int64) -> Int64 {
    return Int64(Builtin.int_ctlz_Int64(value._value, false._value))
}

/// Returns if `x` is a power of 2.
@_transparent
@warn_unused_result
public // @testable
func _isPowerOf2(x: UInt) -> Bool {
  if x == 0 {
    return false
  }
  // Note: use unchecked subtraction because we have checked that `x` is not
  // zero.
  return x & (x &- 1) == 0
}

/// Returns if `x` is a power of 2.
@_transparent
@warn_unused_result
public // @testable
func _isPowerOf2(x: Int) -> Bool {
  if x <= 0 {
    return false
  }
  // Note: use unchecked subtraction because we have checked that `x` is not
  // `Int.min`.
  return x & (x &- 1) == 0
}

#if _runtime(_ObjC)
@_transparent
public func _autorelease(x: AnyObject) {
  Builtin.retain(x)
  Builtin.autorelease(x)
}
#endif

/// Invoke `body` with an allocated, but uninitialized memory suitable for a
/// `String` value.
///
/// This function is primarily useful to call various runtime functions
/// written in C++.
func _withUninitializedString<R>(
  body: (UnsafeMutablePointer<String>) -> R
) -> (R, String) {
  let stringPtr = UnsafeMutablePointer<String>.alloc(1)
  let bodyResult = body(stringPtr)
  let stringResult = stringPtr.move()
  stringPtr.dealloc(1)
  return (bodyResult, stringResult)
}

@_silgen_name("swift_getTypeName")
public func _getTypeName(type: Any.Type, qualified: Bool)
  -> (UnsafePointer<UInt8>, Int)

/// Returns the demangled qualified name of a metatype.
@warn_unused_result
public // @testable
func _typeName(type: Any.Type, qualified: Bool = true) -> String {
  let (stringPtr, count) = _getTypeName(type, qualified: qualified)
  return ._fromWellFormedCodeUnitSequence(UTF8.self,
    input: UnsafeBufferPointer(start: stringPtr, count: count))
}

/// Returns `floor(log(x))`.  This equals to the position of the most
/// significant non-zero bit, or 63 - number-of-zeros before it.
///
/// The function is only defined for positive values of `x`.
///
/// Examples:
///
///      floorLog2(1) == 0
///      floorLog2(2) == floorLog2(3) == 1
///      floorLog2(9) == floorLog2(15) == 3
///
/// TODO: Implement version working on Int instead of Int64.
@warn_unused_result
@_transparent
public // @testable
func _floorLog2(x: Int64) -> Int {
  _sanityCheck(x > 0, "_floorLog2 operates only on non-negative integers")
  // Note: use unchecked subtraction because we this expression can not
  // overflow.
  return 63 &- Int(_countLeadingZeros(x))
}

