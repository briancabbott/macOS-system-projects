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


extension AST {
  public struct CustomCharacterClass: Hashable {
    public var start: Located<Start>
    public var members: [Member]

    public let location: SourceLocation

    public init(
      _ start: Located<Start>,
      _ members: [Member],
      _ sr: SourceLocation
    ) {
      self.start = start
      self.members = members
      self.location = sr
    }

    public enum Member: Hashable {
      /// A nested custom character class `[[ab][cd]]`
      case custom(CustomCharacterClass)

      /// A character range `a-z`
      case range(Range)

      /// A single character or escape
      case atom(Atom)

      /// A quoted sequence. Inside a custom character class this just means
      /// the contents should be interpreted literally.
      case quote(Quote)

      /// Trivia such as non-semantic whitespace.
      case trivia(Trivia)

      /// A binary operator applied to sets of members `abc&&def`
      case setOperation([Member], Located<SetOp>, [Member])
    }
    public struct Range: Hashable {
      public var lhs: Atom
      public var dashLoc: SourceLocation
      public var rhs: Atom
      public var trivia: [AST.Trivia]

      public init(
        _ lhs: Atom, _ dashLoc: SourceLocation, _ rhs: Atom,
        trivia: [AST.Trivia]
      ) {
        self.lhs = lhs
        self.dashLoc = dashLoc
        self.rhs = rhs
        self.trivia = trivia
      }

      public var location: SourceLocation {
        lhs.location.union(with: rhs.location)
      }
    }
    public enum SetOp: String, Hashable {
      case subtraction = "--"
      case intersection = "&&"
      case symmetricDifference = "~~"
    }
    public enum Start: String, Hashable {
      case normal = "["
      case inverted = "[^"
    }
  }
}

extension AST.CustomCharacterClass {
  public var isInverted: Bool { start.value == .inverted }
}

extension CustomCC.Member {
  private var _associatedValue: Any {
    switch self {
    case .custom(let c): return c
    case .range(let r): return r
    case .atom(let a): return a
    case .quote(let q): return q
    case .trivia(let t): return t
    case .setOperation(let lhs, let op, let rhs): return (lhs, op, rhs)
    }
  }

  func `as`<T>(_ t: T.Type = T.self) -> T? {
    _associatedValue as? T
  }

  public var isTrivia: Bool {
    if case .trivia = self { return true }
    return false
  }

  public var asTrivia: AST.Trivia? {
    guard case .trivia(let t) = self else { return nil }
    return t
  }

  public var isSemantic: Bool {
    !isTrivia
  }

  public var location: SourceLocation {
    switch self {
    case let .custom(c): return c.location
    case let .range(r):  return r.location
    case let .atom(a):   return a.location
    case let .quote(q):  return q.location
    case let .trivia(t): return t.location
    case let .setOperation(lhs, dash, rhs):
      var loc = dash.location
      if let lhs = lhs.first {
        loc = loc.union(with: lhs.location)
      }
      if let rhs = rhs.last {
        loc = loc.union(with: rhs.location)
      }
      return loc
    }
  }
}

extension AST.CustomCharacterClass {
  /// Strips trivia from the character class members.
  ///
  /// This method doesn't recurse into nested custom character classes.
  public var strippingTriviaShallow: Self {
    var copy = self
    copy.members = copy.members.filter(\.isSemantic)
    return copy
  }
}
