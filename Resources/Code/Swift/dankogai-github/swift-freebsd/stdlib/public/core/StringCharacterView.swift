//===--- StringCharacterView.swift - String's Collection of Characters ----===//
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
//
//  String is-not-a SequenceType or CollectionType, but it exposes a
//  collection of characters.
//
//===----------------------------------------------------------------------===//

extension String {
  /// A `String`'s collection of `Character`s ([extended grapheme
  /// clusters](http://www.unicode.org/glossary/#extended_grapheme_cluster))
  /// elements.
  public struct CharacterView {
    internal var _core: _StringCore

    /// Create a view of the `Character`s in `text`.
    public init(_ text: String) {
      self._core = text._core
    }
    
    public // @testable
    init(_ _core: _StringCore) {
      self._core = _core
    }
  }

  /// A collection of `Characters` representing the `String`'s
  /// [extended grapheme
  /// clusters](http://www.unicode.org/glossary/#extended_grapheme_cluster).
  public var characters: CharacterView {
    return CharacterView(self)
  }

  /// Efficiently mutate `self` by applying `body` to its `characters`.
  ///
  /// - Warning: Do not rely on anything about `self` (the `String`
  ///   that is the target of this method) during the execution of
  ///   `body`: it may not appear to have its correct value.  Instead,
  ///   use only the `String.CharacterView` argument to `body`.
  public mutating func withMutableCharacters<R>(body: (inout CharacterView)->R) -> R {
    // Naively mutating self.characters forces multiple references to
    // exist at the point of mutation. Instead, temporarily move the
    // core of this string into a CharacterView.
    var tmp = CharacterView("")
    swap(&_core, &tmp._core)
    let r = body(&tmp)
    swap(&_core, &tmp._core)
    return r
  }

  /// Construct the `String` corresponding to the given sequence of
  /// Unicode scalars.
  public init(_ characters: CharacterView) {
    self.init(characters._core)
  }
}

/// `String.CharacterView` is a collection of `Character`.
extension String.CharacterView : CollectionType {
  internal typealias UnicodeScalarView = String.UnicodeScalarView
  internal var unicodeScalars: UnicodeScalarView {
    return UnicodeScalarView(_core)
  }
  
  /// A character position.
  public struct Index : BidirectionalIndexType, Comparable, _Reflectable {
    public // SPI(Foundation)    
    init(_base: String.UnicodeScalarView.Index) {
      self._base = _base
      self._lengthUTF16 = Index._measureExtendedGraphemeClusterForward(_base)
    }

    internal init(_base: UnicodeScalarView.Index, _lengthUTF16: Int) {
      self._base = _base
      self._lengthUTF16 = _lengthUTF16
    }

    /// Returns the next consecutive value after `self`.
    ///
    /// - Requires: The next value is representable.
    public func successor() -> Index {
      _precondition(_base != _base._viewEndIndex, "can not increment endIndex")
      return Index(_base: _endBase)
    }

    /// Returns the previous consecutive value before `self`.
    ///
    /// - Requires: The previous value is representable.
    public func predecessor() -> Index {
      _precondition(_base != _base._viewStartIndex,
          "can not decrement startIndex")
      let predecessorLengthUTF16 =
          Index._measureExtendedGraphemeClusterBackward(_base)
      return Index(
        _base: UnicodeScalarView.Index(
          _utf16Index - predecessorLengthUTF16, _base._core))
    }

    internal let _base: UnicodeScalarView.Index

    /// The length of this extended grapheme cluster in UTF-16 code units.
    internal let _lengthUTF16: Int

    /// The integer offset of this index in UTF-16 code units.
    public // SPI(Foundation)
    var _utf16Index: Int {
      return _base._position
    }

    /// The one past end index for this extended grapheme cluster in Unicode
    /// scalars.
    internal var _endBase: UnicodeScalarView.Index {
      return UnicodeScalarView.Index(
          _utf16Index + _lengthUTF16, _base._core)
    }

    /// Returns the length of the first extended grapheme cluster in UTF-16
    /// code units.
    @warn_unused_result
    internal static func _measureExtendedGraphemeClusterForward(
        start: UnicodeScalarView.Index
    ) -> Int {
      var start = start
      let end = start._viewEndIndex
      if start == end {
        return 0
      }

      let startIndexUTF16 = start._position
      let unicodeScalars = UnicodeScalarView(start._core)
      let graphemeClusterBreakProperty =
          _UnicodeGraphemeClusterBreakPropertyTrie()
      let segmenter = _UnicodeExtendedGraphemeClusterSegmenter()

      var gcb0 = graphemeClusterBreakProperty.getPropertyRawValue(
          unicodeScalars[start].value)
      ++start

      for ; start != end; ++start {
        // FIXME(performance): consider removing this "fast path".  A branch
        // that is hard to predict could be worse for performance than a few
        // loads from cache to fetch the property 'gcb1'.
        if segmenter.isBoundaryAfter(gcb0) {
          break
        }
        let gcb1 = graphemeClusterBreakProperty.getPropertyRawValue(
            unicodeScalars[start].value)
        if segmenter.isBoundary(gcb0, gcb1) {
          break
        }
        gcb0 = gcb1
      }

      return start._position - startIndexUTF16
    }

