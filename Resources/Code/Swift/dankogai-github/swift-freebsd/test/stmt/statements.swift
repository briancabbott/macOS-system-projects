// RUN: %target-parse-verify-swift

/* block comments */
/* /* nested too */ */

func markUsed<T>(t: T) {}

func f1(a: Int, _ y: Int) {}
func f2() {}
func f3() -> Int {}

func invalid_semi() {
  ; // expected-error {{';' statements are not allowed}} {{3-5=}}
}

func nested1(x: Int) {
  var y : Int
  
  func nested2(z: Int) -> Int {
    return x+y+z
  }
  
  nested2(1)
}

func funcdecl5(a: Int, y: Int) {
  var x : Int

  // a few statements
  if (x != 0) {
    if (x != 0 || f3() != 0) {
      // while with and without a space after it.
      while(true) { 4; 2; 1 }
      while (true) { 4; 2; 1 }
    }
  }

  // Assignment statement.
  x = y
  (x) = y

  1 = x        // expected-error {{cannot assign to a literal value}}
  (1) = x      // expected-error {{cannot assign to a literal value}}
  (x:1).x = 1 // expected-error {{cannot assign to immutable expression of type 'Int'}}
  var tup : (x:Int, y:Int)
  tup.x = 1
  _ = tup

  let B : Bool

  // if/then/else.
  if (B) {
  } else if (y == 2) {
  }

  // FIXME: This diagnostic is terrible - rdar://12939553
  if x {}   // expected-error {{type 'Int' does not conform to protocol 'BooleanType'}}

  if true {
    if (B) {
    } else {
    }
  }

  if (B) {
    f1(1,2)
  } else {
    f2()
  }

  if (B) {
    if (B) {
      f1(1,2)
    } else {
      f2()
    }
  } else {
    f2()
  }
  
  // while statement.
  while (B) {
  }

  // It's okay to leave out the spaces in these.
  while(B) {}
  if(B) {}
}

struct infloopbool {
  var boolValue: infloopbool {
    return self
  }
}

func infloopbooltest() {
  if (infloopbool()) {} // expected-error {{type 'infloopbool' does not conform to protocol 'BooleanType'}}
}

// test "builder" API style
extension Int {
  static func builder() -> Int { }
  var builderProp: Int { return 0 }
  func builder2() {}
}
Int
  .builder()
  .builderProp
  .builder2()

struct SomeGeneric<T> {
  static func builder() -> SomeGeneric<T> { }
  var builderProp: SomeGeneric<T> { return .builder() }
  func builder2() {}
}
SomeGeneric<Int>
  .builder()
  .builderProp
  .builder2()

func for_loop() {
  var x = 0
  for ;; { }
  for x = 1; x != 42; ++x { }
  for infloopbooltest(); x != 12; infloopbooltest() {}
  
  for ; { } // expected-error {{expected ';' in 'for' statement}}
  
  for var y = 1; y != 42; ++y {}
  for (var y = 1; y != 42; ++y) {}
  var z = 10
  for (; z != 0; --z) {}
  for (z = 10; z != 0; --z) {}
  for var (a,b) = (0,12); a != b; --b {++a}
  for (var (a,b) = (0,12); a != b; --b) {++a}
  var j, k : Int
  for ((j,k) = (0,10); j != k; --k) {}
  for var i = 0, j = 0; i * j < 10; i++, j++ {}
  for j = 0, k = 52; j < k; ++j, --k { }
  // rdar://19540536
  // expected-error@+4{{expected var declaration in a 'for' statement}}
  // expected-error@+3{{expression resolves to an unused function}}
  // expected-error@+2{{expected an attribute name}}
  // expected-error@+1{{braced block of statements is an unused closure}}
  for @ {}

  // <rdar://problem/17462274> Is increment in for loop optional?
  for (let i = 0; i < 10; ) {}
}

