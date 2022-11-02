// RUN: %target-parse-verify-swift

var func6 : (fn : (Int,Int) -> Int) -> ()
var func6a : ((Int, Int) -> Int) -> ()
var func6b : (Int, (Int, Int) -> Int) -> ()
func func6c(f: (Int, Int) -> Int, _ n: Int = 0) {} // expected-warning{{prior to parameters}}


// Expressions can be auto-closurified, so that they can be evaluated separately
// from their definition.
var closure1 : () -> Int = {4}  // Function producing 4 whenever it is called.
var closure2 : (Int,Int) -> Int = { 4 } // expected-error{{contextual type for closure argument list expects 2 arguments, which cannot be implicitly ignored}} {{36-36= _,_ in}}
var closure3a : ()->()->(Int,Int) = {{ (4, 2) }} // multi-level closing.
var closure3b : (Int,Int)->(Int)->(Int,Int) = {{ (4, 2) }} // expected-error{{contextual type for closure argument list expects 2 arguments, which cannot be implicitly ignored}}  {{48-48=_,_ in }}
var closure4 : (Int,Int) -> Int = { $0 + $1 }
var closure5 : (Double) -> Int = {
       $0 + 1.0  // expected-error {{cannot convert value of type 'Double' to expected argument type 'Int'}}
}

var closure6 = $0  // expected-error {{anonymous closure argument not contained in a closure}}

var closure7 : Int =
   { 4 }  // expected-error {{function produces expected type 'Int'; did you mean to call it with '()'?}} {{9-9=()}}

func funcdecl1(a: Int, _ y: Int) {}
func funcdecl3() -> Int {}
func funcdecl4(a: ((Int)->Int), _ b: Int) {}

func funcdecl5(a: Int, _ y: Int) {
  // Pass in a closure containing the call to funcdecl3.
  funcdecl4({ funcdecl3() }, 12)  // expected-error {{contextual type for closure argument list expects 1 argument, which cannot be implicitly ignored}} {{14-14= _ in}}
  
  
  func6(fn: {$0 + $1})       // Closure with two named anonymous arguments
  func6(fn: {($0) + $1})    // Closure with sequence expr inferred type
  func6(fn: {($0) + $0})    // expected-error{{cannot convert value of type '(Int, Int)' to expected argument type 'Int'}}


  var testfunc : ((), Int) -> Int
  testfunc(          
           {$0+1})  // expected-error {{missing argument for parameter #2 in call}}

  funcdecl5(1, 2) // recursion.

  // Element access from a tuple.
  var a : (Int, f : Int, Int)
  var b = a.1+a.f

  // Tuple expressions with named elements.
  var i : (y : Int, x : Int) = (x : 42, y : 11)
  funcdecl1(123, 444)
  
  // Calls.
  4()  // expected-error {{invalid use of '()' to call a value of non-function type 'Int'}} {{4-6=}}
  
  
  // rdar://12017658 - Infer some argument types from func6.
  func6(fn: { a, b -> Int in a+b})
  // Return type inference.
  func6(fn: { a,b in a+b })
  
  // Infer incompatible type.
  func6(fn: {a,b->Float in 4.0 })    // expected-error {{declared closure result 'Float' is incompatible with contextual type 'Int'}} {{19-24=Int}}  // Pattern doesn't need to name arguments.
  func6(fn: { _,_ in 4 })
  
  func6(fn: {a,b in 4.0 })  // expected-error {{cannot convert value of type 'Double' to closure result type 'Int'}}
  
  // TODO: This diagnostic can be improved: rdar://22128205
  func6(fn: {(a : Float, b) in 4 }) // expected-error {{cannot convert value of type '(Float, _) -> Int' to expected argument type '(Int, Int) -> Int'}}

  
  
  var fn = {}
  var fn2 = { 4 }
  
  
  var c : Int = { a,b-> Int in a+b} // expected-error{{cannot convert value of type '(Int, Int) -> Int' to specified type 'Int'}}
  
  
}

func unlabeledClosureArgument() {

  func add(x: Int, y: Int) -> Int { return x + y }
  func6a({$0 + $1}) // single closure argument
  func6a(add)
  func6b(1, {$0 + $1}) // second arg is closure
  func6b(1, add)
  func6c({$0 + $1}) // second arg is default int
  func6c(add)
}

// rdar://11935352 - closure with no body.
func closure_no_body(p: () -> ()) {
  return closure_no_body({})
}


