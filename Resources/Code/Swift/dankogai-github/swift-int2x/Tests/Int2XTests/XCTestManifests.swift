import XCTest

#if !os(macOS)
public func allTests() -> [XCTestCaseEntry] {
    return [
        testCase(Int2XTests.allTests),
        // testCase(UInt2XTests.allTests),
    ]
}
#endif
