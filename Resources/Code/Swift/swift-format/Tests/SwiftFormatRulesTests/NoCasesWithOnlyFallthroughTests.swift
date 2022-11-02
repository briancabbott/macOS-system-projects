import SwiftFormatRules

final class NoCasesWithOnlyFallthroughTests: LintOrFormatRuleTestCase {
  func testFallthroughCases() {
    XCTAssertFormatting(
      NoCasesWithOnlyFallthrough.self,
      input: """
        switch numbers {
        case 1: print("one")
        case 2: fallthrough
        case 3: fallthrough
        case 4: print("two to four")
        case 5: fallthrough
        case 7: print("five or seven")
        default: break
        }
        switch letters {
        case "a": fallthrough
        case "b", "c": fallthrough
        case "d": print("abcd")
        case "e": print("e")
        case "f": fallthrough
        case "z": print("fz")
        default: break
        }
        switch tokens {
        case .comma: print(",")
        case .rightBrace: fallthrough
        case .leftBrace: fallthrough
        case .braces: print("{}")
        case .period: print(".")
        case .empty: fallthrough
        default: break
        }
        """,
      expected: """
        switch numbers {
        case 1: print("one")
        case 2, 3, 4: print("two to four")
        case 5, 7: print("five or seven")
        default: break
        }
        switch letters {
        case "a", "b", "c", "d": print("abcd")
        case "e": print("e")
        case "f", "z": print("fz")
        default: break
        }
        switch tokens {
        case .comma: print(",")
        case .rightBrace, .leftBrace, .braces: print("{}")
        case .period: print(".")
        case .empty: fallthrough
        default: break
        }
        """,
      checkForUnassertedDiagnostics: true)

    XCTAssertDiagnosed(.collapseCase(name: "2"), line: 3, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "3"), line: 4, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "5"), line: 6, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "\"a\""), line: 11, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "\"b\", \"c\""), line: 12, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "\"f\""), line: 15, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: ".rightBrace"), line: 21, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: ".leftBrace"), line: 22, column: 1)
  }

  func testFallthroughCasesWithCommentsAreNotCombined() {
    XCTAssertFormatting(
      NoCasesWithOnlyFallthrough.self,
      input: """
        switch numbers {
        case 1:
          return 0 // This return has an inline comment.
        case 2: fallthrough
        // This case is commented so it should stay.
        case 3:
          fallthrough
        case 4:
          // This fallthrough is commented so it should stay.
          fallthrough
        case 5: fallthrough  // This fallthrough is relevant.
        case 6:
          fallthrough
        // This case has a descriptive comment.
        case 7: print("got here")
        }
        """,
      expected: """
        switch numbers {
        case 1:
          return 0 // This return has an inline comment.
        // This case is commented so it should stay.
        case 2, 3:
          fallthrough
        case 4:
          // This fallthrough is commented so it should stay.
          fallthrough
        case 5: fallthrough  // This fallthrough is relevant.
        // This case has a descriptive comment.
        case 6, 7: print("got here")
        }
        """,
      checkForUnassertedDiagnostics: true)

    XCTAssertDiagnosed(.collapseCase(name: "2"), line: 4, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "6"), line: 12, column: 1)
  }

  func testCommentsAroundCombinedCasesStayInPlace() {
    XCTAssertFormatting(
      NoCasesWithOnlyFallthrough.self,
      input: """
        switch numbers {
        case 5:
          return 42 // This return is important.
        case 6: fallthrough
        // This case has an important comment.
        case 7: print("6 to 7")
        case 8: fallthrough

        // This case has an extra leading newline for emphasis.
        case 9: print("8 to 9")
        }
        """,
      expected: """
        switch numbers {
        case 5:
          return 42 // This return is important.
        // This case has an important comment.
        case 6, 7: print("6 to 7")

        // This case has an extra leading newline for emphasis.
        case 8, 9: print("8 to 9")
        }
        """,
      checkForUnassertedDiagnostics: true)

    XCTAssertDiagnosed(.collapseCase(name: "6"), line: 4, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "8"), line: 7, column: 1)
  }

  func testNestedSwitches() {
    XCTAssertFormatting(
      NoCasesWithOnlyFallthrough.self,
      input: """
        switch x {
        case 1: fallthrough
        case 2: fallthrough
        case 3:
          switch y {
          case 1: fallthrough
          case 2: print(2)
          }
        case 4:
          switch y {
          case 1: fallthrough
          case 2: print(2)
          }
        }
        """,
      expected: """
        switch x {
        case 1, 2, 3:
          switch y {
          case 1, 2: print(2)
          }
        case 4:
          switch y {
          case 1, 2: print(2)
          }
        }
        """,
      checkForUnassertedDiagnostics: true)

    XCTAssertDiagnosed(.collapseCase(name: "1"), line: 2, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "2"), line: 3, column: 1)
    // TODO: Column 9 seems wrong here; it should be 3. Look into this.
    XCTAssertDiagnosed(.collapseCase(name: "1"), line: 6, column: 9)
    XCTAssertDiagnosed(.collapseCase(name: "1"), line: 11, column: 3)
  }

  func testCasesInsideConditionalCompilationBlock() {
    XCTAssertFormatting(
      NoCasesWithOnlyFallthrough.self,
      input: """
        switch x {
        case 1: fallthrough
        #if FOO
        case 2: fallthrough
        case 3: print(3)
        case 4: print(4)
        #endif
        case 5: fallthrough
        case 6: print(6)
        #if BAR
        #if BAZ
        case 7: print(7)
        case 8: fallthrough
        #endif
        case 9: fallthrough
        #endif
        case 10: print(10)
        }
        """,
      expected: """
        switch x {
        case 1: fallthrough
        #if FOO
        case 2, 3: print(3)
        case 4: print(4)
        #endif
        case 5, 6: print(6)
        #if BAR
        #if BAZ
        case 7: print(7)
        case 8: fallthrough
        #endif
        case 9: fallthrough
        #endif
        case 10: print(10)
        }
        """,
      checkForUnassertedDiagnostics: true)

    XCTAssertDiagnosed(.collapseCase(name: "2"), line: 4, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "5"), line: 8, column: 1)
  }

  func testCasesWithWhereClauses() {
    // As noted in the rule implementation, the formatted result should include a newline before any
    // case items that have `where` clauses if they follow any case items that do not, to avoid
    // compiler warnings. This is handled by the pretty printer, not this rule.
    XCTAssertFormatting(
      NoCasesWithOnlyFallthrough.self,
      input: """
        switch x {
        case 1 where y < 0: fallthrough
        case 2 where y == 0: fallthrough
        case 3 where y < 0: fallthrough
        case 4 where y != 0: print(4)
        case 5: fallthrough
        case 6: fallthrough
        case 7: fallthrough
        case 8: fallthrough
        case 9: fallthrough
        case 10 where y == 0: print(10)
        default: print("?")
        }
        """,
      expected: """
        switch x {
        case 1 where y < 0, 2 where y == 0, 3 where y < 0, 4 where y != 0: print(4)
        case 5, 6, 7, 8, 9, 10 where y == 0: print(10)
        default: print("?")
        }
        """,
      checkForUnassertedDiagnostics: true)

    XCTAssertDiagnosed(.collapseCase(name: "1 where y < 0"), line: 2, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "2 where y == 0"), line: 3, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "3 where y < 0"), line: 4, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "5"), line: 6, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "6"), line: 7, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "7"), line: 8, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "8"), line: 9, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: "9"), line: 10, column: 1)
  }

  func testCasesWithValueBindingsAreNotMerged() {
    XCTAssertFormatting(
      NoCasesWithOnlyFallthrough.self,
      input: """
        switch x {
        case .a: fallthrough
        case .b: fallthrough
        case .c(let x): fallthrough
        case .d(let y): fallthrough
        case .e: fallthrough
        case .f: fallthrough
        case (let g, let h): fallthrough
        case .i: fallthrough
        case .j?: fallthrough
        case let k as K: fallthrough
        case .l: break
        }
        """,
      expected: """
        switch x {
        case .a, .b: fallthrough
        case .c(let x): fallthrough
        case .d(let y): fallthrough
        case .e, .f: fallthrough
        case (let g, let h): fallthrough
        case .i, .j?: fallthrough
        case let k as K: fallthrough
        case .l: break
        }
        """,
      checkForUnassertedDiagnostics: true)

    XCTAssertDiagnosed(.collapseCase(name: ".a"), line: 2, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: ".e"), line: 6, column: 1)
    XCTAssertDiagnosed(.collapseCase(name: ".i"), line: 9, column: 1)
  }

  func testFallthroughOnlyCasesAreNotMergedWithDefault() {
    XCTAssertFormatting(
      NoCasesWithOnlyFallthrough.self,
      input: """
        switch x {
        case .a: fallthrough
        case .b: fallthrough
        default: print("got here")
        }
        """,
      expected: """
        switch x {
        case .a, .b: fallthrough
        default: print("got here")
        }
        """,
      checkForUnassertedDiagnostics: true)

    XCTAssertDiagnosed(.collapseCase(name: ".a"), line: 2, column: 1)
  }

  func testFallthroughOnlyCasesAreNotMergedWithUnknownDefault() {
    XCTAssertFormatting(
      NoCasesWithOnlyFallthrough.self,
      input: """
        switch x {
        case .a: fallthrough
        case .b: fallthrough
        @unknown default: print("got here")
        }
        """,
      expected: """
        switch x {
        case .a, .b: fallthrough
        @unknown default: print("got here")
        }
        """,
      checkForUnassertedDiagnostics: true)

    XCTAssertDiagnosed(.collapseCase(name: ".a"), line: 2, column: 1)
  }
}
