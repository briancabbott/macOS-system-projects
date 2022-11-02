// RUN: %target-parse-verify-swift

import Swift

////
// Members of structs
////

struct X {
  func f0(i: Int) -> X { }

  func f1(i: Int) { }

  mutating func f1(f: Float) { }

  func f2<T>(x: T) -> T { }
}

struct Y<T> {
  func f0(_: T) -> T {}
  func f1<U>(x: U, y: T) -> (T, U) {}
}

var i : Int
var x : X
var yf : Y<Float>

func g0(_: (inout X) -> (Float) -> ()) {}

x.f0(i)
x.f0(i).f1(i)
g0(X.f1)
x.f0(x.f2(1))
x.f0(1).f2(i)
yf.f0(1)
yf.f1(i, y: 1)

// Members referenced from inside the struct
struct Z {
  var i : Int
  func getI() -> Int { return i }
  mutating func incI() {}

  func curried(x: Int)(y: Int) -> Int { return x + y } // expected-warning{{curried function declaration syntax will be removed in a future version of Swift}}

  subscript (k : Int) -> Int {
    get {
      return i + k
    }
    mutating
    set {
      i -= k
    }
  }
}

struct GZ<T> {
  var i : T
  func getI() -> T { return i }

  func f1<U>(a: T, b: U) -> (T, U) { 
    return (a, b)
  }
  
  func f2() {
    var f : Float
    var t = f1(i, b: f)
    f = t.1

    var zi = Z.i; // expected-error{{instance member 'i' cannot be used on type 'Z'}}
    var zj = Z.asdfasdf  // expected-error {{type 'Z' has no member 'asdfasdf'}}
  }
}

var z = Z(i: 0)
var getI = z.getI
var incI = z.incI // expected-error{{partial application of 'mutating'}}
var zi = z.getI()
var zcurried1 = z.curried
var zcurried2 = z.curried(0)
var zcurriedFull = z.curried(0)(y: 1)

////
// Members of modules
////

// Module
Swift.print(3, terminator: "")

var format : String
format._splitFirstIf({ $0.isASCII() })

////
// Unqualified references
////

////
// Members of literals
////

// FIXME: Crappy diagnostic
"foo".lower() // expected-error{{value of type 'String' has no member 'lower'}}
var tmp = "foo".debugDescription

////
// Members of enums
////

enum W {
  case Omega

  func foo(x: Int) {}
  func curried(x: Int)(y: Int) {} // expected-warning{{curried function declaration syntax will be removed in a future version of Swift}}
}

var w = W.Omega
var foo = w.foo
var fooFull : () = w.foo(0)
var wcurried1 = w.curried
var wcurried2 = w.curried(0)
var wcurriedFull : () = w.curried(0)(y: 1)

// Member of enum Type
func enumMetatypeMember(opt: Int?) {
  opt.None // expected-error{{static member 'None' cannot be used on instance of type 'Int?'}}
}

////
// Nested types
////

// Reference a Type member. <rdar://problem/15034920>
class G<T> {
  class In { // expected-error{{nested in generic type}}
    class func foo() {}
  }
}

func goo() {
  G<Int>.In.foo()
}

////
// Members of archetypes
////

func id<T>(t: T) -> T { return t }

func doGetLogicValue<T : BooleanType>(t: T) {
  t.boolValue
}

protocol P {
  init()
  func bar(x: Int)
  mutating func mut(x: Int)
  static func tum()
}

extension P {
  func returnSelfInstance() -> Self {
    return self
  }

  func returnSelfOptionalInstance(b: Bool) -> Self? {
    return b ? self : nil
  }

  func returnSelfIUOInstance(b: Bool) -> Self! {
    return b ? self : nil
  }

  static func returnSelfStatic() -> Self {
    return Self()
  }

  static func returnSelfOptionalStatic(b: Bool) -> Self? {
    return b ? Self() : nil
  }

  static func returnSelfIUOStatic(b: Bool) -> Self! {
    return b ? Self() : nil
  }
}

protocol ClassP : class {
  func bas(x: Int)
}