break // expected-error {{'break' is only allowed inside a loop, if, do, or switch}}
continue // expected-error {{'continue' is only allowed inside a loop}}
while true {
  func f() {
    break // expected-error {{'break' is only allowed inside a loop}}
    continue // expected-error {{'continue' is only allowed inside a loop}}
  }

  // Labeled if
  MyIf: if 1 != 2 {
    break MyIf
    continue MyIf  // expected-error {{'continue' cannot be used with if statements}}
    break          // break the while
    continue       // continue the while.
  }
}

// Labeled if
MyOtherIf: if 1 != 2 {
  break MyOtherIf
  continue MyOtherIf  // expected-error {{'continue' cannot be used with if statements}}
  break          // expected-error {{unlabeled 'break' is only allowed inside a loop or switch, a labeled break is required to exit an if}}
  continue       // expected-error {{'continue' is only allowed inside a loop}}
}

do {
  break  // expected-error {{unlabeled 'break' is only allowed inside a loop or switch, a labeled break is required to exit an if or do}}
}

func tuple_assign() {
  var a,b,c,d : Int
  (a,b) = (1,2)
  func f() -> (Int,Int) { return (1,2) }
  ((a,b), (c,d)) = (f(), f())
}

func missing_semicolons() {
  var w = 321
  func g() {}
  g() ++w             // expected-error{{consecutive statements}} {{6-6=;}}
  var z = w"hello"    // expected-error{{consecutive statements}} {{12-12=;}}
  class  C {}class  C2 {} // expected-error{{consecutive statements}} {{14-14=;}}
  struct S {}struct S2 {} // expected-error{{consecutive statements}} {{14-14=;}}
  func j() {}func k() {}  // expected-error{{consecutive statements}} {{14-14=;}}
}

//===--- Return statement.

return 42 // expected-error {{return invalid outside of a func}}

return // expected-error {{return invalid outside of a func}}

func NonVoidReturn1() -> Int {
  return // expected-error {{non-void function should return a value}}
}

func NonVoidReturn2() -> Int {
  return + // expected-error {{unary operator cannot be separated from its operand}} {{11-1=}} expected-error {{expected expression in 'return' statement}}
}

func VoidReturn1() {
  if true { return }
  // Semicolon should be accepted -- rdar://11344875
  return; // no-error
}

func VoidReturn2() {
  return () // no-error
}

func VoidReturn3() {
  return VoidReturn2() // no-error
}

//===--- If statement.

func IfStmt1() {
  if 1 > 0 // expected-error {{expected '{' after 'if' condition}}
  _ = 42
}

func IfStmt2() {
  if 1 > 0 {
  } else // expected-error {{expected '{' after 'else'}}
  _ = 42
}

//===--- While statement.

func WhileStmt1() {
  while 1 > 0 // expected-error {{expected '{' after 'while' condition}}
  _ = 42
}

//===-- Do statement.
func DoStmt() {
  // This is just a 'do' statement now.
  do {
  }
}

func DoWhileStmt() {
  do { // expected-error {{'do-while' statement is not allowed; use 'repeat-while' instead}} {{3-5=repeat}}
  } while true
}

//===--- Repeat-while statement.

func RepeatWhileStmt1() {
  repeat {} while true

  repeat {} while false

  repeat { break } while true
  repeat { continue } while true
}

func RepeatWhileStmt2() {
  repeat // expected-error {{expected '{' after 'repeat'}}
}

func RepeatWhileStmt4() {
  repeat {
  } while + // expected-error {{unary operator cannot be separated from its operand}} {{12-1=}} expected-error {{expected expression in 'repeat-while' condition}}
}

func brokenSwitch(x: Int) -> Int {
  switch x {
  case .Blah(let rep): // expected-error{{enum case 'Blah' not found in type 'Int'}}
    return rep
  }
}

