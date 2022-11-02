// RUN: %target-parse-verify-swift

func isString(inout s: String) {}

func test_UnicodeScalarDoesNotImplementArithmetic(us: UnicodeScalar, i: Int) {
  var a1 = "a" + "b" // OK
  isString(&a1)
  let a2 = "a" - "b" // expected-error {{binary operator '-' cannot be applied to two 'String' operands}}
  // expected-note @-1 {{overloads for '-' exist with these partially matching parameter lists:}}
  let a3 = "a" * "b" // expected-error {{binary operator '*' cannot be applied to two 'String' operands}}
  // expected-note @-1 {{overloads for '*' exist with these partially matching parameter lists:}}
  let a4 = "a" / "b" // expected-error {{binary operator '/' cannot be applied to two 'String' operands}}
  // expected-note @-1 {{overloads for '/' exist with these partially matching parameter lists:}}

  let b1 = us + us // expected-error {{binary operator '+' cannot be applied to two 'UnicodeScalar' operands}}
  // expected-note @-1 {{overloads for '+' exist with these partially matching parameter lists:}}
  let b2 = us - us // expected-error {{binary operator '-' cannot be applied to two 'UnicodeScalar' operands}}
  // expected-note @-1 {{overloads for '-' exist with these partially matching parameter lists:}}
  let b3 = us * us // expected-error {{binary operator '*' cannot be applied to two 'UnicodeScalar' operands}}
  // expected-note @-1 {{overloads for '*' exist with these partially matching parameter lists:}}
  let b4 = us / us // expected-error {{binary operator '/' cannot be applied to two 'UnicodeScalar' operands}}
  // expected-note @-1 {{overloads for '/' exist with these partially matching parameter lists:}}

  let c1 = us + i // expected-error {{binary operator '+' cannot be applied to operands of type 'UnicodeScalar' and 'Int'}} expected-note{{overloads for '+' exist with these partially matching parameter lists:}}
  let c2 = us - i // expected-error {{binary operator '-' cannot be applied to operands of type 'UnicodeScalar' and 'Int'}} expected-note{{overloads for '-' exist with these partially matching parameter lists:}}
  let c3 = us * i // expected-error {{cannot convert value of type 'UnicodeScalar' to expected argument type 'Int'}}
  let c4 = us / i // expected-error {{cannot convert value of type 'UnicodeScalar' to expected argument type 'Int'}}

  let d1 = i + us // expected-error {{binary operator '+' cannot be applied to operands of type 'Int' and 'UnicodeScalar'}} expected-note{{overloads for '+' exist with these partially matching parameter lists:}}
  let d2 = i - us // expected-error {{cannot convert value of type 'UnicodeScalar' to expected argument type 'Int'}}
  let d3 = i * us // expected-error {{cannot convert value of type 'UnicodeScalar' to expected argument type 'Int'}}
  let d4 = i / us // expected-error {{cannot convert value of type 'UnicodeScalar' to expected argument type 'Int'}}
}

