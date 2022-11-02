// RUN: %target-parse-verify-swift

// REQUIRES: objc_interop

// Test casting through a class type to a bridged value type.

class NSObject { }

class BridgedClass : NSObject { 
}

class SubclassOfBridgedClass : BridgedClass { }

struct BridgedStruct : _ObjectiveCBridgeable {
  static func _isBridgedToObjectiveC() -> Bool {
    return true
  }
  
  static func _getObjectiveCType() -> Any.Type {
    return BridgedClass.self
  }

  func _bridgeToObjectiveC() -> BridgedClass {
    return BridgedClass()
  }

  static func _forceBridgeFromObjectiveC(
    x: BridgedClass,
    inout result: BridgedStruct?
  ) {
  }
  static func _conditionallyBridgeFromObjectiveC(
    x: BridgedClass,
    inout result: BridgedStruct?
  ) -> Bool {
    return true
  }
}

func testBridgeDowncast(obj: AnyObject, objOpt: AnyObject?, 
                        objImplicitOpt: AnyObject!) -> BridgedStruct? {
  let s1Opt = obj as? BridgedStruct
  var s2Opt = objOpt as? BridgedStruct
  var s3Opt = objImplicitOpt as? BridgedStruct

  // Make sure we seem to have the right result type.
  if s1Opt != nil { return s1Opt }
  s2Opt = s1Opt
  s2Opt = s3Opt
  s3Opt = s1Opt
  _ = s2Opt

  return s1Opt
}

func testBridgeIsa(obj: AnyObject, objOpt: AnyObject?, 
                   objImplicitOpt: AnyObject!) {
  if obj is BridgedStruct { }
  if objOpt is BridgedStruct { }
  if objImplicitOpt is BridgedStruct { }
}

func testBridgeDowncastSuperclass(obj: NSObject, objOpt: NSObject?,
                                  objImplicitOpt: NSObject!) 
       -> BridgedStruct? {
  _ = obj as? BridgedStruct
  _ = objOpt as? BridgedStruct
  _ = objImplicitOpt as? BridgedStruct
}

func testBridgeDowncastExact(obj: BridgedClass, objOpt: BridgedClass?,
                             objImplicitOpt: BridgedClass!) -> BridgedStruct? {
  _ = obj as? BridgedStruct // expected-warning{{conditional cast from 'BridgedClass' to 'BridgedStruct' always succeeds}}
  _ = objOpt as? BridgedStruct
  _ = objImplicitOpt as? BridgedStruct // expected-warning{{conditional cast from 'BridgedClass!' to 'BridgedStruct' always succeeds}}
}

func testExplicitBridging(object: BridgedClass, value: BridgedStruct) {
  var object = object
  var value = value
  object = value as BridgedClass
  value = object as BridgedStruct
}

func testBridgingFromSubclass(obj: SubclassOfBridgedClass) {
  _ = obj as! BridgedStruct // expected-warning{{forced cast from 'SubclassOfBridgedClass' to 'BridgedStruct' always succeeds; did you mean to use 'as'?}} {{11-14=as}}
  _ = obj as BridgedStruct
}
