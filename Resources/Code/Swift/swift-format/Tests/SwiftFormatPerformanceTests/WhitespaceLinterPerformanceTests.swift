import SwiftFormatTestSupport
import SwiftFormatWhitespaceLinter
import SwiftSyntax
import SwiftParser
import XCTest

final class WhitespaceLinterPerformanceTests: DiagnosingTestCase {
  func testWhitespaceLinterPerformance() {
    let input = String(
      repeating: """
        import      SomeModule
        public   class   SomeClass : SomeProtocol
        {
        var someProperty : SomeType {
            get{5}set{doSomething()}
            }
            public
            func
            someFunctionName
            (
            firstArg    : FirstArgument , secondArg :
            SecondArgument){
          doSomeThings()
                           }}

        """,
      count: 20
    )
    let expected = String(
      repeating: """
        import SomeModule
        public class SomeClass: SomeProtocol {
          var someProperty: SomeType {
            get { 5 }
            set { doSomething() }
          }
          public func someFunctionName(
            firstArg: FirstArgument,
            secondArg: SecondArgument
          ) {
            doSomeThings()
          }
        }

        """,
      count: 20
    )

    measure { performWhitespaceLint(input: input, expected: expected) }
  }

  /// Perform whitespace linting by comparing the input text from the user with the expected
  /// formatted text, using the default configuration.
  ///
  /// - Parameters:
  ///   - input: The user's input text.
  ///   - expected: The formatted text.
  private func performWhitespaceLint(input: String, expected: String) {
    let sourceFileSyntax: SourceFileSyntax
    do {
      sourceFileSyntax = try Parser.parse(source: input)
    } catch {
      XCTFail("Parsing failed with error: \(error)")
      return
    }

    let context = makeContext(sourceFileSyntax: sourceFileSyntax)
    let linter = WhitespaceLinter(user: input, formatted: expected, context: context)
    linter.lint()
  }
}