func generic<T: P>(t: T) {
  var t = t
  // Instance member of archetype
  let _: Int -> () = id(t.bar)
  let _: () = id(t.bar(0))

  // Static member of archetype metatype
  let _: () -> () = id(T.tum)

  // Instance member of archetype metatype
  let _: T -> Int -> () = id(T.bar)
  let _: Int -> () = id(T.bar(t))

  _ = t.mut // expected-error{{partial application of 'mutating' method is not allowed}}
  _ = t.tum // expected-error{{static member 'tum' cannot be used on instance of type 'T'}}

  // Instance member of extension returning Self)
  let _: T -> () -> T = id(T.returnSelfInstance)
  let _: () -> T = id(T.returnSelfInstance(t))
  let _: T = id(T.returnSelfInstance(t)())

  let _: () -> T = id(t.returnSelfInstance)
  let _: T = id(t.returnSelfInstance())

  let _: T -> Bool -> T? = id(T.returnSelfOptionalInstance)
  let _: Bool -> T? = id(T.returnSelfOptionalInstance(t))
  let _: T? = id(T.returnSelfOptionalInstance(t)(false))

  let _: Bool -> T? = id(t.returnSelfOptionalInstance)
  let _: T? = id(t.returnSelfOptionalInstance(true))

  let _: T -> Bool -> T! = id(T.returnSelfIUOInstance)
  let _: Bool -> T! = id(T.returnSelfIUOInstance(t))
  let _: T! = id(T.returnSelfIUOInstance(t)(true))

  let _: Bool -> T! = id(t.returnSelfIUOInstance)
  let _: T! = id(t.returnSelfIUOInstance(true))

  // Static member of extension returning Self)
  let _: () -> T = id(T.returnSelfStatic)
  let _: T = id(T.returnSelfStatic())

  let _: Bool -> T? = id(T.returnSelfOptionalStatic)
  let _: T? = id(T.returnSelfOptionalStatic(false))

  let _: Bool -> T! = id(T.returnSelfIUOStatic)
  let _: T! = id(T.returnSelfIUOStatic(true))
}

func genericClassP<T: ClassP>(t: T) {
  // Instance member of archetype)
  let _: Int -> () = id(t.bas)
  let _: () = id(t.bas(0))

  // Instance member of archetype metatype)
  let _: T -> Int -> () = id(T.bas)
  let _: Int -> () = id(T.bas(t))
  let _: () = id(T.bas(t)(1))
}

////
// Members of existentials
////

func existential(p: P) {
  var p = p
  // Fully applied mutating method
  p.mut(1)
  _ = p.mut // expected-error{{partial application of 'mutating' method is not allowed}}

  // Instance member of existential)
  let _: Int -> () = id(p.bar)
  let _: () = id(p.bar(0))

  // Static member of existential metatype)
  let _: () -> () = id(p.dynamicType.tum)

  // Instance member of extension returning Self
  let _: () -> P = id(p.returnSelfInstance)
  let _: P = id(p.returnSelfInstance())
  let _: P? = id(p.returnSelfOptionalInstance(true))
  let _: P! = id(p.returnSelfIUOInstance(true))
}

func staticExistential(p: P.Type, pp: P.Protocol) {
  let ppp: P = p.init()
  _ = pp.init() // expected-error{{value of type 'P.Protocol' is a protocol; it cannot be instantiated}}
  _ = P() // expected-error{{protocol type 'P' cannot be instantiated}}

  // Instance member of metatype
  let _: P -> Int -> () = P.bar
  let _: Int -> () = P.bar(ppp)
  P.bar(ppp)(5)

  // Instance member of metatype value
  let _: P -> Int -> () = pp.bar
  let _: Int -> () = pp.bar(ppp)
  pp.bar(ppp)(5)

  // Static member of existential metatype value
  let _: () -> () = p.tum

  // Instance member of existential metatype -- not allowed
  _ = p.bar // expected-error{{instance member 'bar' cannot be used on type 'P'}}
  _ = p.mut // expected-error{{instance member 'mut' cannot be used on type 'P'}}

  // Static member of metatype -- not allowed
  _ = pp.tum // expected-error{{static member 'tum' cannot be used on instance of type 'P.Protocol'}}
  _ = P.tum // expected-error{{static member 'tum' cannot be used on instance of type 'P.Protocol'}}

  // Static member of extension returning Self)
  let _: () -> P = id(p.returnSelfStatic)
  let _: P = id(p.returnSelfStatic())

  let _: Bool -> P? = id(p.returnSelfOptionalStatic)
  let _: P? = id(p.returnSelfOptionalStatic(false))

  let _: Bool -> P! = id(p.returnSelfIUOStatic)
  let _: P! = id(p.returnSelfIUOStatic(true))
}

