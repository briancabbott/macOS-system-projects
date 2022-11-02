// RUN: %target-parse-verify-swift

// -----

protocol Foo {
  typealias Bar : Foo // expected-error{{type may not reference itself as a requirement}}
}

struct Oroborous : Foo {
  typealias Bar = Oroborous
}

// -----

protocol P {
 typealias A : P // expected-error{{type may not reference itself as a requirement}}
}

struct X<T: P> {
}

func f<T : P>(z: T) {
 _ = X<T.A>()
}

// -----

protocol PP2 {
  typealias A : P2 = Self // expected-error{{type may not reference itself as a requirement}}
}

protocol P2 : PP2 {
  typealias A = Self
}

struct X2<T: P2> {
}

struct Y2 : P2 {
  typealias A = Y2
}

func f<T : P2>(z: T) {
 _ = X2<T.A>()
}

// -----

protocol P3 {
 typealias A: P4 = Self // expected-error{{type may not reference itself as a requirement}}
}

protocol P4 : P3 {}

protocol DeclaredP : P3, P4 {}

struct Y3 : DeclaredP {
}

struct X3<T:P4> {}

func f2<T:P4>(a: T) {
 _ = X3<T.A>()
}

f2(Y3())

// -----

protocol Alpha {
  typealias Beta: Gamma // expected-error{{type may not reference itself as a requirement}}
}

protocol Gamma {
  typealias Delta: Alpha // expected-error{{type may not reference itself as a requirement}}
}

struct Epsilon<T: Alpha, U: Gamma where T.Beta == U, U.Delta == T> { }

// -----

protocol AsExistentialA {
  var delegate : AsExistentialB? { get }
}
protocol AsExistentialB {
  func aMethod(object : AsExistentialA)
}

protocol AsExistentialAssocTypeA {
  var delegate : AsExistentialAssocTypeB? { get } // expected-error 3{{protocol 'AsExistentialAssocTypeB' can only be used as a generic constraint because it has Self or associated type requirements}}
}
protocol AsExistentialAssocTypeB {
  func aMethod(object : AsExistentialAssocTypeA)
  typealias Bar
}

protocol AsExistentialAssocTypeAgainA {
  var delegate : AsExistentialAssocTypeAgainB? { get }
  typealias Bar
}
protocol AsExistentialAssocTypeAgainB {
  func aMethod(object : AsExistentialAssocTypeAgainA) // expected-error * {{protocol 'AsExistentialAssocTypeAgainA' can only be used as a generic constraint because it has Self or associated type requirements}}
}

