// RUN: %target-parse-verify-swift

infix operator ==== {}
infix operator <<<< {}
infix operator <><> {}

// <rdar://problem/13782566>
// Check that func op<T>() parses without a space between the name and the
// generic parameter list.
func ====<T>(x: T, y: T) {}
func <<<<<T>(x: T, y: T) {}
func <><><T>(x: T, y: T) {}

//===--- Check that we recover when the parameter tuple is missing.

func recover_missing_parameter_tuple_1 { // expected-error {{expected '(' in argument list of function declaration}}
}

func recover_missing_parameter_tuple_1a // expected-error {{expected '(' in argument list of function declaration}}
{
}

func recover_missing_parameter_tuple_2<T> { // expected-error {{expected '(' in argument list of function declaration}} expected-error {{generic parameter 'T' is not used in function signature}}
}

func recover_missing_parameter_tuple_3 -> Int { // expected-error {{expected '(' in argument list of function declaration}}
}

func recover_missing_parameter_tuple_4<T> -> Int { // expected-error {{expected '(' in argument list of function declaration}} expected-error {{generic parameter 'T' is not used in function signature}}
}

//===--- Check that we recover when the function return type is missing.

// Note: Don't move braces to a different line here.
func recover_missing_return_type_1() -> // expected-error {{expected type for function result}}
{
}

func recover_missing_return_type_2() -> // expected-error {{expected type for function result}} expected-error{{expected '{' in body of function declaration}}

// Note: Don't move braces to a different line here.
func recover_missing_return_type_3 -> // expected-error {{expected '(' in argument list of function declaration}} expected-error {{expected type for function result}}
{
}

//===--- Check that we recover if ':' was used instead of '->' to specify the return type.

func recover_colon_arrow_1() : Int { } // expected-error {{expected '->' after function parameter tuple}} {{30-31=->}}
func recover_colon_arrow_2() : { }     // expected-error {{expected '->' after function parameter tuple}} {{30-31=->}} expected-error {{expected type for function result}}
func recover_colon_arrow_3 : Int { }   // expected-error {{expected '->' after function parameter tuple}} {{28-29=->}} expected-error {{expected '(' in argument list of function declaration}}
func recover_colon_arrow_4 : { }       // expected-error {{expected '->' after function parameter tuple}} {{28-29=->}} expected-error {{expected '(' in argument list of function declaration}} expected-error {{expected type for function result}}

//===--- Check that we recover if the function does not have a body, but the
//===--- context requires the function to have a body.

func recover_missing_body_1() // expected-error {{expected '{' in body of function declaration}}
func recover_missing_body_2() // expected-error {{expected '{' in body of function declaration}}
    -> Int 