func existentialClassP(p: ClassP) {
  // Instance member of existential)
  let _: Int -> () = id(p.bas)
  let _: () = id(p.bas(0))

  // Instance member of existential metatype)
  let _: ClassP -> Int -> () = id(ClassP.bas)
  let _: Int -> () = id(ClassP.bas(p))
  let _: () = id(ClassP.bas(p)(1))
}

// Partial application of curried protocol methods
protocol Scalar {}
protocol Vector {
  func scale(c: Scalar) -> Self
}
protocol Functional {
  func apply(v: Vector) -> Scalar
}
protocol Coalgebra {
  func coproduct(f: Functional)(v1: Vector, v2: Vector) -> Scalar // expected-warning{{curried function declaration syntax will be removed in a future version of Swift}}
}

// Make sure existential is closed early when we partially apply
func wrap<T>(t: T) -> T {
  return t
}

func exercise(c: Coalgebra, f: Functional, v: Vector) {
  let _: (Vector, Vector) -> Scalar = wrap(c.coproduct(f))
  let _: Scalar -> Vector = v.scale
}

// Make sure existential isn't closed too late
protocol Copyable {
  func copy() -> Self
}

func copyTwice(c: Copyable) -> Copyable {
  return c.copy().copy()
}

////
// Misc ambiguities
////

// <rdar://problem/15537772>
struct DefaultArgs {
  static func f(a: Int = 0) -> DefaultArgs {
    return DefaultArgs()
  }
  init() {
    self = .f()
  }
}

class InstanceOrClassMethod {
  func method() -> Bool { return true }
  class func method(other: InstanceOrClassMethod) -> Bool { return false }
}

func testPreferClassMethodToCurriedInstanceMethod(obj: InstanceOrClassMethod) {
  let result = InstanceOrClassMethod.method(obj)
  let _: Bool = result // no-warning
  let _: () -> Bool = InstanceOrClassMethod.method(obj)
}

protocol Numeric {
  func +(x: Self, y: Self) -> Self
}

func acceptBinaryFunc<T>(x: T, _ fn: (T, T) -> T) { }

func testNumeric<T : Numeric>(x: T) {
  acceptBinaryFunc(x, +)
}

/* FIXME: We can't check this directly, but it can happen with
multiple modules.

class PropertyOrMethod {
  func member() -> Int { return 0 }
  let member = false

  class func methodOnClass(obj: PropertyOrMethod) -> Int { return 0 }
  let methodOnClass = false
}

func testPreferPropertyToMethod(obj: PropertyOrMethod) {
  let result = obj.member
  let resultChecked: Bool = result
  let called = obj.member()
  let calledChecked: Int = called
  let curried = obj.member as () -> Int

  let methodOnClass = PropertyOrMethod.methodOnClass
  let methodOnClassChecked: (PropertyOrMethod) -> Int = methodOnClass
}
*/

struct Foo { var foo: Int }

protocol ExtendedWithMutatingMethods { }
extension ExtendedWithMutatingMethods {
  mutating func mutatingMethod() {}
  var mutableProperty: Foo {
    get { }
    set { }
  }
  var nonmutatingProperty: Foo {
    get { }
    nonmutating set { }
  }
  var mutatingGetProperty: Foo {
    mutating get { }
    set { }
  }
}

class ClassExtendedWithMutatingMethods: ExtendedWithMutatingMethods {}
class SubclassExtendedWithMutatingMethods: ClassExtendedWithMutatingMethods {}

