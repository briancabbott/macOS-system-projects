// RUN: %target-swift-frontend -target %target-future-triple -primary-file %s -O -sil-verify-all -Xllvm -sil-disable-pass=FunctionSignatureOpts -module-name=test -emit-ir | %FileCheck %s

// Also do an end-to-end test to check all components, including IRGen.
// RUN: %empty-directory(%t) 
// RUN: %target-build-swift -target %target-future-triple -O -Xllvm -sil-disable-pass=FunctionSignatureOpts -module-name=test %s -o %t/a.out
// RUN: %target-run %t/a.out | %FileCheck %s -check-prefix=CHECK-OUTPUT

// REQUIRES: executable_test,swift_stdlib_no_asserts,optimized_stdlib

// Check if the optimizer is able to convert array literals to constant statically initialized arrays.

// CHECK: @"$s4test11arrayLookupyS2iFTv_r" = {{.*}} constant {{.*}} @"$ss19__EmptyArrayStorageCN", {{.*}} @_swiftImmortalRefCount
// CHECK: @"$s4test11returnArraySaySiGyFTv_r" = {{.*}} constant {{.*}} @"$ss19__EmptyArrayStorageCN", {{.*}} @_swiftImmortalRefCount
// CHECK: @"$s4test9passArrayyyFTv_r" = {{.*}} constant {{.*}} @"$ss19__EmptyArrayStorageCN", {{.*}} @_swiftImmortalRefCount
// CHECK: @"$s4test9passArrayyyFTv0_r" = {{.*}} constant {{.*}} @"$ss19__EmptyArrayStorageCN", {{.*}} @_swiftImmortalRefCount
// CHECK: @"$s4test10storeArrayyyFTv_r" = {{.*}} constant {{.*}} @"$ss19__EmptyArrayStorageCN", {{.*}} @_swiftImmortalRefCount
// CHECK: @"$s4test3StrV14staticVariable_WZTv_r" = {{.*}} constant {{.*}} @"$ss19__EmptyArrayStorageCN", {{.*}} @_swiftImmortalRefCount
// CHECK-NOT: swift_initStaticObject

// UNSUPPORTED: use_os_stdlib

// Currently, constant static arrays only work on Darwin platforms.
// REQUIRES: VENDOR=apple

// REQUIRES: rdar101126543

public struct Str {
  public static let staticVariable = [ 200, 201, 202 ]
}

@inline(never)
public func arrayLookup(_ i: Int) -> Int {
  let lookupTable = [10, 11, 12]
  return lookupTable[i]
}

@inline(never)
public func returnArray() -> [Int] {
  return [20, 21]
}

@inline(never)
public func modifyArray() -> [Int] {
  var a = returnArray()
  a[1] = 27
  return a
}

public var gg: [Int]?

@inline(never)
public func receiveArray(_ a: [Int]) {
  gg = a
}

@inline(never)
public func passArray() {
  receiveArray([27, 28])
  receiveArray([29])
}

@inline(never)
public func storeArray() {
  gg = [227, 228]
}

// CHECK-OUTPUT:      [200, 201, 202]
print(Str.staticVariable)

// CHECK-OUTPUT-NEXT: 11
print(arrayLookup(1))

// CHECK-OUTPUT-NEXT: [20, 21]
print(returnArray())

// CHECK-OUTPUT-NEXT: [20, 27]
// CHECK-OUTPUT-NEXT: [20, 27]
// CHECK-OUTPUT-NEXT: [20, 27]
// CHECK-OUTPUT-NEXT: [20, 27]
// CHECK-OUTPUT-NEXT: [20, 27]
for _ in 0..<5 {
  print(modifyArray())
}

passArray()
// CHECK-OUTPUT-NEXT: [29]
print(gg!)

storeArray()
// CHECK-OUTPUT-NEXT: [227, 228]
print(gg!)

