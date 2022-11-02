//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//


extension String {
  /// Construct an instance that is the concatenation of `count` copies
  /// of `repeatedValue`.
  public init(count: Int, repeatedValue c: Character) {
    let s = String(c)
    self = String(_storage: _StringBuffer(
        capacity: s._core.count * count,
        initialSize: 0,
        elementWidth: s._core.elementWidth))
    for _ in 0..<count {
      self += s
    }
  }

  /// Construct an instance that is the concatenation of `count` copies
  /// of `Character(repeatedValue)`.
  public init(count: Int, repeatedValue c: UnicodeScalar) {
    self = String._fromWellFormedCodeUnitSequence(UTF32.self,
        input: Repeat(count: count, repeatedValue: c.value))
  }
  
  public var _lines : [String] {
    return _split("\n")
  }
  
  @warn_unused_result
  public func _split(separator: UnicodeScalar) -> [String] {
    let scalarSlices = unicodeScalars.split { $0 == separator }
    return scalarSlices.map { String($0) }
  }

  /// `true` iff `self` contains no characters.
  public var isEmpty : Bool {
    return _core.count == 0
  }
}

extension String {
  public init(_ _c: UnicodeScalar) {
    self = String(count: 1, repeatedValue: _c)
  }

  @warn_unused_result
  func _isAll(@noescape predicate: (UnicodeScalar) -> Bool) -> Bool {
    for c in unicodeScalars { if !predicate(c) { return false } }

    return true
  }

  @warn_unused_result
  func _isAlpha() -> Bool { return _isAll({ $0._isAlpha() }) }

  @warn_unused_result
  func _isDigit() -> Bool { return _isAll({ $0._isDigit() }) }

  @warn_unused_result
  func _isSpace() -> Bool { return _isAll({ $0._isSpace() }) }
}

#if _runtime(_ObjC)
/// Determines if `theString` starts with `prefix` comparing the strings under
/// canonical equivalence.
@_silgen_name("swift_stdlib_NSStringHasPrefixNFD")
func _stdlib_NSStringHasPrefixNFD(theString: AnyObject, _ prefix: AnyObject) -> Bool

/// Determines if `theString` ends with `suffix` comparing the strings under
/// canonical equivalence.
@_silgen_name("swift_stdlib_NSStringHasSuffixNFD")
func _stdlib_NSStringHasSuffixNFD(theString: AnyObject, _ suffix: AnyObject) -> Bool

extension String {
  /// Returns `true` iff `self` begins with `prefix`.
  public func hasPrefix(prefix: String) -> Bool {
    return _stdlib_NSStringHasPrefixNFD(
      self._bridgeToObjectiveCImpl(), prefix._bridgeToObjectiveCImpl())
  }

  /// Returns `true` iff `self` ends with `suffix`.
  public func hasSuffix(suffix: String) -> Bool {
    return _stdlib_NSStringHasSuffixNFD(
      self._bridgeToObjectiveCImpl(), suffix._bridgeToObjectiveCImpl())
  }
}
#else
// FIXME: Implement hasPrefix and hasSuffix without objc
// rdar://problem/18878343
#endif

// Conversions to string from other types.
extension String {

  // FIXME: can't just use a default arg for radix below; instead we
  // need these single-arg overloads <rdar://problem/17775455>
  
  /// Create an instance representing `v` in base 10.
  public init<T : _SignedIntegerType>(_ v: T) {
    self = _int64ToString(v.toIntMax())
  }
  
  /// Create an instance representing `v` in base 10.
  public init<T : UnsignedIntegerType>(_ v: T)  {
    self = _uint64ToString(v.toUIntMax())
  }

  /// Create an instance representing `v` in the given `radix` (base).
  ///
  /// Numerals greater than 9 are represented as roman letters,
  /// starting with `a` if `uppercase` is `false` or `A` otherwise.
  public init<T : _SignedIntegerType>(
    _ v: T, radix: Int, uppercase: Bool = false
  ) {
    _precondition(radix > 1, "Radix must be greater than 1")
    self = _int64ToString(
      v.toIntMax(), radix: Int64(radix), uppercase: uppercase)
  }
  
  /// Create an instance representing `v` in the given `radix` (base).
  ///
  /// Numerals greater than 9 are represented as roman letters,
  /// starting with `a` if `uppercase` is `false` or `A` otherwise.
  public init<T : UnsignedIntegerType>(
    _ v: T, radix: Int, uppercase: Bool = false
  )  {
    _precondition(radix > 1, "Radix must be greater than 1")
    self = _uint64ToString(
      v.toUIntMax(), radix: Int64(radix), uppercase: uppercase)
  }
}

// Conversions from string to other types.
extension String {
  /// If the string represents an integer that fits into an Int, returns
  /// the corresponding integer.  This accepts strings that match the regular
  /// expression "[-+]?[0-9]+" only.
  @available(*, unavailable, message="Use Int() initializer")
  public func toInt() -> Int? {
    fatalError("unavailable function can't be called")
  }
}

extension String {
  /// Produce a substring of the given string from the given character
  /// index to the end of the string.
  func _substr(start: Int) -> String {
    let rng = unicodeScalars
    var startIndex = rng.startIndex
    for _ in 0..<start {
      ++startIndex
    }
    return String(rng[startIndex..<rng.endIndex])
  }

  /// Split the given string at the given delimiter character, returning 
  /// the strings before and after that character (neither includes the character
  /// found) and a boolean value indicating whether the delimiter was found.
  public func _splitFirst(delim: UnicodeScalar)
    -> (before: String, after: String, wasFound : Bool)
  {
    let rng = unicodeScalars
    for i in rng.indices {
      if rng[i] == delim {
        return (String(rng[rng.startIndex..<i]), 
                String(rng[i.successor()..<rng.endIndex]), 
                true)
      }
    }
    return (self, "", false)
  }

  /// Split the given string at the first character for which the given
  /// predicate returns true. Returns the string before that character, the 
  /// character that matches, the string after that character, and a boolean value
  /// indicating whether any character was found.
  public func _splitFirstIf(@noescape predicate: (UnicodeScalar) -> Bool)
    -> (before: String, found: UnicodeScalar, after: String, wasFound: Bool)
  {
    let rng = unicodeScalars
    for i in rng.indices {
      if predicate(rng[i]) {
        return (String(rng[rng.startIndex..<i]),
                rng[i], 
                String(rng[i.successor()..<rng.endIndex]), 
                true)
      }
    }
    return (self, "🎃", String(), false)
  }

  /// Split the given string at each occurrence of a character for which
  /// the given predicate evaluates true, returning an array of strings that
  /// before/between/after those delimiters.
  func _splitIf(predicate: (UnicodeScalar) -> Bool) -> [String] {
    let scalarSlices = unicodeScalars.split(isSeparator: predicate)
    return scalarSlices.map { String($0) }
  }
}
