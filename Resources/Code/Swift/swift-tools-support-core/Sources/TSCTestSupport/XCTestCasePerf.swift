/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See http://swift.org/LICENSE.txt for license information
 See http://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

import XCTest
import TSCBasic

/// A helper class to write performance tests for SwiftPM.
///
/// Subclasses of this will always build the tests but only run
/// run the tests when `TSC_ENABLE_PERF_TESTS` is present in the environment.
/// This is useful because we always want to be able to compile the perf tests
/// even if they are not run locally.
open class XCTestCasePerf: XCTestCase {
  #if canImport(Darwin)
    override open class var defaultTestSuite: XCTestSuite {
        if ProcessEnv.vars.keys.contains("TSC_ENABLE_PERF_TESTS") {
            return super.defaultTestSuite
        }
        return XCTestSuite(name: String(describing: type(of: self)))
    }
  #endif
}
