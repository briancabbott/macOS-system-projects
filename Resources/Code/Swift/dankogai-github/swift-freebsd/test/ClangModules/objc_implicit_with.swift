// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk) -parse %s -verify

// REQUIRES: objc_interop

import AppKit

class X {
  init(withInt: Int) { }
  init(withFloat float: Float) { }
  init(withDouble d: Double) { }

  func doSomething(withDouble d: Double) { }

  init(_ withString: String) { } // not corrected
  func doSomething(withString: String) { } // not corrected
}


func testInitWith(url: String) {
  _ = NSInterestingDesignated(URL: url)
}

func testInstanceTypeFactoryMethod(queen: Bee) {
  _ = Hive(queen: queen)
  
  _ = NSObjectFactory() // okay, prefers init method
  _ = NSObjectFactory(integer: 1)
  _ = NSObjectFactory(double: 314159)
  _ = NSObjectFactory(float: 314159)
}

func testInstanceTypeFactoryMethodInherited() {
  _ = NSObjectFactorySub() // okay, prefers init method
  _ = NSObjectFactorySub(integer: 1)
  _ = NSObjectFactorySub(double: 314159)
  // FIXME: Awful diagnostic
  _ = NSObjectFactorySub(float: 314159) // expected-error{{incorrect argument label in call (have 'float:', expected 'integer:')}} {{26-31=integer}}
  let a = NSObjectFactorySub(buildingWidgets: ()) // expected-error{{argument labels '(buildingWidgets:)' do not match any available overloads}}
  // expected-note @-1 {{overloads for 'NSObjectFactorySub' exist with these partially matching parameter lists: (integer: Int), (double: Double)}}
  _ = a
}

func testNSErrorFactoryMethod(path: String) throws {
  _ = try NSString(contentsOfFile: path)
}

func testNonInstanceTypeFactoryMethod(s: String) {
  _ = NSObjectFactory(string: s) // expected-error{{argument labels '(string:)' do not match any available overloads}}
  // expected-note @-1 {{overloads for 'NSObjectFactory' exist with these partially matching parameter lists: (integer: Int), (double: Double), (float: Float)}}
}

func testUseOfFactoryMethod(queen: Bee) {
  _ = Hive.hiveWithQueen(queen) // expected-error{{'hiveWithQueen' is unavailable: use object construction 'Hive(queen:)'}}
}

func testNonsplittableFactoryMethod() {
  _ = NSObjectFactory.factoryBuildingWidgets()
}

// rdar://problem/18797808
func testFactoryMethodWithKeywordArgument() {
  let prot = NSCoding.self
  _ = NSXPCInterface(withProtocol: prot) // not "protocol:"
}
