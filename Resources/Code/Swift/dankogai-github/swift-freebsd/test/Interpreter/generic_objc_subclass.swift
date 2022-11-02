// RUN: rm -rf %t
// RUN: mkdir -p %t
//
// RUN: %target-clang -fobjc-arc %S/Inputs/ObjCClasses/ObjCClasses.m -c -o %t/ObjCClasses.o
// RUN: %target-build-swift -I %S/Inputs/ObjCClasses/ -Xlinker %t/ObjCClasses.o %s -o %t/a.out
// RUN: %target-run %t/a.out | FileCheck %s
// REQUIRES: executable_test

// XFAIL: linux

// rdar://19583881

import Foundation
import ObjCClasses

@objc protocol P {
  func calculatePrice() -> Int
}

class A<T> : HasHiddenIvars, P {
  var x: Int = 16
  var t: T? = nil
  var y: Int = 61

  override var description: String {
    return "Grilled artichokes"
  }

  func calculatePrice() -> Int {
    return 400
  }
}

let a = A<Int>()

// CHECK: Grilled artichokes
// CHECK: Grilled artichokes
print(a.description)
print((a as NSObject).description)

// CHECK: 0
// CHECK: 16
// CHECK: nil
// CHECK: 61
print(a.count)
print(a.x)
print(a.t)
print(a.y)

// CHECK: 25
// CHECK: 16
// CHECK: nil
// CHECK: 61
a.count = 25
print(a.count)
print(a.x)
print(a.t)
print(a.y)

// CHECK: 25
// CHECK: 36
// CHECK: nil
// CHECK: 61
a.x = 36
print(a.count)
print(a.x)
print(a.t)
print(a.y)

// CHECK: 25
// CHECK: 36
// CHECK: 121
// CHECK: 61
a.t = 121
print(a.count)
print(a.x)
print(a.t)
print(a.y)

let aa = A<(Int, Int)>()

// CHECK: 0
// CHECK: 16
// CHECK: nil
// CHECK: 61
print(aa.count)
print(aa.x)
print(aa.t)
print(aa.y)

aa.count = 101
aa.t = (19, 84)
aa.y = 17

// CHECK: 101
// CHECK: 16
// CHECK: (19, 84)
// CHECK: 17
print(aa.count)
print(aa.x)
print(aa.t)
print(aa.y)

class B : A<(Int, Int)> {
  override var description: String {
    return "Salmon"
  }

  @nonobjc override func calculatePrice() -> Int {
    return 1675
  }
}

class C : A<(Int, Int)> {
  @nonobjc override var description: String {
    return "Invisible Chicken"
  }

  override func calculatePrice() -> Int {
    return 650
  }
}

// CHECK: 400
// CHECK: 650
print((B() as P).calculatePrice())
print((C() as P).calculatePrice())

// CHECK: Salmon
// CHECK: Grilled artichokes
print((B() as NSObject).description)
print((C() as NSObject).description)
