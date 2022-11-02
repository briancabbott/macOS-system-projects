//===--- ArrayBridge.swift - Tests of Array casting and bridging ----------===//
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

// RUN: rm -rf %t
// RUN: mkdir -p %t
//
// RUN: %target-clang %S/Inputs/ArrayBridge/ArrayBridge.m -c -o %t/ArrayBridgeObjC.o -g
// RUN: %target-build-swift %s -I %S/Inputs/ArrayBridge/ -Xlinker %t/ArrayBridgeObjC.o -o %t/ArrayBridge

// RUN: %target-run %t/ArrayBridge > %t.txt
// RUN: FileCheck %s < %t.txt
// REQUIRES: executable_test

// XFAIL: linux

import Foundation
import ArrayBridgeObjC

// CHECK: testing...
print("testing...")

var trackedCount = 0
var nextTrackedSerialNumber = 0

@objc protocol Fooable {
  func foo()
}

@objc protocol Barable {
  func bar()
}

@objc protocol Bazable {
  func baz()
}

class Tracked : NSObject, Fooable {
  func foo() { }

  required init(_ value: Int) {
    ++trackedCount
    serialNumber = ++nextTrackedSerialNumber
    self.value = value
  }
  
  deinit {
    assert(serialNumber > 0, "double destruction!")
    --trackedCount
    serialNumber = -serialNumber
  }

  override var description: String {
    assert(serialNumber > 0, "dead Tracked!")
    return "Base#\(serialNumber)(\(value))"
  }

  func successor() -> Self {
    return self.dynamicType.init(self.value.successor())
  }

  var value: Int
  var serialNumber: Int
}

func == (x: Tracked, y: Tracked) -> Bool {
  return x.value == y.value
}

typealias Base = Tracked
class Derived : Base, Barable {
  func bar() { }

  override var description: String {
    assert(serialNumber > 0, "dead Tracked!")
    return "Derived#\(serialNumber)(\(value))"
  }
}

class BridgedObjC : Base, Barable {
  func bar() { }
  override var description: String {
    assert(serialNumber > 0, "dead Tracked!")
    return "BridgedObjC#\(serialNumber)(\(value))"
  }
}

var bridgeFromOperationCount = 0
var bridgeToOperationCount = 0

struct BridgedSwift : CustomStringConvertible, _ObjectiveCBridgeable {
  static func _getObjectiveCType() -> Any.Type {
    return BridgedObjC.self
  }
  
  func _bridgeToObjectiveC() -> BridgedObjC {
    ++bridgeToOperationCount
    return BridgedObjC(trak.value)
  }

  static func _isBridgedToObjectiveC() -> Bool {
    return true
  }

  static func _forceBridgeFromObjectiveC(
    x: BridgedObjC,
    inout result: BridgedSwift?
  ) {
    assert(x.value >= 0, "not bridged")
    ++bridgeFromOperationCount
    result = BridgedSwift(x.value)
  }

  static func _conditionallyBridgeFromObjectiveC(
    x: BridgedObjC,
    inout result: BridgedSwift?
  ) -> Bool {
    if x.value >= 0 {
      result = BridgedSwift(x.value)
      return true
    }

    result = nil
    return false
  }
  
  var description: String {
    assert(trak.serialNumber > 0, "dead Tracked!")
    return "BridgedSwift#\(trak.serialNumber)(\(trak.value))"
  }

  init(_ value: Int) {
    self.trak = Tracked(value)
  }
  
  func successor() -> BridgedSwift {
    return BridgedSwift(trak.value.successor())
  }

  static func printStats() {
    print(
      "bridge operations "
      + "(from, to) = (\(bridgeFromOperationCount), \(bridgeToOperationCount))")
  }
  
  static func resetStats() {
    bridgeFromOperationCount = 0
    bridgeToOperationCount = 0
  }
  
  var trak: Tracked
}

// A class used to test various Objective-C thunks.
class Thunks : NSObject {
  func createBridgedObjC(value: Int) -> AnyObject {
    return BridgedObjC(value)
  }
  
  @objc func acceptBridgedObjCArray(x: [BridgedObjC]) {
    print("acceptBridgedObjCArray(\(x))")
  }

  @objc func produceBridgedObjCArray(numItems: Int) -> [BridgedObjC] {
    var array: [BridgedObjC] = []
    for i in 0..<numItems {
      array.append(BridgedObjC(i))
    }
    print("produceBridgedObjCArray(\(array))")
    return array
  }

  @objc func acceptBridgedSwiftArray(raw: NSArray) {
    let x = raw as! [BridgedSwift]
    print("acceptBridgedSwiftArray(\(x))")
  }

