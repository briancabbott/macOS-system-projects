// RUN: %target-typecheck-verify-swift -enable-objc-interop

protocol P { }
@objc protocol OP { }
protocol CP : class { }

@objc protocol SP : OP {
  static func createNewOne() -> SP
}

func fP<T : P>(_ t: T?) { }
func fOP<T : OP>(_ t: T?) { }
func fOPE(_ t: OP) { }
func fSP<T : SP>(_ t: T?) { }
func fAO<T : AnyObject>(_ t: T?) { }
// expected-note@-1 {{where 'T' = 'any P'}}
// expected-note@-2 {{where 'T' = 'any CP'}}
// expected-note@-3 {{where 'T' = 'any OP & P'}}
func fAOE(_ t: AnyObject) { }
func fT<T>(_ t: T) { }

func testPassExistential(_ p: P, op: OP, opp: OP & P, cp: CP, sp: SP, any: Any, ao: AnyObject) {
  fP(p)
  fAO(p) // expected-error{{global function 'fAO' requires that 'any P' be a class type}}
  fAOE(p) // expected-error{{argument type 'any P' expected to be an instance of a class or class-constrained type}}
  fT(p)

  fOP(op)
  fAO(op)
  fAOE(op)
  fT(op)

  fAO(cp) // expected-error{{global function 'fAO' requires that 'any CP' be a class type}}
  fAOE(cp)
  fT(cp)

  fP(opp)
  fOP(opp)
  fAO(opp) // expected-error{{global function 'fAO' requires that 'any OP & P' be a class type}}
  fAOE(opp)
  fT(opp)

  fOP(sp)
  fSP(sp)
  fAO(sp)
  fAOE(sp)
  fT(sp)

  fT(any)

  fAO(ao)
  fAOE(ao)
}

class GP<T : P> {}
class GOP<T : OP> {}
class GCP<T : CP> {}
class GSP<T : SP> {}
class GAO<T : AnyObject> {} // expected-note 2{{requirement specified as 'T' : 'AnyObject'}}

func blackHole(_ t: Any) {}

func testBindExistential() {
  blackHole(GP<P>()) // expected-error{{type 'any P' cannot conform to 'P'}} expected-note {{only concrete types such as structs, enums and classes can conform to protocols}}
  blackHole(GOP<OP>())
  blackHole(GCP<CP>()) // expected-error{{type 'any CP' cannot conform to 'CP'}} expected-note {{only concrete types such as structs, enums and classes can conform to protocols}}
  blackHole(GAO<P>()) // expected-error{{'GAO' requires that 'any P' be a class type}}
  blackHole(GAO<OP>())
  blackHole(GAO<CP>()) // expected-error{{'GAO' requires that 'any CP' be a class type}}
  blackHole(GSP<SP>()) // expected-error{{'any SP' cannot be used as a type conforming to protocol 'SP' because 'SP' has static requirements}}
  blackHole(GAO<AnyObject>())
}

// rdar://problem/21087341
protocol Mine {}
class M1: Mine {}
class M2: Mine {}
extension Collection where Iterator.Element : Mine {
// expected-note@-1 {{required by referencing instance method 'takeAll()' on 'Collection'}}
    func takeAll() {}
}

func foo() {
  let allMine: [Mine] = [M1(), M2(), M1()]
  // FIXME: we used to have a better diagnostic here -- the type checker
  // would admit the conformance Mine : Mine, and later when computing
  // substitutions, a specific diagnostic was generated. Now the
  // conformance is rejected because Mine is not @objc, and we hit the
  // generic no overloads error path. The error should actually talk
  // about the return type, and this can happen in other contexts as well;
  // <rdar://problem/21900971> tracks improving QoI here.
  allMine.takeAll() // expected-error{{type 'any Mine' cannot conform to 'Mine'}} expected-note {{only concrete types such as structs, enums and classes can conform to protocols}}
}
