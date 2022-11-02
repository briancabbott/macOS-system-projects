import Swift
import Darwin
import StdlibUnittest
import Foundation

func acceptsAnySet<T : Hashable>(s: Set<T>) {}

func acceptsAnyDictionary<KeyTy : Hashable, ValueTy>(
  d: Dictionary<KeyTy, ValueTy>) {
}

func isNativeDictionary<KeyTy : Hashable, ValueTy>(
  d: Dictionary<KeyTy, ValueTy>) -> Bool {
  switch d._variantStorage {
  case .Native:
    return true
  case .Cocoa:
    return false
  }
}

func isCocoaDictionary<KeyTy : Hashable, ValueTy>(
  d: Dictionary<KeyTy, ValueTy>) -> Bool {
  return !isNativeDictionary(d)
}

func isNativeNSDictionary(d: NSDictionary) -> Bool {
  let className: NSString = NSStringFromClass(d.dynamicType)
  return className.rangeOfString("_NativeDictionaryStorageOwner").length > 0
}

func isCocoaNSDictionary(d: NSDictionary) -> Bool {
  let className: NSString = NSStringFromClass(d.dynamicType)
  return className.rangeOfString("NSDictionary").length > 0 ||
    className.rangeOfString("NSCFDictionary").length > 0
}

func isNativeNSArray(d: NSArray) -> Bool {
  let className: NSString = NSStringFromClass(d.dynamicType)
  return className.rangeOfString("_SwiftDeferredNSArray").length > 0
}

// Compare two arrays as sets.
func equalsUnordered<T : Comparable>(
  lhs: Array<(T, T)>, _ rhs: Array<(T, T)>
) -> Bool {
  func comparePair(lhs: (T, T), _ rhs: (T, T)) -> Bool {
    return [ lhs.0, lhs.1 ].lexicographicalCompare([ rhs.0, rhs.1 ])
  }
  return lhs.sort(comparePair).elementsEqual(rhs.sort(comparePair)) {
    (lhs: (T, T), rhs: (T, T)) -> Bool in
    lhs.0 == rhs.0 && lhs.1 == rhs.1
  }
}

func equalsUnordered<T : Comparable>(lhs: [T], _ rhs: [T]) -> Bool {
  return lhs.sort().elementsEqual(rhs.sort())
}

var _keyCount = _stdlib_AtomicInt(0)
var _keySerial = _stdlib_AtomicInt(0)

// A wrapper class that can help us track allocations and find issues with
// object lifetime.
class TestKeyTy : Equatable, Hashable, CustomStringConvertible {
  class var objectCount: Int {
    get {
      return _keyCount.load()
    }
    set {
      _keyCount.store(newValue)
    }
  }

  init(_ value: Int) {
    _keyCount.fetchAndAdd(1)
    serial = _keySerial.addAndFetch(1)
    self.value = value
    self._hashValue = value
  }

  convenience init(value: Int, hashValue: Int) {
    self.init(value)
    self._hashValue = hashValue
  }

  deinit {
    assert(serial > 0, "double destruction")
    _keyCount.fetchAndAdd(-1)
    serial = -serial
  }

  var description: String {
    assert(serial > 0, "dead TestKeyTy")
    return value.description
  }

  var hashValue: Int {
    return _hashValue
  }

  var value: Int
  var _hashValue: Int
  var serial: Int
}

func == (lhs: TestKeyTy, rhs: TestKeyTy) -> Bool {
  return lhs.value == rhs.value
}

var _valueCount = _stdlib_AtomicInt(0)
var _valueSerial = _stdlib_AtomicInt(0)

class TestValueTy : CustomStringConvertible {
  class var objectCount: Int {
    get {
      return _valueCount.load()
    }
    set {
      _valueCount.store(newValue)
    }
  }

  init(_ value: Int) {
    _valueCount.fetchAndAdd(1)
    serial = _valueSerial.addAndFetch(1)
    self.value = value
  }

  deinit {
    assert(serial > 0, "double destruction")
    _valueCount.fetchAndAdd(-1)
    serial = -serial
  }

  var description: String {
    assert(serial > 0, "dead TestValueTy")
    return value.description
  }

  var value: Int
  var serial: Int
}

var _equatableValueCount = _stdlib_AtomicInt(0)
var _equatableValueSerial = _stdlib_AtomicInt(0)

class TestEquatableValueTy : Equatable, CustomStringConvertible {
  class var objectCount: Int {
    get {
      return _equatableValueCount.load()
    }
    set {
      _equatableValueCount.store(newValue)
    }
  }

  init(_ value: Int) {
    _equatableValueCount.fetchAndAdd(1)
    serial = _equatableValueSerial.addAndFetch(1)
    self.value = value
  }

  deinit {
    assert(serial > 0, "double destruction")
    _equatableValueCount.fetchAndAdd(-1)
    serial = -serial
  }

  var description: String {
    assert(serial > 0, "dead TestEquatableValueTy")
    return value.description
  }

