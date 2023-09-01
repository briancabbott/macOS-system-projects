// RUN: %target-typecheck-verify-swift

enum MyEnum {
case test(Int)
}

func test(result: MyEnum, optResult: MyEnum?) {
  if let .co = result { // expected-error {{pattern matching in a condition requires the 'case' keyword}}
    // expected-error@-1 {{type 'MyEnum' has no member 'co'}}
  }

  if let .co(42) = result { // expected-error {{pattern matching in a condition requires the 'case' keyword}}
    // expected-error@-1 {{type of expression is ambiguous without more context}}
  }

  if let .co = optResult { // expected-error {{pattern matching in a condition requires the 'case' keyword}}
    // expected-error@-1 {{type 'MyEnum?' has no member 'co'}}
  }

  let _ = {
    if let .co = result { // expected-error 2 {{pattern matching in a condition requires the 'case' keyword}}
      // expected-error@-1 {{type 'MyEnum' has no member 'co'}}
    }
  }

  let _ = {
    if let .co = optResult { // expected-error 2 {{pattern matching in a condition requires the 'case' keyword}}
      // expected-error@-1 {{type 'MyEnum?' has no member 'co'}}
    }
  }
}
