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

@_exported import Foundation // Clang module
import CoreFoundation
import CoreGraphics

//===----------------------------------------------------------------------===//
// Enums
//===----------------------------------------------------------------------===//

// FIXME: one day this will be bridged from CoreFoundation and we
// should drop it here. <rdar://problem/14497260> (need support
// for CF bridging)
public var kCFStringEncodingASCII: CFStringEncoding { return 0x0600 }

// FIXME: <rdar://problem/16074941> NSStringEncoding doesn't work on 32-bit
public typealias NSStringEncoding = UInt
public var NSASCIIStringEncoding: UInt { return 1 }
public var NSNEXTSTEPStringEncoding: UInt { return 2 }
public var NSJapaneseEUCStringEncoding: UInt { return 3 }
public var NSUTF8StringEncoding: UInt { return 4 }
public var NSISOLatin1StringEncoding: UInt { return 5 }
public var NSSymbolStringEncoding: UInt { return 6 }
public var NSNonLossyASCIIStringEncoding: UInt { return 7 }
public var NSShiftJISStringEncoding: UInt { return 8 }
public var NSISOLatin2StringEncoding: UInt { return 9 }
public var NSUnicodeStringEncoding: UInt { return 10 }
public var NSWindowsCP1251StringEncoding: UInt { return 11 }
public var NSWindowsCP1252StringEncoding: UInt { return 12 }
public var NSWindowsCP1253StringEncoding: UInt { return 13 }
public var NSWindowsCP1254StringEncoding: UInt { return 14 }
public var NSWindowsCP1250StringEncoding: UInt { return 15 }
public var NSISO2022JPStringEncoding: UInt { return 21 }
public var NSMacOSRomanStringEncoding: UInt { return 30 }
public var NSUTF16StringEncoding: UInt { return NSUnicodeStringEncoding }
public var NSUTF16BigEndianStringEncoding: UInt { return 0x90000100 }
public var NSUTF16LittleEndianStringEncoding: UInt { return 0x94000100 }
public var NSUTF32StringEncoding: UInt { return 0x8c000100 }
public var NSUTF32BigEndianStringEncoding: UInt { return 0x98000100 }
public var NSUTF32LittleEndianStringEncoding: UInt { return 0x9c000100 }

//===----------------------------------------------------------------------===//
// NSObject
//===----------------------------------------------------------------------===//

// These conformances should be located in the `ObjectiveC` module, but they can't
// be placed there because string bridging is not available there.
extension NSObject : CustomStringConvertible {}
extension NSObject : CustomDebugStringConvertible {}

//===----------------------------------------------------------------------===//
// Strings
//===----------------------------------------------------------------------===//

@available(*, unavailable, message="Please use String or NSString")
public class NSSimpleCString {}

@available(*, unavailable, message="Please use String or NSString")
public class NSConstantString {}

@warn_unused_result
@_silgen_name("swift_convertStringToNSString")
public // COMPILER_INTRINSIC
func _convertStringToNSString(string: String) -> NSString {
  return string._bridgeToObjectiveC()
}

@warn_unused_result
@_semantics("convertFromObjectiveC")
public // COMPILER_INTRINSIC
func _convertNSStringToString(nsstring: NSString?) -> String {
  if nsstring == nil { return "" }
  var result: String?
  String._forceBridgeFromObjectiveC(nsstring!, result: &result)
  return result!
}

extension NSString : StringLiteralConvertible {
  /// Create an instance initialized to `value`.
  public required convenience init(unicodeScalarLiteral value: StaticString) {
    self.init(stringLiteral: value)
  }

  public required convenience init(
    extendedGraphemeClusterLiteral value: StaticString
  ) {
    self.init(stringLiteral: value)
  }

  /// Create an instance initialized to `value`.
  public required convenience init(stringLiteral value: StaticString) {
    var immutableResult: NSString
    if value.hasPointerRepresentation {
      immutableResult = NSString(
        bytesNoCopy: UnsafeMutablePointer<Void>(value.utf8Start),
        length: Int(value.byteSize),
        encoding: value.isASCII ? NSASCIIStringEncoding : NSUTF8StringEncoding,
        freeWhenDone: false)!
    } else {
      var uintValue = value.unicodeScalar
      immutableResult = NSString(
        bytes: &uintValue,
        length: 4,
        encoding: NSUTF32StringEncoding)!
    }
    self.init(string: immutableResult as String)
  }
}

//===----------------------------------------------------------------------===//
// New Strings
//===----------------------------------------------------------------------===//

//
// Conversion from NSString to Swift's native representation
//

extension String {
  public init(_ cocoaString: NSString) {
    self = String(_cocoaString: cocoaString)
  }
}

extension String : _ObjectiveCBridgeable {
  public static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  public static func _getObjectiveCType() -> Any.Type {
    return NSString.self
  }

  @_semantics("convertToObjectiveC")
  public func _bridgeToObjectiveC() -> NSString {
    // This method should not do anything extra except calling into the
    // implementation inside core.  (These two entry points should be
    // equivalent.)
    return unsafeBitCast(_bridgeToObjectiveCImpl(), NSString.self)
  }

  public static func _forceBridgeFromObjectiveC(
    x: NSString,
    inout result: String?
  ) {
    result = String(x)
  }

  public static func _conditionallyBridgeFromObjectiveC(
    x: NSString,
    inout result: String?
  ) -> Bool {
    self._forceBridgeFromObjectiveC(x, result: &result)
    return result != nil
  }
}

//===----------------------------------------------------------------------===//
// Numbers
//===----------------------------------------------------------------------===//

