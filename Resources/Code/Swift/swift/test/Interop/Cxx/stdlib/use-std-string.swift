// RUN: %target-run-simple-swift(-I %S/Inputs -Xfrontend -enable-experimental-cxx-interop)
// RUN: %target-run-simple-swift(-D USE_CXXSTDLIB_SPELLING -I %S/Inputs -Xfrontend -enable-experimental-cxx-interop)
//
// REQUIRES: executable_test
//
// Enable this everywhere once we have a solution for modularizing other C++ stdlibs: rdar://87654514
// REQUIRES: OS=macosx || OS=linux-gnu

import StdlibUnittest
import StdString
#if os(Linux)
#if USE_CXXSTDLIB_SPELLING
import CxxStdlib
#else
import std
#endif
// FIXME: import CxxStdlib.string once libstdc++ is split into submodules.
#else
#if USE_CXXSTDLIB_SPELLING
import CxxStdlib.string
#else
import std.string
#endif
#endif

var StdStringTestSuite = TestSuite("StdString")

StdStringTestSuite.test("init") {
    let s = CxxString()
    expectEqual(s.size(), 0)
    expectTrue(s.empty())
}

StdStringTestSuite.test("push back") {
    var s = CxxString()
    s.push_back(42)
    expectEqual(s.size(), 1)
    expectFalse(s.empty())
    expectEqual(s[0], 42)
}

runAllTests()