  var value: Int
  var serial: Int
}

func == (lhs: TestEquatableValueTy, rhs: TestEquatableValueTy) -> Bool {
  return lhs.value == rhs.value
}

var _objcKeyCount = _stdlib_AtomicInt(0)
var _objcKeySerial = _stdlib_AtomicInt(0)

class TestObjCKeyTy : NSObject, NSCopying {
  class var objectCount: Int {
    get {
      return _objcKeyCount.load()
    }
    set {
      _objcKeyCount.store(newValue)
    }
  }

  init(_ value: Int) {
    _objcKeyCount.fetchAndAdd(1)
    serial = _objcKeySerial.addAndFetch(1)
    self.value = value
    self._hashValue = value
    super.init()
  }

  convenience init(value: Int, hashValue: Int) {
    self.init(value)
    self._hashValue = hashValue
  }

  deinit {
    assert(serial > 0, "double destruction")
    _objcKeyCount.fetchAndAdd(-1)
    serial = -serial
  }

  @objc
  func copyWithZone(zone: NSZone) -> AnyObject {
    return TestObjCKeyTy(value)
  }

  override var description: String {
    assert(serial > 0, "dead TestObjCKeyTy")
    return value.description
  }

  override func isEqual(object: AnyObject!) -> Bool {
    if let other = object {
      if let otherObjcKey = other as? TestObjCKeyTy {
        return self.value == otherObjcKey.value
      }
    }
    return false
  }

  override var hash : Int {
    return _hashValue
  }

  func _bridgeToObjectiveC() -> TestObjCKeyTy {
    return self
  }

  var value: Int
  var _hashValue: Int
  var serial: Int
}

var _objcValueCount = _stdlib_AtomicInt(0)
var _objcValueSerial = _stdlib_AtomicInt(0)

class TestObjCValueTy : NSObject {
  class var objectCount: Int {
    get {
      return _objcValueCount.load()
    }
    set {
      _objcValueCount.store(newValue)
    }
  }

  init(_ value: Int) {
    _objcValueCount.fetchAndAdd(1)
    serial = _objcValueSerial.addAndFetch(1)
    self.value = value
  }

  deinit {
    assert(serial > 0, "double destruction")
    _objcValueCount.fetchAndAdd(-1)
    serial = -serial
  }

  override var description: String {
    assert(serial > 0, "dead TestObjCValueTy")
    return value.description
  }

  var value: Int
  var serial: Int
}

var _objcEquatableValueCount = _stdlib_AtomicInt(0)
var _objcEquatableValueSerial = _stdlib_AtomicInt(0)

class TestObjCEquatableValueTy : NSObject {
  class var objectCount: Int {
    get {
      return _objcEquatableValueCount.load()
    }
    set {
      _objcEquatableValueCount.store(newValue)
    }
  }

  init(_ value: Int) {
    _objcEquatableValueCount.fetchAndAdd(1)
    serial = _objcEquatableValueSerial.addAndFetch(1)
    self.value = value
  }

  deinit {
    assert(serial > 0, "double destruction")
    _objcEquatableValueCount.fetchAndAdd(-1)
    serial = -serial
  }

  override func isEqual(object: AnyObject!) -> Bool {
    if let other = object {
      if let otherObjcKey = other as? TestObjCEquatableValueTy {
        return self.value == otherObjcKey.value
      }
    }
    return false
  }

  override var description: String {
    assert(serial > 0, "dead TestObjCValueTy")
    return value.description
  }

  var value: Int
  var serial: Int
}

func == (lhs: TestObjCEquatableValueTy, rhs: TestObjCEquatableValueTy) -> Bool {
  return lhs.value == rhs.value
}

var _bridgedKeySerial = _stdlib_AtomicInt(0)
var _bridgedKeyBridgeOperations = _stdlib_AtomicInt(0)

struct TestBridgedKeyTy
  : Equatable, Hashable, CustomStringConvertible, _ObjectiveCBridgeable {
  static var bridgeOperations: Int {
    get {
      return _bridgedKeyBridgeOperations.load()
    }
    set {
      _bridgedKeyBridgeOperations.store(newValue)
    }
  }

  init(_ value: Int) {
    serial = _bridgedKeySerial.addAndFetch(1)
    self.value = value
    self._hashValue = value
  }

  var description: String {
    assert(serial > 0, "dead TestBridgedKeyTy")
    return value.description
  }

  var hashValue: Int {
    return _hashValue
  }

  static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  static func _getObjectiveCType() -> Any.Type {
    return TestObjCKeyTy.self
  }

  func _bridgeToObjectiveC() -> TestObjCKeyTy {
    _bridgedKeyBridgeOperations.fetchAndAdd(1)
    return TestObjCKeyTy(value)
  }

  static func _forceBridgeFromObjectiveC(
    x: TestObjCKeyTy,
    inout result: TestBridgedKeyTy?
  ) {
    _bridgedKeyBridgeOperations.fetchAndAdd(1)
    result = TestBridgedKeyTy(x.value)
  }

  static func _conditionallyBridgeFromObjectiveC(
    x: TestObjCKeyTy,
    inout result: TestBridgedKeyTy?
  ) -> Bool {
    self._forceBridgeFromObjectiveC(x, result: &result)
    return true
  }

  var value: Int
  var _hashValue: Int
  var serial: Int
}

