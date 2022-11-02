// RUN: %target-swift-frontend(mock-sdk: %clang-importer-sdk) -parse -parse-as-library -verify %s

// REQUIRES: objc_interop

import Foundation
import objc_structs

extension String {
  func onlyOnString() -> String { return self }
}

extension Bool {
  func onlyOnBool() -> Bool { return self }
}

extension Array {
  func onlyOnArray() { }
}

extension Dictionary {
  func onlyOnDictionary() { }
}

extension Set {
  func onlyOnSet() { }
}

func foo() {
  _  = NSStringToNSString as (String!) -> String?
  _ = DummyClass().nsstringProperty.onlyOnString() as String

  _  = BOOLtoBOOL as (Bool) -> Bool
  _  = DummyClass().boolProperty.onlyOnBool() as Bool

  _  = arrayToArray as (Array<AnyObject>!) -> (Array<AnyObject>!)
  DummyClass().arrayProperty.onlyOnArray()

  _ = dictToDict as (Dictionary<NSObject, AnyObject>!) -> Dictionary<NSObject, AnyObject>!

  DummyClass().dictProperty.onlyOnDictionary()

  _ = setToSet as (Set<NSObject>!) -> Set<NSObject>!
  DummyClass().setProperty.onlyOnSet()
}

func allocateMagic(zone: NSZone) -> UnsafeMutablePointer<Void> {
  return allocate(zone)
}

func constPointerToObjC(objects: [AnyObject?]) -> NSArray {
  return NSArray(objects: objects, count: objects.count)
}

func mutablePointerToObjC(path: String) throws -> NSString {
  return try NSString(contentsOfFile: path)
}

func objcStructs(s: StructOfNSStrings, sb: StructOfBlocks) {
  // Struct fields must not be bridged.
  _ = s.nsstr! as Bool // expected-error {{cannot convert value of type 'Unmanaged<NSString>' to type 'Bool' in coercion}}

  // FIXME: Blocks should also be Unmanaged.
  _ = sb.block as Bool // expected-error {{cannot convert value of type '@convention(block) () -> Void' to type 'Bool' in coercion}}
  sb.block() // okay
}
