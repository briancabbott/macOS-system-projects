// RUN: %target-run-simple-swift(-I %S/Inputs/ -Xfrontend -enable-experimental-cxx-interop -Xfrontend -validate-tbd-against-ir=none -Xfrontend -disable-llvm-verify)
//
// REQUIRES: executable_test
// TODO: This should work without ObjC interop in the future rdar://97497120
// REQUIRES: objc_interop

// REQUIRES: rdar97532642

import StdlibUnittest
import ReferenceCounted

var ReferenceCountedTestSuite = TestSuite("Foreign reference types that have reference counts")

@inline(never)
public func blackHole<T>(_ _: T) {  }

@inline(never)
func localTest() {
    var x = NS.LocalCount.create()
    expectEqual(x.value, 6) // This is 6 because of "var x" "x.value" * 2 and "(x, x, x)".

    expectEqual(x.returns42(), 42)
    expectEqual(x.constMethod(), 42)

    let t = (x, x, x)
    expectEqual(x.value, 5)
}

ReferenceCountedTestSuite.test("Local") {
    expectNotEqual(finalLocalRefCount, 0)
    localTest()
    expectEqual(finalLocalRefCount, 0)
}

var globalOptional: NS.LocalCount? = nil

ReferenceCountedTestSuite.test("Global optional holding local ref count") {
    expectEqual(finalLocalRefCount, 0)
    globalOptional = NS.LocalCount.create()
    expectEqual(finalLocalRefCount, 1)
}

@inline(never)
func globalTest1() {
    var x = GlobalCount.create()
    let t = (x, x, x)
    expectEqual(globalCount, 4)
    blackHole(t)
}

@inline(never)
func globalTest2() {
    var x = GlobalCount.create()
    expectEqual(globalCount, 1)
}

ReferenceCountedTestSuite.test("Global") {
    expectEqual(globalCount, 0)
    globalTest1()
    globalTest2()
    expectEqual(globalCount, 0)
}

runAllTests()