func == (lhs: TestBridgedKeyTy, rhs: TestBridgedKeyTy) -> Bool {
  return lhs.value == rhs.value
}

func == (lhs: TestBridgedKeyTy, rhs: TestKeyTy) -> Bool {
  return lhs.value == rhs.value
}

var _bridgedValueSerial = _stdlib_AtomicInt(0)
var _bridgedValueBridgeOperations = _stdlib_AtomicInt(0)

struct TestBridgedValueTy : CustomStringConvertible, _ObjectiveCBridgeable {
  static var bridgeOperations: Int {
    get {
      return _bridgedValueBridgeOperations.load()
    }
    set {
      _bridgedValueBridgeOperations.store(newValue)
    }
  }

  init(_ value: Int) {
    serial = _bridgedValueSerial.fetchAndAdd(1)
    self.value = value
  }

  var description: String {
    assert(serial > 0, "dead TestBridgedValueTy")
    return value.description
  }

  static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  static func _getObjectiveCType() -> Any.Type {
    return TestObjCValueTy.self
  }

  func _bridgeToObjectiveC() -> TestObjCValueTy {
    TestBridgedValueTy.bridgeOperations++
    return TestObjCValueTy(value)
  }

  static func _forceBridgeFromObjectiveC(
    x: TestObjCValueTy,
    inout result: TestBridgedValueTy?
  ) {
    TestBridgedValueTy.bridgeOperations++
    result = TestBridgedValueTy(x.value)
  }

  static func _conditionallyBridgeFromObjectiveC(
    x: TestObjCValueTy,
    inout result: TestBridgedValueTy?
  ) -> Bool {
    self._forceBridgeFromObjectiveC(x, result: &result)
    return true
  }

  var value: Int
  var serial: Int
}

var _bridgedEquatableValueSerial = _stdlib_AtomicInt(0)
var _bridgedEquatableValueBridgeOperations = _stdlib_AtomicInt(0)

struct TestBridgedEquatableValueTy
  : Equatable, CustomStringConvertible, _ObjectiveCBridgeable {

  static var bridgeOperations: Int {
    get {
      return _bridgedEquatableValueBridgeOperations.load()
    }
    set {
      _bridgedEquatableValueBridgeOperations.store(newValue)
    }
  }

  init(_ value: Int) {
    serial = _bridgedEquatableValueSerial.addAndFetch(1)
    self.value = value
  }

  var description: String {
    assert(serial > 0, "dead TestBridgedValueTy")
    return value.description
  }

  static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  static func _getObjectiveCType() -> Any.Type {
    return TestObjCValueTy.self
  }

  func _bridgeToObjectiveC() -> TestObjCEquatableValueTy {
    _bridgedEquatableValueBridgeOperations.fetchAndAdd(1)
    return TestObjCEquatableValueTy(value)
  }

  static func _forceBridgeFromObjectiveC(
    x: TestObjCEquatableValueTy,
    inout result: TestBridgedEquatableValueTy?
  ) {
    _bridgedEquatableValueBridgeOperations.fetchAndAdd(1)
    result = TestBridgedEquatableValueTy(x.value)
  }

  static func _conditionallyBridgeFromObjectiveC(
    x: TestObjCEquatableValueTy,
    inout result: TestBridgedEquatableValueTy?
  ) -> Bool {
    self._forceBridgeFromObjectiveC(x, result: &result)
    return true
  }

  var value: Int
  var serial: Int
}

func == (lhs: TestBridgedEquatableValueTy, rhs: TestBridgedEquatableValueTy) -> Bool {
  return lhs.value == rhs.value
}

/// Expect some number of autoreleased key and value objects.
///
/// - parameter opt: applies to platforms that have the return-autoreleased
///   optimization.
///
/// - parameter unopt: applies to platforms that don't.
///
/// FIXME: Some non-zero `opt` might be cases of missed return-autorelease.
func expectAutoreleasedKeysAndValues(
  opt opt: (Int, Int) = (0, 0), unopt: (Int, Int) = (0, 0)) {
  var expectedKeys = 0
  var expectedValues = 0
#if arch(i386)
  (expectedKeys, expectedValues) = unopt
#else
  (expectedKeys, expectedValues) = opt
#endif

  TestObjCKeyTy.objectCount -= expectedKeys
  TestObjCValueTy.objectCount -= expectedValues
}

/// Expect some number of autoreleased value objects.
///
/// - parameter opt: applies to platforms that have the return-autoreleased
///   optimization.
///
/// - parameter unopt: applies to platforms that don't.
///
/// FIXME: Some non-zero `opt` might be cases of missed return-autorelease.
func expectAutoreleasedValues(
  opt opt: Int = 0, unopt: Int = 0) {
  expectAutoreleasedKeysAndValues(opt: (0, opt), unopt: (0, unopt))
}

