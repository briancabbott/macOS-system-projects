//===--- Value.swift - the Value protocol ---------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2021 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import Basic
import SILBridging

public protocol Value : AnyObject, CustomStringConvertible {
  var uses: UseList { get }
  var type: Type { get }
  var ownership: Ownership { get }
  
  /// The instruction which defines the value.
  ///
  /// This is nil if the value is not defined by an instruction, e.g. an `Argument`.
  var definingInstruction: Instruction? { get }
  
  /// The block where the value is defined.
  ///
  /// It's not legal to get the definingBlock of an `Undef` value.
  var definingBlock: BasicBlock { get }
}

public enum Ownership {
  /// A Value with `unowned` ownership kind is an independent value that
  /// has a lifetime that is only guaranteed to last until the next program
  /// visible side-effect. To maintain the lifetime of an unowned value, it
  /// must be converted to an owned representation via a copy_value.
  ///
  /// Unowned ownership kind occurs mainly along method/function boundaries in
  /// between Swift and Objective-C code.
  case unowned
  
  /// A Value with `owned` ownership kind is an independent value that has
  /// an ownership independent of any other ownership imbued within it. The
  /// Value must be paired with a consuming operation that ends the SSA
  /// value's lifetime exactly once along all paths through the program.
  case owned
  
  /// A Value with `guaranteed` ownership kind is an independent value that
  /// is guaranteed to be live over a specific region of the program. This
  /// region can come in several forms:
  ///
  /// 1. @guaranteed function argument. This guarantees that a value will
  /// outlive a function.
  ///
  /// 2. A shared borrow region. This is a region denoted by a
  /// begin_borrow/load_borrow instruction and an end_borrow instruction. The
  /// SSA value must not be destroyed or taken inside the borrowed region.
  ///
  /// Any value with guaranteed ownership must be paired with an end_borrow
  /// instruction exactly once along any path through the program.
  case guaranteed
  
  /// A Value with `none` ownership kind is an independent value outside of
  /// the ownership system. It is used to model values that are statically
  /// determined to be trivial. This includes trivially typed values as well
  /// as trivial cases of non-trivial enums. Naturally `none` can be merged with
  /// any ownership, allowing us to naturally model merge and branch
  /// points in the SSA graph, where more information about the value is
  /// statically available on some control flow paths.
  case none
  
  public var _bridged: BridgedOwnership {
    switch self {
      case .unowned:    return Ownership_Unowned
      case .owned:      return Ownership_Owned
      case .guaranteed: return Ownership_Guaranteed
      case .none:       return Ownership_None
    }
  }
}

extension Value {
  public var description: String {
    let stdString = SILNode_debugDescription(bridgedNode)
    return String(_cxxString: stdString)
  }

  public var uses: UseList {
    UseList(SILValue_firstUse(bridged))
  }
  
  public var function: Function { definingBlock.function }

  public var type: Type { SILValue_getType(bridged).type }

  public var ownership: Ownership { SILValue_getOwnership(bridged).ownership }

  public var hashable: HashableValue { ObjectIdentifier(self) }

  public var bridged: BridgedValue {
    BridgedValue(obj: SwiftObject(self as AnyObject))
  }
  var bridgedNode: BridgedNode {
    BridgedNode(obj: SwiftObject(self as AnyObject))
  }
}

public typealias HashableValue = ObjectIdentifier

// We can't make `Value` inherit from `Equatable`, since `Equatable` is a PAT,
// and we do use `Value` existentials around the codebase. Thus functions from
// `Equatable` are declared separately.
public func ==(_ lhs: Value, _ rhs: Value) -> Bool {
  return lhs === rhs
}

public func !=(_ lhs: Value, _ rhs: Value) -> Bool {
  return !(lhs === rhs)
}

/// A projected value, which is defined by the original value and a projection path.
///
/// For example, if the `value` is of type `struct S { var x: Int }` and `path` is `s0`,
/// then the projected value represents field `x` of the original value.
/// An empty path means represents the "whole" original value.
///
public struct ProjectedValue {
  public let value: Value
  public let path: SmallProjectionPath
}

extension Value {
  /// Returns a projected value, defined by this value and `path`.
  public func at(_ path: SmallProjectionPath) -> ProjectedValue {
    ProjectedValue(value: self, path: path)
  }

  /// Returns a projected value, defined by this value and path containig a single field of `kind` and `index`.
  public func at(_ kind: SmallProjectionPath.FieldKind, index: Int = 0) -> ProjectedValue {
    ProjectedValue(value: self, path: SmallProjectionPath(kind, index: index))
  }
}


extension BridgedValue {
  public func getAs<T: AnyObject>(_ valueType: T.Type) -> T { obj.getAs(T.self) }

  public var value: Value {
    // This is much faster than a conformance lookup with `as! Value`.
    let v = getAs(AnyObject.self)
    switch v {
      case let inst as SingleValueInstruction:
        return inst
      case let arg as Argument:
        return arg
      case let mvr as MultipleValueInstructionResult:
        return mvr
      case let undef as Undef:
        return undef
      default:
        fatalError("unknown Value type")
    }
  }
}

final class Undef : Value {
  public var definingInstruction: Instruction? { nil }
  public var definingBlock: BasicBlock {
    fatalError("undef has no defining block")
  }
}

final class PlaceholderValue : Value {
  public var definingInstruction: Instruction? { nil }
  public var definingBlock: BasicBlock {
    fatalError("PlaceholderValue has no defining block")
  }
}

extension OptionalBridgedValue {
  var value: Value? { obj.getAs(AnyObject.self) as? Value }
}

extension BridgedOwnership {
  var ownership: Ownership {
    switch self {
      case Ownership_Unowned:    return .unowned
      case Ownership_Owned:      return .owned
      case Ownership_Guaranteed: return .guaranteed
      case Ownership_None:       return .none
      default:
        fatalError("unsupported ownership")
    }
  }
}
