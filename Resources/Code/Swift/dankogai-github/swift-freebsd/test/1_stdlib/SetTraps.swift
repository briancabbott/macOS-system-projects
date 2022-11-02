// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: %target-build-swift %s -o %t/a.out_Debug
// RUN: %target-build-swift %s -o %t/a.out_Release -O
//
// RUN: %target-run %t/a.out_Debug
// RUN: %target-run %t/a.out_Release
// REQUIRES: executable_test

// FIXME: rdar://problem/19648117 Needs splitting objc parts out
// XFAIL: linux

import StdlibUnittest
import Foundation

// Also import modules which are used by StdlibUnittest internally. This
// workaround is needed to link all required libraries in case we compile
// StdlibUnittest with -sil-serialize-all.
import SwiftPrivate
#if _runtime(_ObjC)
import ObjectiveC
#endif

struct NotBridgedKeyTy : Equatable, Hashable {
  init(_ value: Int) {
    self.value = value
  }
  var hashValue: Int {
    return value
  }
  var value: Int
}

func == (lhs: NotBridgedKeyTy, rhs: NotBridgedKeyTy) -> Bool {
  return lhs.value == rhs.value
}

assert(!_isBridgedToObjectiveC(NotBridgedKeyTy.self))

struct NotBridgedValueTy {}

assert(!_isBridgedToObjectiveC(NotBridgedValueTy.self))

class BridgedVerbatimRefTy : Equatable, Hashable {
  init(_ value: Int) {
    self.value = value
  }
  var hashValue: Int {
    return value
  }
  var value: Int
}

func == (lhs: BridgedVerbatimRefTy, rhs: BridgedVerbatimRefTy) -> Bool {
  return lhs.value == rhs.value
}

assert(_isBridgedToObjectiveC(BridgedVerbatimRefTy.self))
assert(_isBridgedVerbatimToObjectiveC(BridgedVerbatimRefTy.self))

var SetTraps = TestSuite("SetTraps")

SetTraps.test("sanity") {
  // Sanity checks.  This code should not trap.
  var s = Set<BridgedVerbatimRefTy>()
  var nss = s as NSSet
}

SetTraps.test("RemoveInvalidIndex1")
  .skip(.Custom(
    { _isFastAssertConfiguration() },
    reason: "this trap is not guaranteed to happen in -Ounchecked"))
  .code {
  var s = Set<Int>()
  let index = s.startIndex
  expectCrashLater()
  s.removeAtIndex(index)
}

SetTraps.test("RemoveInvalidIndex2")
  .skip(.Custom(
    { _isFastAssertConfiguration() },
    reason: "this trap is not guaranteed to happen in -Ounchecked"))
  .code {
  var s = Set<Int>()
  let index = s.endIndex
  expectCrashLater()
  s.removeAtIndex(index)
}

SetTraps.test("RemoveInvalidIndex3")
  .skip(.Custom(
    { _isFastAssertConfiguration() },
    reason: "this trap is not guaranteed to happen in -Ounchecked"))
  .code {
  var s: Set<Int> = [ 10, 20, 30 ]
  let index = s.endIndex
  expectCrashLater()
  s.removeAtIndex(index)
}

SetTraps.test("RemoveInvalidIndex4")
  .skip(.Custom(
    { _isFastAssertConfiguration() },
    reason: "this trap is not guaranteed to happen in -Ounchecked"))
  .code {
  var s: Set<Int> = [ 10 ]
  let index = s.indexOf(10)!
  s.removeAtIndex(index)
  expectFalse(s.contains(10))
  expectCrashLater()
  s.removeAtIndex(index)
}

SetTraps.test("RemoveFirstFromEmpty")
  .skip(.Custom(
    { _isFastAssertConfiguration() },
    reason: "this trap is not guaranteed to happen in -Ounchecked"))
  .crashOutputMatches(_isDebugAssertConfiguration() ?
    "can't removeFirst from an empty Set" : "")
  .code {
  var s = Set<Int>()
  expectCrashLater()
  s.removeFirst()
}

class TestObjCKeyTy : NSObject {
  init(_ value: Int) {
    self.value = value
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
    return value
  }

  var value: Int
}

struct TestBridgedKeyTy : Hashable, _ObjectiveCBridgeable {
  static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  init(_ value: Int) { self.value = value }

  var hashValue: Int { return value }

  static func _getObjectiveCType() -> Any.Type {
    return TestObjCKeyTy.self
  }

  func _bridgeToObjectiveC() -> TestObjCKeyTy {
    return TestObjCKeyTy(value)
  }

  static func _forceBridgeFromObjectiveC(
    x: TestObjCKeyTy,
    inout result: TestBridgedKeyTy?
  ) {
    result = TestBridgedKeyTy(x.value)
  }

  static func _conditionallyBridgeFromObjectiveC(
    x: TestObjCKeyTy,
    inout result: TestBridgedKeyTy?
  ) -> Bool {
    result = TestBridgedKeyTy(x.value)
    return true
  }

  var value: Int
}

func ==(x: TestBridgedKeyTy, y: TestBridgedKeyTy) -> Bool {
  return x.value == y.value
}

SetTraps.test("BridgedKeyIsNotNSCopyable1") {
  // This Set is bridged in O(1).
  var s: Set<TestObjCKeyTy> = [ TestObjCKeyTy(10) ]
  var nss = s as NSSet

  // Unlike NSDictionary, NSSet does not require NSCopying from its element
  // type.
  let copiedSet = nss.mutableCopy() as! NSMutableSet
  expectEqual(10, (copiedSet.anyObject() as! TestObjCKeyTy).value)
}

SetTraps.test("Downcast1")
  .skip(.Custom(
    { _isFastAssertConfiguration() },
    reason: "this trap is not guaranteed to happen in -Ounchecked"))
  .code {
  let s: Set<NSObject> = [ NSObject(), NSObject() ]
  let s2: Set<TestObjCKeyTy> = _setDownCast(s)
  expectCrashLater()
  let v1 = s2.contains(TestObjCKeyTy(10))
  let v2 = s2.contains(TestObjCKeyTy(20))

  // This triggers failure.
  for m in s2 { }
}

SetTraps.test("Downcast2")
  .skip(.Custom(
    { _isFastAssertConfiguration() },
    reason: "this trap is not guaranteed to happen in -Ounchecked"))
  .code {
  let s: Set<NSObject> = [ TestObjCKeyTy(10), NSObject() ]
  expectCrashLater()
  let s2: Set<TestBridgedKeyTy> = _setBridgeFromObjectiveC(s)
  let v1 = s2.contains(TestBridgedKeyTy(10))
}

runAllTests()

