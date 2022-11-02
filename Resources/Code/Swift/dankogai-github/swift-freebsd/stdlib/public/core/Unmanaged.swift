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

/// A type for propagating an unmanaged object reference.
///
/// When you use this type, you become partially responsible for
/// keeping the object alive.
public struct Unmanaged<Instance : AnyObject> {
  @available(*, unavailable, renamed="Instance")
  public typealias T = Instance

  internal unowned(unsafe) var _value: Instance

  @_transparent
  internal init(_private: Instance) { _value = _private }

  /// Unsafely turn an opaque C pointer into an unmanaged
  /// class reference.
  ///
  /// This operation does not change reference counts.
  ///
  ///     let str: CFString = Unmanaged.fromOpaque(ptr).takeUnretainedValue()
  @_transparent
  @warn_unused_result
  public static func fromOpaque(value: COpaquePointer) -> Unmanaged {
    // Null pointer check is a debug check, because it guards only against one
    // specific bad pointer value.
    _debugPrecondition(
      value != nil,
      "attempt to create an Unmanaged instance from a null pointer")

    return Unmanaged(_private: unsafeBitCast(value, Instance.self))
  }

  /// Unsafely turn an unmanaged class reference into an opaque
  /// C pointer.
  ///
  /// This operation does not change reference counts.
  ///
  ///     let str: CFString = Unmanaged.fromOpaque(ptr).takeUnretainedValue()
  @_transparent
  @warn_unused_result
  public func toOpaque() -> COpaquePointer {
    return unsafeBitCast(_value, COpaquePointer.self)
  }

  /// Create an unmanaged reference with an unbalanced retain.
  /// The object will leak if nothing eventually balances the retain.
  ///
  /// This is useful when passing an object to an API which Swift
  /// does not know the ownership rules for, but you know that the
  /// API expects you to pass the object at +1.
  @_transparent
  @warn_unused_result
  public static func passRetained(value: Instance) -> Unmanaged {
    return Unmanaged(_private: value).retain()
  }

  /// Create an unmanaged reference without performing an unbalanced
  /// retain.
  ///
  /// This is useful when passing a reference to an API which Swift
  /// does not know the ownership rules for, but you know that the
  /// API expects you to pass the object at +0.
  ///
  ///     CFArraySetValueAtIndex(.passUnretained(array), i,
  ///                            .passUnretained(object))
  @_transparent
  @warn_unused_result
  public static func passUnretained(value: Instance) -> Unmanaged {
    return Unmanaged(_private: value)
  }

  /// Get the value of this unmanaged reference as a managed
  /// reference without consuming an unbalanced retain of it.
  ///
  /// This is useful when a function returns an unmanaged reference
  /// and you know that you're not responsible for releasing the result.
  @warn_unused_result
  public func takeUnretainedValue() -> Instance {
    return _value
  }

  /// Get the value of this unmanaged reference as a managed
  /// reference and consume an unbalanced retain of it.
  ///
  /// This is useful when a function returns an unmanaged reference
  /// and you know that you're responsible for releasing the result.
  @warn_unused_result
  public func takeRetainedValue() -> Instance {
    let result = _value
    release()
    return result
  }

  /// Perform an unbalanced retain of the object.
  @_transparent
  public func retain() -> Unmanaged {
    Builtin.retain(_value)
    return self
  }

  /// Perform an unbalanced release of the object.
  @_transparent
  public func release() {
    Builtin.release(_value)
  }

#if _runtime(_ObjC)
  /// Perform an unbalanced autorelease of the object.
  @_transparent
  public func autorelease() -> Unmanaged {
    Builtin.autorelease(_value)
    return self
  }
#endif
}
