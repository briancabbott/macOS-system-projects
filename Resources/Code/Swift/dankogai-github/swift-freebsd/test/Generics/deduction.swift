// RUN: %target-parse-verify-swift

//===----------------------------------------------------------------------===//
// Deduction of generic arguments
//===----------------------------------------------------------------------===//

func identity<T>(value: T) -> T { return value }

func identity2<T>(value: T) -> T { return value }
func identity2<T>(value: T) -> Int { return 0 }

struct X { }
struct Y { }

func useIdentity(x: Int, y: Float, i32: Int32) {
  var x2 = identity(x)
  var y2 = identity(y)

  // Deduction that involves the result type
  x2 = identity(17)
  var i32_2 : Int32 = identity(17)

  // Deduction where the result type and input type can get different results
  var xx : X, yy : Y
  xx = identity(yy) // expected-error{{cannot convert value of type 'Y' to expected argument type 'X'}}
  xx = identity2(yy) // expected-error{{cannot convert value of type 'Y' to expected argument type 'X'}}
}

// FIXME: Crummy diagnostic!
func twoIdentical<T>(x: T, _ y: T) -> T {}

func useTwoIdentical(xi: Int, yi: Float) {
  var x = xi, y = yi
  x = twoIdentical(x, x)
  y = twoIdentical(y, y)
  x = twoIdentical(x, 1)
  x = twoIdentical(1, x)
  y = twoIdentical(1.0, y)
  y = twoIdentical(y, 1.0)
  
  twoIdentical(x, y) // expected-error{{cannot invoke 'twoIdentical' with an argument list of type '(Int, Float)'}} expected-note{{expected an argument list of type '(T, T)'}}
}

func mySwap<T>(inout x: T,
               inout _ y: T) {
  let tmp = x
  x = y
  y = tmp
}

func useSwap(xi: Int, yi: Float) {
  var x = xi, y = yi
  mySwap(&x, &x)
  mySwap(&y, &y)
  
  mySwap(x, x) // expected-error {{passing value of type 'Int' to an inout parameter requires explicit '&'}} {{10-10=&}}
    // expected-error @-1 {{passing value of type 'Int' to an inout parameter requires explicit '&'}} {{13-13=&}}
  
  mySwap(&x, &y) // expected-error{{cannot convert value of type 'inout Int' to expected argument type 'inout _'}}
}

func takeTuples<T, U>(_: (T, U), _: (U, T)) {
}

func useTuples(x: Int, y: Float, z: (Float, Int)) {
  takeTuples((x, y), (y, x))

  takeTuples((x, y), (x, y)) // expected-error{{cannot convert value of type '(Int, Float)' to expected argument type '(_, _)'}}

  // FIXME: Use 'z', which requires us to fix our tuple-conversion
  // representation.
}

func acceptFunction<T, U>(f: (T) -> U, _ t: T, _ u: U) {}

func passFunction(f: (Int) -> Float, x: Int, y: Float) {
   acceptFunction(f, x, y)
   acceptFunction(f, y, y) // expected-error{{cannot convert value of type '(Int) -> Float' to expected argument type '(_) -> _'}}
}

func returnTuple<T, U>(_: T) -> (T, U) { }

func testReturnTuple(x: Int, y: Float) {
  returnTuple(x) // expected-error{{cannot invoke 'returnTuple' with an argument list of type '(Int)'}}
  // expected-note @-1 {{expected an argument list of type '(T)'}}
  
  var _ : (Int, Float) = returnTuple(x)
  var _ : (Float, Float) = returnTuple(y)

  // <rdar://problem/22333090> QoI: Propagate contextual information in a call to operands
  var _ : (Int, Float) = returnTuple(y) // expected-error{{cannot convert value of type 'Float' to expected argument type 'Int'}}
}


func confusingArgAndParam<T, U>(f: (T) -> U, _ g: (U) -> T) {
  confusingArgAndParam(g, f)
  confusingArgAndParam(f, g)
}

func acceptUnaryFn<T, U>(f: (T) -> U) { }
func acceptUnaryFnSame<T>(f: (T) -> T) { }

func acceptUnaryFnRef<T, U>(inout f: (T) -> U) { }
func acceptUnaryFnSameRef<T>(inout f: (T) -> T) { }

func unaryFnIntInt(_: Int) -> Int {}

func unaryFnOvl(_: Int) -> Int {} // expected-note{{found this candidate}}
func unaryFnOvl(_: Float) -> Int {} // expected-note{{found this candidate}}

// Variable forms of the above functions
var unaryFnIntIntVar : (Int) -> Int = unaryFnIntInt

func passOverloadSet() {
  // Passing a non-generic function to a generic function
  acceptUnaryFn(unaryFnIntInt)
  acceptUnaryFnSame(unaryFnIntInt)

  // Passing an overloaded function set to a generic function
  // FIXME: Yet more terrible diagnostics.
  acceptUnaryFn(unaryFnOvl)  // expected-error{{ambiguous use of 'unaryFnOvl'}}
  acceptUnaryFnSame(unaryFnOvl)

  // Passing a variable of function type to a generic function
  acceptUnaryFn(unaryFnIntIntVar)
  acceptUnaryFnSame(unaryFnIntIntVar)

  // Passing a variable of function type to a generic function to an inout parameter
  acceptUnaryFnRef(&unaryFnIntIntVar)
  acceptUnaryFnSameRef(&unaryFnIntIntVar)

  acceptUnaryFnRef(unaryFnIntIntVar) // expected-error{{passing value of type '(Int) -> Int' to an inout parameter requires explicit '&'}} {{20-20=&}}
}

