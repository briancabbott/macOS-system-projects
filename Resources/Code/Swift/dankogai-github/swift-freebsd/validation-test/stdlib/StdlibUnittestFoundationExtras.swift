// RUN: %target-run-simple-swift
// REQUIRES: executable_test

// REQUIRES: objc_interop

import StdlibUnittest
import Foundation
import StdlibUnittestFoundationExtras

var FoundationExtrasTests = TestSuite("FoundationExtras")

FoundationExtrasTests.test("withOverriddenNSLocaleCurrentLocale(NSLocale)") {
  // Check two locales to make sure the behavior is correct even if one of
  // these locales happens to be the same as the actual current locale.
  if true {
    let result = withOverriddenNSLocaleCurrentLocale(
      NSLocale(localeIdentifier: "en_US")) {
      () -> Int in
      expectEqual("en_US", NSLocale.currentLocale().localeIdentifier)
      return 42
    }
    expectEqual(42, result)
  }
  if true {
    let result = withOverriddenNSLocaleCurrentLocale(
      NSLocale(localeIdentifier: "uk")) {
      () -> Int in
      expectEqual("uk", NSLocale.currentLocale().localeIdentifier)
      return 42
    }
    expectEqual(42, result)
  }
}

FoundationExtrasTests.test("withOverriddenNSLocaleCurrentLocale(NSLocale)/nested") {
  withOverriddenNSLocaleCurrentLocale(
    NSLocale(localeIdentifier: "uk")) {
    () -> Void in

    expectCrashLater()

    withOverriddenNSLocaleCurrentLocale(
      NSLocale(localeIdentifier: "uk")) {
      () -> Void in

      return ()
    }
  }
}

FoundationExtrasTests.test("withOverriddenNSLocaleCurrentLocale(String)") {
  // Check two locales to make sure the behavior is correct even if one of
  // these locales happens to be the same as the actual current locale.
  if true {
    let result = withOverriddenNSLocaleCurrentLocale("en_US") {
      () -> Int in
      expectEqual("en_US", NSLocale.currentLocale().localeIdentifier)
      return 42
    }
    expectEqual(42, result)
  }
  if true {
    let result = withOverriddenNSLocaleCurrentLocale("uk") {
      () -> Int in
      expectEqual("uk", NSLocale.currentLocale().localeIdentifier)
      return 42
    }
    expectEqual(42, result)
  }
}

@_silgen_name("objc_autorelease")
func objc_autorelease(ref: AnyObject)

FoundationExtrasTests.test("objc_autorelease()") {
  autoreleasepool {
    // Check that objc_autorelease indeed autoreleases.
    objc_autorelease(LifetimeTracked(101))
    expectEqual(1, LifetimeTracked.instances)
  }
}

FoundationExtrasTests.test("autoreleasepoolIfUnoptimizedReturnAutoreleased()/autorelease") {
  autoreleasepool {
    autoreleasepoolIfUnoptimizedReturnAutoreleased {
      objc_autorelease(LifetimeTracked(103))
      expectEqual(1, LifetimeTracked.instances)
    }
  }
}

FoundationExtrasTests.test("autoreleasepoolIfUnoptimizedReturnAutoreleased()/return-autoreleased") {
  autoreleasepool {
    autoreleasepoolIfUnoptimizedReturnAutoreleased {
      let nsa = [ LifetimeTracked(104) ] as NSArray
      expectEqual(1, LifetimeTracked.instances)
      _blackHole(nsa[0])
    }
    expectEqual(0, LifetimeTracked.instances)
  }
}

runAllTests()