// Conversions between NSNumber and various numeric types. The
// conversion to NSNumber is automatic (auto-boxing), while conversion
// back to a specific numeric type requires a cast.
// FIXME: Incomplete list of types.
extension Int : _ObjectiveCBridgeable {
  public static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  public init(_ number: NSNumber) {
    self = number.integerValue
  }

  public static func _getObjectiveCType() -> Any.Type {
    return NSNumber.self
  }

  @_semantics("convertToObjectiveC")
  public func _bridgeToObjectiveC() -> NSNumber {
    return NSNumber(integer: self)
  }

  public static func _forceBridgeFromObjectiveC(
    x: NSNumber,
    inout result: Int?
  ) {
    result = x.integerValue
  }

  public static func _conditionallyBridgeFromObjectiveC(
    x: NSNumber,
    inout result: Int?
  ) -> Bool {
    self._forceBridgeFromObjectiveC(x, result: &result)
    return true
  }
}

extension UInt : _ObjectiveCBridgeable {
  public static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  public init(_ number: NSNumber) {
    self = number.unsignedIntegerValue
  }

  public static func _getObjectiveCType() -> Any.Type {
    return NSNumber.self
  }

  @_semantics("convertToObjectiveC")
  public func _bridgeToObjectiveC() -> NSNumber {
    return NSNumber(unsignedInteger: self)
  }

  public static func _forceBridgeFromObjectiveC(
    x: NSNumber,
    inout result: UInt?
  ) {
    result = x.unsignedIntegerValue
  }
  public static func _conditionallyBridgeFromObjectiveC(
    x: NSNumber,
    inout result: UInt?
  ) -> Bool {
    self._forceBridgeFromObjectiveC(x, result: &result)
    return true
  }
}

extension Float : _ObjectiveCBridgeable {
  public static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  public init(_ number: NSNumber) {
    self = number.floatValue
  }

  public static func _getObjectiveCType() -> Any.Type {
    return NSNumber.self
  }

  @_semantics("convertToObjectiveC")
  public func _bridgeToObjectiveC() -> NSNumber {
    return NSNumber(float: self)
  }

  public static func _forceBridgeFromObjectiveC(
    x: NSNumber,
    inout result: Float?
  ) {
    result = x.floatValue
  }

  public static func _conditionallyBridgeFromObjectiveC(
    x: NSNumber,
    inout result: Float?
  ) -> Bool {
    self._forceBridgeFromObjectiveC(x, result: &result)
    return true
  }
}

extension Double : _ObjectiveCBridgeable {
  public static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  public init(_ number: NSNumber) {
    self = number.doubleValue
  }

  public static func _getObjectiveCType() -> Any.Type {
    return NSNumber.self
  }

  @_semantics("convertToObjectiveC")
  public func _bridgeToObjectiveC() -> NSNumber {
    return NSNumber(double: self)
  }

  public static func _forceBridgeFromObjectiveC(
    x: NSNumber,
    inout result: Double?
  ) {
    result = x.doubleValue
  }

  public static func _conditionallyBridgeFromObjectiveC(
    x: NSNumber,
    inout result: Double?
  ) -> Bool {
    self._forceBridgeFromObjectiveC(x, result: &result)
    return true
  }
}

extension Bool: _ObjectiveCBridgeable {
  public static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  public init(_ number: NSNumber) {
    if number.boolValue { self = true }
    else { self = false }
  }

  public static func _getObjectiveCType() -> Any.Type {
    return NSNumber.self
  }

  @_semantics("convertToObjectiveC")
  public func _bridgeToObjectiveC() -> NSNumber {
    return NSNumber(bool: self)
  }

  public static func _forceBridgeFromObjectiveC(
    x: NSNumber,
    inout result: Bool?
  ) {
    result = x.boolValue
  }

  public static func _conditionallyBridgeFromObjectiveC(
    x: NSNumber,
    inout result: Bool?
  ) -> Bool {
    self._forceBridgeFromObjectiveC(x, result: &result)
    return true
  }
}

// CGFloat bridging.
extension CGFloat : _ObjectiveCBridgeable {
  public static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  public init(_ number: NSNumber) {
    self.native = CGFloat.NativeType(number)
  }

  public static func _getObjectiveCType() -> Any.Type {
    return NSNumber.self
  }

  @_semantics("convertToObjectiveC")
  public func _bridgeToObjectiveC() -> NSNumber {
    return self.native._bridgeToObjectiveC()
  }

  public static func _forceBridgeFromObjectiveC(
    x: NSNumber,
    inout result: CGFloat?
  ) {
    var nativeResult: CGFloat.NativeType? = 0.0
    CGFloat.NativeType._forceBridgeFromObjectiveC(x, result: &nativeResult)
    result = CGFloat(nativeResult!)
  }

  public static func _conditionallyBridgeFromObjectiveC(
    x: NSNumber,
    inout result: CGFloat?
  ) -> Bool {
    self._forceBridgeFromObjectiveC(x, result: &result)
    return true
  }
}

// Literal support for NSNumber
extension NSNumber : FloatLiteralConvertible, IntegerLiteralConvertible,
                     BooleanLiteralConvertible {
  /// Create an instance initialized to `value`.
  public required convenience init(integerLiteral value: Int) {
    self.init(integer: value)
  }

  /// Create an instance initialized to `value`.
  public required convenience init(floatLiteral value: Double) {
    self.init(double: value)
  }

  /// Create an instance initialized to `value`.
  public required convenience init(booleanLiteral value: Bool) {
    self.init(bool: value)
  }
}

public let NSNotFound: Int = .max

//===----------------------------------------------------------------------===//
// Arrays
//===----------------------------------------------------------------------===//

