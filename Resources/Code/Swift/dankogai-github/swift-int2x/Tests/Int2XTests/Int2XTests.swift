import XCTest
@testable import Int2X

final class Int2XTests: XCTestCase {
    func runBasic<Q:FixedWidthInteger>(forType T:Q.Type) {
        let ua = Int2XConfig.useAccelerate ? [false, true] : [false]
        for a in ua {
            if 1 < ua.count {
                #if os(macOS) || os(iOS)
                Int2XConfig.useAccelerate = a
                #endif
            }
            XCTAssertEqual(T.min + T.max, T.init(-1))
            for s in [-1, +1] {
                var x = s * Int.max
                for _ in 0 ..< Int.bitWidth {
                    // not XCTAssertEqual because types differ
                    XCTAssert(T.init(x) == x)
                    if !x.addingReportingOverflow(x).overflow {
                        XCTAssert(T.init(x) + T.init(x) == x + x)
                    }
                    if !x.subtractingReportingOverflow(x).overflow {
                        XCTAssert(T.init(x) - T.init(x) == x - x)
                    }
                    if !x.multipliedReportingOverflow(by: x).overflow {
                        XCTAssert(T.init(x) * T.init(x) == x * x)
                    }
                    if x != 0 {
                        XCTAssert(T.init(x) /  T.init(x) == x / x)
                    }
                    x >>= 1
                }
            }
        }
    }
    func testBasicInt128()  { runBasic(forType:Int128.self) }
    func testBasicInt256()  { runBasic(forType:Int256.self) }
    func testBasicInt512()  { runBasic(forType:Int512.self) }
    func testBasicInt1024() { runBasic(forType:Int1024.self) }

    func runShift<Q:FixedWidthInteger>(forType T:Q.Type) {
        let ua = Int2XConfig.useAccelerate ? [false, true] : [false]
        for a in ua {
            if 1 < ua.count {
                #if os(macOS) || os(iOS)
                Int2XConfig.useAccelerate = a
                #endif
            }
            print("\(T.self) bitshift tests: useAccelerate = \(Int2XConfig.useAccelerate)")
            for x in [T.init(-1), T.init(+1)] {
                XCTAssertEqual(x << T.bitWidth, 0)
                var y = x
                for i in 0 ..< (T.bitWidth-1) {
                    // print("\(T.self)(\(x)) << \(i) == \(y)")
                    XCTAssertEqual(x << i, y, "\((i, x, y))")
                    // print("\(T.self)(\(y)) >> \(i) == \(x)")
                    XCTAssertEqual(y >> i, x, "\((i, x, y))")
                    y *= 2
                }
                XCTAssertEqual(y >> T.bitWidth, T.init(-1))
            }
        }
    }
    func testShiftInt128()  { runShift(forType:Int128.self) }
    func testShiftInt256()  { runShift(forType:Int256.self) }
    func testShiftInt512()  { runShift(forType:Int512.self) }
    // func testShiftInt1024() { runShift(forType:Int1024.self) }

    static var allTests = [
        ("testBasicInt128", testBasicInt128),
        ("testBasicInt256", testBasicInt256),
        ("testBasicInt512", testBasicInt512),
        ("testBasicInt1024", testBasicInt1024),
        ("testShiftInt128", testShiftInt128),
        ("testShiftInt256", testShiftInt256),
        ("testShiftInt512", testShiftInt512),
    ]
}