// Ensure that we don't skip over the 'func g' over to the right paren in
// function g, while recovering from parse error in f() parameter tuple.  We
// should produce the error about missing right paren.
//
// FIXME: The errors are awful.  We should produce just the error about paren.
func f_recover_missing_tuple_paren(a: Int // expected-error {{expected parameter type following ':'}} expected-note {{to match this opening '('}} expected-error{{expected '{' in body of function declaration}} expected-error {{expected ')' in parameter}} expected-error 2{{expected ',' separator}} {{42-42=,}} {{42-42=,}}
func g_recover_missing_tuple_paren(b: Int) {
}

//===--- Parse errors.

func parseError1a(a: ) {} // expected-error {{type annotation missing in pattern}} expected-error {{expected parameter type following ':'}}

func parseError1b(a: // expected-error {{type annotation missing in pattern}} expected-error {{expected parameter type following ':'}}
                  ) {}

func parseError2(a: Int, b: ) {} // expected-error {{type annotation missing in pattern}} expected-error {{expected parameter type following ':'}}

func parseError3(a: unknown_type, b: ) {} // expected-error {{use of undeclared type 'unknown_type'}} expected-error {{type annotation missing in pattern}} expected-error {{expected parameter type following ':'}}

func parseError4(a: , b: ) {} // expected-error 2{{type annotation missing in pattern}} expected-error 2{{expected parameter type following ':'}}

func parseError5(a: b: ) {} // expected-error {{use of undeclared type 'b'}} expected-error 2{{expected ',' separator}} {{22-22=,}} {{22-22=,}} expected-error {{expected parameter type following ':'}}

func parseError6(a: unknown_type, b: ) {} // expected-error {{use of undeclared type 'unknown_type'}} expected-error {{type annotation missing in pattern}} expected-error {{expected parameter type following ':'}}

func parseError7(a: Int, goo b: unknown_type) {} // expected-error {{use of undeclared type 'unknown_type'}}

public func foo(a: Bool = true) -> (b: Bar, c: Bar) {} // expected-error {{use of undeclared type 'Bar'}}

// expected-error@+1{{unnamed parameters must be written}} {{24-24=_: }}
func parenPatternInArg((a): Int) -> Int { // expected-error {{use of undeclared type 'a'}} expected-error 2{{expected ',' separator}} {{27-27=,}} {{27-27=,}} expected-error {{expected parameter type following ':'}}
  return a
}
parenPatternInArg(0)

var nullaryClosure: Int -> Int = {_ in 0}
nullaryClosure(0)


// rdar://16737322 - This argument is an unnamed argument that has a labeled
// tuple type as the type.  Because the labels are in the type, they are not
// parameter labels, and they are thus not in scope in the body of the function.
// expected-error@+1{{unnamed parameters must be written}} {{27-27=_: }}
func destructureArgument( (result: Int, error: Bool) ) -> Int {
  return result  // expected-error {{use of unresolved identifier 'result'}}
}

// The former is the same as this:
func destructureArgument2(a: (result: Int, error: Bool) ) -> Int {
  return result  // expected-error {{use of unresolved identifier 'result'}}
}


class ClassWithObjCMethod {
  @objc
  func someMethod(x : Int) {}
}

func testObjCMethodCurry(a : ClassWithObjCMethod) -> (Int) -> () {
  return a.someMethod
}

// We used to crash on this.
func rdar16786220(var let c: Int) -> () { // expected-error {{Use of 'var' binding here is not allowed}} {{19-22=}} expected-error {{parameter may not have multiple 'inout', 'var', or 'let' specifiers}}
  var c = c
  c = 42
  _ = c
}


// <rdar://problem/17763388> ambiguous operator emits same candidate multiple times
infix operator !!! {}

func !!!<T>(lhs: Array<T>, rhs: Array<T>) -> Bool { return false }
func !!!<T>(lhs: UnsafePointer<T>, rhs: UnsafePointer<T>) -> Bool { return false }
[1] !!! [1]   // unambiguously picking the array overload.


// <rdar://problem/16786168> Functions currently permit 'var inout' parameters
func inout_inout_error(inout inout x : Int) {} // expected-error {{parameter may not have multiple 'inout', 'var', or 'let' specifiers}} {{30-36=}}
// expected-error@+1 {{Use of 'var' binding here is not allowed}} {{22-25=}}
func var_inout_error(var inout x : Int) { // expected-error {{parameter may not have multiple 'inout', 'var', or 'let' specifiers}} {{26-32=}}
  var x = x
  x = 2
  _ = x
}

// Unnamed parameters require the name "_":
func unnamed(Int) { } // expected-error{{unnamed parameters must be written with the empty name '_'}}{{14-14=_: }}

// Test fixits on curried functions.
func testCurryFixits() {
  func f1(x: Int)(y: Int) {} // expected-warning{{curried function declaration syntax will be removed in a future version of Swift; use a single parameter list}} {{17-19=, }}
  func f1a(x: Int, y: Int) {}
  func f2(x: Int)(y: Int)(z: Int) {} // expected-warning{{curried function declaration syntax will be removed in a future version of Swift; use a single parameter list}} {{17-19=, }} {{25-27=, }}
  func f2a(x: Int, y: Int, z: Int) {}
  func f3(x: Int)() {} // expected-warning{{curried function declaration syntax will be removed in a future version of Swift; use a single parameter list}} {{17-19=}}
  func f3a(x: Int) {}
  func f4()(x: Int) {} // expected-warning{{curried function declaration syntax will be removed in a future version of Swift; use a single parameter list}} {{11-13=}}
  func f4a(x: Int) {}
  func f5(x: Int)()(y: Int) {} // expected-warning{{curried function declaration syntax will be removed in a future version of Swift; use a single parameter list}} {{17-19=}} {{19-21=, }}
  func f5a(x: Int, y: Int) {}
}
