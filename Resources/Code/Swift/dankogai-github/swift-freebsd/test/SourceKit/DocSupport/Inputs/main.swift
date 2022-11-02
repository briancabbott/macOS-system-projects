var globV: Int

class CC0 {
  var x: Int = 0
}
class CC {
  var instV: CC0
  func meth() {}
  func instanceFunc0(a: Int, b: Float) -> Int {
    return 0
  }
  func instanceFunc1(a x: Int, b y: Float) -> Int {
    return 0
  }
  class func smeth() {}
  init() {
    instV = CC0()
  }
}

func +(a : CC, b: CC0) -> CC {
  return a
}

struct S {
  func meth() {}
  static func smeth() {}
}

enum E {
  case EElem
}

protocol Prot {
  func protMeth(a: Prot)
}

func foo(a: CC, b: E) {
  var b = b
  _ = b
  globV = 0
  a + a.instV
  a.meth()
  CC.smeth()
  b = E.EElem
  var _: CC
  class LocalCC {}
  var _: LocalCC
}

typealias CCAlias = CC

extension CC : Prot {
  func meth2(x: CCAlias) {}
  func protMeth(a: Prot) {}
  var extV : Int { return 0 }
}

class SubCC : CC {}

var globV2: SubCC

class ComputedProperty {
  var value : Int {
    get {
      let result = 0
      return result
    }
    set(newVal) {
      // completely ignore it!
    }
  }

  var readOnly : Int { return 0 }
}

class BC2 {
  func protMeth(a: Prot) {}
}
class SubC2 : BC2, Prot {
  override func protMeth(a: Prot) {}
}

class CC2 {
  subscript (i : Int) -> Int {
    get {
      return i
    }
    set(vvv) {
      vvv+1
    }
  }
}

func test1(cp: ComputedProperty, sub: CC2) {
  var x = cp.value
  x = cp.readOnly
  cp.value = x
  ++cp.value
  x = sub[0]
  sub[0] = x
  ++sub[0]
}

struct S2 {
  func sfoo() {}
}

var globReadOnly : S2 {
  get {
    return S2();
  }
}

func test2() {
  globReadOnly.sfoo()
}

class B1 {
  func foo() {}
}

class SB1 : B1 {
  override func foo() {
    foo()
    self.foo()
    super.foo()
  }
}

func test3(c: SB1, s: S2) {
  test2()
  c.foo()
  s.sfoo()
}

func test4(inout a: Int) {}

protocol Prot2 {
  typealias Element
  var p : Int { get }
  func foo()
}

struct S1 : Prot2 {
  typealias Element = Int
  var p : Int = 0
  func foo() {}
}

func genfoo<T : Prot2 where T.Element == Int>(x: T) {}

protocol Prot3 {
  func +(x: Self, y: Self)
}
