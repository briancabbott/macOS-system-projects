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

@_implementationOnly import _RegexParser

// TODO: Where should this live? Inside TypeConstruction?
func constructExistentialOutputComponent(
  from input: String,
  component: (range: Range<String.Index>, value: Any?)?,
  optionalCount: Int
) -> Any {
  if let component = component {
    var underlying = component.value ?? input[component.range]
    for _ in 0 ..< optionalCount {
      func wrap<T>(_ x: T) {
        underlying = Optional(x) as Any
      }
      _openExistential(underlying, do: wrap)
    }
    return underlying
  } else {
    precondition(optionalCount > 0, "Must have optional type")
    func makeNil<T>(_ x: T.Type) -> Any {
      T?.none as Any
    }
    let underlyingTy = TypeConstruction.optionalType(
      of: Substring.self, depth: optionalCount - 1)
    return _openExistential(underlyingTy, do: makeNil)
  }
}

@available(SwiftStdlib 5.7, *)
extension AnyRegexOutput.Element {
  func existentialOutputComponent(
    from input: String
  ) -> Any {
    constructExistentialOutputComponent(
      from: input,
      component: representation.content,
      optionalCount: representation.optionalDepth
    )
  }

  func slice(from input: String) -> Substring? {
    guard let r = range else { return nil }
    return input[r]
  }
}

@available(SwiftStdlib 5.7, *)
extension Sequence where Element == AnyRegexOutput.Element {
  // FIXME: This is a stop gap where we still slice the input
  // and traffic through existentials
  @available(SwiftStdlib 5.7, *)
  func existentialOutput(from input: String) -> Any {
    let elements = map {
      $0.existentialOutputComponent(from: input)
    }
    return elements.count == 1
      ? elements[0]
      : TypeConstruction.tuple(of: elements)
  }

  func slices(from input: String) -> [Substring?] {
    self.map { $0.slice(from: input) }
  }
}
