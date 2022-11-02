//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2021-2022 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
//
//===----------------------------------------------------------------------===//

@available(SwiftStdlib 5.7, *)
extension Regex {
  /// The result of matching a regex against a string.
  ///
  /// A `Match` forwards API to the `Output` generic parameter,
  /// providing direct access to captures.
  @dynamicMemberLookup
  public struct Match {
    let anyRegexOutput: AnyRegexOutput

    /// The range of the overall match.
    public let range: Range<String.Index>
  }
}

@available(SwiftStdlib 5.7, *)
extension Regex.Match {
  var input: String {
    anyRegexOutput.input
  }

  /// The output produced from the match operation.
  public var output: Output {
    if Output.self == AnyRegexOutput.self {
      return anyRegexOutput as! Output
    }
    let typeErasedMatch = anyRegexOutput.existentialOutput(
      from: anyRegexOutput.input
    )
    return typeErasedMatch as! Output
  }

  /// Accesses a capture by its name or number.
  public subscript<T>(dynamicMember keyPath: KeyPath<Output, T>) -> T {
    // Note: We should be able to get the element offset from the key path
    // itself even at compile time. We need a better way of doing this.
    guard let outputTupleOffset = MemoryLayout.tupleElementIndex(
      of: keyPath, elementTypes: anyRegexOutput.map(\.type)
    ) else {
      return output[keyPath: keyPath]
    }
    return anyRegexOutput[outputTupleOffset].value as! T
  }

  /// Accesses a capture using the `.0` syntax, even when the match isn't a tuple.
  @_disfavoredOverload
  public subscript(
    dynamicMember keyPath: KeyPath<(Output, _doNotUse: ()), Output>
  ) -> Output {
    output
  }

  @_spi(RegexBuilder)
  public subscript<Capture>(_ id: ReferenceID) -> Capture {
    guard let element = anyRegexOutput.first(
      where: { $0.representation.referenceID == id }
    ) else {
      preconditionFailure("Reference did not capture any match in the regex")
    }
    return element.existentialOutputComponent(
      from: input
    ) as! Capture
  }
}

@available(SwiftStdlib 5.7, *)
extension Regex {
  /// Matches a string in its entirety.
  ///
  /// - Parameter s: The string to match this regular expression against.
  /// - Returns: The match, or `nil` if no match was found.
  public func wholeMatch(in s: String) throws -> Regex<Output>.Match? {
    try _match(s, in: s.startIndex..<s.endIndex, mode: .wholeString)
  }

  /// Matches part of a string, starting at its beginning.
  ///
  /// - Parameter s: The string to match this regular expression against.
  /// - Returns: The match, or `nil` if no match was found.
  public func prefixMatch(in s: String) throws -> Regex<Output>.Match? {
    try _match(s, in: s.startIndex..<s.endIndex, mode: .partialFromFront)
  }

  /// Finds the first match in a string.
  ///
  /// - Parameter s: The string to match this regular expression against.
  /// - Returns: The match, or `nil` if no match was found.
  public func firstMatch(in s: String) throws -> Regex<Output>.Match? {
    try _firstMatch(s, in: s.startIndex..<s.endIndex)
  }

  /// Matches a substring in its entirety.
  ///
  /// - Parameter s: The substring to match this regular expression against.
  /// - Returns: The match, or `nil` if no match was found.
  public func wholeMatch(in s: Substring) throws -> Regex<Output>.Match? {
    try _match(s.base, in: s.startIndex..<s.endIndex, mode: .wholeString)
  }

  /// Matches part of a substring, starting at its beginning.
  ///
  /// - Parameter s: The substring to match this regular expression against.
  /// - Returns: The match, or `nil` if no match was found.
  public func prefixMatch(in s: Substring) throws -> Regex<Output>.Match? {
    try _match(s.base, in: s.startIndex..<s.endIndex, mode: .partialFromFront)
  }

  /// Finds the first match in a substring.
  ///
  /// - Parameter s: The substring to match this regular expression against.
  /// - Returns: The match, or `nil` if no match was found.
  public func firstMatch(in s: Substring) throws -> Regex<Output>.Match? {
    try _firstMatch(s.base, in: s.startIndex..<s.endIndex)
  }

  func _match(
    _ input: String,
    in subjectBounds: Range<String.Index>,
    mode: MatchMode = .wholeString
  ) throws -> Regex<Output>.Match? {
    let executor = Executor(program: regex.program.loweredProgram)
    return try executor.match(input, in: subjectBounds, mode)
  }

  func _firstMatch(
    _ input: String,
    in subjectBounds: Range<String.Index>
  ) throws -> Regex<Output>.Match? {
    try _firstMatch(input, subjectBounds: subjectBounds, searchBounds: subjectBounds)
  }

  func _firstMatch(
    _ input: String,
    subjectBounds: Range<String.Index>,
    searchBounds: Range<String.Index>
  ) throws -> Regex<Output>.Match? {
    let executor = Executor(program: regex.program.loweredProgram)
    let graphemeSemantic = regex.initialOptions.semanticLevel == .graphemeCluster
    return try executor.firstMatch(
      input,
      subjectBounds: subjectBounds,
      searchBounds: searchBounds,
      graphemeSemantic: graphemeSemantic)
  }
}

@available(SwiftStdlib 5.7, *)
extension BidirectionalCollection where SubSequence == Substring {
  /// Checks for a match against the string in its entirety.
  ///
  /// - Parameter r: The regular expression being matched.
  /// - Returns: The match, or `nil` if no match was found.
  public func wholeMatch<R: RegexComponent>(
    of r: R
  ) -> Regex<R.RegexOutput>.Match? {
    try? r.regex.wholeMatch(in: self[...])
  }

  /// Checks for a match against the string, starting at its beginning.
  ///
  /// - Parameter r: The regular expression being matched.
  /// - Returns: The match, or `nil` if no match was found.
  public func prefixMatch<R: RegexComponent>(
    of r: R
  ) -> Regex<R.RegexOutput>.Match? {
    try? r.regex.prefixMatch(in: self[...])
  }
}
