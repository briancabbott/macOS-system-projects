// RUN: %target-parse-verify-swift

//===----------------------------------------------------------------------===//
// Tests for various simple enum constructs
//===----------------------------------------------------------------------===//


public enum unionSearchFlags {
  case None
  case Backwards
  case Anchored

  init() { self = .None }
}

func test1() -> unionSearchFlags {
  let _ : unionSearchFlags
  var b = unionSearchFlags.None
  b = unionSearchFlags.Anchored
  _ = b

  return unionSearchFlags.Backwards
}

func test1a() -> unionSearchFlags {
  var _ : unionSearchFlags
  var b : unionSearchFlags = .None
  b = .Anchored
  _ = b

  // ForwardIndexType use of MaybeInt.
  _ = MaybeInt.None

  return .Backwards
}

func test1b(b : Bool) {
  _ = 123
  _ = .description == 1 // expected-error{{type of expression is ambiguous without more context}} 
}

enum MaybeInt {
  case None
  case Some(Int)

  init(_ i: Int) { self = MaybeInt.Some(i) }
}

func test2(a: Int, _ b: Int, _ c: MaybeInt) {
  _ = MaybeInt.Some(4)
  _ = MaybeInt.Some
  _ = MaybeInt.Some(b)

  test2(1, 2, .None)
}

enum ZeroOneTwoThree {
  case Zero
  case One(Int)
  case Two(Int, Int)
  case Three(Int,Int,Int)
  case Unknown(MaybeInt, MaybeInt, MaybeInt)

  init (_ i: Int) { self = .One(i) }
  init (_ i: Int, _ j: Int, _ k: Int) { self = .Three(i, j, k) }
  init (_ i: MaybeInt, _ j: MaybeInt, _ k: MaybeInt) { self = .Unknown(i, j, k) }
}

func test3(a: ZeroOneTwoThree) {
  _ = ZeroOneTwoThree.Three(1,2,3)
  _ = ZeroOneTwoThree.Unknown(MaybeInt.None, MaybeInt.Some(4),
                                  MaybeInt.Some(32))
  _ = ZeroOneTwoThree(MaybeInt.None, MaybeInt(4), MaybeInt(32))

  var _ : Int =
     ZeroOneTwoThree.Zero // expected-error {{cannot convert value of type 'ZeroOneTwoThree' to specified type 'Int'}}

  test3 ZeroOneTwoThree.Zero // expected-error {{expression resolves to an unused function}} expected-error{{consecutive statements}} {{8-8=;}}
  test3 (ZeroOneTwoThree.Zero)
  test3(ZeroOneTwoThree.Zero)
  test3 // expected-error {{expression resolves to an unused function}}
  (ZeroOneTwoThree.Zero)
  
  var _ : ZeroOneTwoThree = .One(4)
  
  var _ : (Int,Int) -> ZeroOneTwoThree = .Two // expected-error{{type '(Int, Int) -> ZeroOneTwoThree' has no member 'Two'}}
  var _ : Int = .Two // expected-error{{type 'Int' has no member 'Two'}}
}

func test3a(a: ZeroOneTwoThree) {
  var e : ZeroOneTwoThree = (.Three(1, 2, 3))
  var f = ZeroOneTwoThree.Unknown(.None, .Some(4), .Some(32))

  var g = .None  // expected-error {{type of expression is ambiguous without more context}}

  // Overload resolution can resolve this to the right constructor.
  var h = ZeroOneTwoThree(1)

  test3a;  // expected-error {{unused function}}
  .Zero   // expected-error {{type of expression is ambiguous without more context}}
  test3a   // expected-error {{unused function}}
  (.Zero) // expected-error {{type of expression is ambiguous without more context}}
  test3a(.Zero)
}


struct CGPoint { var x : Int, y : Int }
typealias OtherPoint = ( x : Int, y : Int)

func test4() {
  var a : CGPoint
  // Note: we reject the following because it conflicts with the current
  // "init" hack.
  var b = CGPoint.CGPoint(1, 2) // expected-error {{type 'CGPoint' has no member 'CGPoint'}}
  var c = CGPoint(x: 2, y : 1)   // Using injected name.

  var e = CGPoint.x // expected-error {{member 'x' cannot be used on type 'CGPoint'}}
  var f = OtherPoint.x  // expected-error {{type 'OtherPoint' (aka '(x: Int, y: Int)') has no member 'x'}}
}



