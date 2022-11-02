// RUN: %target-parse-verify-swift

@noreturn
func exit(_: Int) {}

@noreturn // expected-error {{@noreturn may only be used on 'func' declarations}}{{1-11=}}
class InvalidOnClass {}

@noreturn // expected-error {{@noreturn may only be used on 'func' declarations}}{{1-11=}}
struct InvalidOnStruct {}

@noreturn // expected-error {{@noreturn may only be used on 'func' declarations}}{{1-11=}}
enum InvalidOnEnum {}

@noreturn // expected-error {{@noreturn may only be used on 'func' declarations}}{{1-11=}}
protocol InvalidOnProtocol {}

struct InvalidOnExtension {}

@noreturn // expected-error {{@noreturn may only be used on 'func' declarations}} {{1-11=}}
extension InvalidOnExtension {}

@noreturn // expected-error {{@noreturn may only be used on 'func' declarations}}{{1-11=}}
var invalidOnVar = 0

@noreturn // expected-error {{@noreturn may only be used on 'func' declarations}}{{1-11=}}
let invalidOnLet = 0

class InvalidOnClassMembers {
  @noreturn // expected-error {{@noreturn may only be used on 'func' declarations}}{{3-13=}}
  init() {}

  @noreturn // expected-error {{@noreturn may only be used on 'func' declarations}}{{3-13=}}
  deinit {}

  @noreturn // expected-error {{@noreturn may only be used on 'func' declarations}}{{3-13=}}
  subscript(i: Int) -> Int {
    get {
      return 0
    }
  }
}

protocol TestProtocol {
  // expected-note@+1 {{protocol requires function 'neverReturns()' with type '@noreturn () -> ()'}}
  @noreturn func neverReturns()
  func doesReturn()

  // expected-note@+1 {{protocol requires function 'neverReturnsStatic()' with type '@noreturn () -> ()'}}
  @noreturn static func neverReturnsStatic()
  static func doesReturnStatic()
}
// expected-error@+1 {{type 'ConformsToProtocolA' does not conform to protocol 'TestProtocol'}}
struct ConformsToProtocolA : TestProtocol {
  // expected-note@+1 {{candidate is not @noreturn, but protocol requires it}}
  func neverReturns() {}

  // OK: a @noreturn function conforms to a non-@noreturn protocol requirement.
  @noreturn func doesReturn() { exit(0) }

  // expected-note@+1 {{candidate is not @noreturn, but protocol requires it}}
  static func neverReturnsStatic() {}

  // OK: a @noreturn function conforms to a non-@noreturn protocol requirement.
  @noreturn static func doesReturnStatic() {}
}

class BaseClass {
  @noreturn func neverReturns() { exit(0) } // expected-note 3{{overridden declaration is here}}
  func doesReturn() {} // expected-note {{overridden declaration is here}}

  @noreturn class func neverReturnsClass() { exit(0) } // expected-note 3{{overridden declaration is here}}
  class func doesReturn() {} // expected-note {{overridden declaration is here}}
}
class DerivedClassA : BaseClass {
  // expected-error@+2 {{overriding declaration requires an 'override' keyword}} {{3-3=override }}
  // expected-error@+1 {{an override of a @noreturn method should also be @noreturn}}
  func neverReturns() {}

  // expected-error@+1 {{overriding declaration requires an 'override' keyword}} {{13-13=override }}
  @noreturn func doesReturn() { exit(0) }

  // expected-error@+2 {{overriding declaration requires an 'override' keyword}} {{3-3=override }}
  // expected-error@+1 {{an override of a @noreturn method should also be @noreturn}}
  class func neverReturnsClass() { exit(0) }

  // expected-error@+1 {{overriding declaration requires an 'override' keyword}} {{13-13=override }}
  @noreturn class func doesReturn() {}
}
class DerivedClassB : BaseClass {
  // expected-error@+1 {{an override of a @noreturn method should also be @noreturn}}
  override func neverReturns() {}

  // OK: a @noreturn method overrides a non-@noreturn base method.
  @noreturn override func doesReturn() { exit(0) }

  // expected-error@+1 {{an override of a @noreturn method should also be @noreturn}}
  override class func neverReturnsClass() { exit(0) }

  // OK: a @noreturn method overrides a non-@noreturn base method.
  @noreturn override class func doesReturn() {}
}

struct MethodWithNoreturn {
  @noreturn
  func neverReturns() { exit(0) }
}

func printInt(_: Int) {}
var maybeReturns: (Int) -> () = exit // no-error
var neverReturns1 = exit
neverReturns1 = printInt // expected-error {{cannot assign value of type '(Int) -> ()' to type '@noreturn (Int) -> ()'}}

var neverReturns2: MethodWithNoreturn -> @noreturn () -> () = MethodWithNoreturn.neverReturns

exit(5) // no-error

@noreturn
func exit() -> () {}
@noreturn
func testFunctionOverload() -> () {
  exit()
}

func testRvalue(lhs: (), rhs: @noreturn () -> ()) -> () {
  return rhs()
}

var fnr: @noreturn (_: Int) -> () = exit
// This might be a desirable syntax, but it does not get properly propagated to SIL, so reject it for now.
@noreturn // expected-error {{@noreturn may only be used on 'func' declarations}}{{1-11=}}
var fpr: (_: Int) -> () = exit

func testWitnessMethod<T: TestProtocol>(t: T) {
  _ = T.neverReturnsStatic
}
