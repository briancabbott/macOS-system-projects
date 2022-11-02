//
//  TAPTests.swift
//  tap
//
//  Created by Dan Kogai on 1/21/16.
//  Copyright © 2016-2018 Dan Kogai. All rights reserved.
//
import XCTest
@testable import TAP

class TAPTests: XCTestCase {
    func test() {
        let test = TAP(tests:12)
        XCTAssertTrue(test.ok(42+0.195 == 42.195,     "42 + 0.195 == 42.195"))
        XCTAssertTrue(test.eq(42+0.195,   42.195,     "42 + 0.195 is 42.195"))
        XCTAssertTrue(test.eq([42,0.195],[42,0.195],  "[42,0.195] is [42,0.195]"))
        XCTAssertTrue(test.eq([42:0.195],[42:0.195],  "[42:0.195] is [42,0:195]"))
        XCTAssertTrue(test.ne(42+0.195,   42.0,       "42 + 0.195 is not 42"))
        XCTAssertTrue(test.ne([42,0.195],[42,0.0],    "[42,0.195] is not [42,0.0]"))
        XCTAssertTrue(test.ne([42:0.195],[42:0.0],    "[42:0.195] is not [42:0.0]"))
        let optionalNum: Int? = 1
        let num: Int = 2
        XCTAssertTrue(test.eq(optionalNum, num-1,       "Optional(1) == 2 - 1"))
        XCTAssertTrue(test.eq(num-1, optionalNum,       "2 - 1 == Optional(1)"))
        XCTAssertTrue(test.eq(Optional<Int>.none, Optional<Int>.none, "Optional(nil) == Optional(nil)"))
        XCTAssertTrue(test.ne(nil, num-1,               "Optional(nil) is not 2 - 1"))
        XCTAssertTrue(test.ne(num-1, nil,               "2 - 1 is not Optional(nil)"))
        test.done(dontExit:true)
    }
    
    static var allTests = [
        ("Test the tests.", test)
    ]
}