extension NSArray : ArrayLiteralConvertible {
  /// Create an instance initialized with `elements`.
  public required convenience init(arrayLiteral elements: AnyObject...) {
    // + (instancetype)arrayWithObjects:(const id [])objects count:(NSUInteger)cnt;
    let x = _extractOrCopyToNativeArrayBuffer(elements._buffer)
    self.init(
      objects: UnsafeMutablePointer(x.firstElementAddress), count: x.count)
    _fixLifetime(x)
  }
}

/// The entry point for converting `NSArray` to `Array` in bridge
/// thunks.  Used, for example, to expose :
///
///     func f([NSView]) {}
///
/// to Objective-C code as a method that accepts an `NSArray`.  This operation
/// is referred to as a "forced conversion" in ../../../docs/Arrays.rst
@warn_unused_result
@_semantics("convertFromObjectiveC")
public func _convertNSArrayToArray<T>(source: NSArray?) -> [T] {
  if _slowPath(source == nil) { return [] }
  var result: [T]?
  Array._forceBridgeFromObjectiveC(source!, result: &result)
  return result!
}

/// The entry point for converting `Array` to `NSArray` in bridge
/// thunks.  Used, for example, to expose :
///
///     func f() -> [NSView] { return [] }
///
/// to Objective-C code as a method that returns an `NSArray`.
@warn_unused_result
public func _convertArrayToNSArray<T>(array: [T]) -> NSArray {
  return array._bridgeToObjectiveC()
}

extension Array : _ObjectiveCBridgeable {

  /// Private initializer used for bridging.
  ///
  /// The provided `NSArray` will be copied to ensure that the copy can
  /// not be mutated by other code.
  internal init(_cocoaArray: NSArray) {
    _sanityCheck(_isBridgedVerbatimToObjectiveC(Element.self),
      "Array can be backed by NSArray only when the element type can be bridged verbatim to Objective-C")
    // FIXME: We would like to call CFArrayCreateCopy() to avoid doing an
    // objc_msgSend() for instances of CoreFoundation types.  We can't do that
    // today because CFArrayCreateCopy() copies array contents unconditionally,
    // resulting in O(n) copies even for immutable arrays.
    //
    // <rdar://problem/19773555> CFArrayCreateCopy() is >10x slower than
    // -[NSArray copyWithZone:]
    //
    // The bug is fixed in: OS X 10.11.0, iOS 9.0, all versions of tvOS
    // and watchOS.
    self = Array(
      _immutableCocoaArray:
        unsafeBitCast(_cocoaArray.copyWithZone(nil), _NSArrayCoreType.self))
  }

  public static func _isBridgedToObjectiveC() -> Bool {
    return Swift._isBridgedToObjectiveC(Element.self)
  }

  public static func _getObjectiveCType() -> Any.Type {
    return NSArray.self
  }

  @_semantics("convertToObjectiveC")
  public func _bridgeToObjectiveC() -> NSArray {
    return unsafeBitCast(self._buffer._asCocoaArray(), NSArray.self)
  }

  public static func _forceBridgeFromObjectiveC(
    source: NSArray,
    inout result: Array?
  ) {
    _precondition(
      Swift._isBridgedToObjectiveC(Element.self),
      "array element type is not bridged to Objective-C")

    // If we have the appropriate native storage already, just adopt it.
    if let native = Array._bridgeFromObjectiveCAdoptingNativeStorage(source) {
      result = native
      return
    }

    if _fastPath(_isBridgedVerbatimToObjectiveC(Element.self)) {
      // Forced down-cast (possible deferred type-checking)
      result = Array(_cocoaArray: source)
      return
    }

    result = _arrayForceCast([AnyObject](_cocoaArray: source))
  }

  public static func _conditionallyBridgeFromObjectiveC(
    source: NSArray,
    inout result: Array?
  ) -> Bool {
    // Construct the result array by conditionally bridging each element.
    let anyObjectArr = [AnyObject](_cocoaArray: source)

    result = _arrayConditionalCast(anyObjectArr)
    return result != nil
  }
}

//===----------------------------------------------------------------------===//
// Dictionaries
//===----------------------------------------------------------------------===//

extension NSDictionary : DictionaryLiteralConvertible {
  public required convenience init(
    dictionaryLiteral elements: (NSCopying, AnyObject)...
  ) {
    self.init(
      objects: elements.map { (AnyObject?)($0.1) },
      forKeys: elements.map { (NSCopying?)($0.0) },
      count: elements.count)
  }
}

extension Dictionary {
  /// Private initializer used for bridging.
  ///
  /// The provided `NSDictionary` will be copied to ensure that the copy can
  /// not be mutated by other code.
  public init(_cocoaDictionary: _NSDictionaryType) {
    _sanityCheck(
      _isBridgedVerbatimToObjectiveC(Key.self) &&
      _isBridgedVerbatimToObjectiveC(Value.self),
      "Dictionary can be backed by NSDictionary storage only when both key and value are bridged verbatim to Objective-C")
    // FIXME: We would like to call CFDictionaryCreateCopy() to avoid doing an
    // objc_msgSend() for instances of CoreFoundation types.  We can't do that
    // today because CFDictionaryCreateCopy() copies dictionary contents
    // unconditionally, resulting in O(n) copies even for immutable dictionaries.
    //
    // <rdar://problem/20690755> CFDictionaryCreateCopy() does not call copyWithZone:
    //
    // The bug is fixed in: OS X 10.11.0, iOS 9.0, all versions of tvOS
    // and watchOS.
    self = Dictionary(
      _immutableCocoaDictionary:
        unsafeBitCast(_cocoaDictionary.copyWithZone(nil), _NSDictionaryType.self))
  }
}

