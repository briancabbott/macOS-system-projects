import XCTest
@testable import Int2X

extension UInt2X where Word == UInt8  {
    var asUInt:UInt {
        return (UInt(hi) << UInt8.bitWidth) + UInt(lo)
    }
}

final class UInt2XTests: XCTestCase {
    func runShift<Q:FixedWidthInteger>(forType T:Q.Type) {
        let ua = Int2XConfig.useAccelerate ? [false, true] : [false]
        for a in [false, true] {
            if 1 < ua.count { Int2XConfig.useAccelerate = a }
            print("\(T.self) bitshift tests (Int2XConfig.useAccelerate = \(Int2XConfig.useAccelerate))")
            XCTAssertEqual(T.init(1) << T.bitWidth, 0)
            var y = T.init(1)
            for i in 0 ..< (T.bitWidth-1) {
                // print("\(T.self)(\(x)) << \(i) == \(y)")
                XCTAssertEqual(1 << i, y, "\((i, 1, y))")
                // print("\(T.self)(\(y)) >> \(i) == \(x)")
                XCTAssertEqual(y >> i, 1, "\((i, 1, y))")
                y *= 2
            }
        }
    }
    func testShiftUInt128()  { runShift(forType:UInt128.self) }
    func testShiftUInt256()  { runShift(forType:UInt256.self) }
    func testShiftUInt512()  { runShift(forType:UInt512.self) }
//    func testShiftInt1024() { runShift(forType:Int1024.self) }
    
    static var allTests = [
        ("testShiftInt128", testShiftUInt128),
        ("testShiftInt256", testShiftUInt256),
        ("testShiftInt512", testShiftUInt512),
        ]
}
