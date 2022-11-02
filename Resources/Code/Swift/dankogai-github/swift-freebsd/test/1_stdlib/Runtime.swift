// RUN: rm -rf %t  &&  mkdir %t
//
// RUN: %target-clang %S/Inputs/Mirror/Mirror.mm -c -o %t/Mirror.mm.o -g
// RUN: %target-build-swift -parse-stdlib -Xfrontend -disable-access-control -module-name a -I %S/Inputs/Mirror/ -Xlinker %t/Mirror.mm.o %s -o %t.out
// RUN: %target-run %t.out
// REQUIRES: executable_test

// XFAIL: linux

import Swift
import StdlibUnittest
import Foundation
import CoreGraphics
import SwiftShims
import MirrorObjC

var nsObjectCanaryCount = 0
@objc class NSObjectCanary : NSObject {
  override init() {
    ++nsObjectCanaryCount
  }
  deinit {
    --nsObjectCanaryCount
  }
}

struct NSObjectCanaryStruct {
  var ref = NSObjectCanary()
}

var swiftObjectCanaryCount = 0
class SwiftObjectCanary {
  init() {
    ++swiftObjectCanaryCount
  }
  deinit {
    --swiftObjectCanaryCount
  }
}

struct SwiftObjectCanaryStruct {
  var ref = SwiftObjectCanary()
}

@objc class ClassA {
  init(value: Int) {
    self.value = value
  }

  var value: Int
}

struct NotBridgedValueType {
  // Keep it pointer-sized.
  var canaryRef = SwiftObjectCanary()
}

struct BridgedValueType : _ObjectiveCBridgeable {
  init(value: Int) {
    self.value = value
  }

  static func _getObjectiveCType() -> Any.Type {
    return ClassA.self
  }

  func _bridgeToObjectiveC() -> ClassA {
    return ClassA(value: value)
  }

  static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  static func _forceBridgeFromObjectiveC(
    x: ClassA,
    inout result: BridgedValueType?
  ) {
    assert(x.value % 2 == 0, "not bridged to Objective-C")
    result = BridgedValueType(value: x.value)
  }

  static func _conditionallyBridgeFromObjectiveC(
    x: ClassA,
    inout result: BridgedValueType?
  ) -> Bool {
    if x.value % 2 == 0 {
      result = BridgedValueType(value: x.value)
      return true
    }

    result = nil
    return false
  }

  var value: Int
  var canaryRef = SwiftObjectCanary()
}

struct BridgedLargeValueType : _ObjectiveCBridgeable {
  init(value: Int) {
    value0 = value
    value1 = value
    value2 = value
    value3 = value
    value4 = value
    value5 = value
    value6 = value
    value7 = value
  }

  static func _getObjectiveCType() -> Any.Type {
    return ClassA.self
  }

  func _bridgeToObjectiveC() -> ClassA {
    assert(value == value0)
    return ClassA(value: value0)
  }

  static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  static func _forceBridgeFromObjectiveC(
    x: ClassA,
    inout result: BridgedLargeValueType?
  ) {
    assert(x.value % 2 == 0, "not bridged to Objective-C")
    result = BridgedLargeValueType(value: x.value)
  }

  static func _conditionallyBridgeFromObjectiveC(
    x: ClassA,
    inout result: BridgedLargeValueType?
  ) -> Bool {
    if x.value % 2 == 0 {
      result = BridgedLargeValueType(value: x.value)
      return true
    }

    result = nil
    return false
  }

  var value: Int {
    let x = value0
    assert(value0 == x && value1 == x && value2 == x && value3 == x &&
           value4 == x && value5 == x && value6 == x && value7 == x)
    return x
  }

  var (value0, value1, value2, value3): (Int, Int, Int, Int)
  var (value4, value5, value6, value7): (Int, Int, Int, Int)
  var canaryRef = SwiftObjectCanary()
}


struct ConditionallyBridgedValueType<T> : _ObjectiveCBridgeable {
  init(value: Int) {
    self.value = value
  }

  static func _getObjectiveCType() -> Any.Type {
    return ClassA.self
  }

  func _bridgeToObjectiveC() -> ClassA {
    return ClassA(value: value)
  }

  static func _forceBridgeFromObjectiveC(
    x: ClassA,
    inout result: ConditionallyBridgedValueType?
  ) {
    assert(x.value % 2 == 0, "not bridged from Objective-C")
    result = ConditionallyBridgedValueType(value: x.value)
  }

  static func _conditionallyBridgeFromObjectiveC(
    x: ClassA,
    inout result: ConditionallyBridgedValueType?
  ) -> Bool {
    if x.value % 2 == 0 {
      result = ConditionallyBridgedValueType(value: x.value)
      return true
    }

    result = nil
    return false
  }

  static func _isBridgedToObjectiveC() -> Bool {
    return ((T.self as Any) as? String.Type) == nil
  }

  var value: Int
  var canaryRef = SwiftObjectCanary()
}

class BridgedVerbatimRefType {
  var value: Int = 42
  var canaryRef = SwiftObjectCanary()
}

func withSwiftObjectCanary<T>(
  createValue: () -> T,
  _ check: (T) -> Void,
  file: String = __FILE__, line: UInt = __LINE__
) {
  let stackTrace = SourceLocStack(SourceLoc(file, line))

  swiftObjectCanaryCount = 0
  autoreleasepool {
    var valueWithCanary = createValue()
    expectEqual(1, swiftObjectCanaryCount, stackTrace: stackTrace)
    check(valueWithCanary)
  }
  expectEqual(0, swiftObjectCanaryCount, stackTrace: stackTrace)
}

var Runtime = TestSuite("Runtime")

func _isClassOrObjCExistential_Opaque<T>(x: T.Type) -> Bool {
  return _isClassOrObjCExistential(_opaqueIdentity(x))
}

Runtime.test("_isClassOrObjCExistential") {
  expectTrue(_isClassOrObjCExistential(NSObjectCanary.self))
  expectTrue(_isClassOrObjCExistential_Opaque(NSObjectCanary.self))

  expectFalse(_isClassOrObjCExistential(NSObjectCanaryStruct.self))
  expectFalse(_isClassOrObjCExistential_Opaque(NSObjectCanaryStruct.self))

  expectTrue(_isClassOrObjCExistential(SwiftObjectCanary.self))
  expectTrue(_isClassOrObjCExistential_Opaque(SwiftObjectCanary.self))

  expectFalse(_isClassOrObjCExistential(SwiftObjectCanaryStruct.self))
  expectFalse(_isClassOrObjCExistential_Opaque(SwiftObjectCanaryStruct.self))

  typealias SwiftClosure = ()->()
  expectFalse(_isClassOrObjCExistential(SwiftClosure.self))
  expectFalse(_isClassOrObjCExistential_Opaque(SwiftClosure.self))

  typealias ObjCClosure = @convention(block) ()->()
  expectTrue(_isClassOrObjCExistential(ObjCClosure.self))
  expectTrue(_isClassOrObjCExistential_Opaque(ObjCClosure.self))

  expectTrue(_isClassOrObjCExistential(CFArray.self))
  expectTrue(_isClassOrObjCExistential_Opaque(CFArray.self))

  expectTrue(_isClassOrObjCExistential(CFArray.self))
  expectTrue(_isClassOrObjCExistential_Opaque(CFArray.self))

  expectTrue(_isClassOrObjCExistential(AnyObject.self))
  expectTrue(_isClassOrObjCExistential_Opaque(AnyObject.self))

  // AnyClass == AnyObject.Type
  expectFalse(_isClassOrObjCExistential(AnyClass.self))
  expectFalse(_isClassOrObjCExistential_Opaque(AnyClass.self))

  expectFalse(_isClassOrObjCExistential(AnyObject.Protocol.self))
  expectFalse(_isClassOrObjCExistential_Opaque(AnyObject.Protocol.self))

  expectFalse(_isClassOrObjCExistential(NSObjectCanary.Type.self))
  expectFalse(_isClassOrObjCExistential_Opaque(NSObjectCanary.Type.self))
}

Runtime.test("_canBeClass") {
  expectEqual(1, _canBeClass(NSObjectCanary.self))
  expectEqual(0, _canBeClass(NSObjectCanaryStruct.self))
  expectEqual(1, _canBeClass(SwiftObjectCanary.self))
  expectEqual(0, _canBeClass(SwiftObjectCanaryStruct.self))

  typealias SwiftClosure = ()->()
  expectEqual(0, _canBeClass(SwiftClosure.self))

  typealias ObjCClosure = @convention(block) ()->()
  expectEqual(1, _canBeClass(ObjCClosure.self))

  expectEqual(1, _canBeClass(CFArray.self))
}

Runtime.test("bridgeToObjectiveC") {
  expectEmpty(_bridgeToObjectiveC(NotBridgedValueType()))

  expectEqual(42, (_bridgeToObjectiveC(BridgedValueType(value: 42)) as! ClassA).value)

  expectEqual(42, (_bridgeToObjectiveC(BridgedLargeValueType(value: 42)) as! ClassA).value)

  expectEqual(42, (_bridgeToObjectiveC(ConditionallyBridgedValueType<Int>(value: 42)) as! ClassA).value)

  expectEmpty(_bridgeToObjectiveC(ConditionallyBridgedValueType<String>(value: 42)))

  var bridgedVerbatimRef = BridgedVerbatimRefType()
  expectTrue(_bridgeToObjectiveC(bridgedVerbatimRef) === bridgedVerbatimRef)
}

Runtime.test("bridgeToObjectiveC/NoLeak") {
  withSwiftObjectCanary(
    { NotBridgedValueType() },
    { expectEmpty(_bridgeToObjectiveC($0)) })

  withSwiftObjectCanary(
    { BridgedValueType(value: 42) },
    { expectEqual(42, (_bridgeToObjectiveC($0) as! ClassA).value) })

  withSwiftObjectCanary(
    { BridgedLargeValueType(value: 42) },
    { expectEqual(42, (_bridgeToObjectiveC($0) as! ClassA).value) })

  withSwiftObjectCanary(
    { ConditionallyBridgedValueType<Int>(value: 42) },
    { expectEqual(42, (_bridgeToObjectiveC($0) as! ClassA).value) })

  withSwiftObjectCanary(
    { ConditionallyBridgedValueType<String>(value: 42) },
    { expectEmpty(_bridgeToObjectiveC($0)) })

  withSwiftObjectCanary(
    { BridgedVerbatimRefType() },
    { expectTrue(_bridgeToObjectiveC($0) === $0) })
}