/// The entry point for bridging `NSDictionary` to `Dictionary` in bridge
/// thunks.  Used, for example, to expose:
///
///     func f([String : String]) {}
///
/// to Objective-C code as a method that accepts an `NSDictionary`.
///
/// This is a forced downcast.  This operation should have O(1) complexity
/// when `Key` and `Value` are bridged verbatim.
///
/// The cast can fail if bridging fails.  The actual checks and bridging can be
/// deferred.
@warn_unused_result
@_semantics("convertFromObjectiveC")
public func _convertNSDictionaryToDictionary<
  Key : Hashable, Value
>(d: NSDictionary?) -> [Key : Value] {
  // Note: there should be *a good justification* for doing something else
  // than just dispatching to `_forceBridgeFromObjectiveC`.
  if _slowPath(d == nil) { return [:] }
  var result: [Key : Value]?
  Dictionary._forceBridgeFromObjectiveC(d!, result: &result)
  return result!
}

// FIXME: right now the following is O(n), not O(1).

/// The entry point for bridging `Dictionary` to `NSDictionary` in bridge
/// thunks.  Used, for example, to expose:
///
///     func f() -> [String : String] {}
///
/// to Objective-C code as a method that returns an `NSDictionary`.
///
/// This is a forced downcast.  This operation should have O(1) complexity.
///
/// The cast can fail if bridging fails.  The actual checks and bridging can be
/// deferred.
@warn_unused_result
public func _convertDictionaryToNSDictionary<Key, Value>(
  d: [Key : Value]
) -> NSDictionary {

  // Note: there should be *a good justification* for doing something else
  // than just dispatching to `_bridgeToObjectiveC`.
  return d._bridgeToObjectiveC()
}

// Dictionary<Key, Value> is conditionally bridged to NSDictionary
extension Dictionary : _ObjectiveCBridgeable {
  public static func _getObjectiveCType() -> Any.Type {
    return NSDictionary.self
  }

  @_semantics("convertToObjectiveC")
  public func _bridgeToObjectiveC() -> NSDictionary {
    return unsafeBitCast(_bridgeToObjectiveCImpl(), NSDictionary.self)
  }

  public static func _forceBridgeFromObjectiveC(
    d: NSDictionary,
    inout result: Dictionary?
  ) {
    if let native = [Key : Value]._bridgeFromObjectiveCAdoptingNativeStorage(
        d as AnyObject) {
      result = native
      return
    }

    if _isBridgedVerbatimToObjectiveC(Key.self) &&
       _isBridgedVerbatimToObjectiveC(Value.self) {
      result = [Key : Value](
        _cocoaDictionary: unsafeBitCast(d, _NSDictionaryType.self))
      return
    }

    // `Dictionary<Key, Value>` where either `Key` or `Value` is a value type
    // may not be backed by an NSDictionary.
    var builder = _DictionaryBuilder<Key, Value>(count: d.count)
    d.enumerateKeysAndObjectsUsingBlock {
      (anyObjectKey: AnyObject, anyObjectValue: AnyObject,
       stop: UnsafeMutablePointer<ObjCBool>) in
      builder.add(
          key: Swift._forceBridgeFromObjectiveC(anyObjectKey, Key.self),
          value: Swift._forceBridgeFromObjectiveC(anyObjectValue, Value.self))
    }
    result = builder.take()
  }

  public static func _conditionallyBridgeFromObjectiveC(
    x: NSDictionary,
    inout result: Dictionary?
  ) -> Bool {
    let anyDict = x as [NSObject : AnyObject]
    if _isBridgedVerbatimToObjectiveC(Key.self) &&
       _isBridgedVerbatimToObjectiveC(Value.self) {
      result = Swift._dictionaryDownCastConditional(anyDict)
      return result != nil
    }

    result = Swift._dictionaryBridgeFromObjectiveCConditional(anyDict)
    return result != nil
  }

  public static func _isBridgedToObjectiveC() -> Bool {
    return Swift._isBridgedToObjectiveC(Key.self) &&
           Swift._isBridgedToObjectiveC(Value.self)
  }
}

//===----------------------------------------------------------------------===//
// Fast enumeration
//===----------------------------------------------------------------------===//

// NB: This is a class because fast enumeration passes around interior pointers
// to the enumeration state, so the state cannot be moved in memory. We will
// probably need to implement fast enumeration in the compiler as a primitive
// to implement it both correctly and efficiently.
final public class NSFastGenerator : GeneratorType {
  var enumerable: NSFastEnumeration
  var state: [NSFastEnumerationState]
  var n: Int
  var count: Int

  /// Size of ObjectsBuffer, in ids.
  var STACK_BUF_SIZE: Int { return 4 }

  /// Must have enough space for STACK_BUF_SIZE object references.
  struct ObjectsBuffer {
    var buf = (COpaquePointer(), COpaquePointer(),
               COpaquePointer(), COpaquePointer())
  }
  var objects: [ObjectsBuffer]

  public func next() -> AnyObject? {
    if n == count {
      // FIXME: Is this check necessary before refresh()?
      if count == 0 { return .None }
      refresh()
      if count == 0 { return .None }
    }
    let next : AnyObject = state[0].itemsPtr[n]!
    ++n
    return next
  }

  func refresh() {
    n = 0
    count = enumerable.countByEnumeratingWithState(
      state._baseAddressIfContiguous,
      objects: AutoreleasingUnsafeMutablePointer(
        objects._baseAddressIfContiguous),
      count: STACK_BUF_SIZE)
  }

