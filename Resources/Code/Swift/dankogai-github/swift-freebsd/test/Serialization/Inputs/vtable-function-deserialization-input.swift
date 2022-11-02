// All of this is required in order to produce materializeForSet
// declarations for A's properties.
public protocol NilLiteralConvertible {
  init(nilLiteral: ())
}
public enum Optional<T>: NilLiteralConvertible {
  case Some(T)
  case None

  public init(nilLiteral: ()) { self = .None }
}

public struct Y {}

public struct X<U> {
  public var a : U

  public init(_a : U) {
    a = _a
  }

  public func doneSomething() {}
}

public class A {
  public var y : Y
  public var x : X<Y>

  public init() {
    y = Y()
    x = X<Y>(_a: y)
  }

  public func doSomething() {
    x.doneSomething()
  }
}
