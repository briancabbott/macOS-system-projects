// RUN: %target-run-simple-swift(-I %S/Inputs -Xfrontend -enable-experimental-cxx-interop)
//
// REQUIRES: executable_test
//
// Enable this everywhere once we have a solution for modularizing other C++ stdlibs: rdar://87654514
// REQUIRES: OS=macosx || OS=linux-gnu

import StdlibUnittest
import StdMap

var StdMapTestSuite = TestSuite("StdMap")

#if !os(Linux) // https://github.com/apple/swift/issues/61412
StdMapTestSuite.test("init") {
  let m = Map()
  expectEqual(m.size(), 0)
  expectTrue(m.empty())
}
#endif

StdMapTestSuite.test("subscript") {
  var m = initMap()
  let at1 = m[1]
  expectEqual(at1, 3)
  expectEqual(m[2], 2)
  expectEqual(m[3], 3)
}

runAllTests()
