// RUN: %target-parse-verify-swift

// An inout parameter can be captured.
func foo(inout x: Int) {
  func bar() -> Int {
    return x
  }
}

// But not partially applied.
func curriedFoo(inout x: Int)(y: Int) -> Int { // expected-warning{{curried function declaration syntax will be removed in a future version of Swift}}
  return x + y
}

var score: Int = 0

_ = curriedFoo(&score) // expected-error {{partial application of function with 'inout' parameters is not allowed}}