struct CGSize { var width : Int, height : Int }

extension CGSize {
  func area() -> Int {
    return width*self.height
  }
  
  func area_wrapper() -> Int {
    return area()
  }
}

struct CGRect { 
  var origin : CGPoint,
  size : CGSize
  
  func area() -> Int {
    return self.size.area()
  }
}

func area(r: CGRect) -> Int {
  return r.size.area()
}

extension CGRect {
  func search(x: Int) -> CGSize {}
  func bad_search(_: Int) -> CGSize {}
}

func test5(myorigin: CGPoint) {
  let x1 = CGRect(origin: myorigin, size: CGSize(width: 42, height: 123))
  let x2 = x1

  4+5

  // Dot syntax.
  _ = x2.origin.x
  _ = x1.size.area()
  _ = (r : x1.size).r.area()
  _ = x1.size.area()
  _ = (r : x1.size).r.area()
  
  _ = x1.area

  _ = x1.search(42)
  _ = x1.search(42).width

  // TODO: something like this (name binding on the LHS):
  // var (CGSize(width, height)) = CGSize(1,2)

  // TODO: something like this, how do we get it in scope in the {} block?
  //if (var Some(x) = somemaybeint) { ... }

  
}

struct StructTest1 {
  var a : Int, c, b : Int


  typealias ElementType = Int
}

enum UnionTest1 {
  case x
  case y(Int)

  func foo() {}

  init() { self = .x }
}


extension UnionTest1 {
  func food() {}
  func bar() {}

  // Type method.
  static func baz() {}
}

struct EmptyStruct {
  func foo() {}
}

func f() { 
  let a : UnionTest1
  a.bar()
  UnionTest1.baz()  // dot syntax access to a static method.
  
  // Test that we can get the "address of a member".
  var _ : () -> () = UnionTest1.baz
  var _ : (UnionTest1) -> () -> () = UnionTest1.bar
}

func union_error(a: ZeroOneTwoThree) {
  var _ : ZeroOneTwoThree = .Zero(1) // expected-error {{contextual member 'Zero' has no associated value}}
  var _ : ZeroOneTwoThree = .One // expected-error {{contextual member 'One' expects argument of type 'Int'}}
  var _ : ZeroOneTwoThree = .foo // expected-error {{type 'ZeroOneTwoThree' has no member 'foo'}}
  var _ : ZeroOneTwoThree = .foo() // expected-error {{type 'ZeroOneTwoThree' has no member 'foo'}}
}

func local_struct() {
  struct s { func y() {} }
}

//===----------------------------------------------------------------------===//
// A silly units example showing "user defined literals".
//===----------------------------------------------------------------------===//

struct distance { var v : Int }

func - (lhs: distance, rhs: distance) -> distance {}

extension Int {
  func km() -> distance {}
  func cm() -> distance {}
}

func units(x: Int) -> distance {
  x.km() - 4.cm() - 42.km()
}



var %% : distance -> distance // expected-error {{expected pattern}} 

func badTupleElement() {
  typealias X = (x : Int, y : Int)
  var y = X.y // expected-error{{type 'X' (aka '(x: Int, y: Int)') has no member 'y'}}
  var z = X.z // expected-error{{type 'X' (aka '(x: Int, y: Int)') has no member 'z'}}
}

enum Direction {
  case North(distance: Int)
  case NorthEast(distanceNorth: Int, distanceEast: Int)
}

func testDirection() {
  var dir: Direction = .North(distance: 5)
  dir = .NorthEast(distanceNorth: 5, distanceEast: 7)

  var i: Int
  switch dir {
  case .North(let x):
    i = x
    break;

  case .NorthEast(let x):
    i = x.distanceEast
    break;
  }
  _ = i
}

enum NestedSingleElementTuple {
  case Case(x: (y: Int)) // expected-error{{cannot create a single-element tuple with an element label}} {{17-20=}}
}

enum SimpleEnum {
  case X, Y
}

func testSimpleEnum() {
  let _ : SimpleEnum = .X
  let _ : SimpleEnum = (.X)
  let _ : SimpleEnum=.X    // expected-error {{postfix '=' is reserved}}
}



