// RUN: not %swift -parse -target %target-triple %s -emit-fixits-path %t.remap
// RUN: c-arcmt-test %t.remap | arcmt-test -verify-transformed-files %s.result

class Base {}
class Derived : Base {}

var b : Base
b as Derived
b as Derived

b as! Base

var opti : Int?
// Don't add bang.
var i : Int = opti
// But remove unecessary bang.
var i2 : Int = i!

struct MyMask : OptionSetType {
  init(_ rawValue: UInt) {}
  init(rawValue: UInt) {}
  init(nilLiteral: ()) {}

  var rawValue: UInt { return 0 }

  static var allZeros: MyMask { return MyMask(0) }
  static var Bingo: MyMask { return MyMask(1) }
}

func supported() -> MyMask {
  return Int(MyMask.Bingo.rawValue)
}

func foo() -> Int {
  do {
  } catch var err {
    goo(err)
  }
}

func goo(var e : ErrorType) {}

struct Test1 : RawOptionSetType {
  init(rawValue: Int) {}
  var rawValue: Int { return 0 }
}

print("", appendNewline: false)
Swift.print("", appendNewline: false)
print("", appendNewline: true)
print("", false, appendNewline: false)
print("", false)

func ftest1() {
  // Don't replace the variable name with '_'
  let myvar = 0
}