Runtime.test("forceBridgeFromObjectiveC") {
  // Bridge back using NotBridgedValueType.
  expectEmpty(_conditionallyBridgeFromObjectiveC(
      ClassA(value: 21), NotBridgedValueType.self))

  expectEmpty(_conditionallyBridgeFromObjectiveC(
      ClassA(value: 42), NotBridgedValueType.self))

  expectEmpty(_conditionallyBridgeFromObjectiveC(
      BridgedVerbatimRefType(), NotBridgedValueType.self))

  // Bridge back using BridgedValueType.
  expectEmpty(_conditionallyBridgeFromObjectiveC(
      ClassA(value: 21), BridgedValueType.self))

  expectEqual(42, _forceBridgeFromObjectiveC(
      ClassA(value: 42), BridgedValueType.self).value)
  expectEqual(42, _conditionallyBridgeFromObjectiveC(
      ClassA(value: 42), BridgedValueType.self)!.value)

  expectEmpty(_conditionallyBridgeFromObjectiveC(
      BridgedVerbatimRefType(), BridgedValueType.self))

  // Bridge back using BridgedLargeValueType.
  expectEmpty(_conditionallyBridgeFromObjectiveC(
      ClassA(value: 21), BridgedLargeValueType.self))

  expectEqual(42, _forceBridgeFromObjectiveC(
      ClassA(value: 42), BridgedLargeValueType.self).value)
  expectEqual(42, _conditionallyBridgeFromObjectiveC(
      ClassA(value: 42), BridgedLargeValueType.self)!.value)

  expectEmpty(_conditionallyBridgeFromObjectiveC(
      BridgedVerbatimRefType(), BridgedLargeValueType.self))

  // Bridge back using BridgedVerbatimRefType.
  expectEmpty(_conditionallyBridgeFromObjectiveC(
      ClassA(value: 21), BridgedVerbatimRefType.self))

  expectEmpty(_conditionallyBridgeFromObjectiveC(
      ClassA(value: 42), BridgedVerbatimRefType.self))

  var bridgedVerbatimRef = BridgedVerbatimRefType()
  expectTrue(_forceBridgeFromObjectiveC(
      bridgedVerbatimRef, BridgedVerbatimRefType.self) === bridgedVerbatimRef)
  expectTrue(_conditionallyBridgeFromObjectiveC(
      bridgedVerbatimRef, BridgedVerbatimRefType.self)! === bridgedVerbatimRef)
}

Runtime.test("isBridgedToObjectiveC") {
  expectFalse(_isBridgedToObjectiveC(NotBridgedValueType))
  expectTrue(_isBridgedToObjectiveC(BridgedValueType))
  expectTrue(_isBridgedToObjectiveC(BridgedVerbatimRefType))
}

Runtime.test("isBridgedVerbatimToObjectiveC") {
  expectFalse(_isBridgedVerbatimToObjectiveC(NotBridgedValueType))
  expectFalse(_isBridgedVerbatimToObjectiveC(BridgedValueType))
  expectTrue(_isBridgedVerbatimToObjectiveC(BridgedVerbatimRefType))
}

//===---------------------------------------------------------------------===//

// The protocol should be defined in the standard library, otherwise the cast
// does not work.
typealias P1 = BooleanType
typealias P2 = CustomStringConvertible
protocol Q1 {}

// A small struct that can be stored inline in an opaque buffer.
struct StructConformsToP1 : BooleanType, Q1 {
  var boolValue: Bool {
    return true
  }
}

// A small struct that can be stored inline in an opaque buffer.
struct Struct2ConformsToP1<T : BooleanType> : BooleanType, Q1 {
  init(_ value: T) {
    self.value = value
  }
  var boolValue: Bool {
    return value.boolValue
  }
  var value: T
}

// A large struct that can not be stored inline in an opaque buffer.
struct Struct3ConformsToP2 : CustomStringConvertible, Q1 {
  var a: UInt64 = 10
  var b: UInt64 = 20
  var c: UInt64 = 30
  var d: UInt64 = 40

  var description: String {
    // Don't rely on string interpolation, it uses the casts that we are trying
    // to test.
    var result = ""
    result += _uint64ToString(a) + " "
    result += _uint64ToString(b) + " "
    result += _uint64ToString(c) + " "
    result += _uint64ToString(d)
    return result
  }
}

// A large struct that can not be stored inline in an opaque buffer.
struct Struct4ConformsToP2<T : CustomStringConvertible> : CustomStringConvertible, Q1 {
  var value: T
  var e: UInt64 = 50
  var f: UInt64 = 60
  var g: UInt64 = 70
  var h: UInt64 = 80

  init(_ value: T) {
    self.value = value
  }

  var description: String {
    // Don't rely on string interpolation, it uses the casts that we are trying
    // to test.
    var result = value.description + " "
    result += _uint64ToString(e) + " "
    result += _uint64ToString(f) + " "
    result += _uint64ToString(g) + " "
    result += _uint64ToString(h)
    return result
  }
}

struct StructDoesNotConformToP1 : Q1 {}

class ClassConformsToP1 : BooleanType, Q1 {
  var boolValue: Bool {
    return true
  }
}

class Class2ConformsToP1<T : BooleanType> : BooleanType, Q1 {
  init(_ value: T) {
    self.value = [ value ]
  }
  var boolValue: Bool {
    return value[0].boolValue
  }
  // FIXME: should be "var value: T", but we don't support it now.
  var value: Array<T>
}

class ClassDoesNotConformToP1 : Q1 {}

Runtime.test("dynamicCasting with as") {
  var someP1Value = StructConformsToP1()
  var someP1Value2 = Struct2ConformsToP1(true)
  var someNotP1Value = StructDoesNotConformToP1()
  var someP2Value = Struct3ConformsToP2()
  var someP2Value2 = Struct4ConformsToP2(Struct3ConformsToP2())
  var someP1Ref = ClassConformsToP1()
  var someP1Ref2 = Class2ConformsToP1(true)
  var someNotP1Ref = ClassDoesNotConformToP1()

  expectTrue(someP1Value is P1)
  expectTrue(someP1Value2 is P1)
  expectFalse(someNotP1Value is P1)
  expectTrue(someP2Value is P2)
  expectTrue(someP2Value2 is P2)
  expectTrue(someP1Ref is P1)
  expectTrue(someP1Ref2 is P1)
  expectFalse(someNotP1Ref is P1)

  expectTrue(someP1Value as P1 is P1)
  expectTrue(someP1Value2 as P1 is P1)
  expectTrue(someP2Value as P2 is P2)
  expectTrue(someP2Value2 as P2 is P2)
  expectTrue(someP1Ref as P1 is P1)

  expectTrue(someP1Value as Q1 is P1)
  expectTrue(someP1Value2 as Q1 is P1)
  expectFalse(someNotP1Value as Q1 is P1)
  expectTrue(someP2Value as Q1 is P2)
  expectTrue(someP2Value2 as Q1 is P2)
  expectTrue(someP1Ref as Q1 is P1)
  expectTrue(someP1Ref2 as Q1 is P1)
  expectFalse(someNotP1Ref as Q1 is P1)

  expectTrue(someP1Value as Any is P1)
  expectTrue(someP1Value2 as Any is P1)
  expectFalse(someNotP1Value as Any is P1)
  expectTrue(someP2Value as Any is P2)
  expectTrue(someP2Value2 as Any is P2)
  expectTrue(someP1Ref as Any is P1)
  expectTrue(someP1Ref2 as Any is P1)
  expectFalse(someNotP1Ref as Any is P1)

  expectTrue(someP1Ref as AnyObject is P1)
  expectTrue(someP1Ref2 as AnyObject is P1)
  expectFalse(someNotP1Ref as AnyObject is P1)

  expectTrue((someP1Value as P1).boolValue)
  expectTrue((someP1Value2 as P1).boolValue)
  expectEqual("10 20 30 40", (someP2Value as P2).description)
  expectEqual("10 20 30 40 50 60 70 80", (someP2Value2 as P2).description)

  expectTrue((someP1Ref as P1).boolValue)
  expectTrue((someP1Ref2 as P1).boolValue)

  expectTrue(((someP1Value as Q1) as! P1).boolValue)
  expectTrue(((someP1Value2 as Q1) as! P1).boolValue)
  expectEqual("10 20 30 40", ((someP2Value as Q1) as! P2).description)
  expectEqual("10 20 30 40 50 60 70 80",
    ((someP2Value2 as Q1) as! P2).description)
  expectTrue(((someP1Ref as Q1) as! P1).boolValue)
  expectTrue(((someP1Ref2 as Q1) as! P1).boolValue)

  expectTrue(((someP1Value as Any) as! P1).boolValue)
  expectTrue(((someP1Value2 as Any) as! P1).boolValue)
  expectEqual("10 20 30 40", ((someP2Value as Any) as! P2).description)
  expectEqual("10 20 30 40 50 60 70 80",
    ((someP2Value2 as Any) as! P2).description)
  expectTrue(((someP1Ref as Any) as! P1).boolValue)
  expectTrue(((someP1Ref2 as Any) as! P1).boolValue)

  expectTrue(((someP1Ref as AnyObject) as! P1).boolValue)

  expectEmpty((someNotP1Value as? P1))
  expectEmpty((someNotP1Ref as? P1))

  expectTrue(((someP1Value as Q1) as? P1)!.boolValue)
  expectTrue(((someP1Value2 as Q1) as? P1)!.boolValue)
  expectEmpty(((someNotP1Value as Q1) as? P1))
  expectEqual("10 20 30 40", ((someP2Value as Q1) as? P2)!.description)
  expectEqual("10 20 30 40 50 60 70 80",
    ((someP2Value2 as Q1) as? P2)!.description)
  expectTrue(((someP1Ref as Q1) as? P1)!.boolValue)
  expectTrue(((someP1Ref2 as Q1) as? P1)!.boolValue)
  expectEmpty(((someNotP1Ref as Q1) as? P1))

  expectTrue(((someP1Value as Any) as? P1)!.boolValue)
  expectTrue(((someP1Value2 as Any) as? P1)!.boolValue)
  expectEmpty(((someNotP1Value as Any) as? P1))
  expectEqual("10 20 30 40", ((someP2Value as Any) as? P2)!.description)
  expectEqual("10 20 30 40 50 60 70 80",
    ((someP2Value2 as Any) as? P2)!.description)
  expectTrue(((someP1Ref as Any) as? P1)!.boolValue)
  expectTrue(((someP1Ref2 as Any) as? P1)!.boolValue)
  expectEmpty(((someNotP1Ref as Any) as? P1))

  expectTrue(((someP1Ref as AnyObject) as? P1)!.boolValue)
  expectTrue(((someP1Ref2 as AnyObject) as? P1)!.boolValue)
  expectEmpty(((someNotP1Ref as AnyObject) as? P1))

  let doesThrow: Int throws -> Int = { $0 }
  let doesNotThrow: String -> String = { $0 }

  var any: Any = doesThrow

  expectTrue(doesThrow as Any is Int throws -> Int)
  expectFalse(doesThrow as Any is String throws -> Int)
  expectFalse(doesThrow as Any is String throws -> String)
  expectFalse(doesThrow as Any is Int throws -> String)
  expectFalse(doesThrow as Any is Int -> Int)
  expectFalse(doesThrow as Any is String throws -> String)
  expectFalse(doesThrow as Any is String -> String)
  expectTrue(doesNotThrow as Any is String throws -> String)
  expectTrue(doesNotThrow as Any is String -> String)
  expectFalse(doesNotThrow as Any is Int -> String)
  expectFalse(doesNotThrow as Any is Int -> Int)
  expectFalse(doesNotThrow as Any is String -> Int)
  expectFalse(doesNotThrow as Any is Int throws -> Int)
  expectFalse(doesNotThrow as Any is Int -> Int)
}

