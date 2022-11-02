// RUN: %target-parse-verify-swift

func foo() -> Int? { return .None }
func nonOptional() -> Int { return 0 }
func use(x: Int) {}
func modify(inout x: Int) {}

if let x = foo() {
  use(x)
  modify(&x) // expected-error{{cannot pass immutable value as inout argument: 'x' is a 'let' constant}}
}

use(x) // expected-error{{unresolved identifier 'x'}}

if let x = foo() {
  use(x)
  var x2 = x
  modify(&x2)
}

use(x) // expected-error{{unresolved identifier 'x'}}

if let x = nonOptional() { } // expected-error{{initializer for conditional binding must have Optional type, not 'Int'}}

class B {}
class D : B {}

// TODO poor recovery in these cases
if let {} // expected-error {{expected '{' after 'if' condition}}
if let x = {} // expected-error{{'{' after 'if'}} expected-error {{variable binding in a condition requires an initializer}}

if let x = foo() {
} else {
  // TODO: more contextual error? "x is only available on the true branch"?
  use(x) // expected-error{{unresolved identifier 'x'}}
}

if let x = foo() {
  use(x)
} else if let y = foo() {
  use(x) // expected-error{{unresolved identifier 'x'}}
  use(y)
} else {
  use(x) // expected-error{{unresolved identifier 'x'}}
  use(y) // expected-error{{unresolved identifier 'y'}}
}

var opt: Int? = .None

if let x = opt {}

// <rdar://problem/20800015> Fix error message for invalid if-let
let someInteger = 1
if let y = someInteger {}  // expected-error {{initializer for conditional binding must have Optional type, not 'Int'}}
if case let y? = someInteger {}  // expected-error {{'?' pattern cannot match values of type 'Int'}}

// Test multiple clauses on "if let".
if let x = opt, y = opt where x != y,
   let a = opt, let b = opt {
}

// Leading boolean conditional.
if 1 != 2, let x = opt, y = opt where x != y,
   let a = opt, let b = opt {
}

// <rdar://problem/20457938> typed pattern is not allowed on if/let condition
if 1 != 2, let x : Int? = opt {}


// Test error recovery.
// <rdar://problem/19939746> Improve error recovery for malformed if statements
if 1 != 2, {  // expected-error {{expected 'let' or 'var' in conditional}}
}
if 1 != 2, 4 == 57 {}   // expected-error {{expected 'let' or 'var' in conditional; use '&&' to join boolean conditions}}{{10-11= &&}}
if 1 != 2, 4 == 57, let x = opt {} // expected-error {{expected 'let' or 'var' in conditional; use '&&' to join boolean conditions}} {{10-11= &&}}

// Test that these don't cause the parser to crash.
if true { if a == 0; {} }   // expected-error {{expected '{' after 'if' condition}} expected-error 2{{}}
if a == 0, where b == 0 {}  // expected-error {{expected 'let' or 'var' in conditional; use '&&' to join boolean conditions}} {{10-11= &&}} expected-error 4{{}} expected-note {{}} {{25-25=_ = }}




func testIfCase(a : Int?) {
  if case nil = a where a != nil {}
  if case let (b?) = a where b != 42 {}
  
  if let case (b?) = a where b != 42 {}  // expected-error {{pattern matching binding is spelled with 'case let', not 'let case'}} {{6-10=}} {{14-14= let}}
  
  if a != nil, let c = a, case nil = a { _ = c}
  
  if let p? = a {_ = p}  // expected-error {{pattern matching in a condition implicitly unwraps optionals}} {{11-12=}}
  
  
  if let .Some(x) = a {_ = x}  // expected-error {{pattern matching in a condition requires the 'case' keyword}} {{6-6=case }}

  if case _ = a {}  // expected-warning {{'if' condition is always true}}
  while case _ = a {}  // expected-warning {{'while' condition is always true}}

}

// <rdar://problem/20883147> Type annotation for 'let' condition still expected to be optional
func testTypeAnnotations() {
  if let x: Int = Optional(1) {_ = x}
  if let x: Int = .Some(1) {_ = x}

  if case _ : Int8 = 19 {}  // expected-warning {{'if' condition is always true}}
}

func testShadowing(a: Int?, b: Int?, c: Int?, d: Int?) {
  guard let a = a, let b = a > 0 ? b : nil else { return }
  _ = b

  if let c = c, let d = c > 0 ? d : nil {
    _ = d
  }
}

func useInt(x: Int) {}

func testWhileScoping(a: Int?) {
  while let x = a { }
  useInt(x) // expected-error{{use of unresolved identifier 'x'}}
}