  @objc func produceBridgedSwiftArray(numItems: Int) -> NSArray {
    var array: [BridgedSwift] = []
    for i in 0..<numItems {
      array.append(BridgedSwift(i))
    }
    print("produceBridgedSwiftArray(\(array))")
    return array as NSArray
  }
}


//===--- Bridged Verbatim -------------------------------------------------===//
// Base is "bridged verbatim"
//===----------------------------------------------------------------------===//

func testBridgedVerbatim() {
  let bases: [Base] = [Base(100), Base(200), Base(300)]

  //===--- Implicit conversion to/from NSArray ------------------------------===//

  // CHECK-NEXT: Base#1(100)
  let basesConvertedToNSArray = bases as NSArray
  print(basesConvertedToNSArray.objectAtIndex(0) as! Base)

  // Create an ordinary NSArray, not a native one
  let nsArrayOfBase: NSArray = NSArray(object: Base(42))

  // NSArray can be unconditionally cast to [AnyObject]...
  let nsArrayOfBaseConvertedToAnyObjectArray = nsArrayOfBase as [AnyObject]

  // Capture the representation of the first element
  // CHECK-NEXT: [[base42:Base.*42]]
  print(nsArrayOfBase.objectAtIndex(0) as! Base)

  // ...with the same elements
  // CHECK-NEXT: [[base42]]
  print(nsArrayOfBaseConvertedToAnyObjectArray[0] as! Base)

  // Verify that NSArray class methods are inherited by a Swift bridging class.
  // CHECK-NEXT: Swift.{{.*}}Array
  debugPrint(basesConvertedToNSArray.dynamicType)
  // CHECK-NEXT: true
  print(basesConvertedToNSArray.dynamicType.supportsSecureCoding())

  //===--- Up- and Down-casts -----------------------------------------------===//
  var derived: [Derived] = [Derived(11), Derived(22)]
  // CHECK-NEXT: [[derived0:\[Derived#[0-9]+\(11\), Derived#[0-9]+\(22\)\]{1}]]
  print(derived)

  // upcast is implicit
  let derivedAsBases: [Base] = derived

  // CHECK-NEXT: [[derived0]]
  print(derivedAsBases)

  // Arrays are logically distinct after upcast
  derived[0] = Derived(33)
  
  // CHECK-NEXT: {{\[Derived#[0-9]+\(33\), Derived#[0-9]+\(22\)]}}
  print(derived)
  // CHECK-NEXT: [[derived0]]
  print(derivedAsBases)

  // CHECK-NEXT: [[derived0]]
  if let roundTripDerived = derivedAsBases as? [Derived] {
    print(roundTripDerived)
  }
  else {
    print("roundTripDerived upcast failed")
  }

  // CHECK-NEXT: [[derived2:\[Derived#[0-9]+\(44\), Derived#[0-9]+\(55\)\]{1}]]
  let derivedInBaseBuffer: [Base] = [Derived(44), Derived(55)]
  print(derivedInBaseBuffer)
  
  // CHECK-NEXT: Explicit downcast-ability is based on element type, not buffer type
  if let downcastBaseBuffer = derivedInBaseBuffer as? [Derived] {
    print("Explicit downcast-ability is based on element type, not buffer type")
  }
  else {
    print("Unexpected downcast failure")
  }

  // We can up-cast to array of AnyObject
  // CHECK-NEXT: [[derived2]]
  let derivedAsAnyObjectArray: [AnyObject] = derivedInBaseBuffer
  print(derivedAsAnyObjectArray)

  // CHECK-NEXT: downcastBackToBase = [[derived2]]
  if let downcastBackToBase = derivedAsAnyObjectArray as? [Base] {
    print("downcastBackToBase = \(downcastBackToBase)")
  }
  else {
    print("downcastBackToBase failed")
  }

  // CHECK-NEXT: downcastBackToDerived = [[derived2]]
  if let downcastBackToDerived = derivedAsAnyObjectArray as? [Derived] {
    print("downcastBackToDerived = \(downcastBackToDerived)")
  }
  else {
    print("downcastBackToDerived failed")
  }

  // CHECK-NEXT: downcastToProtocols = [[derived2]]
  if let downcastToProtocols = derivedAsAnyObjectArray as? [Fooable] {
    print("downcastToProtocols = \(downcastToProtocols)")
  } else {
    print("downcastToProtocols failed")
  }

  // CHECK-NEXT: downcastToProtocols = [[derived2]]
  if let downcastToProtocols = derivedAsAnyObjectArray as? [Barable] {
    print("downcastToProtocols = \(downcastToProtocols)")
  } else {
    print("downcastToProtocols failed")
  }

  // CHECK-NEXT: downcastToProtocols = [[derived2]]
  if let downcastToProtocols = derivedAsAnyObjectArray as? [protocol<Barable, Fooable>] {
    print("downcastToProtocols = \(downcastToProtocols)")
  } else {
    print("downcastToProtocols failed")
  }

  // CHECK-NEXT: downcastToProtocols failed
  if let downcastToProtocols = derivedAsAnyObjectArray as? [protocol<Barable, Bazable>] {
    print("downcastToProtocols = \(downcastToProtocols)")
  } else {
    print("downcastToProtocols failed")
  }

  // CHECK-NEXT: produceBridgedObjCArray([BridgedObjC[[A:#[0-9]+]](0), BridgedObjC[[B:#[0-9]+]](1), BridgedObjC[[C:#[0-9]+]](2), BridgedObjC[[D:#[0-9]+]](3), BridgedObjC[[E:#[0-9]+]](4)])
  testBridgedObjC(Thunks())
  // CHECK-NEXT: 5 elements in the array
  // CHECK-NEXT: BridgedObjC[[A]](0)
  // CHECK-NEXT: BridgedObjC[[B]](1)
  // CHECK-NEXT: BridgedObjC[[C]](2)
  // CHECK-NEXT: BridgedObjC[[D]](3)
  // CHECK-NEXT: BridgedObjC[[E]](4)

  // CHECK-NEXT: acceptBridgedObjCArray([BridgedObjC[[A:#[0-9]+]](10), BridgedObjC[[B:#[0-9]+]](11), BridgedObjC[[C:#[0-9]+]](12), BridgedObjC[[D:#[0-9]+]](13), BridgedObjC[[E:#[0-9]+]](14)])
}
testBridgedVerbatim()