func resetLeaksOfDictionaryKeysValues() {
  TestKeyTy.objectCount = 0
  TestValueTy.objectCount = 0
  TestEquatableValueTy.objectCount = 0

  TestObjCKeyTy.objectCount = 0
  TestObjCValueTy.objectCount = 0
  TestObjCEquatableValueTy.objectCount = 0
}

func expectNoLeaksOfDictionaryKeysValues() {
  expectEqual(0, TestKeyTy.objectCount, "TestKeyTy leak")
  expectEqual(0, TestValueTy.objectCount, "TestValueTy leak")
  expectEqual(0, TestEquatableValueTy.objectCount, "TestEquatableValueTy leak")

  expectEqual(0, TestObjCKeyTy.objectCount, "TestObjCKeyTy leak")
  expectEqual(0, TestObjCValueTy.objectCount, "TestObjCValueTy leak")
  expectEqual(
    0, TestObjCEquatableValueTy.objectCount, "TestObjCEquatableValueTy leak")
}

func getBridgedNSDictionaryOfRefTypesBridgedVerbatim() -> NSDictionary {
  assert(_isBridgedVerbatimToObjectiveC(TestObjCKeyTy.self))
  assert(_isBridgedVerbatimToObjectiveC(TestObjCValueTy.self))

  var d = Dictionary<TestObjCKeyTy, TestObjCValueTy>(minimumCapacity: 32)
  d[TestObjCKeyTy(10)] = TestObjCValueTy(1010)
  d[TestObjCKeyTy(20)] = TestObjCValueTy(1020)
  d[TestObjCKeyTy(30)] = TestObjCValueTy(1030)

  let bridged =
    unsafeBitCast(_convertDictionaryToNSDictionary(d), NSDictionary.self)

  assert(isNativeNSDictionary(bridged))

  return bridged
}

func getBridgedEmptyNSDictionary() -> NSDictionary {
  let d = Dictionary<TestObjCKeyTy, TestObjCValueTy>()

  let bridged =
    unsafeBitCast(_convertDictionaryToNSDictionary(d), NSDictionary.self)
  assert(isNativeNSDictionary(bridged))

  return bridged
}

func getBridgedNSDictionaryOfKeyValue_ValueTypesCustomBridged(
  numElements numElements: Int = 3
) -> NSDictionary {
  assert(!_isBridgedVerbatimToObjectiveC(TestBridgedKeyTy.self))
  assert(!_isBridgedVerbatimToObjectiveC(TestBridgedValueTy.self))

  var d = Dictionary<TestBridgedKeyTy, TestBridgedValueTy>()
  for i in 1..<(numElements + 1) {
    d[TestBridgedKeyTy(i * 10)] = TestBridgedValueTy(i * 10 + 1000)
  }

  let bridged = _convertDictionaryToNSDictionary(d)
  assert(isNativeNSDictionary(bridged))

  return bridged
}

func slurpFastEnumerationFromSwift(
  a: NSArray, _ fe: NSFastEnumeration, _ sink: (AnyObject) -> Void,
  maxItems: Int? = nil
) {
  var state = NSFastEnumerationState()

  let stackBufLength = 3
  let stackBuf = _HeapBuffer<(), AnyObject?>(
    _HeapBufferStorage<(), AnyObject?>.self, (), stackBufLength)

  var itemsReturned = 0
  while true {
    let returnedCount = fe.countByEnumeratingWithState(
      &state, objects: AutoreleasingUnsafeMutablePointer(stackBuf.baseAddress),
      count: stackBufLength)
    expectNotEqual(0, state.state)
    expectNotEqual(nil, state.mutationsPtr)
    if returnedCount == 0 {
      break
    }
    for i in 0..<returnedCount {
      let value: AnyObject = state.itemsPtr[i]!
      sink(value)
      ++itemsReturned
    }
    if maxItems != nil && itemsReturned >= maxItems! {
      return
    }
  }

  for _ in 0..<3 {
    let returnedCount = fe.countByEnumeratingWithState(
      &state, objects: AutoreleasingUnsafeMutablePointer(stackBuf.baseAddress),
      count: stackBufLength)
    expectNotEqual(0, state.state)
    expectNotEqual(nil, state.mutationsPtr)
    expectEqual(0, returnedCount)
  }
}

typealias AnyObjectTuple2 = (AnyObject, AnyObject)

