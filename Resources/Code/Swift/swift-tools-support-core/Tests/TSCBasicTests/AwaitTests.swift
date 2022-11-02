/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See http://swift.org/LICENSE.txt for license information
 See http://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

import XCTest
import Dispatch

import TSCBasic

class AwaitTests: XCTestCase {

    enum DummyError: Error {
        case error
    }

    func async(_ param: String, _ completion: @escaping (Result<String, Error>) -> Void) {
        DispatchQueue.global().async {
            completion(.success(param))
        }
    }

    func throwingAsync(_ param: String, _ completion: @escaping (Result<String, Error>) -> Void) {
        DispatchQueue.global().async {
            completion(.failure(DummyError.error))
        }
    }

    func testBasics() throws {
        let value = try tsc_await { async("Hi", $0) }
        XCTAssertEqual("Hi", value)

        do {
            let value = try tsc_await { throwingAsync("Hi", $0) }
            XCTFail("Unexpected success \(value)")
        } catch {
            XCTAssertEqual(error as? DummyError, DummyError.error)
        }
    }
}
