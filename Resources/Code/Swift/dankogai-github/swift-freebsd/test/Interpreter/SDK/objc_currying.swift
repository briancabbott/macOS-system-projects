// RUN: %target-run-simple-swift | FileCheck %s
// REQUIRES: executable_test

// REQUIRES: objc_interop
import Foundation

func foo(f: (String) -> String) {
  print(f(", "))
  print(f(" • "))
}

let x: NSArray = ["foo", "bar", "bas"]
foo(x.componentsJoinedByString)
// CHECK: foo, bar, bas
// CHECK: foo • bar • bas

let a: AnyObject = x
if let componentsJoinedByString = a.componentsJoinedByString {
  foo(componentsJoinedByString)
}
// CHECK: foo, bar, bas
// CHECK: foo • bar • bas

