import XCTest
@testable import BF

final class BFTests: XCTestCase {
    func testBF() {
        // This is an example of a functional test case.
        // Use XCTAssert and related functions to verify your tests produce the correct
        // results.
        var bf = BF("+[,<>.]-")!
        XCTAssertEqual(bf.run(input:"Hello, Swift!"), "Hello, Swift!")
    }
    static var allTests = [
        ("testBF", testBF),
    ]
}