extension Int {
  class ExtensionClassConformsToP2 : P2 {
    var description: String { return "abc" }
  }

  private class PrivateExtensionClassConformsToP2 : P2 {
    var description: String { return "def" }
  }
}

Runtime.test("dynamic cast to existential with cross-module extensions") {
  let internalObj = Int.ExtensionClassConformsToP2()
  let privateObj = Int.PrivateExtensionClassConformsToP2()

  expectTrue(internalObj is P2)
  expectTrue(privateObj is P2)
}

class SomeClass {}
@objc class SomeObjCClass {}
class SomeNSObjectSubclass : NSObject {}
struct SomeStruct {}
enum SomeEnum {
  case A
  init() { self = .A }
}

Runtime.test("typeName") {
  expectEqual("a.SomeClass", _typeName(SomeClass.self))
  expectEqual("a.SomeObjCClass", _typeName(SomeObjCClass.self))
  expectEqual("a.SomeNSObjectSubclass", _typeName(SomeNSObjectSubclass.self))
  expectEqual("NSObject", _typeName(NSObject.self))
  expectEqual("a.SomeStruct", _typeName(SomeStruct.self))
  expectEqual("a.SomeEnum", _typeName(SomeEnum.self))
  expectEqual("protocol<>.Protocol", _typeName(Any.Protocol.self))
  expectEqual("Swift.AnyObject.Protocol", _typeName(AnyObject.Protocol.self))
  expectEqual("Swift.AnyObject.Type.Protocol", _typeName(AnyClass.Protocol.self))
  expectEqual("Swift.Optional<Swift.AnyObject>.Type", _typeName((AnyObject?).Type.self))

  var a: Any = SomeClass()
  expectEqual("a.SomeClass", _typeName(a.dynamicType))

  a = SomeObjCClass()
  expectEqual("a.SomeObjCClass", _typeName(a.dynamicType))

  a = SomeNSObjectSubclass()
  expectEqual("a.SomeNSObjectSubclass", _typeName(a.dynamicType))

  a = NSObject()
  expectEqual("NSObject", _typeName(a.dynamicType))

  a = SomeStruct()
  expectEqual("a.SomeStruct", _typeName(a.dynamicType))

  a = SomeEnum()
  expectEqual("a.SomeEnum", _typeName(a.dynamicType))

  a = AnyObject.self
  expectEqual("Swift.AnyObject.Protocol", _typeName(a.dynamicType))

  a = AnyClass.self
  expectEqual("Swift.AnyObject.Type.Protocol", _typeName(a.dynamicType))

  a = (AnyObject?).self
  expectEqual("Swift.Optional<Swift.AnyObject>.Type",
    _typeName(a.dynamicType))

  a = Any.self
  expectEqual("protocol<>.Protocol", _typeName(a.dynamicType))
}

Runtime.test("_stdlib_atomicCompareExchangeStrongPtr") {
  typealias IntPtr = UnsafeMutablePointer<Int>
  var origP1 = IntPtr(bitPattern: 0x10101010)
  var origP2 = IntPtr(bitPattern: 0x20202020)
  var origP3 = IntPtr(bitPattern: 0x30303030)

  do {
    var object = origP1
    var expected = origP1
    let r = _stdlib_atomicCompareExchangeStrongPtr(
      object: &object, expected: &expected, desired: origP2)
    expectTrue(r)
    expectEqual(origP2, object)
    expectEqual(origP1, expected)
  }
  do {
    var object = origP1
    var expected = origP2
    let r = _stdlib_atomicCompareExchangeStrongPtr(
      object: &object, expected: &expected, desired: origP3)
    expectFalse(r)
    expectEqual(origP1, object)
    expectEqual(origP1, expected)
  }

  struct FooStruct {
    var i: Int
    var object: IntPtr
    var expected: IntPtr

    init(_ object: IntPtr, _ expected: IntPtr) {
      self.i = 0
      self.object = object
      self.expected = expected
    }
  }
  do {
    var foo = FooStruct(origP1, origP1)
    let r = _stdlib_atomicCompareExchangeStrongPtr(
      object: &foo.object, expected: &foo.expected, desired: origP2)
    expectTrue(r)
    expectEqual(origP2, foo.object)
    expectEqual(origP1, foo.expected)
  }
  do {
    var foo = FooStruct(origP1, origP2)
    let r = _stdlib_atomicCompareExchangeStrongPtr(
      object: &foo.object, expected: &foo.expected, desired: origP3)
    expectFalse(r)
    expectEqual(origP1, foo.object)
    expectEqual(origP1, foo.expected)
  }
}

class GenericClass<T> {}
class MultiGenericClass<T, U> {}
struct GenericStruct<T> {}
enum GenericEnum<T> {}

struct PlainStruct {}
enum PlainEnum {}

protocol ProtocolA {}
protocol ProtocolB {}

Runtime.test("Generic class ObjC runtime names") {
  expectEqual("_TtGC1a12GenericClassSi_",
              NSStringFromClass(GenericClass<Int>.self))
  expectEqual("_TtGC1a12GenericClassVS_11PlainStruct_",
              NSStringFromClass(GenericClass<PlainStruct>.self))
  expectEqual("_TtGC1a12GenericClassOS_9PlainEnum_",
              NSStringFromClass(GenericClass<PlainEnum>.self))
  expectEqual("_TtGC1a12GenericClassTVS_11PlainStructOS_9PlainEnumS1___",
              NSStringFromClass(GenericClass<(PlainStruct, PlainEnum, PlainStruct)>.self))
  expectEqual("_TtGC1a12GenericClassMVS_11PlainStruct_",
              NSStringFromClass(GenericClass<PlainStruct.Type>.self))
  expectEqual("_TtGC1a12GenericClassFMVS_11PlainStructS1__",
              NSStringFromClass(GenericClass<PlainStruct.Type -> PlainStruct>.self))

  expectEqual("_TtGC1a12GenericClassFzMVS_11PlainStructS1__",
              NSStringFromClass(GenericClass<PlainStruct.Type throws -> PlainStruct>.self))
  expectEqual("_TtGC1a12GenericClassFTVS_11PlainStructROS_9PlainEnum_Si_",
              NSStringFromClass(GenericClass<(PlainStruct, inout PlainEnum) -> Int>.self))

  expectEqual("_TtGC1a12GenericClassPS_9ProtocolA__",
              NSStringFromClass(GenericClass<ProtocolA>.self))
  expectEqual("_TtGC1a12GenericClassPS_9ProtocolAS_9ProtocolB__",
              NSStringFromClass(GenericClass<protocol<ProtocolA, ProtocolB>>.self))
  expectEqual("_TtGC1a12GenericClassPMPS_9ProtocolAS_9ProtocolB__",
              NSStringFromClass(GenericClass<protocol<ProtocolA, ProtocolB>.Type>.self))
  expectEqual("_TtGC1a12GenericClassMPS_9ProtocolAS_9ProtocolB__",
              NSStringFromClass(GenericClass<protocol<ProtocolB, ProtocolA>.Protocol>.self))

  expectEqual("_TtGC1a12GenericClassCSo7CFArray_",
              NSStringFromClass(GenericClass<CFArray>.self))
  expectEqual("_TtGC1a12GenericClassVSC9NSDecimal_",
              NSStringFromClass(GenericClass<NSDecimal>.self))
  expectEqual("_TtGC1a12GenericClassCSo8NSObject_",
              NSStringFromClass(GenericClass<NSObject>.self))
  expectEqual("_TtGC1a12GenericClassCSo8NSObject_",
              NSStringFromClass(GenericClass<NSObject>.self))
  expectEqual("_TtGC1a12GenericClassPSo9NSCopying__",
              NSStringFromClass(GenericClass<NSCopying>.self))
  expectEqual("_TtGC1a12GenericClassPSo9NSCopyingS_9ProtocolAS_9ProtocolB__",
              NSStringFromClass(GenericClass<protocol<ProtocolB, NSCopying, ProtocolA>>.self))

  expectEqual("_TtGC1a17MultiGenericClassGVS_13GenericStructSi_GOS_11GenericEnumGS2_Si___",
              NSStringFromClass(MultiGenericClass<GenericStruct<Int>,
                                                  GenericEnum<GenericEnum<Int>>>.self))
}