func slurpFastEnumerationFromSwift(
  d: NSDictionary, _ fe: NSFastEnumeration, _ sink: (AnyObjectTuple2) -> Void,
  maxItems: Int? = nil
) {
  var state = NSFastEnumerationState()

  let stackBufLength = 3
  let stackBuf = _HeapBuffer<(), AnyObject?>(
    _HeapBufferStorage<(), AnyObject?>.self, (), stackBufLength)

  var itemsReturned = 0
  while true {
    let returnedCount = fe.countByEnumeratingWithState(
      &state, objects: AutoreleasingUnsafeMutablePointer(stackBuf.baseAddress),
      count: stackBufLength)
    expectNotEqual(0, state.state)
    expectNotEqual(nil, state.mutationsPtr)
    if returnedCount == 0 {
      break
    }
    for i in 0..<returnedCount {
      let key: AnyObject = state.itemsPtr[i]!
      let value: AnyObject = d.objectForKey(key)!
      let kv = (key, value)
      sink(kv)
      ++itemsReturned
    }
    if maxItems != nil && itemsReturned >= maxItems! {
      return
    }
  }

  for _ in 0..<3 {
    let returnedCount = fe.countByEnumeratingWithState(
      &state, objects: AutoreleasingUnsafeMutablePointer(stackBuf.baseAddress),
      count: stackBufLength)
    expectEqual(0, returnedCount)
  }
}

func slurpFastEnumerationOfNSEnumeratorFromSwift(
  a: NSArray, _ enumerator: NSEnumerator, _ sink: (AnyObject) -> Void,
  maxFastEnumerationItems: Int
) {
  slurpFastEnumerationFromSwift(
    a, enumerator, sink, maxItems: maxFastEnumerationItems)
  while let value = enumerator.nextObject() {
    sink(value)
  }
}

func slurpFastEnumerationOfNSEnumeratorFromSwift(
  d: NSDictionary, _ enumerator: NSEnumerator,
  _ sink: (AnyObjectTuple2) -> Void,
  maxFastEnumerationItems: Int
) {
  slurpFastEnumerationFromSwift(
    d, enumerator, sink, maxItems: maxFastEnumerationItems)
  while let key = enumerator.nextObject() {
    let value: AnyObject = d.objectForKey(key)!
    let kv = (key, value)
    sink(kv)
  }
}

import SlurpFastEnumeration

func slurpFastEnumerationFromObjC(
  a: NSArray, _ fe: NSFastEnumeration, _ sink: (AnyObject) -> Void
) {
  let objcValues = NSMutableArray()
  slurpFastEnumerationOfArrayFromObjCImpl(a, fe, objcValues)
  for value in objcValues {
    sink(value)
  }
}

struct ExpectedArrayElement : Comparable, CustomStringConvertible {
  var value: Int
  var valueIdentity: UInt

  init(value: Int, valueIdentity: UInt = 0) {
    self.value = value
    self.valueIdentity = valueIdentity
  }

  var description: String {
    return "(\(value), \(valueIdentity))"
  }
}

func == (
  lhs: ExpectedArrayElement,
  rhs: ExpectedArrayElement
) -> Bool {
  return
    lhs.value == rhs.value &&
    lhs.valueIdentity == rhs.valueIdentity
}

func < (
  lhs: ExpectedArrayElement,
  rhs: ExpectedArrayElement
) -> Bool {
  let lhsElements = [ lhs.value, Int(bitPattern: lhs.valueIdentity) ]
  let rhsElements = [ rhs.value, Int(bitPattern: rhs.valueIdentity) ]
  return lhsElements.lexicographicalCompare(rhsElements)
}

func _equalsWithoutElementIdentity(
  lhs: [ExpectedArrayElement], _ rhs: [ExpectedArrayElement]
) -> Bool {
  func stripIdentity(
    list: [ExpectedArrayElement]
  ) -> [ExpectedArrayElement] {
    return list.map { ExpectedArrayElement(value: $0.value) }
  }

  return stripIdentity(lhs).elementsEqual(stripIdentity(rhs))
}

func _makeExpectedArrayContents(
  expected: [Int]
) -> [ExpectedArrayElement] {
  var result = [ExpectedArrayElement]()
  for value in expected {
    result.append(ExpectedArrayElement(value: value))
  }
  return result
}

func _checkArrayFastEnumerationImpl(
  expected: [Int],
  _ a: NSArray,
  _ makeEnumerator: () -> NSFastEnumeration,
  _ useEnumerator: (NSArray, NSFastEnumeration, (AnyObject)->()) -> Void,
  _ convertValue: (AnyObject) -> Int
) {
  let expectedContentsWithoutIdentity =
  _makeExpectedArrayContents(expected)
  
  var expectedContents = [ExpectedArrayElement]()
  
  for i in 0..<3 {
    var actualContents = [ExpectedArrayElement]()
    let sink = {
      (value: AnyObject) in
      actualContents.append(ExpectedArrayElement(
        value: convertValue(value),
        valueIdentity: unsafeBitCast(value, UInt.self)))
    }

    useEnumerator(a, makeEnumerator(), sink)

    expectTrue(
      _equalsWithoutElementIdentity(
        expectedContentsWithoutIdentity, actualContents),
      "expected: \(expectedContentsWithoutIdentity)\n" +
      "actual: \(actualContents)\n")

    if i == 0 {
      expectedContents = actualContents
    }
    
    expectEqualSequence(expectedContents, actualContents)
  }
}