func breakContinue(x : Int) -> Int {

Outer:
  for _ in 0...1000 {

  Switch:
    switch x {
    case 42: break Outer
    case 97: continue Outer
    case 102: break Switch
    case 13: continue
    case 139: break   // <rdar://problem/16563853> 'break' should be able to break out of switch statements
    }
  }
  
  // <rdar://problem/16692437> shadowing loop labels should be an error
Loop:  // expected-note {{previously declared here}}
  for _ in 0...2 {
  Loop:  // expected-error {{label 'Loop' cannot be reused on an inner statement}}
    for _ in 0...2 {
    }
  }


  // <rdar://problem/16798323> Following a 'break' statment by another statement on a new line result in an error/fit-it
  switch 5 {
  case 5:
    markUsed("before the break")
    break
    markUsed("after the break")    // 'markUsed' is not a label for the break.
  default:
    markUsed("")
  }
  
  let x : Int? = 42
  
  // <rdar://problem/16879701> Should be able to pattern match 'nil' against optionals
  switch x {
  case .Some(42): break
  case nil: break
  
  }
  
}


enum MyEnumWithCaseLabels {
  case Case(one: String, two: Int)
}

func testMyEnumWithCaseLabels(a : MyEnumWithCaseLabels) {
  // <rdar://problem/20135489> Enum case labels are ignored in "case let" statements
  switch a {
  case let .Case(one: _, two: x): break // ok
  case let .Case(xxx: _, two: x): break // expected-error {{tuple pattern element label 'xxx' must be 'one'}}
  // TODO: In principle, reordering like this could be supported.
  case let .Case(two: _, one: x): break // expected-error {{tuple pattern element label}}
  }
}



// "defer"

func test_defer(a : Int) {
  
  defer { VoidReturn1() }
  defer { breakContinue(1)+42 }
  
  // Ok:
  defer { while false { break } }

  // Not ok.
  while false { defer { break } }   // expected-error {{'break' cannot transfer control out of a defer statement}}
  defer { return }  // expected-error {{'return' cannot transfer control out of a defer statement}}
}

class SomeTestClass {
  var x = 42
  
  func method() {
    defer { x = 97 }  // self. not required here!
  }
}


func test_require(x : Int, y : Int??, cond : Bool) {
  
  // These are all ok.
  guard let a = y else {}
  markUsed(a)
  guard let b = y where cond else {}
  guard case let c = x where cond else {}
  guard case let Optional.Some(d) = y else {}
  guard x != 4, case _ = x else { }


  guard let e where cond else {}    // expected-error {{variable binding in a condition requires an initializer}}
  guard case let f? : Int? where cond else {}    // expected-error {{variable binding in a condition requires an initializer}}

  guard let g = y else {
    markUsed(g)  // expected-error {{variable declared in 'guard' condition is not usable in its body}}
  }

  guard let h = y where cond {}  // expected-error {{expected 'else' after 'guard' condition}}


  guard case _ = x else {}  // expected-warning {{'guard' condition is always true, body is unreachable}}
}

func test_is_as_patterns() {
  switch 4 {
  case is Int: break        // expected-warning {{'is' test is always true}}
  case _ as Int: break  // expected-warning {{'as' test is always true}}
  case _: break
  }
}

// <rdar://problem/21387308> Fuzzing SourceKit: crash in Parser::parseStmtForEach(...)
func matching_pattern_recursion() {
  switch 42 {
  case {  // expected-error {{expression pattern of type '() -> ()' cannot match values of type 'Int'}}
      for i in zs {  // expected-error {{use of unresolved identifier 'zs'}}
      }
  }: break
  }
}

// <rdar://problem/18776073> Swift's break operator in switch should be indicated in errors
func r18776073(a : Int?) {
  switch a {
    case nil:   // expected-error {{'case' label in a 'switch' should have at least one executable statement}} {{14-14= break}}
    case _?: break
  }
}

// <rdar://problem/22491782> unhelpful error message from "throw nil"
func testThrowNil() throws {
  throw nil  // expected-error {{cannot infer concrete ErrorType for thrown 'nil' value}}
}


// <rdar://problem/16650625>
func for_ignored_lvalue_init() {
  var i = 0
  for i;  // expected-error {{expression resolves to an unused l-value}}
    i < 10; ++i {}
}

// rdar://problem/18643692
func for_loop_multi_iter() {
  for (var i = 0, x = 0; i < 10; i++,
       x) { // expected-error {{expression resolves to an unused l-value}}
    --x
  }
}