  public init(_ enumerable: NSFastEnumeration) {
    self.enumerable = enumerable
    self.state = [ NSFastEnumerationState(
      state: 0, itemsPtr: nil,
      mutationsPtr: _fastEnumerationStorageMutationsPtr,
      extra: (0, 0, 0, 0, 0)) ]
    self.objects = [ ObjectsBuffer() ]
    self.n = -1
    self.count = -1
  }
}

extension NSArray : SequenceType {
  /// Return a *generator* over the elements of this *sequence*.
  ///
  /// - Complexity: O(1).
  final public func generate() -> NSFastGenerator {
    return NSFastGenerator(self)
  }
}

/* TODO: API review
extension NSArray : Swift.CollectionType {
  final public var startIndex: Int {
    return 0
  }

  final public var endIndex: Int {
    return count
  }
}
 */

extension Set {
  /// Private initializer used for bridging.
  ///
  /// The provided `NSSet` will be copied to ensure that the copy can
  /// not be mutated by other code.
  public init(_cocoaSet: _NSSetType) {
    _sanityCheck(_isBridgedVerbatimToObjectiveC(Element.self),
      "Set can be backed by NSSet _variantStorage only when the member type can be bridged verbatim to Objective-C")
    // FIXME: We would like to call CFSetCreateCopy() to avoid doing an
    // objc_msgSend() for instances of CoreFoundation types.  We can't do that
    // today because CFSetCreateCopy() copies dictionary contents
    // unconditionally, resulting in O(n) copies even for immutable dictionaries.
    //
    // <rdar://problem/20697680> CFSetCreateCopy() does not call copyWithZone:
    //
    // The bug is fixed in: OS X 10.11.0, iOS 9.0, all versions of tvOS
    // and watchOS.
    self = Set(
      _immutableCocoaSet:
        unsafeBitCast(_cocoaSet.copyWithZone(nil), _NSSetType.self))
  }
}

extension NSSet : SequenceType {
  /// Return a *generator* over the elements of this *sequence*.
  ///
  /// - Complexity: O(1).
  public func generate() -> NSFastGenerator {
    return NSFastGenerator(self)
  }
}

extension NSOrderedSet : SequenceType {
  /// Return a *generator* over the elements of this *sequence*.
  ///
  /// - Complexity: O(1).
  public func generate() -> NSFastGenerator {
    return NSFastGenerator(self)
  }
}

// FIXME: move inside NSIndexSet when the compiler supports this.
public struct NSIndexSetGenerator : GeneratorType {
  public typealias Element = Int

  internal let _set: NSIndexSet
  internal var _first: Bool = true
  internal var _current: Int?

  internal init(set: NSIndexSet) {
    self._set = set
    self._current = nil
  }

  public mutating func next() -> Int? {
    if _first {
      _current = _set.firstIndex
      _first = false
    } else if let c = _current {
      _current = _set.indexGreaterThanIndex(c)
    } else {
      // current is already nil
    }
    if _current == NSNotFound {
      _current = nil
    }
    return _current
  }
}

extension NSIndexSet : SequenceType {
  /// Return a *generator* over the elements of this *sequence*.
  ///
  /// - Complexity: O(1).
  public func generate() -> NSIndexSetGenerator {
    return NSIndexSetGenerator(set: self)
  }
}

// FIXME: right now the following is O(n), not O(1).

/// The entry point for bridging `Set` to `NSSet` in bridge
/// thunks.  Used, for example, to expose:
///
///     func f() -> Set<String> {}
///
/// to Objective-C code as a method that returns an `NSSet`.
///
/// This is a forced downcast.  This operation should have O(1) complexity.
///
/// The cast can fail if bridging fails.  The actual checks and bridging can be
/// deferred.
@warn_unused_result
public func _convertSetToNSSet<T>(s: Set<T>) -> NSSet {
  return s._bridgeToObjectiveC()
}

/// The entry point for bridging `NSSet` to `Set` in bridge
/// thunks.  Used, for example, to expose:
///
///     func f(Set<String>) {}
///
/// to Objective-C code as a method that accepts an `NSSet`.
///
/// This is a forced downcast.  This operation should have O(1) complexity
/// when `T` is bridged verbatim.
///
/// The cast can fail if bridging fails.  The actual checks and bridging can be
/// deferred.
@warn_unused_result
@_semantics("convertFromObjectiveC")
public func _convertNSSetToSet<T : Hashable>(s: NSSet?) -> Set<T> {
  if _slowPath(s == nil) { return [] }
  var result: Set<T>?
  Set._forceBridgeFromObjectiveC(s!, result: &result)
  return result!
}

// Set<T> is conditionally bridged to NSSet
extension Set : _ObjectiveCBridgeable {
  public static func _getObjectiveCType() -> Any.Type {
    return NSSet.self
  }

  @_semantics("convertToObjectiveC")
  public func _bridgeToObjectiveC() -> NSSet {
    return unsafeBitCast(_bridgeToObjectiveCImpl(), NSSet.self)
  }

  public static func _forceBridgeFromObjectiveC(s: NSSet, inout result: Set?) {
    if let native =
      Set<Element>._bridgeFromObjectiveCAdoptingNativeStorage(s as AnyObject) {

      result = native
      return
    }

    if _isBridgedVerbatimToObjectiveC(Element.self) {
      result = Set<Element>(_cocoaSet: unsafeBitCast(s, _NSSetType.self))
      return
    }

    // `Set<Element>` where `Element` is a value type may not be backed by
    // an NSSet.
    var builder = _SetBuilder<Element>(count: s.count)
    s.enumerateObjectsUsingBlock {
      (anyObjectMember: AnyObject, stop: UnsafeMutablePointer<ObjCBool>) in
      builder.add(member: Swift._forceBridgeFromObjectiveC(
        anyObjectMember, Element.self))
    }
    result = builder.take()
  }