func acceptFnFloatFloat(f: (Float) -> Float) {}
func acceptFnDoubleDouble(f: (Double) -> Double) {}

func passGeneric() {
  acceptFnFloatFloat(identity)
  acceptFnFloatFloat(identity2)
}

//===----------------------------------------------------------------------===//
// Simple deduction for generic member functions
//===----------------------------------------------------------------------===//
struct SomeType {
  func identity<T>(x: T) -> T { return x }

  func identity2<T>(x: T) -> T { return x } // expected-note 2{{found this candidate}}
  func identity2<T>(x: T) -> Float { } // expected-note 2{{found this candidate}}

  func returnAs<T>() -> T {}
}

func testMemberDeduction(sti: SomeType, ii: Int, fi: Float) {
  var st = sti, i = ii, f = fi
  i = st.identity(i)
  f = st.identity(f)
  i = st.identity2(i)
  f = st.identity2(f) // expected-error{{ambiguous use of 'identity2'}}
  i = st.returnAs()
  f = st.returnAs()
  acceptFnFloatFloat(st.identity)
  acceptFnFloatFloat(st.identity2) // expected-error{{ambiguous use of 'identity2'}}
  acceptFnDoubleDouble(st.identity2)
}

struct StaticFuncs {
  static func chameleon<T>() -> T {}
  func chameleon2<T>() -> T {}
}

struct StaticFuncsGeneric<U> {
  // FIXME: Nested generics are very broken
  // static func chameleon<T>() -> T {}
}

func chameleon<T>() -> T {}

func testStatic(sf: StaticFuncs, sfi: StaticFuncsGeneric<Int>) {
  var x: Int16
  x = StaticFuncs.chameleon()
  x = sf.chameleon2()
  // FIXME: Nested generics are very broken
  // x = sfi.chameleon()
  // typealias SFI = StaticFuncsGeneric<Int>
  // x = SFI.chameleon()
  _ = x
}

//===----------------------------------------------------------------------===//
// Deduction checking for constraints
//===----------------------------------------------------------------------===//
protocol IsBefore {
  func isBefore(other: Self) -> Bool
}

func min2<T : IsBefore>(x: T, _ y: T) -> T {
  if y.isBefore(x) { return y }
  return x
}

extension Int : IsBefore {
  func isBefore(other: Int) -> Bool { return self < other }
}

func callMin(x: Int, y: Int, a: Float, b: Float) {
  min2(x, y)
  min2(a, b) // expected-error{{cannot invoke 'min2' with an argument list of type '(Float, Float)'}} expected-note {{expected an argument list of type '(T, T)'}}
}

func rangeOfIsBefore<
  R : GeneratorType where R.Element : IsBefore
>(range : R) { }


func callRangeOfIsBefore(ia: [Int], da: [Double]) {
  rangeOfIsBefore(ia.generate())
  rangeOfIsBefore(da.generate()) // expected-error{{cannot invoke 'rangeOfIsBefore' with an argument list of type '(IndexingGenerator<[Double]>)'}} expected-note{{expected an argument list of type '(R)'}}
}

//===----------------------------------------------------------------------===//
// Deduction for member operators
//===----------------------------------------------------------------------===//
protocol Addable {
  func +(x: Self, y: Self) -> Self
}
func addAddables<T : Addable, U>(x: T, y: T, u: U) -> T {
  u + u // expected-error{{binary operator '+' cannot be applied to two 'U' operands}}
  // expected-note @-1 {{overloads for '+' exist with these partially matching parameter lists: }}
  return x+y
}

//===----------------------------------------------------------------------===//
// Deduction for bound generic types
//===----------------------------------------------------------------------===//
struct MyVector<T> { func size() -> Int {} }

func getVectorSize<T>(v: MyVector<T>) -> Int {
  return v.size()
}

func ovlVector<T>(v: MyVector<T>) -> X {}
func ovlVector<T>(v: MyVector<MyVector<T>>) -> Y {}

func testGetVectorSize(vi: MyVector<Int>, vf: MyVector<Float>) {
  var i : Int
  i = getVectorSize(vi)
  i = getVectorSize(vf)

  getVectorSize(i) // expected-error{{cannot convert value of type 'Int' to expected argument type 'MyVector<_>'}}

  var x : X, y : Y
  x = ovlVector(vi)
  x = ovlVector(vf)
  
  var vvi : MyVector<MyVector<Int>>
  y = ovlVector(vvi)

  var yy = ovlVector(vvi)
  yy = y
  y = yy
}

// <rdar://problem/15104554>
postfix operator <*> {}

protocol MetaFunction {
  typealias Result
  postfix func <*> (_: Self) -> Result?
}

protocol Bool_ {}
struct False : Bool_ {}
struct True : Bool_ {}

postfix func <*> <B:Bool_>(_: Test<B>) -> Int? { return .None }
postfix func <*> (_: Test<True>) -> String? { return .None }

class Test<C: Bool_> : MetaFunction {
  typealias Result = Int
} // picks first <*>
typealias Inty = Test<True>.Result 
var iy : Inty = 5 // okay, because we picked the first <*>
var iy2 : Inty = "hello" // expected-error{{cannot convert value of type 'String' to specified type 'Inty' (aka 'Int')}}

// rdar://problem/20577950
class DeducePropertyParams {
  let badSet: Set = ["Hello"]
}