Runtime.test("casting AnyObject to class metatypes") {
  do {
    var ao: AnyObject = SomeClass()
    expectTrue(ao as? Any.Type == nil)
    expectTrue(ao as? AnyClass == nil)

    ao = SomeNSObjectSubclass()
    expectTrue(ao as? Any.Type == nil)
    expectTrue(ao as? AnyClass == nil)

    ao = SomeClass.self
    expectTrue(ao as? Any.Type == SomeClass.self)
    expectTrue(ao as? AnyClass == SomeClass.self)
    expectTrue(ao as? SomeClass.Type == SomeClass.self)

    ao = SomeNSObjectSubclass.self
    expectTrue(ao as? Any.Type == SomeNSObjectSubclass.self)
    expectTrue(ao as? AnyClass == SomeNSObjectSubclass.self)
    expectTrue(ao as? SomeNSObjectSubclass.Type == SomeNSObjectSubclass.self)
  }

  do {
    var a: Any = SomeClass()
    expectTrue(a as? Any.Type == nil)
    expectTrue(a as? AnyClass == nil)

    a = SomeNSObjectSubclass()
    expectTrue(a as? Any.Type == nil)
    expectTrue(a as? AnyClass == nil)

    a = SomeClass.self
    expectTrue(a as? Any.Type == SomeClass.self)
    expectTrue(a as? AnyClass == SomeClass.self)
    expectTrue(a as? SomeClass.Type == SomeClass.self)
  }

  do {
    var nso: NSObject = SomeNSObjectSubclass()
    expectTrue(nso as? AnyClass == nil)
    
    nso = (SomeNSObjectSubclass.self as AnyObject) as! NSObject
    expectTrue(nso as? Any.Type == SomeNSObjectSubclass.self)
    expectTrue(nso as? AnyClass == SomeNSObjectSubclass.self)
    expectTrue(nso as? SomeNSObjectSubclass.Type == SomeNSObjectSubclass.self)
  }
}

class Malkovich: Malkovichable {
  var malkovich: String { return "malkovich" }
}
protocol Malkovichable: class {
  var malkovich: String { get }
}

struct GenericStructWithReferenceStorage<T> {
  var a: T
  unowned(safe)   var unownedConcrete: Malkovich
  unowned(unsafe) var unmanagedConcrete: Malkovich
  weak            var weakConcrete: Malkovich?

  unowned(safe)   var unownedProto: Malkovichable
  unowned(unsafe) var unmanagedProto: Malkovichable
  weak            var weakProto: Malkovichable?
}

func exerciseReferenceStorageInGenericContext<T>(
    x: GenericStructWithReferenceStorage<T>,
    forceCopy y: GenericStructWithReferenceStorage<T>
) {
  expectEqual(x.unownedConcrete.malkovich, "malkovich")
  expectEqual(x.unmanagedConcrete.malkovich, "malkovich")
  expectEqual(x.weakConcrete!.malkovich, "malkovich")
  expectEqual(x.unownedProto.malkovich, "malkovich")
  expectEqual(x.unmanagedProto.malkovich, "malkovich")
  expectEqual(x.weakProto!.malkovich, "malkovich")

  expectEqual(y.unownedConcrete.malkovich, "malkovich")
  expectEqual(y.unmanagedConcrete.malkovich, "malkovich")
  expectEqual(y.weakConcrete!.malkovich, "malkovich")
  expectEqual(y.unownedProto.malkovich, "malkovich")
  expectEqual(y.unmanagedProto.malkovich, "malkovich")
  expectEqual(y.weakProto!.malkovich, "malkovich")
}

Runtime.test("Struct layout with reference storage types") {
  let malkovich = Malkovich()

  let x = GenericStructWithReferenceStorage(a:                 malkovich,
                                            unownedConcrete:   malkovich,
                                            unmanagedConcrete: malkovich,
                                            weakConcrete:      malkovich,
                                            unownedProto:      malkovich,
                                            unmanagedProto:    malkovich,
                                            weakProto:         malkovich)
  exerciseReferenceStorageInGenericContext(x, forceCopy: x)

  expectEqual(x.unownedConcrete.malkovich, "malkovich")
  expectEqual(x.unmanagedConcrete.malkovich, "malkovich")
  expectEqual(x.weakConcrete!.malkovich, "malkovich")
  expectEqual(x.unownedProto.malkovich, "malkovich")
  expectEqual(x.unmanagedProto.malkovich, "malkovich")
  expectEqual(x.weakProto!.malkovich, "malkovich")

  // Make sure malkovich lives long enough.
  print(malkovich)
}

var RuntimeFoundationWrappers = TestSuite("RuntimeFoundationWrappers")

RuntimeFoundationWrappers.test("_stdlib_NSObject_isEqual/NoLeak") {
  nsObjectCanaryCount = 0
  autoreleasepool {
    let a = NSObjectCanary()
    let b = NSObjectCanary()
    expectEqual(2, nsObjectCanaryCount)
    _stdlib_NSObject_isEqual(a, b)
  }
  expectEqual(0, nsObjectCanaryCount)
}

var nsStringCanaryCount = 0
@objc class NSStringCanary : NSString {
  override init() {
    ++nsStringCanaryCount
    super.init()
  }
  required init(coder: NSCoder) {
    fatalError("don't call this initializer")
  }
  deinit {
    --nsStringCanaryCount
  }
  @objc override var length: Int {
    return 0
  }
  @objc override func characterAtIndex(index: Int) -> unichar {
    fatalError("out-of-bounds access")
  }
}

RuntimeFoundationWrappers.test(
  "_stdlib_compareNSStringDeterministicUnicodeCollation/NoLeak"
) {
  nsStringCanaryCount = 0
  autoreleasepool {
    let a = NSStringCanary()
    let b = NSStringCanary()
    expectEqual(2, nsStringCanaryCount)
    _stdlib_compareNSStringDeterministicUnicodeCollation(a, b)
  }
  expectEqual(0, nsStringCanaryCount)
}

RuntimeFoundationWrappers.test("_stdlib_NSStringNFDHashValue/NoLeak") {
  nsStringCanaryCount = 0
  autoreleasepool {
    let a = NSStringCanary()
    expectEqual(1, nsStringCanaryCount)
    _stdlib_NSStringNFDHashValue(a)
  }
  expectEqual(0, nsStringCanaryCount)
}

RuntimeFoundationWrappers.test("_stdlib_NSStringASCIIHashValue/NoLeak") {
  nsStringCanaryCount = 0
  autoreleasepool {
    let a = NSStringCanary()
    expectEqual(1, nsStringCanaryCount)
    _stdlib_NSStringASCIIHashValue(a)
  }
  expectEqual(0, nsStringCanaryCount)
}

RuntimeFoundationWrappers.test("_stdlib_NSStringHasPrefixNFD/NoLeak") {
  nsStringCanaryCount = 0
  autoreleasepool {
    let a = NSStringCanary()
    let b = NSStringCanary()
    expectEqual(2, nsStringCanaryCount)
    _stdlib_NSStringHasPrefixNFD(a, b)
  }
  expectEqual(0, nsStringCanaryCount)
}

RuntimeFoundationWrappers.test("_stdlib_NSStringHasSuffixNFD/NoLeak") {
  nsStringCanaryCount = 0
  autoreleasepool {
    let a = NSStringCanary()
    let b = NSStringCanary()
    expectEqual(2, nsStringCanaryCount)
    _stdlib_NSStringHasSuffixNFD(a, b)
  }
  expectEqual(0, nsStringCanaryCount)
}

RuntimeFoundationWrappers.test("_stdlib_NSStringLowercaseString/NoLeak") {
  nsStringCanaryCount = 0
  autoreleasepool {
    let a = NSStringCanary()
    expectEqual(1, nsStringCanaryCount)
    _stdlib_NSStringLowercaseString(a)
  }
  expectEqual(0, nsStringCanaryCount)
}

RuntimeFoundationWrappers.test("_stdlib_NSStringUppercaseString/NoLeak") {
  nsStringCanaryCount = 0
  autoreleasepool {
    let a = NSStringCanary()
    expectEqual(1, nsStringCanaryCount)
    _stdlib_NSStringUppercaseString(a)
  }
  expectEqual(0, nsStringCanaryCount)
}

RuntimeFoundationWrappers.test("_stdlib_CFStringCreateCopy/NoLeak") {
  nsStringCanaryCount = 0
  autoreleasepool {
    let a = NSStringCanary()
    expectEqual(1, nsStringCanaryCount)
    _stdlib_binary_CFStringCreateCopy(a)
  }
  expectEqual(0, nsStringCanaryCount)
}

RuntimeFoundationWrappers.test("_stdlib_CFStringGetLength/NoLeak") {
  nsStringCanaryCount = 0
  autoreleasepool {
    let a = NSStringCanary()
    expectEqual(1, nsStringCanaryCount)
    _stdlib_binary_CFStringGetLength(a)
  }
  expectEqual(0, nsStringCanaryCount)
}

RuntimeFoundationWrappers.test("_stdlib_CFStringGetCharactersPtr/NoLeak") {
  nsStringCanaryCount = 0
  autoreleasepool {
    let a = NSStringCanary()
    expectEqual(1, nsStringCanaryCount)
    _stdlib_binary_CFStringGetCharactersPtr(a)
  }
  expectEqual(0, nsStringCanaryCount)
}

RuntimeFoundationWrappers.test("bridgedNSArray") {
  var c = [NSObject]()
  autoreleasepool {
    let a = [NSObject]()
    let b = a as NSArray
    c = b as! [NSObject]
  }
  c.append(NSObject())
  // expect no crash.
}

var Reflection = TestSuite("Reflection")

func wrap1   (x: Any) -> Any { return x }
func wrap2<T>(x: T)   -> Any { return wrap1(x) }
func wrap3   (x: Any) -> Any { return wrap2(x) }
func wrap4<T>(x: T)   -> Any { return wrap3(x) }
func wrap5   (x: Any) -> Any { return wrap4(x) }

class JustNeedAMetatype {}

Reflection.test("nested existential containers") {
  let wrapped = wrap5(JustNeedAMetatype.self)
  expectEqual("\(wrapped)", "JustNeedAMetatype")
}

Reflection.test("dumpToAStream") {
  var output = ""
  dump([ 42, 4242 ], &output)
  expectEqual("▿ 2 elements\n  - [0]: 42\n  - [1]: 4242\n", output)
}

struct StructWithDefaultMirror {
  let s: String

  init (_ s: String) {
    self.s = s
  }
}