func checkArrayFastEnumerationFromSwift(
  expected: [Int],
  _ a: NSArray, _ makeEnumerator: () -> NSFastEnumeration,
  _ convertValue: (AnyObject) -> Int
) {
  _checkArrayFastEnumerationImpl(
    expected, a, makeEnumerator,
    { (a, fe, sink) in
      slurpFastEnumerationFromSwift(a, fe, sink)
    },
    convertValue)
}

func checkArrayFastEnumerationFromObjC(
  expected: [Int],
  _ a: NSArray, _ makeEnumerator: () -> NSFastEnumeration,
  _ convertValue: (AnyObject) -> Int
) {
  _checkArrayFastEnumerationImpl(
    expected, a, makeEnumerator,
    { (a, fe, sink) in
      slurpFastEnumerationFromObjC(a, fe, sink)
    },
    convertValue)
}

func checkArrayEnumeratorPartialFastEnumerationFromSwift(
  expected: [Int],
  _ a: NSArray,
  maxFastEnumerationItems: Int,
  _ convertValue: (AnyObject) -> Int
) {
  _checkArrayFastEnumerationImpl(
    expected, a, { a.objectEnumerator() },
    { (a, fe, sink) in
      slurpFastEnumerationOfNSEnumeratorFromSwift(
        a, fe as! NSEnumerator, sink,
        maxFastEnumerationItems: maxFastEnumerationItems)
    },
    convertValue)
}

struct ExpectedSetElement : Comparable, CustomStringConvertible {
  var value: Int
  var valueIdentity: UInt

  init(value: Int, valueIdentity: UInt = 0) {
    self.value = value
    self.valueIdentity = valueIdentity
  }

  var description: String {
    return "(\(value), \(valueIdentity))"
  }
}

func == (
  lhs: ExpectedSetElement,
  rhs: ExpectedSetElement
) -> Bool {
  return
    lhs.value == rhs.value &&
    lhs.valueIdentity == rhs.valueIdentity
}

func < (
  lhs: ExpectedSetElement,
  rhs: ExpectedSetElement
) -> Bool {
  let lhsElements = [ lhs.value, Int(bitPattern: lhs.valueIdentity) ]
  let rhsElements = [ rhs.value, Int(bitPattern: rhs.valueIdentity) ]
  return lhsElements.lexicographicalCompare(rhsElements)
}

func _makeExpectedSetContents(
  expected: [Int]
) -> [ExpectedSetElement] {
  var result = [ExpectedSetElement]()
  for value in expected {
    result.append(ExpectedSetElement(value: value))
  }
  return result
}

func _equalsUnorderedWithoutElementIdentity(
  lhs: [ExpectedSetElement], _ rhs: [ExpectedSetElement]
) -> Bool {
  func stripIdentity(
    list: [ExpectedSetElement]
  ) -> [ExpectedSetElement] {
    return list.map { ExpectedSetElement(value: $0.value) }
  }

  return equalsUnordered(stripIdentity(lhs), stripIdentity(rhs))
}

func _checkSetFastEnumerationImpl(
  expected: [Int],
  _ s: NSSet,
  _ makeEnumerator: () -> NSFastEnumeration,
  _ useEnumerator: (NSSet, NSFastEnumeration, (AnyObject)->()) -> Void,
  _ convertMember: (AnyObject) -> Int
) {
  let expectedContentsWithoutIdentity =
    _makeExpectedSetContents(expected)
  var expectedContents = [ExpectedSetElement]()

  for i in 0..<3 {
    var actualContents = [ExpectedSetElement]()
    let sink = {
      (value: AnyObject) in
      actualContents.append(ExpectedSetElement(
        value: convertMember(value),
        valueIdentity: unsafeBitCast(value, UInt.self)))
    }

    useEnumerator(s, makeEnumerator(), sink)

    expectTrue(
      _equalsUnorderedWithoutElementIdentity(
        expectedContentsWithoutIdentity, actualContents),
      "expected: \(expectedContentsWithoutIdentity)\n" +
      "actual: \(actualContents)\n")

    if i == 0 {
      expectedContents = actualContents
    }
    expectTrue(equalsUnordered(expectedContents, actualContents))
  }
}

func slurpFastEnumerationFromObjC(
  s: NSSet, _ fe: NSFastEnumeration, _ sink: (AnyObject) -> Void
) {
  let objcValues = NSMutableArray()
  slurpFastEnumerationOfArrayFromObjCImpl(s, fe, objcValues)
  for value in objcValues {
    sink(value)
  }
}

func slurpFastEnumerationOfNSEnumeratorFromSwift(
  s: NSSet, _ enumerator: NSEnumerator, _ sink: (AnyObject) -> Void,
  maxFastEnumerationItems: Int
) {
  slurpFastEnumerationFromSwift(
    s, enumerator, sink, maxItems: maxFastEnumerationItems)
  while let value = enumerator.nextObject() {
    sink(value)
  }
}

