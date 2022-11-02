// RUN: %target-parse-verify-swift

// REQUIRES: objc_interop

class Root : Hashable { 
  var hashValue: Int {
    return 0
  }
}

func ==(x: Root, y: Root) -> Bool { return true }

class ObjC : Root {
  var x = 0
}

class DerivesObjC : ObjC { }

struct BridgedToObjC : Hashable, _ObjectiveCBridgeable {
  static func _isBridgedToObjectiveC() -> Bool {
    return true
  }
  
  static func _getObjectiveCType() -> Any.Type {
    return ObjC.self
  }
  func _bridgeToObjectiveC() -> ObjC {
    return ObjC()
  }
  static func _forceBridgeFromObjectiveC(
    x: ObjC,
    inout result: BridgedToObjC?
  ) {
  }
  static func _conditionallyBridgeFromObjectiveC(
    x: ObjC,
    inout result: BridgedToObjC?
  ) -> Bool {
    return true
  }

  var hashValue: Int {
    return 0
  }
}

func ==(x: BridgedToObjC, y: BridgedToObjC) -> Bool { return true }

func testUpcastBridge() {
  var setR = Set<Root>()
  var setO = Set<ObjC>()
  var setD = Set<DerivesObjC>()
  var setB = Set<BridgedToObjC>()

  // Upcast to object types.
  setR = setB; _ = setR
  setO = setB; _ = setO

  // Upcast object to bridged type
  setB = setO // expected-error{{cannot assign value of type 'Set<ObjC>' to type 'Set<BridgedToObjC>'}}

  // Failed upcast
  setD = setB // expected-error{{cannot assign value of type 'Set<BridgedToObjC>' to type 'Set<DerivesObjC>'}}
  _ = setD
}

func testForcedDowncastBridge() {
  let setR = Set<Root>()
  let setO = Set<ObjC>()
  let setD = Set<DerivesObjC>()
  let setB = Set<BridgedToObjC>()

  setR as! Set<BridgedToObjC>
  setO as! Set<BridgedToObjC>
  setD as! Set<BridgedToObjC> // expected-error {{'ObjC' is not a subtype of 'DerivesObjC'}}
  // expected-note @-1 {{in cast from type 'Set<DerivesObjC>' to 'Set<BridgedToObjC>'}}

  // TODO: the diagnostic for the below two examples should indicate that 'as'
  // should be used instead of 'as!'
  setB as! Set<Root> // expected-error {{'Root' is not a subtype of 'BridgedToObjC'}}
  // expected-note @-1 {{in cast from type 'Set<BridgedToObjC>' to 'Set<Root>'}}
  setB as! Set<ObjC> // expected-error {{'ObjC' is not a subtype of 'BridgedToObjC'}}
  // expected-note @-1 {{in cast from type 'Set<BridgedToObjC>' to 'Set<ObjC>'}}
  setB as! Set<DerivesObjC> // expected-error {{'DerivesObjC' is not a subtype of 'BridgedToObjC'}}
  // expected-note @-1 {{in cast from type 'Set<BridgedToObjC>' to 'Set<DerivesObjC>'}}
}

func testConditionalDowncastBridge() {
  var setR = Set<Root>()
  var setO = Set<ObjC>()
  var setD = Set<DerivesObjC>()
  var setB = Set<BridgedToObjC>()

  if let s = setR as? Set<BridgedToObjC> { }
  if let s = setO as? Set<BridgedToObjC> { }
  if let s = setD as? Set<BridgedToObjC> { } // expected-error {{'ObjC' is not a subtype of 'DerivesObjC'}}
  // expected-note @-1 {{in cast from type 'Set<DerivesObjC>' to 'Set<BridgedToObjC>'}}

  if let s = setB as? Set<Root> { } // expected-error {{'Root' is not a subtype of 'BridgedToObjC'}}
  // expected-note @-1 {{in cast from type 'Set<BridgedToObjC>' to 'Set<Root>'}}
  if let s = setB as? Set<ObjC> { } // expected-error {{'ObjC' is not a subtype of 'BridgedToObjC'}}
  // expected-note @-1 {{in cast from type 'Set<BridgedToObjC>' to 'Set<ObjC>'}}
  if let s = setB as? Set<DerivesObjC> { } // expected-error {{'DerivesObjC' is not a subtype of 'BridgedToObjC'}}
  // expected-note @-1 {{in cast from type 'Set<BridgedToObjC>' to 'Set<DerivesObjC>'}}
}