Reflection.test("Struct/NonGeneric/DefaultMirror") {
  do {
    var output = ""
    dump(StructWithDefaultMirror("123"), &output)
    expectEqual("▿ a.StructWithDefaultMirror\n  - s: 123\n", output)
  }

  do {
    // Build a String around an interpolation as a way of smoke-testing that
    // the internal _MirrorType implementation gets memory management right.
    var output = ""
    dump(StructWithDefaultMirror("\(456)"), &output)
    expectEqual("▿ a.StructWithDefaultMirror\n  - s: 456\n", output)
  }

  // Structs have no identity and thus no object identifier
  expectEmpty(_reflect(StructWithDefaultMirror("")).objectIdentifier)

  // The default mirror provides no quick look object
  expectEmpty(_reflect(StructWithDefaultMirror("")).quickLookObject)

  expectEqual(.Struct, _reflect(StructWithDefaultMirror("")).disposition)
}

struct GenericStructWithDefaultMirror<T, U> {
  let first: T
  let second: U
}

Reflection.test("Struct/Generic/DefaultMirror") {
  do {
    var value = GenericStructWithDefaultMirror<Int, [Any?]>(
      first: 123,
      second: [ "abc", 456, 789.25 ])
    var output = ""
    dump(value, &output)

    let expected =
      "▿ a.GenericStructWithDefaultMirror<Swift.Int, Swift.Array<Swift.Optional<protocol<>>>>\n" +
      "  - first: 123\n" +
      "  ▿ second: 3 elements\n" +
      "    ▿ [0]: abc\n" +
      "      - Some: abc\n" +
      "    ▿ [1]: 456\n" +
      "      - Some: 456\n" +
      "    ▿ [2]: 789.25\n" +
      "      - Some: 789.25\n"

    expectEqual(expected, output)

  }
}

enum NoPayloadEnumWithDefaultMirror {
  case A, ß
}

Reflection.test("Enum/NoPayload/DefaultMirror") {
  do {
    let value: [NoPayloadEnumWithDefaultMirror] =
        [.A, .ß]
    var output = ""
    dump(value, &output)

    let expected =
      "▿ 2 elements\n" +
      "  - [0]: a.NoPayloadEnumWithDefaultMirror.A\n" +
      "  - [1]: a.NoPayloadEnumWithDefaultMirror.ß\n"

    expectEqual(expected, output)
  }
}

enum SingletonNonGenericEnumWithDefaultMirror {
  case OnlyOne(Int)
}

Reflection.test("Enum/SingletonNonGeneric/DefaultMirror") {
  do {
    let value = SingletonNonGenericEnumWithDefaultMirror.OnlyOne(5)
    var output = ""
    dump(value, &output)

    let expected =
      "▿ a.SingletonNonGenericEnumWithDefaultMirror.OnlyOne\n" +
      "  - OnlyOne: 5\n"

    expectEqual(expected, output)
  }
}

enum SingletonGenericEnumWithDefaultMirror<T> {
  case OnlyOne(T)
}

Reflection.test("Enum/SingletonGeneric/DefaultMirror") {
  do {
    let value = SingletonGenericEnumWithDefaultMirror.OnlyOne("IIfx")
    var output = ""
    dump(value, &output)

    let expected =
      "▿ a.SingletonGenericEnumWithDefaultMirror<Swift.String>.OnlyOne\n" +
      "  - OnlyOne: IIfx\n"

    expectEqual(expected, output)
  }
  expectEqual(0, LifetimeTracked.instances)
  do {
    let value = SingletonGenericEnumWithDefaultMirror.OnlyOne(
        LifetimeTracked(0))
    expectEqual(1, LifetimeTracked.instances)
    var output = ""
    dump(value, &output)
  }
  expectEqual(0, LifetimeTracked.instances)
}

enum SinglePayloadNonGenericEnumWithDefaultMirror {
  case Cat
  case Dog
  case Volleyball(String, Int)
}

Reflection.test("Enum/SinglePayloadNonGeneric/DefaultMirror") {
  do {
    let value: [SinglePayloadNonGenericEnumWithDefaultMirror] =
        [.Cat,
         .Dog,
         .Volleyball("Wilson", 2000)]
    var output = ""
    dump(value, &output)

    let expected =
      "▿ 3 elements\n" +
      "  - [0]: a.SinglePayloadNonGenericEnumWithDefaultMirror.Cat\n" +
      "  - [1]: a.SinglePayloadNonGenericEnumWithDefaultMirror.Dog\n" +
      "  ▿ [2]: a.SinglePayloadNonGenericEnumWithDefaultMirror.Volleyball\n" +
      "    ▿ Volleyball: (2 elements)\n" +
      "      - .0: Wilson\n" +
      "      - .1: 2000\n"

    expectEqual(expected, output)
  }
}

enum SinglePayloadGenericEnumWithDefaultMirror<T, U> {
  case Well
  case Faucet
  case Pipe(T, U)
}

Reflection.test("Enum/SinglePayloadGeneric/DefaultMirror") {
  do {
    let value: [SinglePayloadGenericEnumWithDefaultMirror<Int, [Int]>] =
        [.Well,
         .Faucet,
         .Pipe(408, [415])]
    var output = ""
    dump(value, &output)

    let expected =
      "▿ 3 elements\n" +
      "  - [0]: a.SinglePayloadGenericEnumWithDefaultMirror<Swift.Int, Swift.Array<Swift.Int>>.Well\n" +
      "  - [1]: a.SinglePayloadGenericEnumWithDefaultMirror<Swift.Int, Swift.Array<Swift.Int>>.Faucet\n" +
      "  ▿ [2]: a.SinglePayloadGenericEnumWithDefaultMirror<Swift.Int, Swift.Array<Swift.Int>>.Pipe\n" +
      "    ▿ Pipe: (2 elements)\n" +
      "      - .0: 408\n" +
      "      ▿ .1: 1 element\n" +
      "        - [0]: 415\n"

    expectEqual(expected, output)
  }
}

enum MultiPayloadTagBitsNonGenericEnumWithDefaultMirror {
  case Plus
  case SE30
  case Classic(mhz: Int)
  case Performa(model: Int)
}

Reflection.test("Enum/MultiPayloadTagBitsNonGeneric/DefaultMirror") {
  do {
    let value: [MultiPayloadTagBitsNonGenericEnumWithDefaultMirror] =
        [.Plus,
         .SE30,
         .Classic(mhz: 16),
         .Performa(model: 220)]
    var output = ""
    dump(value, &output)

    let expected =
      "▿ 4 elements\n" +
      "  - [0]: a.MultiPayloadTagBitsNonGenericEnumWithDefaultMirror.Plus\n" +
      "  - [1]: a.MultiPayloadTagBitsNonGenericEnumWithDefaultMirror.SE30\n" +
      "  ▿ [2]: a.MultiPayloadTagBitsNonGenericEnumWithDefaultMirror.Classic\n" +
      "    - Classic: 16\n" +
      "  ▿ [3]: a.MultiPayloadTagBitsNonGenericEnumWithDefaultMirror.Performa\n" +
      "    - Performa: 220\n"

    expectEqual(expected, output)
  }
}

class Floppy {
  let capacity: Int

  init(capacity: Int) { self.capacity = capacity }
}

class CDROM {
  let capacity: Int

  init(capacity: Int) { self.capacity = capacity }
}

enum MultiPayloadSpareBitsNonGenericEnumWithDefaultMirror {
  case MacWrite
  case MacPaint
  case FileMaker
  case ClarisWorks(floppy: Floppy)
  case HyperCard(cdrom: CDROM)
}

Reflection.test("Enum/MultiPayloadSpareBitsNonGeneric/DefaultMirror") {
  do {
    let value: [MultiPayloadSpareBitsNonGenericEnumWithDefaultMirror] =
        [.MacWrite,
         .MacPaint,
         .FileMaker,
         .ClarisWorks(floppy: Floppy(capacity: 800)),
         .HyperCard(cdrom: CDROM(capacity: 600))]

    var output = ""
    dump(value, &output)

    let expected =
      "▿ 5 elements\n" +
      "  - [0]: a.MultiPayloadSpareBitsNonGenericEnumWithDefaultMirror.MacWrite\n" +
      "  - [1]: a.MultiPayloadSpareBitsNonGenericEnumWithDefaultMirror.MacPaint\n" +
      "  - [2]: a.MultiPayloadSpareBitsNonGenericEnumWithDefaultMirror.FileMaker\n" +
      "  ▿ [3]: a.MultiPayloadSpareBitsNonGenericEnumWithDefaultMirror.ClarisWorks\n" +
      "    ▿ ClarisWorks: a.Floppy #0\n" +
      "      - capacity: 800\n" +
      "  ▿ [4]: a.MultiPayloadSpareBitsNonGenericEnumWithDefaultMirror.HyperCard\n" +
      "    ▿ HyperCard: a.CDROM #1\n" +
      "      - capacity: 600\n"

    expectEqual(expected, output)
  }
}

enum MultiPayloadTagBitsSmallNonGenericEnumWithDefaultMirror {
  case MacWrite
  case MacPaint
  case FileMaker
  case ClarisWorks(floppy: Bool)
  case HyperCard(cdrom: Bool)
}

Reflection.test("Enum/MultiPayloadTagBitsSmallNonGeneric/DefaultMirror") {
  do {
    let value: [MultiPayloadTagBitsSmallNonGenericEnumWithDefaultMirror] =
        [.MacWrite,
         .MacPaint,
         .FileMaker,
         .ClarisWorks(floppy: true),
         .HyperCard(cdrom: false)]

    var output = ""
    dump(value, &output)

    let expected =
      "▿ 5 elements\n" +
      "  - [0]: a.MultiPayloadTagBitsSmallNonGenericEnumWithDefaultMirror.MacWrite\n" +
      "  - [1]: a.MultiPayloadTagBitsSmallNonGenericEnumWithDefaultMirror.MacPaint\n" +
      "  - [2]: a.MultiPayloadTagBitsSmallNonGenericEnumWithDefaultMirror.FileMaker\n" +
      "  ▿ [3]: a.MultiPayloadTagBitsSmallNonGenericEnumWithDefaultMirror.ClarisWorks\n" +
      "    - ClarisWorks: true\n" +
      "  ▿ [4]: a.MultiPayloadTagBitsSmallNonGenericEnumWithDefaultMirror.HyperCard\n" +
      "    - HyperCard: false\n"

    expectEqual(expected, output)
  }
}

enum MultiPayloadGenericEnumWithDefaultMirror<T, U> {
  case IIe
  case IIgs
  case Centris(ram: T)
  case Quadra(hdd: U)
  case PowerBook170
  case PowerBookDuo220
}