  public static func _conditionallyBridgeFromObjectiveC(
    x: NSSet, inout result: Set?
  ) -> Bool {
    let anySet = x as Set<NSObject>
    if _isBridgedVerbatimToObjectiveC(Element.self) {
      result = Swift._setDownCastConditional(anySet)
      return result != nil
    }

    result = Swift._setBridgeFromObjectiveCConditional(anySet)
    return result != nil
  }

  public static func _isBridgedToObjectiveC() -> Bool {
    return Swift._isBridgedToObjectiveC(Element.self)
  }
}

extension NSDictionary : SequenceType {
  // FIXME: A class because we can't pass a struct with class fields through an
  // [objc] interface without prematurely destroying the references.
  final public class Generator : GeneratorType {
    var _fastGenerator: NSFastGenerator
    var _dictionary: NSDictionary {
      return _fastGenerator.enumerable as! NSDictionary
    }

    public func next() -> (key: AnyObject, value: AnyObject)? {
      switch _fastGenerator.next() {
      case .None:
        return .None
      case .Some(let key):
        // Deliberately avoid the subscript operator in case the dictionary
        // contains non-copyable keys. This is rare since NSMutableDictionary
        // requires them, but we don't want to paint ourselves into a corner.
        return (key: key, value: _dictionary.objectForKey(key)!)
      }
    }

    init(_ _dict: NSDictionary) {
      _fastGenerator = NSFastGenerator(_dict)
    }
  }

  /// Return a *generator* over the elements of this *sequence*.
  ///
  /// - Complexity: O(1).
  public func generate() -> Generator {
    return Generator(self)
  }
}

extension NSEnumerator : SequenceType {
  /// Return a *generator* over the *enumerator*.
  ///
  /// - Complexity: O(1).
  public func generate() -> NSFastGenerator {
    return NSFastGenerator(self)
  }
}

//===----------------------------------------------------------------------===//
// Ranges
//===----------------------------------------------------------------------===//

extension NSRange {
  public init(_ x: Range<Int>) {
    location = x.startIndex
    length = x.count
  }

  @warn_unused_result
  public func toRange() -> Range<Int>? {
    if location == NSNotFound { return nil }
    return Range(start: location, end: location + length)
  }
}

//===----------------------------------------------------------------------===//
// NSLocalizedString
//===----------------------------------------------------------------------===//

/// Returns a localized string, using the main bundle if one is not specified.
@warn_unused_result
public
func NSLocalizedString(key: String,
                       tableName: String? = nil,
                       bundle: NSBundle = NSBundle.mainBundle(),
                       value: String = "",
                       comment: String) -> String {
  return bundle.localizedStringForKey(key, value:value, table:tableName)
}

//===----------------------------------------------------------------------===//
// NSLog
//===----------------------------------------------------------------------===//

public func NSLog(format: String, _ args: CVarArgType...) {
  withVaList(args) { NSLogv(format, $0) }
}

#if os(OSX)

//===----------------------------------------------------------------------===//
// NSRectEdge
//===----------------------------------------------------------------------===//

// In the SDK, the following NS*Edge constants are defined as macros for the
// corresponding CGRectEdge enumerators.  Thus, in the SDK, NS*Edge constants
// have CGRectEdge type.  This is not correct for Swift (as there is no
// implicit conversion to NSRectEdge).

@available(*, unavailable, renamed="NSRectEdge.MinX")
public var NSMinXEdge: NSRectEdge {
  fatalError("unavailable property can't be accessed")
}
@available(*, unavailable, renamed="NSRectEdge.MinY")
public var NSMinYEdge: NSRectEdge {
  fatalError("unavailable property can't be accessed")
}
@available(*, unavailable, renamed="NSRectEdge.MaxX")
public var NSMaxXEdge: NSRectEdge {
  fatalError("unavailable property can't be accessed")
}
@available(*, unavailable, renamed="NSRectEdge.MaxY")
public var NSMaxYEdge: NSRectEdge {
  fatalError("unavailable property can't be accessed")
}

extension NSRectEdge {
  public init(rectEdge: CGRectEdge) {
    self = NSRectEdge(rawValue: UInt(rectEdge.rawValue))!
  }
}

extension CGRectEdge {
  public init(rectEdge: NSRectEdge) {
    self = CGRectEdge(rawValue: UInt32(rectEdge.rawValue))!
  }
}

#endif

//===----------------------------------------------------------------------===//
// NSError (as an out parameter).
//===----------------------------------------------------------------------===//

public typealias NSErrorPointer = AutoreleasingUnsafeMutablePointer<NSError?>

@warn_unused_result
@_silgen_name("swift_convertNSErrorToErrorType")
public // COMPILER_INTRINSIC
func _convertNSErrorToErrorType(error: NSError?) -> ErrorType

@warn_unused_result
@_silgen_name("swift_convertErrorTypeToNSError")
public // COMPILER_INTRINSIC
func _convertErrorTypeToNSError(error: ErrorType) -> NSError

//===----------------------------------------------------------------------===//
// Variadic initializers and methods
//===----------------------------------------------------------------------===//

extension NSPredicate {
  // + (NSPredicate *)predicateWithFormat:(NSString *)predicateFormat, ...;
  public
  convenience init(format predicateFormat: String, _ args: CVarArgType...) {
    let va_args = getVaList(args)
    self.init(format: predicateFormat, arguments: va_args)
  }
}

