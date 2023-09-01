// RUN: %target-run-simple-swift(-I %S/Inputs -Xfrontend -enable-experimental-cxx-interop)
//
// REQUIRES: executable_test
// REQUIRES: OS=macosx || OS=linux-gnu

import StdlibUnittest
import CustomSequence
import Cxx

var CxxSequenceTestSuite = TestSuite("CxxConvertibleToCollection")

CxxSequenceTestSuite.test("SimpleSequence to Swift.Array") {
  let seq = SimpleSequence()
  let array = Array(seq)
  expectEqual([1, 2, 3, 4] as [Int32], array)
}

CxxSequenceTestSuite.test("SimpleSequence to Swift.Set") {
  let seq = SimpleSequence()
  let set = Set(seq)
  expectEqual(Set([1, 2, 3, 4] as [Int32]), set)
}

CxxSequenceTestSuite.test("SimpleEmptySequence to Swift.Array") {
  let seq = SimpleEmptySequence()
  let array = Array(seq)
  expectTrue(array.isEmpty)
}

CxxSequenceTestSuite.test("SimpleEmptySequence to Swift.Set") {
  let seq = SimpleEmptySequence()
  let set = Set(seq)
  expectTrue(set.isEmpty)
}

runAllTests()
