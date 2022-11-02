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

/// Wraps a searcher that searches for a given pattern. If the pattern is empty, falls back on matching every empty index range exactly once.
struct PatternOrEmpty<Searcher: CollectionSearcher> {
  let searcher: Searcher?
}

extension PatternOrEmpty: CollectionSearcher {
  typealias Searched = Searcher.Searched
  
  struct State {
    enum Representation {
      case state(Searcher.State)
      case empty(index: Searched.Index, end: Searched.Index)
      case emptyDone
    }
    
    let representation: Representation
  }
  
  func state(
    for searched: Searcher.Searched,
    in range: Range<Searched.Index>
  ) -> State {
    if let searcher = searcher {
      return State(
        representation: .state(searcher.state(for: searched, in: range)))
    } else {
      return State(
        representation: .empty(index: range.lowerBound, end: range.upperBound))
    }
  }
  
  func search(
    _ searched: Searched,
    _ state: inout State
  ) -> Range<Searched.Index>? {
    switch state.representation {
    case .state(var s):
      // TODO: Avoid a potential copy-on-write copy here
      let result = searcher!.search(searched, &s)
      state = State(representation: .state(s))
      return result
    case .empty(let index, let end):
      if index == end {
        state = State(representation: .emptyDone)
      } else {
        state = State(
          representation: .empty(index: searched.index(after: index), end: end))
      }
      return index..<index
    case .emptyDone:
      return nil
    }
  }
}
