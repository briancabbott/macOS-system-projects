// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk) -parse %s -verify

// REQUIRES: objc_interop

import Foundation

func testDictionary() {
  // -[NSDictionary init] returns non-nil.
  var dictNonOpt = NSDictionary()
  if dictNonOpt == nil { } // expected-error{{value of type 'NSDictionary' can never be nil, comparison isn't allowed}}
}

func testString() throws {
  // Optional
  let stringOpt = NSString(path: "blah", encoding: 0)
  _ = stringOpt as NSString // expected-error{{value of optional type 'NSString?' not unwrapped; did you mean to use '!' or '?'?}}

  // Implicitly unwrapped optional
  let stringIUO = NSString(path: "blah")
  if stringIUO == nil { }
  _ = stringIUO as NSString
}

func testHive() {
  let hiveIUO = Hive()
  if hiveIUO == nil { }
  _ = hiveIUO as Hive
}
