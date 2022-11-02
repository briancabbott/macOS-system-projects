// RUN: %target-parse-verify-swift

// Types in expression contexts must be followed by a member access or
// constructor call.

struct Foo {
  struct Bar {
    init() {}
    static var prop: Int = 0
    static func meth() {}
    func instMeth() {}
  }
  init() {}
  static var prop: Int = 0
  static func meth() {}
  func instMeth() {}
}

protocol Zim {
  typealias Zang

  init()
  // TODO class var prop: Int { get }
  static func meth() {} // expected-error{{protocol methods may not have bodies}}
  func instMeth() {} // expected-error{{protocol methods may not have bodies}}
}

protocol Bad {
  init() {} // expected-error{{protocol initializers may not have bodies}}
}

struct Gen<T> {
  struct Bar { // expected-error{{nested in generic type}}
    init() {}
    static var prop: Int { return 0 }
    static func meth() {}
    func instMeth() {}
  }

  init() {}
  static var prop: Int { return 0 }
  static func meth() {}
  func instMeth() {}
}

func unqualifiedType() {
  _ = Foo.self
  _ = Foo.self
  _ = Foo()
  _ = Foo.prop
  _ = Foo.meth
  let _ : () = Foo.meth()
  _ = Foo.instMeth

  _ = Foo // expected-error{{expected member name or constructor call after type name}} expected-note{{add arguments}} {{10-10=()}} expected-note{{use '.self'}} {{10-10=.self}}
  _ = Foo.dynamicType // expected-error{{'.dynamicType' is not allowed after a type name}} {{11-22=self}}

  _ = Bad // expected-error{{expected member name or constructor call after type name}}
  // expected-note@-1{{use '.self' to reference the type object}}{{10-10=.self}}
}

func qualifiedType() {
  _ = Foo.Bar.self
  let _ : Foo.Bar.Type = Foo.Bar.self
  let _ : Foo.Protocol = Foo.self // expected-error{{cannot use 'Protocol' with non-protocol type 'Foo'}}
  _ = Foo.Bar()
  _ = Foo.Bar.prop
  _ = Foo.Bar.meth
  let _ : () = Foo.Bar.meth()
  _ = Foo.Bar.instMeth

  _ = Foo.Bar // expected-error{{expected member name or constructor call after type name}} expected-note{{add arguments}} {{14-14=()}} expected-note{{use '.self'}} {{14-14=.self}}
  _ = Foo.Bar.dynamicType // expected-error{{'.dynamicType' is not allowed after a type name}} {{15-26=self}}
}

/* TODO allow '.Type' in expr context
func metaType() {
  let ty = Foo.Type.self
  let metaTy = Foo.Type.self

  let badTy = Foo.Type
  let badMetaTy = Foo.Type.dynamicType
}
 */

func genType() {
  _ = Gen<Foo>.self
  _ = Gen<Foo>()
  _ = Gen<Foo>.prop
  _ = Gen<Foo>.meth
  let _ : () = Gen<Foo>.meth()
  _ = Gen<Foo>.instMeth

  // Misparses because generic parameter disambiguation rejects '>' not
  // followed by '.' or '('
  _ = Gen<Foo> // expected-error{{not a postfix unary operator}}
  _ = Gen<Foo>.dynamicType // expected-error{{'.dynamicType' is not allowed after a type name}} {{16-27=self}}
}

func genQualifiedType() {
  _ = Gen<Foo>.Bar.self
  _ = Gen<Foo>.Bar()
  _ = Gen<Foo>.Bar.prop
  _ = Gen<Foo>.Bar.meth
  let _ : () = Gen<Foo>.Bar.meth()
  _ = Gen<Foo>.Bar.instMeth

  _ = Gen<Foo>.Bar // expected-error{{expected member name or constructor call after type name}} expected-note{{add arguments}} {{19-19=()}} expected-note{{use '.self'}} {{19-19=.self}}
  _ = Gen<Foo>.Bar.dynamicType // expected-error{{'.dynamicType' is not allowed after a type name}} {{20-31=self}}
}

func archetype<T: Zim>(_: T) {
  _ = T.self
  _ = T()
  // TODO let prop = T.prop
  _ = T.meth
  let _ : () = T.meth()

  _ = T // expected-error{{expected member name or constructor call after type name}} expected-note{{add arguments}} {{8-8=()}} expected-note{{use '.self'}} {{8-8=.self}}
  _ = T.dynamicType // expected-error{{'.dynamicType' is not allowed after a type name}} {{9-20=self}}
}

func assocType<T: Zim where T.Zang: Zim>(_: T) {
  _ = T.Zang.self
  _ = T.Zang()
  // TODO _ = T.Zang.prop
  _ = T.Zang.meth
  let _ : () = T.Zang.meth()

  _ = T.Zang // expected-error{{expected member name or constructor call after type name}} expected-note{{add arguments}} {{13-13=()}} expected-note{{use '.self'}} {{13-13=.self}}
  _ = T.Zang.dynamicType // expected-error{{'.dynamicType' is not allowed after a type name}} {{14-25=self}}
}

class B {
  class func baseMethod() {}
}
class D: B {
  class func derivedMethod() {}
}

func derivedType() {
  let _: B.Type = D.self
  _ = D.baseMethod
  let _ : () = D.baseMethod()

  let _: D.Type = D.self
  _ = D.derivedMethod
  let _ : () = D.derivedMethod()

  let _: B.Type = D // expected-error{{expected member name or constructor call after type name}} expected-note{{add arguments}} {{20-20=()}} expected-note{{use '.self'}} {{20-20=.self}}
  let _: D.Type = D // expected-error{{expected member name or constructor call after type name}} expected-note{{add arguments}} {{20-20=()}} expected-note{{use '.self'}} {{20-20=.self}}
  let _: D.Type.Type = D.dynamicType // expected-error{{'.dynamicType' is not allowed after a type name}} {{26-37=self}}
}

// Referencing a nonexistent member or constructor should not trigger errors
// about the type expression.
func nonexistentMember() {
  let cons = Foo("this constructor does not exist") // expected-error{{argument passed to call that takes no arguments}}
  let prop = Foo.nonexistent // expected-error{{type 'Foo' has no member 'nonexistent'}}
  let meth = Foo.nonexistent() // expected-error{{type 'Foo' has no member 'nonexistent'}}
}

protocol P {}

func meta_metatypes() {
  let _: P.Protocol = P.self
  _ = P.Type.self
  _ = P.Protocol.self
  _ = P.Protocol.Protocol.self // expected-error{{cannot use 'Protocol' with non-protocol type 'P.Protocol'}}
  _ = P.Protocol.Type.self
  _ = B.Type.self
}