//===--- Explicitly Bridged -----------------------------------------------===//
// BridgedSwift conforms to _ObjectiveCBridgeable
//===----------------------------------------------------------------------===//
func testExplicitlyBridged() {
  // CHECK-LABEL: testExplicitlyBridged()
  print("testExplicitlyBridged()")

  let bridgedSwifts = [BridgedSwift(42), BridgedSwift(17)]
  
  let bridgedSwiftsAsNSArray = bridgedSwifts as NSArray
  // CHECK-NEXT: [BridgedObjC#{{[0-9]+}}(42), BridgedObjC#{{[0-9]+}}(17)]
  print("bridgedSwiftsAsNSArray = \(bridgedSwiftsAsNSArray as [AnyObject]))")

  // Make sure we can bridge back.
  let roundTripBridgedSwifts
    = Swift._forceBridgeFromObjectiveC(bridgedSwiftsAsNSArray, 
                                       [BridgedSwift].self)
  // CHECK-NEXT-NOT: [BridgedSwift#[[id00]](42), BridgedSwift#[[id01]](17)]
  // CHECK-NEXT: [BridgedSwift#[[id10:[0-9]+]](42), BridgedSwift#[[id11:[0-9]+]](17)]
  print("roundTripBridgedSwifts = \(roundTripBridgedSwifts))")

  // Make a real Cocoa NSArray of these...
  let cocoaBridgedSwifts = NSArray(array: bridgedSwiftsAsNSArray as [AnyObject])

  // ...and bridge *that* back
  let bridgedBackSwifts
    = Swift._forceBridgeFromObjectiveC(cocoaBridgedSwifts, [BridgedSwift].self)
  // CHECK-NEXT-NOT: [BridgedSwift#[[id00]](42), BridgedSwift#[[id01]](17)]
  // CHECK-NEXT-NOT: [BridgedSwift#[[id10]](42), BridgedSwift#[[id11]](17)]
  // CHECK-NEXT: [BridgedSwift#{{[0-9]+}}(42), BridgedSwift#{{[0-9]+}}(17)]
  print("bridgedBackSwifts      = \(bridgedBackSwifts)")
  
  // all: verbatim,  not, and doesn't bridge
  // implicit conversions to/from NSArray 
  // [Base] -> [Derived] and [Derived] -> [Base] where Base can be AnyObject
  // defining @objc method taking [T] and returning [T]

  // Up-casts.
  let bridgedSwiftsAsBridgedObjCs: [BridgedObjC] = bridgedSwifts
  // CHECK-NEXT: BridgedObjC#[[ID0:[0-9]+]](42)
  print(bridgedSwiftsAsBridgedObjCs[0])
  // CHECK-NEXT: BridgedObjC#[[ID1:[0-9]+]](17)
  print(bridgedSwiftsAsBridgedObjCs[1])

  let bridgedSwiftsAsBases: [Base] = bridgedSwifts
  // CHECK-NEXT: BridgedObjC#[[ID0:[0-9]+]](42)
  print(bridgedSwiftsAsBases[0])
  // CHECK-NEXT: BridgedObjC#[[ID1:[0-9]+]](17)
  print(bridgedSwiftsAsBases[1])

  let bridgedSwiftsAsAnyObjects: [AnyObject] = bridgedSwifts
  // CHECK-NEXT: BridgedObjC#[[ID0:[0-9]+]](42)
  print(bridgedSwiftsAsAnyObjects[0])
  // CHECK-NEXT: BridgedObjC#[[ID1:[0-9]+]](17)
  print(bridgedSwiftsAsAnyObjects[1])

  // Downcasts of non-verbatim bridged value types to objects.
  if true {
    let downcasted = bridgedSwifts as [BridgedObjC]
    // CHECK-NEXT: BridgedObjC#[[ID0:[0-9]+]](42)
    print(downcasted[0])
    // CHECK-NEXT: BridgedObjC#[[ID1:[0-9]+]](17)
    print(downcasted[1])
  }

  if true {
    let downcasted = bridgedSwifts as [Base]
    // CHECK-NEXT: BridgedObjC#[[ID0:[0-9]+]](42)
    print(downcasted[0])
    // CHECK-NEXT: BridgedObjC#[[ID1:[0-9]+]](17)
    print(downcasted[1])
  }

  if true {
    let downcasted = bridgedSwifts as [AnyObject]
    // CHECK-NEXT: BridgedObjC#[[ID0:[0-9]+]](42)
    print(downcasted[0])
    // CHECK-NEXT: BridgedObjC#[[ID1:[0-9]+]](17)
    print(downcasted[1])
  }

  // Downcasts of up-casted arrays.
  if let downcasted = bridgedSwiftsAsAnyObjects as? [BridgedObjC] {
    // CHECK-NEXT: BridgedObjC#[[ID0:[0-9]+]](42)
    print(downcasted[0])
    // CHECK-NEXT: BridgedObjC#[[ID1:[0-9]+]](17)
    print(downcasted[1])
  } else {
    print("Could not downcast [AnyObject] to [BridgedObjC]?")
  }

  if let downcasted = bridgedSwiftsAsAnyObjects as? [Base] {
    // CHECK-NEXT: BridgedObjC#[[ID0:[0-9]+]](42)
    print(downcasted[0])
    // CHECK-NEXT: BridgedObjC#[[ID1:[0-9]+]](17)
    print(downcasted[1])
  } else {
    print("Could not downcast [AnyObject] to [Base]?")
  }

  // Downcast of Cocoa array to an array of classes.
  let wrappedCocoaBridgedSwifts = cocoaBridgedSwifts as [AnyObject]
  if let downcasted = wrappedCocoaBridgedSwifts as? [BridgedObjC] {
    // CHECK-NEXT: BridgedObjC#[[ID0:[0-9]+]](42)
    print(downcasted[0])
    // CHECK-NEXT: BridgedObjC#[[ID1:[0-9]+]](17)
    print(downcasted[1])
  } else {
    print("Could not downcast [AnyObject] to [BridgedObjC]?")
  }

  // Downcast of Cocoa array to an array of values.
  if let downcasted = wrappedCocoaBridgedSwifts as? [BridgedSwift] {
    // CHECK-NEXT: BridgedSwift#[[ID0:[0-9]+]](42)
    print(downcasted[0])
    // CHECK-NEXT: BridgedSwift#[[ID1:[0-9]+]](17)
    print(downcasted[1])
  } else {
    print("Could not downcast [AnyObject] to [BridgedSwift]?")
  }

  // Downcast of Cocoa array to an array of strings (which should fail)
  // CHECK-NEXT: Could not downcast [AnyObject] to [String]
  if let downcasted = wrappedCocoaBridgedSwifts as? [String] {
    print("Shouldn't be able to downcast to an array of strings")
  } else {
    print("Could not downcast [AnyObject] to [String]")
  }

  // Downcast from an implicitly unwrapped optional array of AnyObjects.
  var wrappedCocoaBridgedSwiftsIUO: [AnyObject]! = wrappedCocoaBridgedSwifts
    if let downcasted = wrappedCocoaBridgedSwiftsIUO as? [BridgedSwift] {
    // CHECK-NEXT: BridgedSwift#[[ID0:[0-9]+]](42)
    print(downcasted[0])
    // CHECK-NEXT: BridgedSwift#[[ID1:[0-9]+]](17)
    print(downcasted[1])
  } else {
    print("Could not downcast [AnyObject]! to [BridgedSwift]")
  }

  // Downcast from a nil implicitly unwrapped optional array of AnyObjects.
  wrappedCocoaBridgedSwiftsIUO = nil
  if let downcasted = wrappedCocoaBridgedSwiftsIUO as? [BridgedSwift] {
    print("Cannot downcast from a nil array!")
  } else {
    // CHECK-NEXT: Correctly rejected downcast of nil array
    print("Correctly rejected downcast of nil array")
  }

  // Downcast from an optional array of AnyObjects.
  var wrappedCocoaBridgedSwiftsOpt: [AnyObject]? = wrappedCocoaBridgedSwifts
    if let downcasted = wrappedCocoaBridgedSwiftsOpt as? [BridgedSwift] {
    // CHECK-NEXT: BridgedSwift#[[ID0:[0-9]+]](42)
    print(downcasted[0])
    // CHECK-NEXT: BridgedSwift#[[ID1:[0-9]+]](17)
    print(downcasted[1])
  } else {
    print("Could not downcast [AnyObject]! to [BridgedSwift]")
  }

  // Downcast from a nil optional array of AnyObjects.
  wrappedCocoaBridgedSwiftsOpt = nil
  if let downcasted = wrappedCocoaBridgedSwiftsOpt as? [BridgedSwift] {
    print("Cannot downcast from a nil array!")
  } else {
    // CHECK-NEXT: Correctly rejected downcast of nil array
    print("Correctly rejected downcast of nil array")
  }

  // CHECK-NEXT: produceBridgedSwiftArray([BridgedSwift[[A:#[0-9]+]](0), BridgedSwift[[B:#[0-9]+]](1), BridgedSwift[[C:#[0-9]+]](2), BridgedSwift[[D:#[0-9]+]](3), BridgedSwift[[E:#[0-9]+]](4)])
  testBridgedSwift(Thunks())
  // CHECK-NEXT: 5 elements in the array
  // CHECK-NEXT: BridgedObjC[[A:#[0-9]+]](0)
  // CHECK-NEXT: BridgedObjC[[B:#[0-9]+]](1)
  // CHECK-NEXT: BridgedObjC[[C:#[0-9]+]](2)
  // CHECK-NEXT: BridgedObjC[[D:#[0-9]+]](3)
  // CHECK-NEXT: BridgedObjC[[E:#[0-9]+]](4)

  // CHECK-NEXT: acceptBridgedSwiftArray([BridgedSwift[[A:#[0-9]+]](10), BridgedSwift[[B:#[0-9]+]](11), BridgedSwift[[C:#[0-9]+]](12), BridgedSwift[[D:#[0-9]+]](13), BridgedSwift[[E:#[0-9]+]](14)])
}
testExplicitlyBridged()

