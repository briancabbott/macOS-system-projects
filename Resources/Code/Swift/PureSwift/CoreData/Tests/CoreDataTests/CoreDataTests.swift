import XCTest
@testable import CoreData

final class CoreDataTests: XCTestCase {
    func testExample() {
        // This is an example of a functional test case.
        // Use XCTAssert and related functions to verify your tests produce the correct
        // results.
        XCTAssertEqual(CoreData().text, "Hello, World!")
    }

    static var allTests = [
        ("testExample", testExample),
    ]
}