// rdar://12019415
func t() {
  let u8 : UInt8 = 1
  let x : Bool = true

  if 0xA0..<0xBF ~= Int(u8) && x {
  }
}

// <rdar://problem/11927184>
func f0(a: Any) -> Int { return 1 }
assert(f0(1) == 1)


var selfRef = { selfRef() } // expected-error {{variable used within its own initial value}}
var nestedSelfRef = {
  var recursive = { nestedSelfRef() } // expected-error {{variable used within its own initial value}}
  recursive()
}

var shadowed = { (shadowed: Int) -> Int in
  let x = shadowed
  return x
} // no-warning
var shadowedShort = { (shadowedShort: Int) -> Int in shadowedShort+1 } // no-warning


func anonymousClosureArgsInClosureWithArgs() {
  var a1 = { () in $0 } // expected-error {{anonymous closure arguments cannot be used inside a closure that has explicit arguments}}
  var a2 = { () -> Int in $0 } // expected-error {{anonymous closure arguments cannot be used inside a closure that has explicit arguments}}
  var a3 = { (z: Int) in $0 } // expected-error {{anonymous closure arguments cannot be used inside a closure that has explicit arguments}}
}

func doStuff(fn : () -> Int) {}
func doVoidStuff(fn : () -> ()) {}

// <rdar://problem/16193162> Require specifying self for locations in code where strong reference cycles are likely
class ExplicitSelfRequiredTest {
  var x = 42
  func method() -> Int {
    // explicit closure requires an explicit "self." base.
    doStuff({ ++self.x })
    doStuff({ ++x })    // expected-error {{reference to property 'x' in closure requires explicit 'self.' to make capture semantics explicit}} {{17-17=self.}}
    doVoidStuff({ ++x })    // expected-error {{reference to property 'x' in closure requires explicit 'self.' to make capture semantics explicit}} {{21-21=self.}}

    // Methods follow the same rules as properties, uses of 'self' must be marked with "self."
    doStuff { method() }  // expected-error {{call to method 'method' in closure requires explicit 'self.' to make capture semantics explicit}} {{15-15=self.}}
    doVoidStuff { method() }  // expected-error {{call to method 'method' in closure requires explicit 'self.' to make capture semantics explicit}} {{19-19=self.}}
    doStuff { self.method() }

    // <rdar://problem/18877391> "self." shouldn't be required in the initializer expression in a capture list
    // This should not produce an error, "x" isn't being captured by the closure.
    doStuff({ [myX = x] in myX })

    // This should produce an error, since x is used within the inner closure.
    doStuff({ [myX = {x}] in 4 })    // expected-error {{reference to property 'x' in closure requires explicit 'self.' to make capture semantics explicit}} {{23-23=self.}}
    // expected-warning @-1 {{capture 'myX' was never used}}

    return 42
  }
}

// expected-error@+2 {{Use of 'var' binding here is not allowed}} {{57-60=}}
// expected-warning@+1 {{'let' keyword is unnecessary; function parameters are immutable by default}} {{64-68=}}
var testClosureArgumentPatterns: (Int, Int) -> Int = { (var x, let y) in x+y+1 }

class SomeClass {
  var field : SomeClass?
  func foo() -> Int {}
}

func testCaptureBehavior(ptr : SomeClass) {
  // Test normal captures.
  weak var wv : SomeClass? = ptr
  unowned let uv : SomeClass = ptr
  unowned(unsafe) let uv1 : SomeClass = ptr
  unowned(safe) let uv2 : SomeClass = ptr
  doStuff { wv!.foo() }
  doStuff { uv.foo() }
  doStuff { uv1.foo() }
  doStuff { uv2.foo() }

  
  // Capture list tests
  let v1 : SomeClass? = ptr
  let v2 : SomeClass = ptr

  doStuff { [weak v1] in v1!.foo() }
  // expected-warning @+2 {{variable 'v1' was written to, but never read}}
  doStuff { [weak v1,                 // expected-note {{previous}}
             weak v1] in v1!.foo() }  // expected-error {{definition conflicts with previous value}}
  doStuff { [unowned v2] in v2.foo() }
  doStuff { [unowned(unsafe) v2] in v2.foo() }
  doStuff { [unowned(safe) v2] in v2.foo() }
  doStuff { [weak v1, weak v2] in v1!.foo() + v2!.foo() }

  let i = 42
  // expected-warning @+1 {{variable 'i' was never mutated}} {{19-20=let}}
  doStuff { [weak i] in i! }   // expected-error {{'weak' may only be applied to class and class-bound protocol types, not 'Int'}}
}

