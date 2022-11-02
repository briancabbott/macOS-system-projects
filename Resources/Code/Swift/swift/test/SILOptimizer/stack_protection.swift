// RUN: %target-swift-frontend -module-name=test -emit-sil %s -O -enable-stack-protector | %FileCheck %s

// REQUIRES: swift_in_compiler

@_silgen_name("potentiallyBadCFunction")
func potentiallyBadCFunction(_ arg: UnsafePointer<Int>)

// CHECK-LABEL: sil [stack_protection] @$s4test20overflowInCFunction1yyF
// CHECK-NOT:     copy_addr
// CHECK:       } // end sil function '$s4test20overflowInCFunction1yyF'
public func overflowInCFunction1() {
  var x = 0
  withUnsafeMutablePointer(to: &x) {
    potentiallyBadCFunction($0)
  }
}

// CHECK-LABEL: sil @$s4test19unprotectedOverflowyyF
// CHECK-NOT:     copy_addr
// CHECK:       } // end sil function '$s4test19unprotectedOverflowyyF'
public func unprotectedOverflow() {
  var x = 0
  _withUnprotectedUnsafeMutablePointer(to: &x) {
    potentiallyBadCFunction($0)
  }
}

// CHECK-LABEL: sil [stack_protection] @$s4test23overflowWithUnsafeBytesyyF
// CHECK-NOT:     copy_addr
// CHECK:       } // end sil function '$s4test23overflowWithUnsafeBytesyyF'
public func overflowWithUnsafeBytes() {
  var x = 0
  withUnsafeBytes(of: &x) {
    potentiallyBadCFunction($0.bindMemory(to: Int.self).baseAddress!)
  }
}

// CHECK-LABEL: sil @$s4test22unprotectedUnsafeBytesyyF
// CHECK-NOT:     copy_addr
// CHECK:       } // end sil function '$s4test22unprotectedUnsafeBytesyyF'
public func unprotectedUnsafeBytes() {
  var x = 0
  _withUnprotectedUnsafeBytes(of: &x) {
    potentiallyBadCFunction($0.bindMemory(to: Int.self).baseAddress!)
  }
}

// CHECK-LABEL: sil [stack_protection] @$s4test20overflowInCFunction2yyF
// CHECK-NOT:     copy_addr
// CHECK:       } // end sil function '$s4test20overflowInCFunction2yyF'
public func overflowInCFunction2() {
  var x = 0
  potentiallyBadCFunction(&x)
}

// CHECK-LABEL: sil hidden [noinline] @$s4test20inoutWithKnownCalleryySizF
// CHECK-NOT:     copy_addr
// CHECK:       } // end sil function '$s4test20inoutWithKnownCalleryySizF'
@inline(never)
func inoutWithKnownCaller(_ x: inout Int) {
  withUnsafeMutablePointer(to: &x) {
    $0[1] = 0
  }
}

// CHECK-LABEL: sil [stack_protection] @$s4test24callOverflowInoutPointeryyF
// CHECK-NOT:     copy_addr
// CHECK:       } // end sil function '$s4test24callOverflowInoutPointeryyF'
public func callOverflowInoutPointer() {
  var x = 27
  inoutWithKnownCaller(&x)
}

// CHECK-LABEL: sil [stack_protection] @$s4test22inoutWithUnknownCalleryySizF
// CHECK:         copy_addr [take] {{.*}} to [initialization]
// CHECK:         copy_addr [take] {{.*}} to [initialization]
// CHECK:       } // end sil function '$s4test22inoutWithUnknownCalleryySizF'
public func inoutWithUnknownCaller(_ x: inout Int) {
  withUnsafeMutablePointer(to: &x) {
    $0[1] = 0
  }
}

// CHECK-LABEL: sil [stack_protection] @$s4test0A29WithUnsafeTemporaryAllocationyyF
// CHECK-NOT:     copy_addr
// CHECK:       } // end sil function '$s4test0A29WithUnsafeTemporaryAllocationyyF'
public func testWithUnsafeTemporaryAllocation() {
  withUnsafeTemporaryAllocation(of: Int.self, capacity: 10) {
    potentiallyBadCFunction($0.baseAddress!)
  }
}