func testRoundTrip() {
  class Test : NSObject {
    @objc dynamic func call(array: NSArray) -> NSArray {

      // CHECK-NEXT: ---Passed array---
      print("---Passed array---")
      let result = array as! [BridgedSwift]
      // CHECK-NEXT: bridge operations (from, to) = (0, 0)
      BridgedSwift.printStats()


      // Clear out the stats before returning array
      BridgedSwift.resetStats()
      return result
    }
  }
  
  var test = Test()
  
  let array = [
    BridgedSwift(10), BridgedSwift(20),  BridgedSwift(30),
    BridgedSwift(40), BridgedSwift(50) ]
  
  BridgedSwift.resetStats()
  test.call(array)
  
  // CHECK-NEXT: ---Returned Array---
  print("---Returned Array---")
  // CHECK-NEXT: bridge operations (from, to) = (0, 0)  
  BridgedSwift.printStats()
}
testRoundTrip()
//===--- Non-bridging -----------------------------------------------------===//
// X is not bridged to Objective-C
//===----------------------------------------------------------------------===//

struct X {}

/*
let x: NSArray = arrayAsID(bases)!

print(x.objectAtIndex(0) as Base)
*/

func testMutableArray() {
  var m = NSMutableArray(array: ["fu", "bar", "buzz"])
  let a = m as NSArray as! [NSString]
  print(a) // CHECK-NEXT: [fu, bar, buzz]
  m.addObject("goop")
  print(a) // CHECK-NEXT: [fu, bar, buzz]
}
testMutableArray()

// CHECK-NEXT: done.
print("done.")