Reflection.test("Enum/MultiPayloadGeneric/DefaultMirror") {
  do {
    let value: [MultiPayloadGenericEnumWithDefaultMirror<Int, String>] =
        [.IIe,
         .IIgs,
         .Centris(ram: 4096),
         .Quadra(hdd: "160MB"),
         .PowerBook170,
         .PowerBookDuo220]

    var output = ""
    dump(value, &output)

    let expected =
      "▿ 6 elements\n" +
      "  - [0]: a.MultiPayloadGenericEnumWithDefaultMirror<Swift.Int, Swift.String>.IIe\n" +
      "  - [1]: a.MultiPayloadGenericEnumWithDefaultMirror<Swift.Int, Swift.String>.IIgs\n" +
      "  ▿ [2]: a.MultiPayloadGenericEnumWithDefaultMirror<Swift.Int, Swift.String>.Centris\n" +
      "    - Centris: 4096\n" +
      "  ▿ [3]: a.MultiPayloadGenericEnumWithDefaultMirror<Swift.Int, Swift.String>.Quadra\n" +
      "    - Quadra: 160MB\n" +
      "  - [4]: a.MultiPayloadGenericEnumWithDefaultMirror<Swift.Int, Swift.String>.PowerBook170\n" +
      "  - [5]: a.MultiPayloadGenericEnumWithDefaultMirror<Swift.Int, Swift.String>.PowerBookDuo220\n"

    expectEqual(expected, output)
  }
  expectEqual(0, LifetimeTracked.instances)
  do {
    let value = MultiPayloadGenericEnumWithDefaultMirror<LifetimeTracked,
                                                         LifetimeTracked>
        .Quadra(hdd: LifetimeTracked(0))
    expectEqual(1, LifetimeTracked.instances)
    var output = ""
    dump(value, &output)
  }
  expectEqual(0, LifetimeTracked.instances)
}

enum Foo<T> {
  indirect case Foo(Int)
  case Bar(T)
}

enum List<T> {
  case Nil
  indirect case Cons(first: T, rest: List<T>)
}

Reflection.test("Enum/IndirectGeneric/DefaultMirror") {
  let x = Foo<String>.Foo(22)
  let y = Foo<String>.Bar("twenty-two")

  expectEqual("\(x)", "Foo(22)")
  expectEqual("\(y)", "Bar(\"twenty-two\")")

  let list = List.Cons(first: 0, rest: .Cons(first: 1, rest: .Nil))
  expectEqual("\(list)",
              "Cons(0, a.List<Swift.Int>.Cons(1, a.List<Swift.Int>.Nil))")
}

/// A type that provides its own mirror.
struct BrilliantMirror : _MirrorType {
  let _value: Brilliant

  init (_ _value: Brilliant) {
    self._value = _value
  }

  var value: Any {
    return _value
  }

  var valueType: Any.Type {
    return value.dynamicType
  }

  var objectIdentifier: ObjectIdentifier? {
    return ObjectIdentifier(_value)
  }

  var count: Int {
    return 3
  }

  subscript(i: Int) -> (String, _MirrorType) {
    switch i {
    case 0:
      return ("first", _reflect(_value.first))
    case 1:
      return ("second", _reflect(_value.second))
    case 2:
      return ("self", self)
    case _:
      _preconditionFailure("child index out of bounds")
    }
  }

  var summary: String {
    return "Brilliant(\(_value.first), \(_value.second))"
  }

  var quickLookObject: PlaygroundQuickLook? {
    return nil
  }

  var disposition: _MirrorDisposition {
    return .Container
  }
}

class Brilliant : _Reflectable {
  let first: Int
  let second: String

  init(_ fst: Int, _ snd: String) {
    self.first = fst
    self.second = snd
  }

  func _getMirror() -> _MirrorType {
    return BrilliantMirror(self)
  }
}

/// Subclasses inherit their parents' custom mirrors.
class Irradiant : Brilliant {
  init() {
    super.init(400, "")
  }
}

Reflection.test("CustomMirror") {
  do {
    var output = ""
    dump(Brilliant(123, "four five six"), &output)

    let expected =
      "▿ Brilliant(123, four five six) #0\n" +
      "  - first: 123\n" +
      "  - second: four five six\n" +
      "  ▿ self: Brilliant(123, four five six) #0\n"

    expectEqual(expected, output)
  }

  do {
    var output = ""
    dump(Brilliant(123, "four five six"), &output, maxDepth: 0)
    expectEqual("▹ Brilliant(123, four five six) #0\n", output)
  }

  do {
    var output = ""
    dump(Brilliant(123, "four five six"), &output, maxItems: 3)

    let expected =
      "▿ Brilliant(123, four five six) #0\n" +
      "  - first: 123\n" +
      "  - second: four five six\n" +
      "    (1 more child)\n"

    expectEqual(expected, output)
  }

  do {
    var output = ""
    dump(Brilliant(123, "four five six"), &output, maxItems: 2)

    let expected =
      "▿ Brilliant(123, four five six) #0\n" +
      "  - first: 123\n" +
      "    (2 more children)\n"

    expectEqual(expected, output)
  }

  do {
    var output = ""
    dump(Brilliant(123, "four five six"), &output, maxItems: 1)

    let expected =
      "▿ Brilliant(123, four five six) #0\n" +
      "    (3 children)\n"

    expectEqual(expected, output)
  }

  expectEqual(.Container, _reflect(Brilliant(123, "four five six")).disposition)

  do {
    // Check that object identifiers are unique to class instances.
    let a = Brilliant(1, "")
    let b = Brilliant(2, "")
    let c = Brilliant(3, "")

    // Equatable
    checkEquatable(true, ObjectIdentifier(a), ObjectIdentifier(a))
    checkEquatable(false, ObjectIdentifier(a), ObjectIdentifier(b))

    // Comparable
    func isComparable<X : Comparable>(x: X) {}
    isComparable(ObjectIdentifier(a))
    // Check the ObjectIdentifier created is stable
    expectTrue(
      (ObjectIdentifier(a) < ObjectIdentifier(b))
      != (ObjectIdentifier(a) > ObjectIdentifier(b)))
    expectFalse(
      ObjectIdentifier(a) >= ObjectIdentifier(b)
      && ObjectIdentifier(a) <= ObjectIdentifier(b))

    // Check ordering is transitive
    expectEqual(
      [ ObjectIdentifier(a), ObjectIdentifier(b), ObjectIdentifier(c) ].sort(),
      [ ObjectIdentifier(c), ObjectIdentifier(b), ObjectIdentifier(a) ].sort())
  }
}

Reflection.test("CustomMirrorIsInherited") {
  do {
    var output = ""
    dump(Irradiant(), &output)

    let expected =
      "▿ Brilliant(400, ) #0\n" +
      "  - first: 400\n" +
      "  - second: \n" +
      "  ▿ self: Brilliant(400, ) #0\n"

    expectEqual(expected, output)
  }
}

class SwiftFooMoreDerivedObjCClass : FooMoreDerivedObjCClass {
  let first: Int = 123
  let second: String = "abc"
}

Reflection.test("Class/ObjectiveCBase/Default") {
  do {
    let value = SwiftFooMoreDerivedObjCClass()
    var output = ""
    dump(value, &output)

    let expected =
      "▿ a.SwiftFooMoreDerivedObjCClass #0\n" +
      "  ▿ super: This is FooObjCClass\n" +
      "    ▿ FooDerivedObjCClass: This is FooObjCClass\n" +
      "      ▿ FooObjCClass: This is FooObjCClass\n" +
      "        - NSObject: This is FooObjCClass\n" +
      "  - first: 123\n" +
      "  - second: abc\n"

    expectEqual(expected, output)
  }
}

protocol SomeNativeProto {}
extension Int: SomeNativeProto {}

@objc protocol SomeObjCProto {}
extension SomeClass: SomeObjCProto {}

Reflection.test("MetatypeMirror") {
  do {
    var output = ""
    let concreteMetatype = Int.self
    dump(concreteMetatype, &output)

    let expectedInt = "- Swift.Int #0\n"
    expectEqual(expectedInt, output)

    let anyMetatype: Any.Type = Int.self
    output = ""
    dump(anyMetatype, &output)
    expectEqual(expectedInt, output)

    let nativeProtocolMetatype: SomeNativeProto.Type = Int.self
    output = ""
    dump(nativeProtocolMetatype, &output)
    expectEqual(expectedInt, output)

    expectEqual(_reflect(concreteMetatype).objectIdentifier!,
                _reflect(anyMetatype).objectIdentifier!)
    expectEqual(_reflect(concreteMetatype).objectIdentifier!,
                _reflect(nativeProtocolMetatype).objectIdentifier!)


    let concreteClassMetatype = SomeClass.self
    let expectedSomeClass = "- a.SomeClass #0\n"
    output = ""
    dump(concreteClassMetatype, &output)
    expectEqual(expectedSomeClass, output)

    let objcProtocolMetatype: SomeObjCProto.Type = SomeClass.self
    output = ""
    dump(objcProtocolMetatype, &output)
    expectEqual(expectedSomeClass, output)

    expectEqual(_reflect(concreteClassMetatype).objectIdentifier!,
                _reflect(objcProtocolMetatype).objectIdentifier!)

    let nativeProtocolConcreteMetatype = SomeNativeProto.self
    let expectedNativeProtocolConcrete = "- a.SomeNativeProto #0\n"
    output = ""
    dump(nativeProtocolConcreteMetatype, &output)
    expectEqual(expectedNativeProtocolConcrete, output)

    let objcProtocolConcreteMetatype = SomeObjCProto.self
    let expectedObjCProtocolConcrete = "- a.SomeObjCProto #0\n"
    output = ""
    dump(objcProtocolConcreteMetatype, &output)
    expectEqual(expectedObjCProtocolConcrete, output)

    typealias Composition = protocol<SomeNativeProto, SomeObjCProto>
    let compositionConcreteMetatype = Composition.self
    let expectedComposition = "- protocol<a.SomeNativeProto, a.SomeObjCProto> #0\n"
    output = ""
    dump(compositionConcreteMetatype, &output)
    expectEqual(expectedComposition, output)

    let objcDefinedProtoType = NSObjectProtocol.self
    expectEqual(String(objcDefinedProtoType), "NSObject")
  }
}