func slurpFastEnumerationFromSwift(
  s: NSSet, _ fe: NSFastEnumeration, _ sink: (AnyObject) -> Void,
  maxItems: Int? = nil
) {
  var state = NSFastEnumerationState()

  let stackBufLength = 3
  let stackBuf = _HeapBuffer<(), AnyObject?>(
    _HeapBufferStorage<(), AnyObject?>.self, (), stackBufLength)

  var itemsReturned = 0
  while true {
    let returnedCount = fe.countByEnumeratingWithState(
      &state, objects: AutoreleasingUnsafeMutablePointer(stackBuf.baseAddress),
      count: stackBufLength)
    expectNotEqual(0, state.state)
    expectNotEqual(nil, state.mutationsPtr)
    if returnedCount == 0 {
      break
    }
    for i in 0..<returnedCount {
      let value: AnyObject = state.itemsPtr[i]!
      sink(value)
      ++itemsReturned
    }
    if maxItems != nil && itemsReturned >= maxItems! {
      return
    }
  }

  for _ in 0..<3 {
    let returnedCount = fe.countByEnumeratingWithState(
      &state, objects: AutoreleasingUnsafeMutablePointer(stackBuf.baseAddress),
      count: stackBufLength)
    expectNotEqual(0, state.state)
    expectNotEqual(nil, state.mutationsPtr)
    expectEqual(0, returnedCount)
  }
}

func checkSetFastEnumerationFromSwift(
  expected: [Int],
  _ s: NSSet, _ makeEnumerator: () -> NSFastEnumeration,
  _ convertMember: (AnyObject) -> Int
) {
  _checkSetFastEnumerationImpl(
    expected, s, makeEnumerator,
    { (s, fe, sink) in
      slurpFastEnumerationFromSwift(s, fe, sink)
    },
    convertMember)
}

func checkSetFastEnumerationFromObjC(
  expected: [Int],
  _ s: NSSet, _ makeEnumerator: () -> NSFastEnumeration,
  _ convertMember: (AnyObject) -> Int
) {
  _checkSetFastEnumerationImpl(
    expected, s, makeEnumerator,
    { (s, fe, sink) in
      slurpFastEnumerationFromObjC(s, fe, sink)
    },
    convertMember)
}

func checkSetEnumeratorPartialFastEnumerationFromSwift(
  expected: [Int],
  _ s: NSSet,
  maxFastEnumerationItems: Int,
  _ convertMember: (AnyObject) -> Int
) {
  _checkSetFastEnumerationImpl(
    expected, s, { s.objectEnumerator() },
    { (s, fe, sink) in
      slurpFastEnumerationOfNSEnumeratorFromSwift(
        s, fe as! NSEnumerator, sink,
        maxFastEnumerationItems: maxFastEnumerationItems)
    },
    convertMember)
}


func slurpFastEnumerationFromObjC(
  d: NSDictionary, _ fe: NSFastEnumeration, _ sink: (AnyObjectTuple2) -> Void
) {
  let objcPairs = NSMutableArray()
  slurpFastEnumerationOfDictionaryFromObjCImpl(d, fe, objcPairs)
  for i in 0..<objcPairs.count/2 {
    let key: AnyObject = objcPairs[i * 2]
    let value: AnyObject = objcPairs[i * 2 + 1]
    let kv = (key, value)
    sink(kv)
  }
}

struct ExpectedDictionaryElement : Comparable, CustomStringConvertible {
  var key: Int
  var value: Int
  var keyIdentity: UInt
  var valueIdentity: UInt

  init(key: Int, value: Int, keyIdentity: UInt = 0, valueIdentity: UInt = 0) {
    self.key = key
    self.value = value
    self.keyIdentity = keyIdentity
    self.valueIdentity = valueIdentity
  }

  var description: String {
    return "(\(key), \(value), \(keyIdentity), \(valueIdentity))"
  }
}

func == (
  lhs: ExpectedDictionaryElement,
  rhs: ExpectedDictionaryElement
) -> Bool {
  return
    lhs.key == rhs.key &&
    lhs.value == rhs.value &&
    lhs.keyIdentity == rhs.keyIdentity &&
    lhs.valueIdentity == rhs.valueIdentity
}

func < (
  lhs: ExpectedDictionaryElement,
  rhs: ExpectedDictionaryElement
) -> Bool {
  let lhsElements = [
    lhs.key, lhs.value, Int(bitPattern: lhs.keyIdentity),
    Int(bitPattern: lhs.valueIdentity)
  ]
  let rhsElements = [
    rhs.key, rhs.value, Int(bitPattern: rhs.keyIdentity),
    Int(bitPattern: rhs.valueIdentity)
  ]
  return lhsElements.lexicographicalCompare(rhsElements)
}

