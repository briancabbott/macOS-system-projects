public func getZero() -> Int {
  return 0
}

public func getInput(x x: Int) -> Int {
  return x
}

public func getSecond(_: Int, y: Int) -> Int {
  return y
}

public func useNested(_: (x: Int, y: Int), n: Int) {}

public func variadic(x x: Double, _ y: Int...) {}
public func variadic2(y: Int..., x: Double) {}

public func slice(x x: [Int]) {}
public func optional(x x: Int?) {}

public func overloaded(x x: Int) {}
public func overloaded(x x: Bool) {}

// Generic functions.
public func makePair<A, B>(a a: A, b: B) -> (A, B) {
  return (a, b)
}

public func different<T : Equatable>(a a: T, b: T) -> Bool {
  return a != b
}

public func different2<T where T : Equatable>(a a: T, b: T) -> Bool {
  return a != b
}

public func selectorFunc1(a a: Int, b x: Int) {}

public protocol Wrapped {
  typealias Value : Equatable
  
  //var value : Value
  func getValue() -> Value
}

public func differentWrapped<
  T : Wrapped, U : Wrapped
  where
  T.Value == U.Value
>(a a: T, b: U) -> Bool {
  return a.getValue() != b.getValue()
}

@noreturn @_silgen_name("exit") public func exit ()->()

@noreturn public func testNoReturnAttr() -> () { exit() }
@noreturn public func testNoReturnAttrPoly<T>(x x: T) -> () { exit() }


@_silgen_name("primitive") public func primitive()

public protocol EqualOperator {
  func ==(x: Self, y: Self) -> Bool
}

public func throws1() throws {}
public func throws2<T>(t: T) throws -> T { return t }

@warn_unused_result(message="you might want to keep it")
public func mineGold() -> Int { return 1 }

public struct Foo {
  public init() { }

  @warn_unused_result(mutable_variant="reverseInPlace")
  public func reverse() -> Foo { return self }

  public mutating func reverseInPlace() { }
}
