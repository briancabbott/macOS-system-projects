// RUN: %empty-directory(%t)
// RUN: %target-run-stdlib-swift %S/Inputs/

// REQUIRES: executable_test
// REQUIRES: objc_interop
// REQUIRES: optimized_stdlib

@_spi(_Unicode)
import Swift

import StdlibUnittest
import StdlibUnicodeUnittest
import Foundation

let StringWordBreaking = TestSuite("StringWordBreaking")

// FIXME: Reenable once we figure out what to do with WordView
// @available(SwiftStdlib 5.7, *)
// extension String._WordView {
//   var backwardsCount: Int {
//     var c = 0
//     var index = endIndex
//     while index != startIndex {
//       c += 1
//       formIndex(before: &index)
//     }
//     return c
//   }
// }

extension String {
  @available(SwiftStdlib 5.7, *)
  var _words: [String] {
    var result: [String] = []

    var i = startIndex

    while i < endIndex {
      let start = i
      let end = _wordIndex(after: i)

      let substr = self[start ..< end]
      result.append(String(substr))

      i = end
    }

    return result
  }

  @available(SwiftStdlib 5.7, *)
  var _wordsBackwards: [String] {
    var result: [String] = []

    var i = endIndex

    while i > startIndex {
      let end = i
      let start = _wordIndex(before: i)

      let substr = self[start ..< end]
      result.append(String(substr))

      i = start
    }

    return result
  }
}

if #available(SwiftStdlib 5.7, *) {
  StringWordBreaking.test("word breaking") {
    for wordBreakTest in wordBreakTests {
      expectEqual(
        wordBreakTest.1,
        wordBreakTest.0._words,
        "string: \(String(reflecting: wordBreakTest.0))")
      expectEqual(
        wordBreakTest.1.reversed(),
        wordBreakTest.0._wordsBackwards,
        "string: \(String(reflecting: wordBreakTest.0))")
    }
  }
}

// The most simple subclass of NSString that CoreFoundation does not know
// about.
class NonContiguousNSString : NSString {
  required init(coder aDecoder: NSCoder) {
    fatalError("don't call this initializer")
  }
  required init(itemProviderData data: Data, typeIdentifier: String) throws {
    fatalError("don't call this initializer")    
  }

  override init() { 
    _value = []
    super.init() 
  }

  init(_ value: [UInt16]) {
    _value = value
    super.init()
  }

  @objc(copyWithZone:) override func copy(with zone: NSZone?) -> Any {
    // Ensure that copying this string produces a class that CoreFoundation
    // does not know about.
    return self
  }

  @objc override var length: Int {
    return _value.count
  }

  @objc override func character(at index: Int) -> unichar {
    return _value[index]
  }

  var _value: [UInt16]
}

extension _StringGuts {
  @_silgen_name("$ss11_StringGutsV9isForeignSbvg")
  func _isForeign() -> Bool
}

func getUTF16Array(from string: String) -> [UInt16] {
  var result: [UInt16] = []

  for cp in string.utf16 {
    result.append(cp)
  }

  return result
}

if #available(SwiftStdlib 5.7, *) {
  StringWordBreaking.test("word breaking foreign") {
    for wordBreakTest in wordBreakTests {
      let foreignTest = NonContiguousNSString(
        getUTF16Array(from: wordBreakTest.0)
      )
      let test = foreignTest as String

      expectTrue(test._guts._isForeign())
      expectEqual(
        wordBreakTest.1,
        test._words,
        "string: \(String(reflecting: wordBreakTest.0))")
      expectEqual(
        wordBreakTest.1.reversed(),
        test._wordsBackwards,
        "string: \(String(reflecting: wordBreakTest.0))")
    }
  }
}

runAllTests()
