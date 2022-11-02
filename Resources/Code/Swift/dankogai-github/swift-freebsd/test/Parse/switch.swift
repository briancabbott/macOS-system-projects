// RUN: %target-parse-verify-swift

// TODO: Implement tuple equality in the library.
// BLOCKED: <rdar://problem/13822406>
func ~= (x: (Int,Int), y: (Int,Int)) -> Bool {
  return true
}

func parseError1(x: Int) {
  switch func {} // expected-error {{expected expression in 'switch' statement}} expected-error {{expected '{' after 'switch' subject expression}} expected-error {{expected identifier in function declaration}} expected-error {{braced block of statements is an unused closure}} expected-error{{expression resolves to an unused function}}
}

func parseError2(x: Int) {
  switch x // expected-error {{expected '{' after 'switch' subject expression}}
}

func parseError3(x: Int) {
  switch x {
    case // expected-error {{expected pattern}} expected-error {{expected ':' after 'case'}}
  }
}

func parseError4(x: Int) {
  switch x {
  case let z where // expected-error {{expected expression for 'where' guard of 'case'}} expected-error {{expected ':' after 'case'}}
  }
}

func parseError5(x: Int) {
  switch x {
  case let z // expected-error {{expected ':' after 'case'}} expected-warning {{immutable value 'z' was never used}} {{12-13=_}}
  }
}

func parseError6(x: Int) {
  switch x {
  default // expected-error {{expected ':' after 'default'}}
  }
}

var x: Int

switch x {} // expected-error {{'switch' statement body must have at least one 'case' or 'default' block}}

switch x {
case 0:
  x = 0
// Multiple patterns per case
case 1, 2, 3:
  x = 0
// 'where' guard
case _ where x % 2 == 0:
  x = 1
  x = 2
  x = 3
case _ where x % 2 == 0,
     _ where x % 3 == 0:
  x = 1
case 10,
     _ where x % 3 == 0:
  x = 1
case _ where x % 2 == 0,
     20:
  x = 1
case let y where y % 2 == 0:
  x = y + 1
case _ where 0: // expected-error {{type 'Int' does not conform to protocol 'BooleanType'}}
  x = 0
default:
  x = 1
}

// Multiple cases per case block
switch x {
case 0: // expected-error {{'case' label in a 'switch' should have at least one executable statement}} {{8-8= break}}
case 1:
  x = 0
}

switch x {
case 0: // expected-error{{'case' label in a 'switch' should have at least one executable statement}} {{8-8= break}}
default:
  x = 0
}

switch x {
case 0:
  x = 0
case 1: // expected-error {{'case' label in a 'switch' should have at least one executable statement}} {{8-8= break}}
}

switch x {
case 0:
  x = 0
default: // expected-error {{'default' label in a 'switch' should have at least one executable statement}} {{9-9= break}}
}

switch x {
case 0:
  ; // expected-error {{';' statements are not allowed}} {{3-5=}}
case 1:
  x = 0
}



switch x {
  x = 1 // expected-error{{all statements inside a switch must be covered by a 'case' or 'default'}}
default:
  x = 0
case 0: // expected-error{{additional 'case' blocks cannot appear after the 'default' block of a 'switch'}}
  x = 0
case 1:
  x = 0
}

switch x {
default:
  x = 0
default: // expected-error{{additional 'case' blocks cannot appear after the 'default' block of a 'switch'}}
  x = 0
}

switch x {
  x = 1 // expected-error{{all statements inside a switch must be covered by a 'case' or 'default'}}
}

switch x {
  x = 1 // expected-error{{all statements inside a switch must be covered by a 'case' or 'default'}}
  x = 2
}

switch x {
default: // expected-error{{'default' label in a 'switch' should have at least one executable statement}} {{9-9= break}}
case 0: // expected-error{{additional 'case' blocks cannot appear after the 'default' block of a 'switch'}}
  x = 0
}

switch x {
default: // expected-error{{'default' label in a 'switch' should have at least one executable statement}} {{9-9= break}}
default: // expected-error{{additional 'case' blocks cannot appear after the 'default' block of a 'switch'}}
  x = 0
}

switch x {
default where x == 0: // expected-error{{'default' cannot be used with a 'where' guard expression}}
  x = 0
}

switch x {
case 0: // expected-error {{'case' label in a 'switch' should have at least one executable statement}} {{8-8= break}}
}

switch x {
case 0: // expected-error{{'case' label in a 'switch' should have at least one executable statement}} {{8-8= break}}
case 1:
  x = 0
}

switch x {
case 0:
  x = 0
case 1: // expected-error{{'case' label in a 'switch' should have at least one executable statement}} {{8-8= break}}
}


case 0: // expected-error{{'case' label can only appear inside a 'switch' statement}}
var y = 0
default: // expected-error{{'default' label can only appear inside a 'switch' statement}}
var z = 1

fallthrough // expected-error{{'fallthrough' is only allowed inside a switch}}

switch x {
case 0:
  fallthrough
case 1:
  fallthrough
default:
  fallthrough // expected-error{{'fallthrough' without a following 'case' or 'default' block}}
}

// Fallthrough can transfer control anywhere within a case and can appear
// multiple times in the same case.
switch x {
case 0:
  if true { fallthrough }
  if false { fallthrough }
  x += 1
default:
  x += 1
}

// Cases cannot contain 'var' bindings if there are multiple matching patterns
// attached to a block. They may however contain other non-binding patterns.

var t = (1, 2)

switch t {

case (let a, 2): // expected-error {{'case' label in a 'switch' should have at least one executable statement}} {{17-17= break}}
case (1, _):
  ()

case (_, 2): // expected-error {{'case' label in a 'switch' should have at least one executable statement}} {{13-13= break}}
case (1, let a):
  ()

case (let a, 2): // expected-error {{'case' label in a 'switch' should have at least one executable statement}} {{17-17= break}}
case (1, let b):
  ()

case (1, let b): // let bindings
  ()

// var bindings are not allowed in cases.
// FIXME: rdar://problem/23378003
// This will eventually be an error.
case (1, var b): // expected-error {{Use of 'var' binding here is not allowed}} {{10-13=let}}
  ()
case (var a, 2): // expected-error {{Use of 'var' binding here is not allowed}} {{7-10=let}}
  ()

case (let a, 2), (1, let b): // expected-error {{'case' labels with multiple patterns cannot declare variables}}
  ()
case (let a, 2), (1, _): // expected-error {{'case' labels with multiple patterns cannot declare variables}}
  ()
case (_, 2), (let a, _): // expected-error {{'case' labels with multiple patterns cannot declare variables}}
  ()

// OK
case (_, 2), (1, _):
  ()

case (_, 2): // expected-error {{'case' label in a 'switch' should have at least one executable statement}} {{13-13= break}}
case (1, _):
  ()
}

// Fallthroughs can't transfer control into a case label with bindings.
switch t {
case (1, 2):
  fallthrough // expected-error {{'fallthrough' cannot transfer control to a case label that declares variables}}
case (let a, let b):
  t = (b, a)
}

func test_label(x : Int) {
Gronk:
  switch x {
  case 42: return
  }
}

func enumElementSyntaxOnTuple() {
  switch (1, 1) {
  case .Bar: // expected-error {{enum case 'Bar' not found in type '(Int, Int)'}}
    break
  default:
    break
  }
}