    /// Returns the length of the previous extended grapheme cluster in UTF-16
    /// code units.
    @warn_unused_result
    internal static func _measureExtendedGraphemeClusterBackward(
        end: UnicodeScalarView.Index
    ) -> Int {
      let start = end._viewStartIndex
      if start == end {
        return 0
      }

      let endIndexUTF16 = end._position
      let unicodeScalars = UnicodeScalarView(start._core)
      let graphemeClusterBreakProperty =
          _UnicodeGraphemeClusterBreakPropertyTrie()
      let segmenter = _UnicodeExtendedGraphemeClusterSegmenter()

      var graphemeClusterStart = end

      --graphemeClusterStart
      var gcb0 = graphemeClusterBreakProperty.getPropertyRawValue(
          unicodeScalars[graphemeClusterStart].value)

      var graphemeClusterStartUTF16 = graphemeClusterStart._position

      while graphemeClusterStart != start {
        --graphemeClusterStart
        let gcb1 = graphemeClusterBreakProperty.getPropertyRawValue(
            unicodeScalars[graphemeClusterStart].value)
        if segmenter.isBoundary(gcb1, gcb0) {
          break
        }
        gcb0 = gcb1
        graphemeClusterStartUTF16 = graphemeClusterStart._position
      }

      return endIndexUTF16 - graphemeClusterStartUTF16
    }

    /// Returns a mirror that reflects `self`.
    public func _getMirror() -> _MirrorType {
      return _IndexMirror(self)
    }
  }

  /// The position of the first `Character` if `self` is
  /// non-empty; identical to `endIndex` otherwise.
  public var startIndex: Index {
    return Index(_base: unicodeScalars.startIndex)
  }

  /// The "past the end" position.
  ///
  /// `endIndex` is not a valid argument to `subscript`, and is always
  /// reachable from `startIndex` by zero or more applications of
  /// `successor()`.
  public var endIndex: Index {
    return Index(_base: unicodeScalars.endIndex)
  }

  /// Access the `Character` at `position`.
  ///
  /// - Requires: `position` is a valid position in `self` and
  ///   `position != endIndex`.
  public subscript(i: Index) -> Character {
    return Character(String(unicodeScalars[i._base..<i._endBase]))
  }

  internal struct _IndexMirror : _MirrorType {
    var _value: Index

    init(_ x: Index) {
      _value = x
    }

    var value: Any { return _value }

    var valueType: Any.Type { return (_value as Any).dynamicType }

    var objectIdentifier: ObjectIdentifier? { return .None }

    var disposition: _MirrorDisposition { return .Aggregate }

    var count: Int { return 0 }

    subscript(i: Int) -> (String, _MirrorType) {
      _preconditionFailure("_MirrorType access out of bounds")
    }

    var summary: String { return "\(_value._utf16Index)" }

    var quickLookObject: PlaygroundQuickLook? {
      return .Some(.Int(Int64(_value._utf16Index)))
    }
  }
}

extension String.CharacterView : RangeReplaceableCollectionType {
  /// Create an empty instance.
  public init() {
    self.init("")
  }

  /// Replace the given `subRange` of elements with `newElements`.
  ///
  /// Invalidates all indices with respect to `self`.
  ///
  /// - Complexity: O(`subRange.count`) if `subRange.endIndex
  ///   == self.endIndex` and `newElements.isEmpty`, O(N) otherwise.
  public mutating func replaceRange<
    C: CollectionType where C.Generator.Element == Character
  >(
    subRange: Range<Index>, with newElements: C
  ) {
    let rawSubRange = subRange.startIndex._base._position
      ..< subRange.endIndex._base._position
    let lazyUTF16 = newElements.lazy.flatMap { $0.utf16 }
    _core.replaceRange(rawSubRange, with: lazyUTF16)
  }

  /// Reserve enough space to store `n` ASCII characters.
  ///
  /// - Complexity: O(`n`).
  public mutating func reserveCapacity(n: Int) {
    _core.reserveCapacity(n)
  }

  /// Append `c` to `self`.
  ///
  /// - Complexity: Amortized O(1).
  public mutating func append(c: Character) {
    switch c._representation {
    case .Small(let _63bits):
      let bytes = Character._smallValue(_63bits)
      _core.appendContentsOf(Character._SmallUTF16(bytes))
    case .Large(_):
      _core.append(String(c)._core)
    }
  }

  /// Append the elements of `newElements` to `self`.
  public mutating func appendContentsOf<
      S : SequenceType
      where S.Generator.Element == Character
  >(newElements: S) {
    reserveCapacity(_core.count + newElements.underestimateCount())
    for c in newElements {
      self.append(c)
    }
  }

  /// Create an instance containing `characters`.
  public init<
      S : SequenceType
      where S.Generator.Element == Character
  >(_ characters: S) {
    self = String.CharacterView()
    self.appendContentsOf(characters)
  }
}

// Algorithms
extension String.CharacterView {
  /// Access the characters in the given `subRange`.
  ///
  /// - Complexity: O(1) unless bridging from Objective-C requires an
  ///   O(N) conversion.
  public subscript(subRange: Range<Index>) -> String.CharacterView {
    let unicodeScalarRange =
      subRange.startIndex._base..<subRange.endIndex._base
    return String.CharacterView(
      String(_core).unicodeScalars[unicodeScalarRange]._core)
  }
}