extension NSExpression {
  // + (NSExpression *) expressionWithFormat:(NSString *)expressionFormat, ...;
  public
  convenience init(format expressionFormat: String, _ args: CVarArgType...) {
    let va_args = getVaList(args)
    self.init(format: expressionFormat, arguments: va_args)
  }
}

extension NSString {
  public convenience init(format: NSString, _ args: CVarArgType...) {
    // We can't use withVaList because 'self' cannot be captured by a closure
    // before it has been initialized.
    let va_args = getVaList(args)
    self.init(format: format as String, arguments: va_args)
  }

  public convenience init(
    format: NSString, locale: NSLocale?, _ args: CVarArgType...
  ) {
    // We can't use withVaList because 'self' cannot be captured by a closure
    // before it has been initialized.
    let va_args = getVaList(args)
    self.init(format: format as String, locale: locale, arguments: va_args)
  }

  @warn_unused_result
  public class func localizedStringWithFormat(
    format: NSString, _ args: CVarArgType...
  ) -> Self {
    return withVaList(args) {
      self.init(format: format as String, locale: NSLocale.currentLocale(), arguments: $0)
    }
  }

  @warn_unused_result
  public func stringByAppendingFormat(format: NSString, _ args: CVarArgType...)
  -> NSString {
    return withVaList(args) {
      self.stringByAppendingString(NSString(format: format as String, arguments: $0) as String) as NSString
    }
  }
}

extension NSMutableString {
  public func appendFormat(format: NSString, _ args: CVarArgType...) {
    return withVaList(args) {
      self.appendString(NSString(format: format as String, arguments: $0) as String)
    }
  }
}

extension NSArray {
  // Overlay: - (instancetype)initWithObjects:(id)firstObj, ...
  public convenience init(objects elements: AnyObject...) {
    // - (instancetype)initWithObjects:(const id [])objects count:(NSUInteger)cnt;
    let x = _extractOrCopyToNativeArrayBuffer(elements._buffer)
    // Use Imported:
    // @objc(initWithObjects:count:)
    //    init(withObjects objects: UnsafePointer<AnyObject?>,
    //    count cnt: Int)
    self.init(objects: UnsafeMutablePointer(x.firstElementAddress), count: x.count)
    _fixLifetime(x)
  }
}

extension NSOrderedSet {
  // - (instancetype)initWithObjects:(id)firstObj, ...
  public convenience init(objects elements: AnyObject...) {
    self.init(array: elements)
  }
}

extension NSSet {
  // - (instancetype)initWithObjects:(id)firstObj, ...
  public convenience init(objects elements: AnyObject...) {
    self.init(array: elements)
  }
}

extension NSSet : ArrayLiteralConvertible {
  public required convenience init(arrayLiteral elements: AnyObject...) {
    self.init(array: elements)
  }
}

extension NSOrderedSet : ArrayLiteralConvertible {
  public required convenience init(arrayLiteral elements: AnyObject...) {
    self.init(array: elements)
  }
}

//===--- "Copy constructors" ----------------------------------------------===//
// These are needed to make Cocoa feel natural since we eliminated
// implicit briding conversions from Objective-C to Swift
//===----------------------------------------------------------------------===//

extension NSArray {
  /// Initializes a newly allocated array by placing in it the objects
  /// contained in a given array.
  ///
  /// - Returns: An array initialized to contain the objects in
  ///    `anArray``. The returned object might be different than the
  ///    original receiver.
  ///
  /// Discussion: After an immutable array has been initialized in
  /// this way, it cannot be modified.
  @objc(_swiftInitWithArray_NSArray:)
  public convenience init(array anArray: NSArray) {
    self.init(array: anArray as Array)
  }
}

extension NSString {
  /// Returns an `NSString` object initialized by copying the characters
  /// from another given string.
  ///
  /// - Returns: An `NSString` object initialized by copying the
  ///   characters from `aString`. The returned object may be different
  ///   from the original receiver.
  @objc(_swiftInitWithString_NSString:)
  public convenience init(string aString: NSString) {
    self.init(string: aString as String)
  }
}

extension NSSet {
  /// Initializes a newly allocated set and adds to it objects from
  /// another given set.
  ///
  /// - Returns: An initialized objects set containing the objects from
  ///   `set`. The returned set might be different than the original
  ///   receiver.
  @objc(_swiftInitWithSet_NSSet:)
  public convenience init(set anSet: NSSet) {
    self.init(set: anSet as Set)
  }
}

extension NSDictionary {
  /// Initializes a newly allocated dictionary and adds to it objects from
  /// another given dictionary.
  ///
  /// - Returns: An initialized dictionary—which might be different
  ///   than the original receiver—containing the keys and values
  ///   found in `otherDictionary`.
  @objc(_swiftInitWithDictionary_NSDictionary:)
  public convenience init(dictionary otherDictionary: NSDictionary) {
    self.init(dictionary: otherDictionary as Dictionary)
  }
}

//===----------------------------------------------------------------------===//
// NSUndoManager
//===----------------------------------------------------------------------===//

@_silgen_name("NS_Swift_NSUndoManager_registerUndoWithTargetHandler")
internal func NS_Swift_NSUndoManager_registerUndoWithTargetHandler(
  self_: AnyObject,
  _ target: AnyObject,
  _ handler: @convention(block) (AnyObject) -> Void)

extension NSUndoManager {
  @available(OSX 10.11, iOS 9.0, *)
  public func registerUndoWithTarget<TargetType : AnyObject>(
    target: TargetType, handler: (TargetType) -> Void
  ) {
    // The generic blocks use a different ABI, so we need to wrap the provided
    // handler in something ObjC compatible.
    let objcCompatibleHandler: (AnyObject) -> Void = { internalTarget in
      handler(internalTarget as! TargetType)
    }
    NS_Swift_NSUndoManager_registerUndoWithTargetHandler(
      self as AnyObject, target as AnyObject, objcCompatibleHandler)
  }
}