Reflection.test("TupleMirror") {
  do {
    var output = ""
    let tuple =
      (Brilliant(384, "seven six eight"), StructWithDefaultMirror("nine"))
    dump(tuple, &output)

    let expected =
      "▿ (2 elements)\n" +
      "  ▿ .0: Brilliant(384, seven six eight) #0\n" +
      "    - first: 384\n" +
      "    - second: seven six eight\n" +
      "    ▿ self: Brilliant(384, seven six eight) #0\n" +
      "  ▿ .1: a.StructWithDefaultMirror\n" +
      "    - s: nine\n"

    expectEqual(expected, output)

    expectEmpty(_reflect(tuple).quickLookObject)
    expectEqual(.Tuple, _reflect(tuple).disposition)
  }

  do {
    // A tuple of stdlib types with mirrors.
    var output = ""
    let tuple = (1, 2.5, false, "three")
    dump(tuple, &output)

    let expected =
      "▿ (4 elements)\n" +
      "  - .0: 1\n" +
      "  - .1: 2.5\n" +
      "  - .2: false\n" +
      "  - .3: three\n"

    expectEqual(expected, output)
  }

  do {
    // A nested tuple.
    var output = ""
    let tuple = (1, ("Hello", "World"))
    dump(tuple, &output)

    let expected =
      "▿ (2 elements)\n" +
      "  - .0: 1\n" +
      "  ▿ .1: (2 elements)\n" +
      "    - .0: Hello\n" +
      "    - .1: World\n"

    expectEqual(expected, output)
  }
}

class DullClass {}
Reflection.test("ObjectIdentity") {
  // Check that the primitive _MirrorType implementation produces appropriately
  // unique identifiers for class instances.

  let x = DullClass()
  let y = DullClass()
  let o = NSObject()
  let p = NSObject()

  checkEquatable(
    true, _reflect(x).objectIdentifier!, _reflect(x).objectIdentifier!)
  checkEquatable(
    false, _reflect(x).objectIdentifier!, _reflect(y).objectIdentifier!)
  checkEquatable(
    true, _reflect(o).objectIdentifier!, _reflect(o).objectIdentifier!)
  checkEquatable(
    false, _reflect(o).objectIdentifier!, _reflect(p).objectIdentifier!)
  checkEquatable(
    false, _reflect(o).objectIdentifier!, _reflect(y).objectIdentifier!)

  expectEmpty(_reflect(x).quickLookObject)

  expectEqual(.Class, _reflect(x).disposition)
}

Reflection.test("String/Mirror") {
  do {
    var output = ""
    dump("", &output)

    let expected =
      "- \n"

    expectEqual(expected, output)
  }

  do {
    // U+0061 LATIN SMALL LETTER A
    // U+304B HIRAGANA LETTER KA
    // U+3099 COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
    // U+1F425 FRONT-FACING BABY CHICK
    var output = ""
    dump("\u{61}\u{304b}\u{3099}\u{1f425}", &output)

    let expected =
      "- \u{61}\u{304b}\u{3099}\u{1f425}\n"

    expectEqual(expected, output)
  }
}

Reflection.test("String.UTF8View/Mirror") {
  // U+0061 LATIN SMALL LETTER A
  // U+304B HIRAGANA LETTER KA
  // U+3099 COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
  var output = ""
  dump("\u{61}\u{304b}\u{3099}".utf8, &output)

  let expected =
    "▿ \u{61}\u{304b}\u{3099}\n" +
    "  - [0]: 97\n" +
    "  - [1]: 227\n" +
    "  - [2]: 129\n" +
    "  - [3]: 139\n" +
    "  - [4]: 227\n" +
    "  - [5]: 130\n" +
    "  - [6]: 153\n"

  expectEqual(expected, output)
}

Reflection.test("String.UTF16View/Mirror") {
  // U+0061 LATIN SMALL LETTER A
  // U+304B HIRAGANA LETTER KA
  // U+3099 COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
  // U+1F425 FRONT-FACING BABY CHICK
  var output = ""
  dump("\u{61}\u{304b}\u{3099}\u{1f425}".utf16, &output)

  let expected =
    "▿ \u{61}\u{304b}\u{3099}\u{1f425}\n" +
    "  - [0]: 97\n" +
    "  - [1]: 12363\n" +
    "  - [2]: 12441\n" +
    "  - [3]: 55357\n" +
    "  - [4]: 56357\n"

  expectEqual(expected, output)
}

Reflection.test("String.UnicodeScalarView/Mirror") {
  // U+0061 LATIN SMALL LETTER A
  // U+304B HIRAGANA LETTER KA
  // U+3099 COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
  // U+1F425 FRONT-FACING BABY CHICK
  var output = ""
  dump("\u{61}\u{304b}\u{3099}\u{1f425}".unicodeScalars, &output)

  let expected =
    "▿ \u{61}\u{304b}\u{3099}\u{1f425}\n" +
    "  - [0]: \u{61}\n" +
    "  - [1]: \u{304b}\n" +
    "  - [2]: \u{3099}\n" +
    "  - [3]: \u{1f425}\n"

  expectEqual(expected, output)
}

Reflection.test("Character/Mirror") {
  do {
    // U+0061 LATIN SMALL LETTER A
    let input: Character = "\u{61}"
    var output = ""
    dump(input, &output)

    let expected =
      "- \u{61}\n"

    expectEqual(expected, output)
  }

  do {
    // U+304B HIRAGANA LETTER KA
    // U+3099 COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
    let input: Character = "\u{304b}\u{3099}"
    var output = ""
    dump(input, &output)

    let expected =
      "- \u{304b}\u{3099}\n"

    expectEqual(expected, output)
  }

  do {
    // U+1F425 FRONT-FACING BABY CHICK
    let input: Character = "\u{1f425}"
    var output = ""
    dump(input, &output)

    let expected =
      "- \u{1f425}\n"

    expectEqual(expected, output)
  }
}

Reflection.test("UnicodeScalar") {
  do {
    // U+0061 LATIN SMALL LETTER A
    let input: UnicodeScalar = "\u{61}"
    var output = ""
    dump(input, &output)

    let expected =
      "- \u{61}\n"

    expectEqual(expected, output)
  }

  do {
    // U+304B HIRAGANA LETTER KA
    let input: UnicodeScalar = "\u{304b}"
    var output = ""
    dump(input, &output)

    let expected =
      "- \u{304b}\n"

    expectEqual(expected, output)
  }

  do {
    // U+3099 COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
    let input: UnicodeScalar = "\u{3099}"
    var output = ""
    dump(input, &output)

    let expected =
      "- \u{3099}\n"

    expectEqual(expected, output)
  }

  do {
    // U+1F425 FRONT-FACING BABY CHICK
    let input: UnicodeScalar = "\u{1f425}"
    var output = ""
    dump(input, &output)

    let expected =
      "- \u{1f425}\n"

    expectEqual(expected, output)
  }
}

Reflection.test("Bool") {
  do {
    var output = ""
    dump(false, &output)

    let expected =
      "- false\n"

    expectEqual(expected, output)
  }

  do {
    var output = ""
    dump(true, &output)

    let expected =
      "- true\n"

    expectEqual(expected, output)
  }
}

// FIXME: these tests should cover Float80.
// FIXME: these tests should be automatically generated from the list of
// available floating point types.
Reflection.test("Float") {
  do {
    var output = ""
    dump(Float.NaN, &output)

    let expected =
      "- nan\n"

    expectEqual(expected, output)
  }

  do {
    var output = ""
    dump(Float.infinity, &output)

    let expected =
      "- inf\n"

    expectEqual(expected, output)
  }

  do {
    var input: Float = 42.125
    var output = ""
    dump(input, &output)

    let expected =
      "- 42.125\n"

    expectEqual(expected, output)
  }
}

Reflection.test("Double") {
  do {
    var output = ""
    dump(Double.NaN, &output)

    let expected =
      "- nan\n"

    expectEqual(expected, output)
  }

  do {
    var output = ""
    dump(Double.infinity, &output)

    let expected =
      "- inf\n"

    expectEqual(expected, output)
  }

  do {
    var input: Double = 42.125
    var output = ""
    dump(input, &output)

    let expected =
      "- 42.125\n"

    expectEqual(expected, output)
  }
}

Reflection.test("CGPoint") {
  var output = ""
  dump(CGPoint(x: 1.25, y: 2.75), &output)

  let expected =
    "▿ (1.25, 2.75)\n" +
    "  - x: 1.25\n" +
    "  - y: 2.75\n"

  expectEqual(expected, output)
}

Reflection.test("CGSize") {
  var output = ""
  dump(CGSize(width: 1.25, height: 2.75), &output)

  let expected =
    "▿ (1.25, 2.75)\n" +
    "  - width: 1.25\n" +
    "  - height: 2.75\n"

  expectEqual(expected, output)
}

Reflection.test("CGRect") {
  var output = ""
  dump(
    CGRect(
      origin: CGPoint(x: 1.25, y: 2.25),
      size: CGSize(width: 10.25, height: 11.75)),
    &output)

  let expected =
    "▿ (1.25, 2.25, 10.25, 11.75)\n" +
    "  ▿ origin: (1.25, 2.25)\n" +
    "    - x: 1.25\n" +
    "    - y: 2.25\n" +
    "  ▿ size: (10.25, 11.75)\n" +
    "    - width: 10.25\n" +
    "    - height: 11.75\n"

  expectEqual(expected, output)
}

Reflection.test("Unmanaged/nil") {
  var output = ""
  var optionalURL: Unmanaged<CFURL>? = nil
  dump(optionalURL, &output)

  let expected = "- nil\n"

  expectEqual(expected, output)
}

Reflection.test("Unmanaged/not-nil") {
  var output = ""
  var optionalURL: Unmanaged<CFURL>? =
    Unmanaged.passRetained(CFURLCreateWithString(nil, "http://llvm.org/", nil))
  dump(optionalURL, &output)

  let expected =
    "▿ Swift.Unmanaged<__ObjC.CFURL>\n" +
    "  ▿ Some: Swift.Unmanaged<__ObjC.CFURL>\n" +
    "    ▿ _value: http://llvm.org/ #0\n" +
    "      - NSObject: http://llvm.org/\n"

  expectEqual(expected, output)

  optionalURL!.release()
}

