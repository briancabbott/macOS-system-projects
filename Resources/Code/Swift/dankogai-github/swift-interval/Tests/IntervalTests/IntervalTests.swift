import XCTest
@testable import Interval

final class IntervalTests: XCTestCase {
    typealias D = Double
    typealias I = Interval
    func testBasic() {
        // This is an example of a functional test case.
        // Use XCTAssert and related functions to verify your tests produce the correct
        // results.
        XCTAssertEqual(I(1.0).min, 1.0-Double.ulpOfOne)
        XCTAssertEqual(I(1.0).max, 1.0+Double.ulpOfOne)
        XCTAssertEqual(I(1.0) + I(1.0), I(D(2.0)))
        XCTAssertEqual(I(1.0) - I(1.0), I(D(0.0)))
        XCTAssertEqual(I(1.0) * I(1.0), I(D(1.0)))
        XCTAssertEqual(I(1.0) / I(1.0), I(D(1.0)))
    }
    static var allTests = [
        ("testBasic", testBasic),
    ]
}