//===----------------------------------------------------------------------===//
// NSCoder
//===----------------------------------------------------------------------===//

@warn_unused_result
@_silgen_name("NS_Swift_NSCoder_decodeObject")
internal func NS_Swift_NSCoder_decodeObject(
  self_: AnyObject,
  _ error: NSErrorPointer) -> AnyObject?

@warn_unused_result
@_silgen_name("NS_Swift_NSCoder_decodeObjectForKey")
internal func NS_Swift_NSCoder_decodeObjectForKey(
  self_: AnyObject,
  _ key: AnyObject,
  _ error: NSErrorPointer) -> AnyObject?

@warn_unused_result
@_silgen_name("NS_Swift_NSCoder_decodeObjectOfClassForKey")
internal func NS_Swift_NSCoder_decodeObjectOfClassForKey(
  self_: AnyObject,
  _ cls: AnyObject,
  _ key: AnyObject,
  _ error: NSErrorPointer) -> AnyObject?

@warn_unused_result
@_silgen_name("NS_Swift_NSCoder_decodeObjectOfClassesForKey")
internal func NS_Swift_NSCoder_decodeObjectOfClassesForKey(
  self_: AnyObject,
  _ classes: NSSet?,
  _ key: AnyObject,
  _ error: NSErrorPointer) -> AnyObject?


@available(OSX 10.11, iOS 9.0, *)
internal func resolveError(error: NSError?) throws {
  if let error = error where error.code != NSCoderValueNotFoundError {
    throw error
  }
}

extension NSCoder {
  @warn_unused_result
  public func decodeObjectOfClass<DecodedObjectType: NSCoding where DecodedObjectType: NSObject>(cls: DecodedObjectType.Type, forKey key: String) -> DecodedObjectType? {
    let result = NS_Swift_NSCoder_decodeObjectOfClassForKey(self as AnyObject, cls as AnyObject, key as AnyObject, nil)
    return result as! DecodedObjectType?
  }

  @warn_unused_result
  @nonobjc
  public func decodeObjectOfClasses(classes: NSSet?, forKey key: String) -> AnyObject? {
    var classesAsNSObjects: Set<NSObject>? = nil
    if let theClasses = classes {
      classesAsNSObjects =
        Set(GeneratorSequence(NSFastGenerator(theClasses)).map {
          unsafeBitCast($0, NSObject.self)
        })
    }
    return self.__decodeObjectOfClasses(classesAsNSObjects, forKey: key)
  }

  @warn_unused_result
  @available(OSX 10.11, iOS 9.0, *)
  public func decodeTopLevelObject() throws -> AnyObject? {
    var error: NSError?
    let result = NS_Swift_NSCoder_decodeObject(self as AnyObject, &error)
    try resolveError(error)
    return result
  }

  @warn_unused_result
  @available(OSX 10.11, iOS 9.0, *)
  public func decodeTopLevelObjectForKey(key: String) throws -> AnyObject? {
    var error: NSError?
    let result = NS_Swift_NSCoder_decodeObjectForKey(self as AnyObject, key as AnyObject, &error)
    try resolveError(error)
    return result
  }

  @warn_unused_result
  @available(OSX 10.11, iOS 9.0, *)
  public func decodeTopLevelObjectOfClass<DecodedObjectType: NSCoding where DecodedObjectType: NSObject>(cls: DecodedObjectType.Type, forKey key: String) throws -> DecodedObjectType? {
    var error: NSError?
    let result = NS_Swift_NSCoder_decodeObjectOfClassForKey(self as AnyObject, cls as AnyObject, key as AnyObject, &error)
    try resolveError(error)
    return result as! DecodedObjectType?
  }

  @warn_unused_result
  @available(OSX 10.11, iOS 9.0, *)
  public func decodeTopLevelObjectOfClasses(classes: NSSet?, forKey key: String) throws -> AnyObject? {
    var error: NSError?
    let result = NS_Swift_NSCoder_decodeObjectOfClassesForKey(self as AnyObject, classes, key as AnyObject, &error)
    try resolveError(error)
    return result
  }
}

//===----------------------------------------------------------------------===//
// NSKeyedUnarchiver
//===----------------------------------------------------------------------===//

@warn_unused_result
@_silgen_name("NS_Swift_NSKeyedUnarchiver_unarchiveObjectWithData")
internal func NS_Swift_NSKeyedUnarchiver_unarchiveObjectWithData(
  self_: AnyObject,
  _ data: AnyObject,
  _ error: NSErrorPointer) -> AnyObject?

extension NSKeyedUnarchiver {
  @warn_unused_result
  @available(OSX 10.11, iOS 9.0, *)
  public class func unarchiveTopLevelObjectWithData(data: NSData) throws -> AnyObject? {
    var error: NSError?
    let result = NS_Swift_NSKeyedUnarchiver_unarchiveObjectWithData(self, data as AnyObject, &error)
    try resolveError(error)
    return result
  }
}

extension NSURL : _FileReferenceLiteralConvertible {
  private convenience init(failableFileReferenceLiteral path: String) {
    let fullPath = NSBundle.mainBundle().pathForResource(path, ofType: nil)!
    self.init(fileURLWithPath: fullPath)
  }

  public required convenience init(fileReferenceLiteral path: String) {
    self.init(failableFileReferenceLiteral: path)
  }
}

public typealias _FileReferenceLiteralType = NSURL
