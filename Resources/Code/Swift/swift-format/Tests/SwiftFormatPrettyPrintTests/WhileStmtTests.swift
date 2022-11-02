final class WhileStmtTests: PrettyPrintTestCase {
  func testBasicWhileLoops() {
    let input =
      """
      while condition {
        let a = 123
        var b = "abc"
      }
      while var1, var2 {
        let a = 123
        var b = "abc"
      }
      while var123, var456 {
        let a = 123
        var b = "abc"
      }
      while condition1 && condition2 || condition3 {
        let a = 123
        var b = "abc"
      }
      """

    let expected =
      """
      while condition {
        let a = 123
        var b = "abc"
      }
      while var1, var2 {
        let a = 123
        var b = "abc"
      }
      while var123, var456
      {
        let a = 123
        var b = "abc"
      }
      while condition1
        && condition2
        || condition3
      {
        let a = 123
        var b = "abc"
      }

      """

    assertPrettyPrintEqual(input: input, expected: expected, linelength: 20)
  }

  func testLabeledWhileLoops() {
    let input =
      """
      myLabel: while condition {
        let a = 123
        var b = "abc"
      }
      myLabel: while var123, var456 {
        let a = 123
        var b = "abc"
      }
      myLabel: while condition1 && condition2 || condition3 || condition4 {
        let a = 123
        var b = "abc"
      }
      myLabel: while myVeryLongFirstCondition && condition2 || condition3
      {
        let a = 123
        var b = "abc"
      }
      """

    let expected =
      """
      myLabel: while condition {
        let a = 123
        var b = "abc"
      }
      myLabel: while var123, var456
      {
        let a = 123
        var b = "abc"
      }
      myLabel: while condition1
        && condition2 || condition3
        || condition4
      {
        let a = 123
        var b = "abc"
      }
      myLabel: while myVeryLongFirstCondition
        && condition2 || condition3
      {
        let a = 123
        var b = "abc"
      }

      """

    assertPrettyPrintEqual(input: input, expected: expected, linelength: 29)
  }

  func testWhileLoopMultipleConditionElements() {
    let input =
      """
      while x >= 0 && y >= 0 && x < foo && y < bar, let object = foo.value(at: y), let otherObject = foo.value(at: x), isEqual(a, b) {
        foo()
      }
      while x >= 0 && y >= 0
      && x < foo && y < bar,
      let object =
      foo.value(at: y),
      let otherObject = foo.value(at: x), isEqual(a, b) {
        foo()
      }
      """

    let expected =
      """
      while x >= 0 && y >= 0
        && x < foo && y < bar,
        let object = foo.value(
          at: y),
        let otherObject =
          foo.value(at: x),
        isEqual(a, b)
      {
        foo()
      }
      while x >= 0 && y >= 0
        && x < foo && y < bar,
        let object =
          foo.value(at: y),
        let otherObject =
          foo.value(at: x),
        isEqual(a, b)
      {
        foo()
      }

      """

    assertPrettyPrintEqual(input: input, expected: expected, linelength: 29)
  }
}