extension SomeClass {
  func bar() {
    doStuff { [unowned self] in self.foo() }
    doStuff { [unowned xyz = self.field!] in xyz.foo() }
    doStuff { [weak xyz = self.field] in xyz!.foo() }

    // rdar://16889886 - Assert when trying to weak capture a property of self in a lazy closure
    doStuff { [weak self.field] in field!.foo() }   // expected-error {{fields may only be captured by assigning to a specific name}} expected-error {{reference to property 'field' in closure requires explicit 'self.' to make capture semantics explicit}} {{36-36=self.}}
    // expected-warning @+1 {{variable 'self' was written to, but never read}}
    doStuff { [weak self&field] in 42 }  // expected-error {{expected ']' at end of capture list}}

  }

  func strong_in_capture_list() {
    // <rdar://problem/18819742> QOI: "[strong self]" in capture list generates unhelpful error message
    _ = {[strong self] () -> () in return }  // expected-error {{expected 'weak', 'unowned', or no specifier in capture list}}
  }
}


// <rdar://problem/16955318> Observed variable in a closure triggers an assertion
var closureWithObservedProperty: () -> () = {
  var a: Int = 42 {
  willSet {
    _ = "Will set a to \(newValue)"
  }
  didSet {
    _ = "Did set a with old value of \(oldValue)"
  }
  }
}

;

{}() // expected-error{{statement cannot begin with a closure expression}} expected-note{{explicitly discard the result of the closure by assigning to '_'}} {{1-1=_ = }}



// rdar://19179412 - Crash on valid code.
func rdar19179412() -> Int -> Int {
  return { x in
    class A {
      let d : Int = 0
    }
  }
}

// Test coercion of single-expression closure return types to void.
func takesVoidFunc(f: ()->()) {}
var i: Int = 1

takesVoidFunc({i})
var f1: ()->() = {i}
var x = {return $0}(1)

func returnsInt() -> Int { return 0 }
takesVoidFunc(returnsInt) // expected-error {{cannot convert value of type '() -> Int' to expected argument type '() -> ()'}}
takesVoidFunc({()->Int in 0}) // expected-error {{declared closure result 'Int' is incompatible with contextual type '()'}} {{20-23=()}}
  
// These used to crash the compiler, but were fixed to support the implemenation of rdar://problem/17228969
Void(0) // expected-error{{argument passed to call that takes no arguments}}
_ = {0}

// <rdar://problem/22086634> "multi-statement closures require an explicit return type" should be an error not a note
let samples = {
          if (i > 10) { return true }
          else { return false }
        }()  // expected-error {{cannot invoke closure of type '() -> _' with an argument list of type '()'}}

// <rdar://problem/19756953> Swift error: cannot capture '$0' before it is declared
func f(fp : (Bool, Bool)-> Bool) {}
f { $0 && !$1 }


// <rdar://problem/18123596> unexpected error on self. capture inside class method
func TakesIntReturnsVoid(fp : (Int -> ())) {}

struct TestStructWithStaticMethod {
  static func myClassMethod(count: Int) {
    // Shouldn't require "self."
    TakesIntReturnsVoid { _ in myClassMethod(0) }
  }
}

class TestClassWithStaticMethod {
  class func myClassMethod(count: Int) {
    // Shouldn't require "self."
    TakesIntReturnsVoid { _ in myClassMethod(0) }
  }
}

// Test that we can infer () as the result type of these closures.
func genericOne<T>(a: () -> T) {}
func genericTwo<T>(a: () -> T, _ b: () -> T) {}
genericOne {}
genericTwo({}, {})


// <rdar://problem/22344208> QoI: Warning for unused capture list variable should be customized
class r22344208 {
  func f() {
    let q = 42
    let _: () -> Int = {
      [unowned self,  // expected-warning {{capture 'self' was never used}}
       q] in       // expected-warning {{capture 'q' was never used}}
      1 }
  }
}

var f = { (s: Undeclared)-> Int in 0 } // expected-error {{use of undeclared type 'Undeclared'}}

// <rdar://problem/21375863> Swift compiler crashes when using closure, declared to return illegal type.
func r21375863() {
  var width = 0
  var height = 0
  var bufs: [[UInt8]] = (0..<4).map { _ -> [asdf] in  // expected-error {{use of undeclared type 'asdf'}}
    [UInt8](count: width*height, repeatedValue: 0)
  }
}
