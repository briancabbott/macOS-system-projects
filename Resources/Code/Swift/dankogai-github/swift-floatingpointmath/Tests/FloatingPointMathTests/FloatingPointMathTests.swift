import XCTest
@testable import FloatingPointMath

final class FloatingPointMathTests: XCTestCase {
    typealias P = FloatingPointMath & FloatingPoint & ExpressibleByFloatLiteral
    func runBasic<F:P>(forType T:F.Type) {
        let pi_4 = F(Double.pi/4)
        XCTAssertEqual(T.acos (+1.0), 0.0)
        XCTAssertEqual(T.acosh(+1.0), 0.0)
        XCTAssertEqual(T.asin (+0.0), 0.0)
        XCTAssertEqual(T.asinh(+0.0), 0.0)
        XCTAssertEqual(T.atan (+0.0), 0.0)
        XCTAssertEqual(T.atanh(+0.0), 0.0)
        XCTAssertEqual(T.cbrt (+8.0), 2.0)
        XCTAssertEqual(T.cos  (+0.0), 1.0)
        XCTAssertEqual(T.cosh (+0.0), 1.0)
        XCTAssertEqual(T.exp  (+0.0), 1.0)
        XCTAssertEqual(T.exp2 (+2.0), 4.0)
        XCTAssertEqual(T.expm1(+0.0), 0.0)
        XCTAssertEqual(T.log  (+1.0), 0.0)
        XCTAssertEqual(T.log2 (+4.0), 2.0)
        XCTAssertEqual(T.log10(+100), 2.0)
        XCTAssertEqual(T.log1p(+0.0), 0.0)
        XCTAssertEqual(T.sin  (+0.0), 0.0)
        XCTAssertEqual(T.sinh (+0.0), 0.0)
        XCTAssertEqual(T.sqrt (+4.0), 2.0)
        XCTAssertEqual(T.tan  (+0.0), 0.0)
        XCTAssertEqual(T.tanh (+0.0), 0.0)
        XCTAssertEqual(T.atan2(+1.0, +1.0), pi_4)
        XCTAssertEqual(T.hypot(+3.0, -4.0), 5.0 )
        XCTAssertEqual(T.pow  (-2.0, -2.0), 0.25)
    }

    func testDouble() { runBasic(forType: Double.self) }
    func testFloat()  { runBasic(forType: Float.self) }

    static var allTests = [
        ("testDouble", testDouble),
        ("testFloat", testFloat),
    ]
}