func testClassExtendedWithMutatingMethods(c: ClassExtendedWithMutatingMethods, // expected-note* {{}}
                                     sub: SubclassExtendedWithMutatingMethods) { // expected-note* {{}}
  c.mutatingMethod() // expected-error{{cannot use mutating member on immutable value: 'c' is a 'let' constant}}
  c.mutableProperty = Foo(foo: 0) // expected-error{{cannot assign to property}}
  c.mutableProperty.foo = 0 // expected-error{{cannot assign to property}}
  c.nonmutatingProperty = Foo(foo: 0)
  c.nonmutatingProperty.foo = 0
  c.mutatingGetProperty = Foo(foo: 0) // expected-error{{cannot use mutating}}
  c.mutatingGetProperty.foo = 0 // expected-error{{cannot use mutating}}
  _ = c.mutableProperty
  _ = c.mutableProperty.foo
  _ = c.nonmutatingProperty
  _ = c.nonmutatingProperty.foo
  // FIXME: diagnostic nondeterministically says "member" or "getter"
  _ = c.mutatingGetProperty // expected-error{{cannot use mutating}}
  _ = c.mutatingGetProperty.foo // expected-error{{cannot use mutating}}

  sub.mutatingMethod() // expected-error{{cannot use mutating member on immutable value: 'sub' is a 'let' constant}}
  sub.mutableProperty = Foo(foo: 0) // expected-error{{cannot assign to property}}
  sub.mutableProperty.foo = 0 // expected-error{{cannot assign to property}}
  sub.nonmutatingProperty = Foo(foo: 0)
  sub.nonmutatingProperty.foo = 0
  sub.mutatingGetProperty = Foo(foo: 0) // expected-error{{cannot use mutating}}
  sub.mutatingGetProperty.foo = 0 // expected-error{{cannot use mutating}}
  _ = sub.mutableProperty
  _ = sub.mutableProperty.foo
  _ = sub.nonmutatingProperty
  _ = sub.nonmutatingProperty.foo
  _ = sub.mutatingGetProperty // expected-error{{cannot use mutating}}
  _ = sub.mutatingGetProperty.foo // expected-error{{cannot use mutating}}

  var mutableC = c
  mutableC.mutatingMethod()
  mutableC.mutableProperty = Foo(foo: 0)
  mutableC.mutableProperty.foo = 0
  mutableC.nonmutatingProperty = Foo(foo: 0)
  mutableC.nonmutatingProperty.foo = 0
  mutableC.mutatingGetProperty = Foo(foo: 0)
  mutableC.mutatingGetProperty.foo = 0
  _ = mutableC.mutableProperty
  _ = mutableC.mutableProperty.foo
  _ = mutableC.nonmutatingProperty
  _ = mutableC.nonmutatingProperty.foo
  _ = mutableC.mutatingGetProperty
  _ = mutableC.mutatingGetProperty.foo

  var mutableSub = sub
  mutableSub.mutatingMethod()
  mutableSub.mutableProperty = Foo(foo: 0)
  mutableSub.mutableProperty.foo = 0
  mutableSub.nonmutatingProperty = Foo(foo: 0)
  mutableSub.nonmutatingProperty.foo = 0
  _ = mutableSub.mutableProperty
  _ = mutableSub.mutableProperty.foo
  _ = mutableSub.nonmutatingProperty
  _ = mutableSub.nonmutatingProperty.foo
  _ = mutableSub.mutatingGetProperty
  _ = mutableSub.mutatingGetProperty.foo
}

// <rdar://problem/18879585> QoI: error message for attempted access to instance properties in static methods are bad.
enum LedModules: Int {
  case WS2811_1x_5V
}

extension LedModules {
  static var watts: Double {
    return [0.30][self.rawValue] // expected-error {{instance member 'rawValue' cannot be used on type 'LedModules'}}
  }
}


// <rdar://problem/15117741> QoI: calling a static function on an instance produces a non-helpful diagnostic
class r15117741S {
  static func g() {}
}
func test15117741(s: r15117741S) {
  s.g() // expected-error {{static member 'g' cannot be used on instance of type 'r15117741S'}}
}


// <rdar://problem/22491394> References to unavailable decls sometimes diagnosed as ambiguous
struct UnavailMember {
  @available(*, unavailable)
  static var XYZ : X { get {} } // expected-note {{'XYZ' has been explicitly marked unavailable here}}
}

let _ : [UnavailMember] = [.XYZ] // expected-error {{'XYZ' is unavailable}}
let _ : [UnavailMember] = [.ABC] // expected-error {{type 'UnavailMember' has no member 'ABC'}}


// <rdar://problem/22490787> QoI: Poor error message iterating over property with non-sequence type that defines a Generator type alias
struct S22490787 {
  typealias Generator = AnyGenerator<Int>
}

func f22490787() {
  var path: S22490787 = S22490787()
  
  for p in path {  // expected-error {{type 'S22490787' does not conform to protocol 'SequenceType'}}
  }
}