func _equalsUnorderedWithoutElementIdentity(
  lhs: [ExpectedDictionaryElement], _ rhs: [ExpectedDictionaryElement]
) -> Bool {
  func stripIdentity(
    list: [ExpectedDictionaryElement]
  ) -> [ExpectedDictionaryElement] {
    return list.map { ExpectedDictionaryElement(key: $0.key, value: $0.value) }
  }

  return equalsUnordered(stripIdentity(lhs), stripIdentity(rhs))
}

func _makeExpectedDictionaryContents(
  expected: [(Int, Int)]
) -> [ExpectedDictionaryElement] {
  var result = [ExpectedDictionaryElement]()
  for (key, value) in expected {
    result.append(ExpectedDictionaryElement(key: key, value: value))
  }
  return result
}

func _checkDictionaryFastEnumerationImpl(
  expected: [(Int, Int)],
  _ d: NSDictionary,
  _ makeEnumerator: () -> NSFastEnumeration,
  _ useEnumerator: (NSDictionary, NSFastEnumeration, (AnyObjectTuple2)->()) -> Void,
  _ convertKey: (AnyObject) -> Int,
  _ convertValue: (AnyObject) -> Int
) {
  let expectedContentsWithoutIdentity =
    _makeExpectedDictionaryContents(expected)
  var expectedContents = [ExpectedDictionaryElement]()

  for i in 0..<3 {
    var actualContents = [ExpectedDictionaryElement]()
    let sink: (AnyObjectTuple2) -> Void = {
      (key, value) in
      actualContents.append(ExpectedDictionaryElement(
        key: convertKey(key),
        value: convertValue(value),
        keyIdentity: unsafeBitCast(key, UInt.self),
        valueIdentity: unsafeBitCast(value, UInt.self)))
    }

    useEnumerator(d, makeEnumerator(), sink)

    expectTrue(
      _equalsUnorderedWithoutElementIdentity(
        expectedContentsWithoutIdentity, actualContents),
      "expected: \(expectedContentsWithoutIdentity)\n" +
      "actual: \(actualContents)\n")

    if i == 0 {
      expectedContents = actualContents
    }
    expectTrue(equalsUnordered(expectedContents, actualContents))
  }
}

func checkDictionaryFastEnumerationFromSwift(
  expected: [(Int, Int)],
  _ d: NSDictionary, _ makeEnumerator: () -> NSFastEnumeration,
  _ convertKey: (AnyObject) -> Int,
  _ convertValue: (AnyObject) -> Int
) {
  _checkDictionaryFastEnumerationImpl(
    expected, d, makeEnumerator,
    { (d, fe, sink) in
      slurpFastEnumerationFromSwift(d, fe, sink)
    },
    convertKey, convertValue)
}

func checkDictionaryFastEnumerationFromObjC(
  expected: [(Int, Int)],
  _ d: NSDictionary, _ makeEnumerator: () -> NSFastEnumeration,
  _ convertKey: (AnyObject) -> Int,
  _ convertValue: (AnyObject) -> Int
) {
  _checkDictionaryFastEnumerationImpl(
    expected, d, makeEnumerator,
    { (d, fe, sink) in
      slurpFastEnumerationFromObjC(d, fe, sink)
    },
    convertKey, convertValue)
}

func checkDictionaryEnumeratorPartialFastEnumerationFromSwift(
  expected: [(Int, Int)],
  _ d: NSDictionary,
  maxFastEnumerationItems: Int,
  _ convertKey: (AnyObject) -> Int,
  _ convertValue: (AnyObject) -> Int
) {
  _checkDictionaryFastEnumerationImpl(
    expected, d, { d.keyEnumerator() },
    { (d, fe, sink) in
      slurpFastEnumerationOfNSEnumeratorFromSwift(
        d, fe as! NSEnumerator, sink,
        maxFastEnumerationItems: maxFastEnumerationItems)
    },
    convertKey, convertValue)
}

func getBridgedNSArrayOfRefTypeVerbatimBridged(
  numElements numElements: Int = 3,
  capacity: Int? = nil
) -> NSArray {
  assert(_isBridgedVerbatimToObjectiveC(TestObjCValueTy.self))

  var a = [TestObjCValueTy]()
  if let requestedCapacity = capacity {
    a.reserveCapacity(requestedCapacity)
  }
  for i in 1..<(numElements + 1) {
    a.append(TestObjCValueTy(i * 10))
  }

  let bridged = _convertArrayToNSArray(a)
  assert(isNativeNSArray(bridged))

  return bridged
}

func getBridgedNSArrayOfValueTypeCustomBridged(
  numElements numElements: Int = 3,
  capacity: Int? = nil
) -> NSArray {
  assert(!_isBridgedVerbatimToObjectiveC(TestBridgedValueTy.self))

  var a = [TestBridgedValueTy]()
  if let requestedCapacity = capacity {
    a.reserveCapacity(requestedCapacity)
  }
  for i in 1..<(numElements + 1) {
    a.append(TestBridgedValueTy(i * 10))
  }

  let bridged = _convertArrayToNSArray(a)
  assert(isNativeNSArray(bridged))

  return bridged
}