Reflection.test("TupleMirror/NoLeak") {
  do {
    nsObjectCanaryCount = 0
    autoreleasepool {
      var tuple = (1, NSObjectCanary())
      expectEqual(1, nsObjectCanaryCount)
      var output = ""
      dump(tuple, &output)
    }
    expectEqual(0, nsObjectCanaryCount)
  }
  do {
    nsObjectCanaryCount = 0
    autoreleasepool {
      var tuple = (1, NSObjectCanaryStruct())
      expectEqual(1, nsObjectCanaryCount)
      var output = ""
      dump(tuple, &output)
    }
    expectEqual(0, nsObjectCanaryCount)
  }
  do {
    swiftObjectCanaryCount = 0
    autoreleasepool {
      var tuple = (1, SwiftObjectCanary())
      expectEqual(1, swiftObjectCanaryCount)
      var output = ""
      dump(tuple, &output)
    }
    expectEqual(0, swiftObjectCanaryCount)
  }
  do {
    swiftObjectCanaryCount = 0
    autoreleasepool {
      var tuple = (1, SwiftObjectCanaryStruct())
      expectEqual(1, swiftObjectCanaryCount)
      var output = ""
      dump(tuple, &output)
    }
    expectEqual(0, swiftObjectCanaryCount)
  }
}

// A struct type and class type whose NominalTypeDescriptor.FieldNames 
// data is exactly eight bytes long. FieldNames data of exactly 
// 4 or 8 or 16 bytes was once miscompiled on arm64.
struct EightByteFieldNamesStruct {
  let abcdef = 42
}
class EightByteFieldNamesClass {
  let abcdef = 42
}

Reflection.test("FieldNamesBug") {
  do {
    let expected =
      "▿ a.EightByteFieldNamesStruct\n" +
      "  - abcdef: 42\n"
    var output = ""
    dump(EightByteFieldNamesStruct(), &output)
    expectEqual(expected, output)
  }

  do {
    let expected =
      "▿ a.EightByteFieldNamesClass #0\n" +
      "  - abcdef: 42\n"
    var output = ""
    dump(EightByteFieldNamesClass(), &output)
    expectEqual(expected, output)
  }
}

Reflection.test("MirrorMirror") {
  var object = 1
  var mirror = Mirror(reflecting: object)
  var mirrorMirror = Mirror(reflecting: mirror)

  expectEqual(0, mirrorMirror.children.count)
}

Reflection.test("COpaquePointer/null") {
  // Don't crash on null pointers. rdar://problem/19708338
  var sequence = COpaquePointer()
  var mirror = _reflect(sequence)
  var child = mirror[0]
  expectEqual("(Opaque Value)", child.1.summary)
}

Reflection.test("StaticString/Mirror") {
  do {
    var output = ""
    dump("" as StaticString, &output)

    let expected =
      "- \n"

    expectEqual(expected, output)
  }

  do {
    // U+0061 LATIN SMALL LETTER A
    // U+304B HIRAGANA LETTER KA
    // U+3099 COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK
    // U+1F425 FRONT-FACING BABY CHICK
    var output = ""
    dump("\u{61}\u{304b}\u{3099}\u{1f425}" as StaticString, &output)

    let expected =
      "- \u{61}\u{304b}\u{3099}\u{1f425}\n"

    expectEqual(expected, output)
  }
}

class TestArtificialSubclass: NSObject {
  dynamic var foo = "foo"
}

var KVOHandle = 0

Reflection.test("Name of metatype of artificial subclass") {
  let obj = TestArtificialSubclass()
  // Trigger the creation of a KVO subclass for TestArtificialSubclass.
  obj.addObserver(obj, forKeyPath: "foo", options: [.New], context: &KVOHandle)
  obj.removeObserver(obj, forKeyPath: "foo")

  expectEqual("\(obj.dynamicType)", "TestArtificialSubclass")
}

@objc class StringConvertibleInDebugAndOtherwise : NSObject {
  override var description: String { return "description" }
  override var debugDescription: String { return "debugDescription" }
}

Reflection.test("NSObject is properly CustomDebugStringConvertible") {
  let object = StringConvertibleInDebugAndOtherwise()
  expectEqual(String(reflecting: object), object.debugDescription)
}

Reflection.test("NSRange QuickLook") {
  let rng = NSRange(location:Int.min, length:5)
  let ql = PlaygroundQuickLook(reflecting: rng)
  switch ql {
  case .Range(let loc, let len):
    expectEqual(loc, Int64(Int.min))
    expectEqual(len, 5)
  default:
    expectUnreachable("PlaygroundQuickLook for NSRange did not match Range")
  }
}

var BitTwiddlingTestSuite = TestSuite("BitTwiddling")

func computeCountLeadingZeroes(x: Int64) -> Int64 {
  var x = x
  var r: Int64 = 64
  while x != 0 {
    x >>= 1
    r--
  }
  return r
}

BitTwiddlingTestSuite.test("_countLeadingZeros") {
  for i in Int64(0)..<1000 {
    expectEqual(computeCountLeadingZeroes(i), _countLeadingZeros(i))
  }
  expectEqual(0, _countLeadingZeros(Int64.min))
}

BitTwiddlingTestSuite.test("_isPowerOf2/Int") {
  func asInt(a: Int) -> Int { return a }

  expectFalse(_isPowerOf2(asInt(-1025)))
  expectFalse(_isPowerOf2(asInt(-1024)))
  expectFalse(_isPowerOf2(asInt(-1023)))
  expectFalse(_isPowerOf2(asInt(-4)))
  expectFalse(_isPowerOf2(asInt(-3)))
  expectFalse(_isPowerOf2(asInt(-2)))
  expectFalse(_isPowerOf2(asInt(-1)))
  expectFalse(_isPowerOf2(asInt(0)))
  expectTrue(_isPowerOf2(asInt(1)))
  expectTrue(_isPowerOf2(asInt(2)))
  expectFalse(_isPowerOf2(asInt(3)))
  expectTrue(_isPowerOf2(asInt(1024)))
#if arch(i386) || arch(arm)
  // Not applicable to 32-bit architectures.
#elseif arch(x86_64) || arch(arm64)
  expectTrue(_isPowerOf2(asInt(0x8000_0000)))
#else
  fatalError("implement")
#endif
  expectFalse(_isPowerOf2(Int.min))
  expectFalse(_isPowerOf2(Int.max))
}

BitTwiddlingTestSuite.test("_isPowerOf2/UInt") {
  func asUInt(a: UInt) -> UInt { return a }

  expectFalse(_isPowerOf2(asUInt(0)))
  expectTrue(_isPowerOf2(asUInt(1)))
  expectTrue(_isPowerOf2(asUInt(2)))
  expectFalse(_isPowerOf2(asUInt(3)))
  expectTrue(_isPowerOf2(asUInt(1024)))
  expectTrue(_isPowerOf2(asUInt(0x8000_0000)))
  expectFalse(_isPowerOf2(UInt.max))
}

BitTwiddlingTestSuite.test("_floorLog2") {
  expectEqual(_floorLog2(1), 0)
  expectEqual(_floorLog2(8), 3)
  expectEqual(_floorLog2(15), 3)
  expectEqual(_floorLog2(Int64.max), 62) // 63 minus 1 for sign bit.
}

class SomeSubclass : SomeClass {}

var ObjCConformsToProtocolTestSuite = TestSuite("ObjCConformsToProtocol")

ObjCConformsToProtocolTestSuite.test("cast/instance") {
  expectTrue(SomeClass() is SomeObjCProto)
  expectTrue(SomeSubclass() is SomeObjCProto)
}
ObjCConformsToProtocolTestSuite.test("cast/metatype") {
  expectTrue(SomeClass.self is SomeObjCProto.Type)
  expectTrue(SomeSubclass.self is SomeObjCProto.Type)
}

var AvailabilityVersionsTestSuite = TestSuite("AvailabilityVersions")

AvailabilityVersionsTestSuite.test("lexicographic_compare") {
  func version(
    major: Int,
    _ minor: Int,
    _ patch: Int
  ) -> _SwiftNSOperatingSystemVersion {
    return _SwiftNSOperatingSystemVersion(
      majorVersion: major,
      minorVersion: minor,
      patchVersion: patch
    )
  }

  checkComparable(.EQ, version(0, 0, 0), version(0, 0, 0))

  checkComparable(.LT, version(0, 0, 0), version(0, 0, 1))
  checkComparable(.LT, version(0, 0, 0), version(0, 1, 0))
  checkComparable(.LT, version(0, 0, 0), version(1, 0, 0))

  checkComparable(.LT,version(10, 9, 0), version(10, 10, 0))
  checkComparable(.LT,version(10, 9, 11), version(10, 10, 0))
  checkComparable(.LT,version(10, 10, 3), version(10, 11, 0))

  checkComparable(.LT, version(8, 3, 0), version(9, 0, 0))

  checkComparable(.LT, version(0, 11, 0), version(10, 10, 4))
  checkComparable(.LT, version(0, 10, 0), version(10, 10, 4))
  checkComparable(.LT, version(3, 2, 1), version(4, 3, 2))
  checkComparable(.LT, version(1, 2, 3), version(2, 3, 1))

  checkComparable(.EQ, version(10, 11, 12), version(10, 11, 12))

  checkEquatable(true, version(1, 2, 3), version(1, 2, 3))
  checkEquatable(false, version(1, 2, 3), version(1, 2, 42))
  checkEquatable(false, version(1, 2, 3), version(1, 42, 3))
  checkEquatable(false, version(1, 2, 3), version(42, 2, 3))
}

AvailabilityVersionsTestSuite.test("_stdlib_isOSVersionAtLeast") {
  func isAtLeastOS(major: Int, _ minor: Int, _ patch: Int) -> Bool {
    return _getBool(_stdlib_isOSVersionAtLeast(major._builtinWordValue,
                                               minor._builtinWordValue,
                                               patch._builtinWordValue))
  }

// _stdlib_isOSVersionAtLeast is broken for
// watchOS. rdar://problem/20234735
#if os(OSX) || os(iOS) || os(tvOS) || os(watchOS)
  // This test assumes that no version component on an OS we test upon
  // will ever be greater than 1066 and that every major version will always
  // be greater than 1.
  expectFalse(isAtLeastOS(1066, 0, 0))
  expectTrue(isAtLeastOS(0, 1066, 0))
  expectTrue(isAtLeastOS(0, 0, 1066))
#endif
}

runAllTests()

