// RUN: %target-parse-verify-swift

struct IntList : ArrayLiteralConvertible {
  typealias Element = Int
  init(arrayLiteral elements: Int...) {}
}

struct DoubleList : ArrayLiteralConvertible {
  typealias Element = Double
  init(arrayLiteral elements: Double...) {}
}

struct  IntDict : ArrayLiteralConvertible {
  typealias Element = (String, Int)
  init(arrayLiteral elements: Element...) {}
}

final class DoubleDict : ArrayLiteralConvertible {
  typealias Element = (String, Double)
  init(arrayLiteral elements: Element...) {}
}

final class List<T> : ArrayLiteralConvertible {
  typealias Element = T
  init(arrayLiteral elements: T...) {}
}

final class Dict<K,V> : ArrayLiteralConvertible {
  typealias Element = (K,V)

  init(arrayLiteral elements: (K,V)...) {}
}

infix operator => {}

func => <K, V>(k: K, v: V) -> (K,V) { return (k,v) }

func useIntList(l: IntList) {}
func useDoubleList(l: DoubleList) {}
func useIntDict(l: IntDict) {}
func useDoubleDict(l: DoubleDict) {}
func useList<T>(l: List<T>) {}
func useDict<K,V>(d: Dict<K,V>) {}

useIntList([1,2,3])
useIntList([1.0,2,3]) // expected-error{{cannot convert value of type 'Double' to expected element type 'Int'}}
useIntList([nil])  // expected-error {{nil is not compatible with expected element type 'Int'}}

useDoubleList([1.0,2,3])
useDoubleList([1.0,2.0,3.0])

useIntDict(["Niners" => 31, "Ravens" => 34])
useIntDict(["Niners" => 31, "Ravens" => 34.0]) // expected-error{{cannot convert value of type 'Double' to expected argument type 'Int'}}
// <rdar://problem/22333090> QoI: Propagate contextual information in a call to operands
useDoubleDict(["Niners" => 31, "Ravens" => 34.0])
useDoubleDict(["Niners" => 31.0, "Ravens" => 34])
useDoubleDict(["Niners" => 31.0, "Ravens" => 34.0])

// Generic slices
useList([1,2,3])
useList([1.0,2,3])
useList([1.0,2.0,3.0])
useDict(["Niners" => 31, "Ravens" => 34])
useDict(["Niners" => 31, "Ravens" => 34.0])
useDict(["Niners" => 31.0, "Ravens" => 34.0])

// Fall back to [T] if no context is otherwise available.
var a = [1,2,3]
var a2 : [Int] = a

var b = [1,2,3.0]
var b2 : [Double] = b

var arrayOfStreams = [1..<2, 3..<4]

struct MyArray : ArrayLiteralConvertible {
  typealias Element = Double

  init(arrayLiteral elements: Double...) {
    
  }
}

var myArray : MyArray = [2.5, 2.5]

// Inference for tuple elements.
var x1 = [1]
x1[0] = 0
var x2 = [(1, 2)]
x2[0] = (3, 4)
var x3 = [1, 2, 3]
x3[0] = 4

func trailingComma() {
  _ = [1, ]
  _ = [1, 2, ]
  _ = ["a": 1, ]
  _ = ["a": 1, "b": 2, ]
}

func longArray() {
  var _=["1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1", "1"]
}

[1,2].map // expected-error {{expression type '(@noescape (Int) throws -> _) throws -> [_]' is ambiguous without more context}}
