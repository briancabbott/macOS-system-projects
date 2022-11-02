// RUN: %target-parse-verify-swift

class A {
  func do_a() {}

  func do_b(x: Int) {}
  func do_b(x: Float) {}

  func do_c(x x: Int) {}
  func do_c(y y: Int) {} 
}

func test0(a : A!) {
  a.do_a()

  a.do_b(1)
  a.do_b(5.0)

  a.do_c(1) // expected-error {{argument labels '(_:)' do not match any available overloads}}
  // expected-note @-1 {{overloads for 'do_c' exist with these partially matching parameter lists: (x: Int), (y: Int)}}
  a.do_c(x: 1)
}

func test1(a : A!) {
  a?.do_a()

  a?.do_b(1)
  a?.do_b(5.0)

  a?.do_c(1) // expected-error {{argument labels '(_:)' do not match any available overloads}}
  // expected-note @-1 {{overloads for 'do_c' exist with these partially matching parameter lists: (x: Int), (y: Int)}}
  a?.do_c(x: 1)
}

struct B {
  var x : Int
}

func test2(b : B!) {
  var b = b
  let x = b.x
  b.x = x
  b = nil
}

struct Subscriptable {
  subscript(x : Int) -> Int {
    get {
      return x
    }
  }
}

func test3(x: Subscriptable!) -> Int {
  return x[0]
}

// Callable
func test4(f: (Int -> Float)!) -> Float {
  return f(5)
}

func test5(value : Int!) {
  let _ : Int? = value
}

func test6(value : Int!) {
  let _ : Int? = value
}

class Test9a {}
class Test9b : Test9a {}
func test9_produceUnchecked() -> Test9b! { return Test9b() }
func test9_consume(foo : Test9b) {}
func test9() -> Test9a {
  let foo = test9_produceUnchecked()
  test9_consume(foo)
  let _ : Test9a = foo
  return foo
}

func test10_helper(x : Int!) -> Int? { return x }
func test10(x : Int?) -> Int! { return test10_helper(x) }

// Fall back to object type behind an implicitly-unwrapped optional.
protocol P11 { }
extension Int : P11 { }
func test11_helper<T : P11>(t: T) { }
func test11(i: Int!, j: Int!) {
  var j = j
  test11_helper(i)
  test11_helper(j)
  j = nil
}
