/*
 This source file is part of the Swift System open source project

 Copyright (c) 2020 Apple Inc. and the Swift System project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
*/

import XCTest

#if SYSTEM_PACKAGE
@testable import SystemPackage
#else
@testable import System
#endif

final class TimeTest: XCTestCase {
    
    func testTime() {
        XCTAssertEqual(Time.zero, 0)
        XCTAssertEqual(Time.min, Time(rawValue: .min))
        XCTAssertEqual(Time.max, Time(rawValue: .max))
        XCTAssertEqual(Time.init(rawValue: 1).description, "1")
    }
    
    func testMicroseconds() {
        XCTAssertEqual(Time.Microseconds.zero, 0)
        XCTAssertEqual(Time.Microseconds.min, Time.Microseconds(rawValue: .min))
        XCTAssertEqual(Time.Microseconds.max, Time.Microseconds(rawValue: .max))
        XCTAssertEqual(Time.Microseconds.init(rawValue: 1).description, "1")
    }
    
    func testNanoseconds() {
        XCTAssertEqual(Time.Nanoseconds.zero, 0)
        XCTAssertEqual(Time.Nanoseconds.min, Time.Nanoseconds(rawValue: .min))
        XCTAssertEqual(Time.Nanoseconds.max, Time.Nanoseconds(rawValue: .max))
        XCTAssertEqual(Time.Nanoseconds.init(rawValue: 1).description, "1")
    }
    
    func testProcessorTime() {
        XCTAssertNotEqual(ProcessorTime.current, 0)
        XCTAssertGreaterThan(ProcessorTime.current, 0)
        XCTAssertEqual(ProcessorTime(rawValue: 1).description, "1")
        XCTAssertEqual(ProcessorTime(seconds: 2.0), 2_000_000)
        XCTAssertEqual(ProcessorTime(seconds: 2.5).seconds, 2.5)
        XCTAssertEqual(ProcessorTime(seconds: 3.5) - ProcessorTime(seconds: 2.0), ProcessorTime(seconds: 1.5))
        XCTAssertEqual(ProcessorTime(seconds: 1.5) + ProcessorTime(seconds: 2.0), ProcessorTime(seconds: 3.5))
    }
    
    func testTimeInterval() {
        
        typealias µs = SystemPackage.TimeInterval.Microseconds
        typealias ns = SystemPackage.TimeInterval.Nanoseconds
        
        XCTAssertNoThrow({ try TimeInterval.timeInvervalSince1970() })
        XCTAssertEqual(TimeInterval(seconds: 10).description, "10s")
        XCTAssertEqual(µs(seconds: 1, microseconds: 12345).description, "1s 12345µs")
        XCTAssertEqual(ns(seconds: 1, nanoseconds: 12345).description, "1s 12345ns")
        XCTAssertEqual(Double(µs(seconds: 1, microseconds: 1000)), 1.001)
        XCTAssertEqual(Double(ns(seconds: 1, nanoseconds: 1000)), 1.000001)
        XCTAssertEqual(µs(seconds: 1.1), TimeInterval(seconds: 1) + µs(seconds: 0, microseconds: 100000))
        XCTAssertEqual(ns(seconds: 1.1), TimeInterval(seconds: 1) + ns(seconds: 0, nanoseconds: 100000000))
    }
    
    func testClock() {
        XCTAssertEqual(Clock.realtime.description, "Clock.realtime")
        XCTAssertEqual(try? Clock.realtime.precision(), .init(seconds: 0, nanoseconds: 1000))
        XCTAssertEqual(try? Clock.monotonic.precision(), .init(seconds: 0, nanoseconds: 1000))
        XCTAssertNotEqual(try? Clock.realtime.time(), .zero)
    }
    
    func testTimeComponents() {
        let timeComponents = TimeComponents(time: 1635539546)
        XCTAssertEqual(timeComponents.description, "Fri Oct 29 20:32:26 2021")
        XCTAssertEqual(timeComponents.year, 2021)
        XCTAssertEqual(timeComponents.month, .october)
        XCTAssertEqual(timeComponents.dayOfMonth, 29)
        XCTAssertEqual(timeComponents.weekday, .friday)
        XCTAssertEqual(timeComponents.hour, 20)
        XCTAssertEqual(timeComponents.minute, 32)
        XCTAssertEqual(timeComponents.second, 26)
    }
}
