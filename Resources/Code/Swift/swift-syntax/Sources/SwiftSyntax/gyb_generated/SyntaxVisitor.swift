//// Automatically Generated From SyntaxVisitor.swift.gyb.
//// Do Not Edit Directly!
//===------------- SyntaxVisitor.swift - Syntax Visitor class -------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2022 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

/// The enum describes how the SyntaxVistor should continue after visiting
/// the current node.
public enum SyntaxVisitorContinueKind {

  /// The visitor should visit the descendents of the current node.
  case visitChildren

  /// The visitor should avoid visiting the descendents of the current node.
  case skipChildren
}

open class SyntaxVisitor {
  public let viewMode: SyntaxTreeViewMode

  @available(*, deprecated, message: "Use init(viewMode:) instead")
  public convenience init() {
    self.init(viewMode: .sourceAccurate)
  }

  public init(viewMode: SyntaxTreeViewMode) {
    self.viewMode = viewMode
  }

  /// Walk all nodes of the given syntax tree, calling the corresponding `visit` 
  /// function for every node that is being visited.
  public func walk<SyntaxType: SyntaxProtocol>(_ node: SyntaxType) {
    visit(node.data)
  }

  /// Visiting `UnknownDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnknownDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `UnknownDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnknownDeclSyntax) {}
  /// Visiting `UnknownExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnknownExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `UnknownExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnknownExprSyntax) {}
  /// Visiting `UnknownStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnknownStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `UnknownStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnknownStmtSyntax) {}
  /// Visiting `UnknownTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnknownTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `UnknownTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnknownTypeSyntax) {}
  /// Visiting `UnknownPatternSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnknownPatternSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `UnknownPatternSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnknownPatternSyntax) {}
  /// Visiting `MissingSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MissingSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MissingSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MissingSyntax) {}
  /// Visiting `MissingDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MissingDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MissingDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MissingDeclSyntax) {}
  /// Visiting `MissingExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MissingExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MissingExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MissingExprSyntax) {}
  /// Visiting `MissingStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MissingStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MissingStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MissingStmtSyntax) {}
  /// Visiting `MissingTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MissingTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MissingTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MissingTypeSyntax) {}
  /// Visiting `MissingPatternSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MissingPatternSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MissingPatternSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MissingPatternSyntax) {}
  /// Visiting `CodeBlockItemSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CodeBlockItemSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CodeBlockItemSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CodeBlockItemSyntax) {}
  /// Visiting `CodeBlockItemListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CodeBlockItemListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CodeBlockItemListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CodeBlockItemListSyntax) {}
  /// Visiting `CodeBlockSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CodeBlockSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CodeBlockSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CodeBlockSyntax) {}
  /// Visiting `UnexpectedNodesSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnexpectedNodesSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `UnexpectedNodesSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnexpectedNodesSyntax) {}
  /// Visiting `InOutExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: InOutExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `InOutExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: InOutExprSyntax) {}
  /// Visiting `PoundColumnExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundColumnExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundColumnExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundColumnExprSyntax) {}
  /// Visiting `TupleExprElementListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TupleExprElementListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TupleExprElementListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TupleExprElementListSyntax) {}
  /// Visiting `ArrayElementListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ArrayElementListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ArrayElementListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ArrayElementListSyntax) {}
  /// Visiting `DictionaryElementListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DictionaryElementListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DictionaryElementListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DictionaryElementListSyntax) {}
  /// Visiting `StringLiteralSegmentsSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: StringLiteralSegmentsSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `StringLiteralSegmentsSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: StringLiteralSegmentsSyntax) {}
  /// Visiting `TryExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TryExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TryExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TryExprSyntax) {}
  /// Visiting `AwaitExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AwaitExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AwaitExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AwaitExprSyntax) {}
  /// Visiting `MoveExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MoveExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MoveExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MoveExprSyntax) {}
  /// Visiting `DeclNameArgumentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DeclNameArgumentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DeclNameArgumentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DeclNameArgumentSyntax) {}
  /// Visiting `DeclNameArgumentListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DeclNameArgumentListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DeclNameArgumentListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DeclNameArgumentListSyntax) {}
  /// Visiting `DeclNameArgumentsSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DeclNameArgumentsSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DeclNameArgumentsSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DeclNameArgumentsSyntax) {}
  /// Visiting `IdentifierExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: IdentifierExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `IdentifierExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: IdentifierExprSyntax) {}
  /// Visiting `SuperRefExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SuperRefExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SuperRefExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SuperRefExprSyntax) {}
  /// Visiting `NilLiteralExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: NilLiteralExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `NilLiteralExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: NilLiteralExprSyntax) {}
  /// Visiting `DiscardAssignmentExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DiscardAssignmentExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DiscardAssignmentExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DiscardAssignmentExprSyntax) {}
  /// Visiting `AssignmentExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AssignmentExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AssignmentExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AssignmentExprSyntax) {}
  /// Visiting `SequenceExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SequenceExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SequenceExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SequenceExprSyntax) {}
  /// Visiting `ExprListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ExprListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ExprListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ExprListSyntax) {}
  /// Visiting `PoundLineExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundLineExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundLineExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundLineExprSyntax) {}
  /// Visiting `PoundFileExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundFileExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundFileExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundFileExprSyntax) {}
  /// Visiting `PoundFileIDExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundFileIDExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundFileIDExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundFileIDExprSyntax) {}
  /// Visiting `PoundFilePathExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundFilePathExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundFilePathExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundFilePathExprSyntax) {}
  /// Visiting `PoundFunctionExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundFunctionExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundFunctionExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundFunctionExprSyntax) {}
  /// Visiting `PoundDsohandleExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundDsohandleExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundDsohandleExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundDsohandleExprSyntax) {}
  /// Visiting `SymbolicReferenceExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SymbolicReferenceExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SymbolicReferenceExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SymbolicReferenceExprSyntax) {}
  /// Visiting `PrefixOperatorExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PrefixOperatorExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PrefixOperatorExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PrefixOperatorExprSyntax) {}
  /// Visiting `BinaryOperatorExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: BinaryOperatorExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `BinaryOperatorExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: BinaryOperatorExprSyntax) {}
  /// Visiting `ArrowExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ArrowExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ArrowExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ArrowExprSyntax) {}
  /// Visiting `InfixOperatorExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: InfixOperatorExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `InfixOperatorExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: InfixOperatorExprSyntax) {}
  /// Visiting `FloatLiteralExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: FloatLiteralExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `FloatLiteralExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: FloatLiteralExprSyntax) {}
  /// Visiting `TupleExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TupleExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TupleExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TupleExprSyntax) {}
  /// Visiting `ArrayExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ArrayExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ArrayExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ArrayExprSyntax) {}
  /// Visiting `DictionaryExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DictionaryExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DictionaryExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DictionaryExprSyntax) {}
  /// Visiting `TupleExprElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TupleExprElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TupleExprElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TupleExprElementSyntax) {}
  /// Visiting `ArrayElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ArrayElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ArrayElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ArrayElementSyntax) {}
  /// Visiting `DictionaryElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DictionaryElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DictionaryElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DictionaryElementSyntax) {}
  /// Visiting `IntegerLiteralExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: IntegerLiteralExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `IntegerLiteralExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: IntegerLiteralExprSyntax) {}
  /// Visiting `BooleanLiteralExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: BooleanLiteralExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `BooleanLiteralExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: BooleanLiteralExprSyntax) {}
  /// Visiting `UnresolvedTernaryExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnresolvedTernaryExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `UnresolvedTernaryExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnresolvedTernaryExprSyntax) {}
  /// Visiting `TernaryExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TernaryExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TernaryExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TernaryExprSyntax) {}
  /// Visiting `MemberAccessExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MemberAccessExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MemberAccessExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MemberAccessExprSyntax) {}
  /// Visiting `UnresolvedIsExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnresolvedIsExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `UnresolvedIsExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnresolvedIsExprSyntax) {}
  /// Visiting `IsExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: IsExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `IsExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: IsExprSyntax) {}
  /// Visiting `UnresolvedAsExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnresolvedAsExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `UnresolvedAsExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnresolvedAsExprSyntax) {}
  /// Visiting `AsExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AsExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AsExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AsExprSyntax) {}
  /// Visiting `TypeExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TypeExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TypeExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TypeExprSyntax) {}
  /// Visiting `ClosureCaptureItemSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ClosureCaptureItemSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ClosureCaptureItemSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ClosureCaptureItemSyntax) {}
  /// Visiting `ClosureCaptureItemListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ClosureCaptureItemListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ClosureCaptureItemListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ClosureCaptureItemListSyntax) {}
  /// Visiting `ClosureCaptureSignatureSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ClosureCaptureSignatureSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ClosureCaptureSignatureSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ClosureCaptureSignatureSyntax) {}
  /// Visiting `ClosureParamSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ClosureParamSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ClosureParamSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ClosureParamSyntax) {}
  /// Visiting `ClosureParamListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ClosureParamListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ClosureParamListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ClosureParamListSyntax) {}
  /// Visiting `ClosureSignatureSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ClosureSignatureSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ClosureSignatureSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ClosureSignatureSyntax) {}
  /// Visiting `ClosureExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ClosureExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ClosureExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ClosureExprSyntax) {}
  /// Visiting `UnresolvedPatternExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnresolvedPatternExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `UnresolvedPatternExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnresolvedPatternExprSyntax) {}
  /// Visiting `MultipleTrailingClosureElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MultipleTrailingClosureElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MultipleTrailingClosureElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MultipleTrailingClosureElementSyntax) {}
  /// Visiting `MultipleTrailingClosureElementListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MultipleTrailingClosureElementListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MultipleTrailingClosureElementListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MultipleTrailingClosureElementListSyntax) {}
  /// Visiting `FunctionCallExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: FunctionCallExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `FunctionCallExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: FunctionCallExprSyntax) {}
  /// Visiting `SubscriptExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SubscriptExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SubscriptExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SubscriptExprSyntax) {}
  /// Visiting `OptionalChainingExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: OptionalChainingExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `OptionalChainingExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: OptionalChainingExprSyntax) {}
  /// Visiting `ForcedValueExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ForcedValueExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ForcedValueExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ForcedValueExprSyntax) {}
  /// Visiting `PostfixUnaryExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PostfixUnaryExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PostfixUnaryExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PostfixUnaryExprSyntax) {}
  /// Visiting `SpecializeExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SpecializeExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SpecializeExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SpecializeExprSyntax) {}
  /// Visiting `StringSegmentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: StringSegmentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `StringSegmentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: StringSegmentSyntax) {}
  /// Visiting `ExpressionSegmentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ExpressionSegmentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ExpressionSegmentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ExpressionSegmentSyntax) {}
  /// Visiting `StringLiteralExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: StringLiteralExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `StringLiteralExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: StringLiteralExprSyntax) {}
  /// Visiting `RegexLiteralExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: RegexLiteralExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `RegexLiteralExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: RegexLiteralExprSyntax) {}
  /// Visiting `KeyPathExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: KeyPathExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `KeyPathExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: KeyPathExprSyntax) {}
  /// Visiting `KeyPathComponentListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: KeyPathComponentListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `KeyPathComponentListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: KeyPathComponentListSyntax) {}
  /// Visiting `KeyPathComponentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: KeyPathComponentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `KeyPathComponentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: KeyPathComponentSyntax) {}
  /// Visiting `KeyPathPropertyComponentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: KeyPathPropertyComponentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `KeyPathPropertyComponentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: KeyPathPropertyComponentSyntax) {}
  /// Visiting `KeyPathSubscriptComponentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: KeyPathSubscriptComponentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `KeyPathSubscriptComponentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: KeyPathSubscriptComponentSyntax) {}
  /// Visiting `KeyPathOptionalComponentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: KeyPathOptionalComponentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `KeyPathOptionalComponentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: KeyPathOptionalComponentSyntax) {}
  /// Visiting `OldKeyPathExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: OldKeyPathExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `OldKeyPathExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: OldKeyPathExprSyntax) {}
  /// Visiting `KeyPathBaseExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: KeyPathBaseExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `KeyPathBaseExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: KeyPathBaseExprSyntax) {}
  /// Visiting `ObjcNamePieceSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ObjcNamePieceSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ObjcNamePieceSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ObjcNamePieceSyntax) {}
  /// Visiting `ObjcNameSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ObjcNameSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ObjcNameSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ObjcNameSyntax) {}
  /// Visiting `ObjcKeyPathExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ObjcKeyPathExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ObjcKeyPathExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ObjcKeyPathExprSyntax) {}
  /// Visiting `ObjcSelectorExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ObjcSelectorExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ObjcSelectorExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ObjcSelectorExprSyntax) {}
  /// Visiting `PostfixIfConfigExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PostfixIfConfigExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PostfixIfConfigExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PostfixIfConfigExprSyntax) {}
  /// Visiting `EditorPlaceholderExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: EditorPlaceholderExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `EditorPlaceholderExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: EditorPlaceholderExprSyntax) {}
  /// Visiting `ObjectLiteralExprSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ObjectLiteralExprSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ObjectLiteralExprSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ObjectLiteralExprSyntax) {}
  /// Visiting `YieldExprListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: YieldExprListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `YieldExprListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: YieldExprListSyntax) {}
  /// Visiting `YieldExprListElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: YieldExprListElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `YieldExprListElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: YieldExprListElementSyntax) {}
  /// Visiting `TypeInitializerClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TypeInitializerClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TypeInitializerClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TypeInitializerClauseSyntax) {}
  /// Visiting `TypealiasDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TypealiasDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TypealiasDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TypealiasDeclSyntax) {}
  /// Visiting `AssociatedtypeDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AssociatedtypeDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AssociatedtypeDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AssociatedtypeDeclSyntax) {}
  /// Visiting `FunctionParameterListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: FunctionParameterListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `FunctionParameterListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: FunctionParameterListSyntax) {}
  /// Visiting `ParameterClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ParameterClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ParameterClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ParameterClauseSyntax) {}
  /// Visiting `ReturnClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ReturnClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ReturnClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ReturnClauseSyntax) {}
  /// Visiting `FunctionSignatureSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: FunctionSignatureSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `FunctionSignatureSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: FunctionSignatureSyntax) {}
  /// Visiting `IfConfigClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: IfConfigClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `IfConfigClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: IfConfigClauseSyntax) {}
  /// Visiting `IfConfigClauseListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: IfConfigClauseListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `IfConfigClauseListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: IfConfigClauseListSyntax) {}
  /// Visiting `IfConfigDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: IfConfigDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `IfConfigDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: IfConfigDeclSyntax) {}
  /// Visiting `PoundErrorDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundErrorDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundErrorDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundErrorDeclSyntax) {}
  /// Visiting `PoundWarningDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundWarningDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundWarningDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundWarningDeclSyntax) {}
  /// Visiting `PoundSourceLocationSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundSourceLocationSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundSourceLocationSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundSourceLocationSyntax) {}
  /// Visiting `PoundSourceLocationArgsSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundSourceLocationArgsSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundSourceLocationArgsSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundSourceLocationArgsSyntax) {}
  /// Visiting `DeclModifierDetailSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DeclModifierDetailSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DeclModifierDetailSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DeclModifierDetailSyntax) {}
  /// Visiting `DeclModifierSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DeclModifierSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DeclModifierSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DeclModifierSyntax) {}
  /// Visiting `InheritedTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: InheritedTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `InheritedTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: InheritedTypeSyntax) {}
  /// Visiting `InheritedTypeListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: InheritedTypeListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `InheritedTypeListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: InheritedTypeListSyntax) {}
  /// Visiting `TypeInheritanceClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TypeInheritanceClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TypeInheritanceClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TypeInheritanceClauseSyntax) {}
  /// Visiting `ClassDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ClassDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ClassDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ClassDeclSyntax) {}
  /// Visiting `ActorDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ActorDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ActorDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ActorDeclSyntax) {}
  /// Visiting `StructDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: StructDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `StructDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: StructDeclSyntax) {}
  /// Visiting `ProtocolDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ProtocolDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ProtocolDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ProtocolDeclSyntax) {}
  /// Visiting `ExtensionDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ExtensionDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ExtensionDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ExtensionDeclSyntax) {}
  /// Visiting `MemberDeclBlockSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MemberDeclBlockSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MemberDeclBlockSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MemberDeclBlockSyntax) {}
  /// Visiting `MemberDeclListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MemberDeclListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MemberDeclListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MemberDeclListSyntax) {}
  /// Visiting `MemberDeclListItemSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MemberDeclListItemSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MemberDeclListItemSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MemberDeclListItemSyntax) {}
  /// Visiting `SourceFileSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SourceFileSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SourceFileSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SourceFileSyntax) {}
  /// Visiting `InitializerClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: InitializerClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `InitializerClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: InitializerClauseSyntax) {}
  /// Visiting `FunctionParameterSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: FunctionParameterSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `FunctionParameterSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: FunctionParameterSyntax) {}
  /// Visiting `ModifierListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ModifierListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ModifierListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ModifierListSyntax) {}
  /// Visiting `FunctionDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: FunctionDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `FunctionDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: FunctionDeclSyntax) {}
  /// Visiting `InitializerDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: InitializerDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `InitializerDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: InitializerDeclSyntax) {}
  /// Visiting `DeinitializerDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DeinitializerDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DeinitializerDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DeinitializerDeclSyntax) {}
  /// Visiting `SubscriptDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SubscriptDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SubscriptDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SubscriptDeclSyntax) {}
  /// Visiting `AccessLevelModifierSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AccessLevelModifierSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AccessLevelModifierSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AccessLevelModifierSyntax) {}
  /// Visiting `AccessPathComponentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AccessPathComponentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AccessPathComponentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AccessPathComponentSyntax) {}
  /// Visiting `AccessPathSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AccessPathSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AccessPathSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AccessPathSyntax) {}
  /// Visiting `ImportDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ImportDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ImportDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ImportDeclSyntax) {}
  /// Visiting `AccessorParameterSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AccessorParameterSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AccessorParameterSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AccessorParameterSyntax) {}
  /// Visiting `AccessorDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AccessorDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AccessorDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AccessorDeclSyntax) {}
  /// Visiting `AccessorListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AccessorListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AccessorListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AccessorListSyntax) {}
  /// Visiting `AccessorBlockSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AccessorBlockSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AccessorBlockSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AccessorBlockSyntax) {}
  /// Visiting `PatternBindingSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PatternBindingSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PatternBindingSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PatternBindingSyntax) {}
  /// Visiting `PatternBindingListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PatternBindingListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PatternBindingListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PatternBindingListSyntax) {}
  /// Visiting `VariableDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: VariableDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `VariableDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: VariableDeclSyntax) {}
  /// Visiting `EnumCaseElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: EnumCaseElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `EnumCaseElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: EnumCaseElementSyntax) {}
  /// Visiting `EnumCaseElementListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: EnumCaseElementListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `EnumCaseElementListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: EnumCaseElementListSyntax) {}
  /// Visiting `EnumCaseDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: EnumCaseDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `EnumCaseDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: EnumCaseDeclSyntax) {}
  /// Visiting `EnumDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: EnumDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `EnumDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: EnumDeclSyntax) {}
  /// Visiting `OperatorDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: OperatorDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `OperatorDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: OperatorDeclSyntax) {}
  /// Visiting `DesignatedTypeListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DesignatedTypeListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DesignatedTypeListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DesignatedTypeListSyntax) {}
  /// Visiting `DesignatedTypeElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DesignatedTypeElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DesignatedTypeElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DesignatedTypeElementSyntax) {}
  /// Visiting `OperatorPrecedenceAndTypesSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: OperatorPrecedenceAndTypesSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `OperatorPrecedenceAndTypesSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: OperatorPrecedenceAndTypesSyntax) {}
  /// Visiting `PrecedenceGroupDeclSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PrecedenceGroupDeclSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PrecedenceGroupDeclSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PrecedenceGroupDeclSyntax) {}
  /// Visiting `PrecedenceGroupAttributeListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PrecedenceGroupAttributeListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PrecedenceGroupAttributeListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PrecedenceGroupAttributeListSyntax) {}
  /// Visiting `PrecedenceGroupRelationSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PrecedenceGroupRelationSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PrecedenceGroupRelationSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PrecedenceGroupRelationSyntax) {}
  /// Visiting `PrecedenceGroupNameListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PrecedenceGroupNameListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PrecedenceGroupNameListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PrecedenceGroupNameListSyntax) {}
  /// Visiting `PrecedenceGroupNameElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PrecedenceGroupNameElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PrecedenceGroupNameElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PrecedenceGroupNameElementSyntax) {}
  /// Visiting `PrecedenceGroupAssignmentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PrecedenceGroupAssignmentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PrecedenceGroupAssignmentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PrecedenceGroupAssignmentSyntax) {}
  /// Visiting `PrecedenceGroupAssociativitySyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PrecedenceGroupAssociativitySyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PrecedenceGroupAssociativitySyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PrecedenceGroupAssociativitySyntax) {}
  /// Visiting `TokenListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TokenListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TokenListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TokenListSyntax) {}
  /// Visiting `NonEmptyTokenListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: NonEmptyTokenListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `NonEmptyTokenListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: NonEmptyTokenListSyntax) {}
  /// Visiting `CustomAttributeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CustomAttributeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CustomAttributeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CustomAttributeSyntax) {}
  /// Visiting `AttributeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AttributeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AttributeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AttributeSyntax) {}
  /// Visiting `AttributeListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AttributeListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AttributeListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AttributeListSyntax) {}
  /// Visiting `SpecializeAttributeSpecListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SpecializeAttributeSpecListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SpecializeAttributeSpecListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SpecializeAttributeSpecListSyntax) {}
  /// Visiting `AvailabilityEntrySyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AvailabilityEntrySyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AvailabilityEntrySyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AvailabilityEntrySyntax) {}
  /// Visiting `LabeledSpecializeEntrySyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: LabeledSpecializeEntrySyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `LabeledSpecializeEntrySyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: LabeledSpecializeEntrySyntax) {}
  /// Visiting `TargetFunctionEntrySyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TargetFunctionEntrySyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TargetFunctionEntrySyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TargetFunctionEntrySyntax) {}
  /// Visiting `NamedAttributeStringArgumentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: NamedAttributeStringArgumentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `NamedAttributeStringArgumentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: NamedAttributeStringArgumentSyntax) {}
  /// Visiting `DeclNameSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DeclNameSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DeclNameSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DeclNameSyntax) {}
  /// Visiting `ImplementsAttributeArgumentsSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ImplementsAttributeArgumentsSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ImplementsAttributeArgumentsSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ImplementsAttributeArgumentsSyntax) {}
  /// Visiting `ObjCSelectorPieceSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ObjCSelectorPieceSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ObjCSelectorPieceSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ObjCSelectorPieceSyntax) {}
  /// Visiting `ObjCSelectorSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ObjCSelectorSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ObjCSelectorSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ObjCSelectorSyntax) {}
  /// Visiting `DifferentiableAttributeArgumentsSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DifferentiableAttributeArgumentsSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DifferentiableAttributeArgumentsSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DifferentiableAttributeArgumentsSyntax) {}
  /// Visiting `DifferentiabilityParamsClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DifferentiabilityParamsClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DifferentiabilityParamsClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DifferentiabilityParamsClauseSyntax) {}
  /// Visiting `DifferentiabilityParamsSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DifferentiabilityParamsSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DifferentiabilityParamsSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DifferentiabilityParamsSyntax) {}
  /// Visiting `DifferentiabilityParamListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DifferentiabilityParamListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DifferentiabilityParamListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DifferentiabilityParamListSyntax) {}
  /// Visiting `DifferentiabilityParamSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DifferentiabilityParamSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DifferentiabilityParamSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DifferentiabilityParamSyntax) {}
  /// Visiting `DerivativeRegistrationAttributeArgumentsSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DerivativeRegistrationAttributeArgumentsSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DerivativeRegistrationAttributeArgumentsSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DerivativeRegistrationAttributeArgumentsSyntax) {}
  /// Visiting `QualifiedDeclNameSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: QualifiedDeclNameSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `QualifiedDeclNameSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: QualifiedDeclNameSyntax) {}
  /// Visiting `FunctionDeclNameSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: FunctionDeclNameSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `FunctionDeclNameSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: FunctionDeclNameSyntax) {}
  /// Visiting `BackDeployAttributeSpecListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: BackDeployAttributeSpecListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `BackDeployAttributeSpecListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: BackDeployAttributeSpecListSyntax) {}
  /// Visiting `BackDeployVersionListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: BackDeployVersionListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `BackDeployVersionListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: BackDeployVersionListSyntax) {}
  /// Visiting `BackDeployVersionArgumentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: BackDeployVersionArgumentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `BackDeployVersionArgumentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: BackDeployVersionArgumentSyntax) {}
  /// Visiting `OpaqueReturnTypeOfAttributeArgumentsSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: OpaqueReturnTypeOfAttributeArgumentsSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `OpaqueReturnTypeOfAttributeArgumentsSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: OpaqueReturnTypeOfAttributeArgumentsSyntax) {}
  /// Visiting `ConventionAttributeArgumentsSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ConventionAttributeArgumentsSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ConventionAttributeArgumentsSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ConventionAttributeArgumentsSyntax) {}
  /// Visiting `ConventionWitnessMethodAttributeArgumentsSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ConventionWitnessMethodAttributeArgumentsSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ConventionWitnessMethodAttributeArgumentsSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ConventionWitnessMethodAttributeArgumentsSyntax) {}
  /// Visiting `LabeledStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: LabeledStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `LabeledStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: LabeledStmtSyntax) {}
  /// Visiting `ContinueStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ContinueStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ContinueStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ContinueStmtSyntax) {}
  /// Visiting `WhileStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: WhileStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `WhileStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: WhileStmtSyntax) {}
  /// Visiting `DeferStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DeferStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DeferStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DeferStmtSyntax) {}
  /// Visiting `ExpressionStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ExpressionStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ExpressionStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ExpressionStmtSyntax) {}
  /// Visiting `SwitchCaseListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SwitchCaseListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SwitchCaseListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SwitchCaseListSyntax) {}
  /// Visiting `RepeatWhileStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: RepeatWhileStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `RepeatWhileStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: RepeatWhileStmtSyntax) {}
  /// Visiting `GuardStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: GuardStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `GuardStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: GuardStmtSyntax) {}
  /// Visiting `WhereClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: WhereClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `WhereClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: WhereClauseSyntax) {}
  /// Visiting `ForInStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ForInStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ForInStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ForInStmtSyntax) {}
  /// Visiting `SwitchStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SwitchStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SwitchStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SwitchStmtSyntax) {}
  /// Visiting `CatchClauseListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CatchClauseListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CatchClauseListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CatchClauseListSyntax) {}
  /// Visiting `DoStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DoStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DoStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DoStmtSyntax) {}
  /// Visiting `ReturnStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ReturnStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ReturnStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ReturnStmtSyntax) {}
  /// Visiting `YieldStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: YieldStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `YieldStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: YieldStmtSyntax) {}
  /// Visiting `YieldListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: YieldListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `YieldListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: YieldListSyntax) {}
  /// Visiting `FallthroughStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: FallthroughStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `FallthroughStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: FallthroughStmtSyntax) {}
  /// Visiting `BreakStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: BreakStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `BreakStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: BreakStmtSyntax) {}
  /// Visiting `CaseItemListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CaseItemListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CaseItemListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CaseItemListSyntax) {}
  /// Visiting `CatchItemListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CatchItemListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CatchItemListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CatchItemListSyntax) {}
  /// Visiting `ConditionElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ConditionElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ConditionElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ConditionElementSyntax) {}
  /// Visiting `AvailabilityConditionSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AvailabilityConditionSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AvailabilityConditionSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AvailabilityConditionSyntax) {}
  /// Visiting `MatchingPatternConditionSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MatchingPatternConditionSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MatchingPatternConditionSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MatchingPatternConditionSyntax) {}
  /// Visiting `OptionalBindingConditionSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: OptionalBindingConditionSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `OptionalBindingConditionSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: OptionalBindingConditionSyntax) {}
  /// Visiting `UnavailabilityConditionSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnavailabilityConditionSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `UnavailabilityConditionSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnavailabilityConditionSyntax) {}
  /// Visiting `HasSymbolConditionSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: HasSymbolConditionSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `HasSymbolConditionSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: HasSymbolConditionSyntax) {}
  /// Visiting `ConditionElementListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ConditionElementListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ConditionElementListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ConditionElementListSyntax) {}
  /// Visiting `DeclarationStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DeclarationStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DeclarationStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DeclarationStmtSyntax) {}
  /// Visiting `ThrowStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ThrowStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ThrowStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ThrowStmtSyntax) {}
  /// Visiting `IfStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: IfStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `IfStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: IfStmtSyntax) {}
  /// Visiting `ElseIfContinuationSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ElseIfContinuationSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ElseIfContinuationSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ElseIfContinuationSyntax) {}
  /// Visiting `ElseBlockSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ElseBlockSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ElseBlockSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ElseBlockSyntax) {}
  /// Visiting `SwitchCaseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SwitchCaseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SwitchCaseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SwitchCaseSyntax) {}
  /// Visiting `SwitchDefaultLabelSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SwitchDefaultLabelSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SwitchDefaultLabelSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SwitchDefaultLabelSyntax) {}
  /// Visiting `CaseItemSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CaseItemSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CaseItemSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CaseItemSyntax) {}
  /// Visiting `CatchItemSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CatchItemSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CatchItemSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CatchItemSyntax) {}
  /// Visiting `SwitchCaseLabelSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SwitchCaseLabelSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SwitchCaseLabelSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SwitchCaseLabelSyntax) {}
  /// Visiting `CatchClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CatchClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CatchClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CatchClauseSyntax) {}
  /// Visiting `PoundAssertStmtSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PoundAssertStmtSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PoundAssertStmtSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PoundAssertStmtSyntax) {}
  /// Visiting `GenericWhereClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: GenericWhereClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `GenericWhereClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: GenericWhereClauseSyntax) {}
  /// Visiting `GenericRequirementListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: GenericRequirementListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `GenericRequirementListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: GenericRequirementListSyntax) {}
  /// Visiting `GenericRequirementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: GenericRequirementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `GenericRequirementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: GenericRequirementSyntax) {}
  /// Visiting `SameTypeRequirementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SameTypeRequirementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SameTypeRequirementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SameTypeRequirementSyntax) {}
  /// Visiting `LayoutRequirementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: LayoutRequirementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `LayoutRequirementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: LayoutRequirementSyntax) {}
  /// Visiting `GenericParameterListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: GenericParameterListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `GenericParameterListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: GenericParameterListSyntax) {}
  /// Visiting `GenericParameterSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: GenericParameterSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `GenericParameterSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: GenericParameterSyntax) {}
  /// Visiting `PrimaryAssociatedTypeListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PrimaryAssociatedTypeListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PrimaryAssociatedTypeListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PrimaryAssociatedTypeListSyntax) {}
  /// Visiting `PrimaryAssociatedTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PrimaryAssociatedTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PrimaryAssociatedTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PrimaryAssociatedTypeSyntax) {}
  /// Visiting `GenericParameterClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: GenericParameterClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `GenericParameterClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: GenericParameterClauseSyntax) {}
  /// Visiting `ConformanceRequirementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ConformanceRequirementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ConformanceRequirementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ConformanceRequirementSyntax) {}
  /// Visiting `PrimaryAssociatedTypeClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PrimaryAssociatedTypeClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PrimaryAssociatedTypeClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PrimaryAssociatedTypeClauseSyntax) {}
  /// Visiting `SimpleTypeIdentifierSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: SimpleTypeIdentifierSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `SimpleTypeIdentifierSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: SimpleTypeIdentifierSyntax) {}
  /// Visiting `MemberTypeIdentifierSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MemberTypeIdentifierSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MemberTypeIdentifierSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MemberTypeIdentifierSyntax) {}
  /// Visiting `ClassRestrictionTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ClassRestrictionTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ClassRestrictionTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ClassRestrictionTypeSyntax) {}
  /// Visiting `ArrayTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ArrayTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ArrayTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ArrayTypeSyntax) {}
  /// Visiting `DictionaryTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: DictionaryTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `DictionaryTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: DictionaryTypeSyntax) {}
  /// Visiting `MetatypeTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: MetatypeTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `MetatypeTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: MetatypeTypeSyntax) {}
  /// Visiting `OptionalTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: OptionalTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `OptionalTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: OptionalTypeSyntax) {}
  /// Visiting `ConstrainedSugarTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ConstrainedSugarTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ConstrainedSugarTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ConstrainedSugarTypeSyntax) {}
  /// Visiting `ImplicitlyUnwrappedOptionalTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ImplicitlyUnwrappedOptionalTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ImplicitlyUnwrappedOptionalTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ImplicitlyUnwrappedOptionalTypeSyntax) {}
  /// Visiting `CompositionTypeElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CompositionTypeElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CompositionTypeElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CompositionTypeElementSyntax) {}
  /// Visiting `CompositionTypeElementListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CompositionTypeElementListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CompositionTypeElementListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CompositionTypeElementListSyntax) {}
  /// Visiting `CompositionTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: CompositionTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `CompositionTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: CompositionTypeSyntax) {}
  /// Visiting `PackExpansionTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: PackExpansionTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `PackExpansionTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: PackExpansionTypeSyntax) {}
  /// Visiting `TupleTypeElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TupleTypeElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TupleTypeElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TupleTypeElementSyntax) {}
  /// Visiting `TupleTypeElementListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TupleTypeElementListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TupleTypeElementListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TupleTypeElementListSyntax) {}
  /// Visiting `TupleTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TupleTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TupleTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TupleTypeSyntax) {}
  /// Visiting `FunctionTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: FunctionTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `FunctionTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: FunctionTypeSyntax) {}
  /// Visiting `AttributedTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AttributedTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AttributedTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AttributedTypeSyntax) {}
  /// Visiting `GenericArgumentListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: GenericArgumentListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `GenericArgumentListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: GenericArgumentListSyntax) {}
  /// Visiting `GenericArgumentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: GenericArgumentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `GenericArgumentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: GenericArgumentSyntax) {}
  /// Visiting `GenericArgumentClauseSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: GenericArgumentClauseSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `GenericArgumentClauseSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: GenericArgumentClauseSyntax) {}
  /// Visiting `NamedOpaqueReturnTypeSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: NamedOpaqueReturnTypeSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `NamedOpaqueReturnTypeSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: NamedOpaqueReturnTypeSyntax) {}
  /// Visiting `TypeAnnotationSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TypeAnnotationSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TypeAnnotationSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TypeAnnotationSyntax) {}
  /// Visiting `EnumCasePatternSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: EnumCasePatternSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `EnumCasePatternSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: EnumCasePatternSyntax) {}
  /// Visiting `IsTypePatternSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: IsTypePatternSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `IsTypePatternSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: IsTypePatternSyntax) {}
  /// Visiting `OptionalPatternSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: OptionalPatternSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `OptionalPatternSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: OptionalPatternSyntax) {}
  /// Visiting `IdentifierPatternSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: IdentifierPatternSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `IdentifierPatternSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: IdentifierPatternSyntax) {}
  /// Visiting `AsTypePatternSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AsTypePatternSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AsTypePatternSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AsTypePatternSyntax) {}
  /// Visiting `TuplePatternSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TuplePatternSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TuplePatternSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TuplePatternSyntax) {}
  /// Visiting `WildcardPatternSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: WildcardPatternSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `WildcardPatternSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: WildcardPatternSyntax) {}
  /// Visiting `TuplePatternElementSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TuplePatternElementSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TuplePatternElementSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TuplePatternElementSyntax) {}
  /// Visiting `ExpressionPatternSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ExpressionPatternSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ExpressionPatternSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ExpressionPatternSyntax) {}
  /// Visiting `TuplePatternElementListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: TuplePatternElementListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `TuplePatternElementListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TuplePatternElementListSyntax) {}
  /// Visiting `ValueBindingPatternSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: ValueBindingPatternSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `ValueBindingPatternSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: ValueBindingPatternSyntax) {}
  /// Visiting `AvailabilitySpecListSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AvailabilitySpecListSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AvailabilitySpecListSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AvailabilitySpecListSyntax) {}
  /// Visiting `AvailabilityArgumentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AvailabilityArgumentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AvailabilityArgumentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AvailabilityArgumentSyntax) {}
  /// Visiting `AvailabilityLabeledArgumentSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AvailabilityLabeledArgumentSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AvailabilityLabeledArgumentSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AvailabilityLabeledArgumentSyntax) {}
  /// Visiting `AvailabilityVersionRestrictionSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: AvailabilityVersionRestrictionSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `AvailabilityVersionRestrictionSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: AvailabilityVersionRestrictionSyntax) {}
  /// Visiting `VersionTupleSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: VersionTupleSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting `VersionTupleSyntax` and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: VersionTupleSyntax) {}

  /// Visiting `TokenSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ token: TokenSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting the node and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: TokenSyntax) {}

  /// Visiting `UnknownSyntax` specifically.
  ///   - Parameter node: the node we are visiting.
  ///   - Returns: how should we continue visiting.
  open func visit(_ node: UnknownSyntax) -> SyntaxVisitorContinueKind {
    return .visitChildren
  }

  /// The function called after visiting the node and its descendents.
  ///   - node: the node we just finished visiting.
  open func visitPost(_ node: UnknownSyntax) {}

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDeclSyntax(_ data: SyntaxData) {
      let node = UnknownDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplExprSyntax(_ data: SyntaxData) {
      let node = UnknownExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplStmtSyntax(_ data: SyntaxData) {
      let node = UnknownStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTypeSyntax(_ data: SyntaxData) {
      let node = UnknownTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPatternSyntax(_ data: SyntaxData) {
      let node = UnknownPatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplUnknownDeclSyntax(_ data: SyntaxData) {
      let node = UnknownDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplUnknownExprSyntax(_ data: SyntaxData) {
      let node = UnknownExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplUnknownStmtSyntax(_ data: SyntaxData) {
      let node = UnknownStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplUnknownTypeSyntax(_ data: SyntaxData) {
      let node = UnknownTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplUnknownPatternSyntax(_ data: SyntaxData) {
      let node = UnknownPatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMissingSyntax(_ data: SyntaxData) {
      let node = MissingSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMissingDeclSyntax(_ data: SyntaxData) {
      let node = MissingDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMissingExprSyntax(_ data: SyntaxData) {
      let node = MissingExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMissingStmtSyntax(_ data: SyntaxData) {
      let node = MissingStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMissingTypeSyntax(_ data: SyntaxData) {
      let node = MissingTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMissingPatternSyntax(_ data: SyntaxData) {
      let node = MissingPatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCodeBlockItemSyntax(_ data: SyntaxData) {
      let node = CodeBlockItemSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCodeBlockItemListSyntax(_ data: SyntaxData) {
      let node = CodeBlockItemListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCodeBlockSyntax(_ data: SyntaxData) {
      let node = CodeBlockSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplUnexpectedNodesSyntax(_ data: SyntaxData) {
      let node = UnexpectedNodesSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplInOutExprSyntax(_ data: SyntaxData) {
      let node = InOutExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundColumnExprSyntax(_ data: SyntaxData) {
      let node = PoundColumnExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTupleExprElementListSyntax(_ data: SyntaxData) {
      let node = TupleExprElementListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplArrayElementListSyntax(_ data: SyntaxData) {
      let node = ArrayElementListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDictionaryElementListSyntax(_ data: SyntaxData) {
      let node = DictionaryElementListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplStringLiteralSegmentsSyntax(_ data: SyntaxData) {
      let node = StringLiteralSegmentsSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTryExprSyntax(_ data: SyntaxData) {
      let node = TryExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAwaitExprSyntax(_ data: SyntaxData) {
      let node = AwaitExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMoveExprSyntax(_ data: SyntaxData) {
      let node = MoveExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDeclNameArgumentSyntax(_ data: SyntaxData) {
      let node = DeclNameArgumentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDeclNameArgumentListSyntax(_ data: SyntaxData) {
      let node = DeclNameArgumentListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDeclNameArgumentsSyntax(_ data: SyntaxData) {
      let node = DeclNameArgumentsSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplIdentifierExprSyntax(_ data: SyntaxData) {
      let node = IdentifierExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSuperRefExprSyntax(_ data: SyntaxData) {
      let node = SuperRefExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplNilLiteralExprSyntax(_ data: SyntaxData) {
      let node = NilLiteralExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDiscardAssignmentExprSyntax(_ data: SyntaxData) {
      let node = DiscardAssignmentExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAssignmentExprSyntax(_ data: SyntaxData) {
      let node = AssignmentExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSequenceExprSyntax(_ data: SyntaxData) {
      let node = SequenceExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplExprListSyntax(_ data: SyntaxData) {
      let node = ExprListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundLineExprSyntax(_ data: SyntaxData) {
      let node = PoundLineExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundFileExprSyntax(_ data: SyntaxData) {
      let node = PoundFileExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundFileIDExprSyntax(_ data: SyntaxData) {
      let node = PoundFileIDExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundFilePathExprSyntax(_ data: SyntaxData) {
      let node = PoundFilePathExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundFunctionExprSyntax(_ data: SyntaxData) {
      let node = PoundFunctionExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundDsohandleExprSyntax(_ data: SyntaxData) {
      let node = PoundDsohandleExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSymbolicReferenceExprSyntax(_ data: SyntaxData) {
      let node = SymbolicReferenceExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPrefixOperatorExprSyntax(_ data: SyntaxData) {
      let node = PrefixOperatorExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplBinaryOperatorExprSyntax(_ data: SyntaxData) {
      let node = BinaryOperatorExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplArrowExprSyntax(_ data: SyntaxData) {
      let node = ArrowExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplInfixOperatorExprSyntax(_ data: SyntaxData) {
      let node = InfixOperatorExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplFloatLiteralExprSyntax(_ data: SyntaxData) {
      let node = FloatLiteralExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTupleExprSyntax(_ data: SyntaxData) {
      let node = TupleExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplArrayExprSyntax(_ data: SyntaxData) {
      let node = ArrayExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDictionaryExprSyntax(_ data: SyntaxData) {
      let node = DictionaryExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTupleExprElementSyntax(_ data: SyntaxData) {
      let node = TupleExprElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplArrayElementSyntax(_ data: SyntaxData) {
      let node = ArrayElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDictionaryElementSyntax(_ data: SyntaxData) {
      let node = DictionaryElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplIntegerLiteralExprSyntax(_ data: SyntaxData) {
      let node = IntegerLiteralExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplBooleanLiteralExprSyntax(_ data: SyntaxData) {
      let node = BooleanLiteralExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplUnresolvedTernaryExprSyntax(_ data: SyntaxData) {
      let node = UnresolvedTernaryExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTernaryExprSyntax(_ data: SyntaxData) {
      let node = TernaryExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMemberAccessExprSyntax(_ data: SyntaxData) {
      let node = MemberAccessExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplUnresolvedIsExprSyntax(_ data: SyntaxData) {
      let node = UnresolvedIsExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplIsExprSyntax(_ data: SyntaxData) {
      let node = IsExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplUnresolvedAsExprSyntax(_ data: SyntaxData) {
      let node = UnresolvedAsExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAsExprSyntax(_ data: SyntaxData) {
      let node = AsExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTypeExprSyntax(_ data: SyntaxData) {
      let node = TypeExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplClosureCaptureItemSyntax(_ data: SyntaxData) {
      let node = ClosureCaptureItemSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplClosureCaptureItemListSyntax(_ data: SyntaxData) {
      let node = ClosureCaptureItemListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplClosureCaptureSignatureSyntax(_ data: SyntaxData) {
      let node = ClosureCaptureSignatureSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplClosureParamSyntax(_ data: SyntaxData) {
      let node = ClosureParamSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplClosureParamListSyntax(_ data: SyntaxData) {
      let node = ClosureParamListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplClosureSignatureSyntax(_ data: SyntaxData) {
      let node = ClosureSignatureSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplClosureExprSyntax(_ data: SyntaxData) {
      let node = ClosureExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplUnresolvedPatternExprSyntax(_ data: SyntaxData) {
      let node = UnresolvedPatternExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMultipleTrailingClosureElementSyntax(_ data: SyntaxData) {
      let node = MultipleTrailingClosureElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMultipleTrailingClosureElementListSyntax(_ data: SyntaxData) {
      let node = MultipleTrailingClosureElementListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplFunctionCallExprSyntax(_ data: SyntaxData) {
      let node = FunctionCallExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSubscriptExprSyntax(_ data: SyntaxData) {
      let node = SubscriptExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplOptionalChainingExprSyntax(_ data: SyntaxData) {
      let node = OptionalChainingExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplForcedValueExprSyntax(_ data: SyntaxData) {
      let node = ForcedValueExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPostfixUnaryExprSyntax(_ data: SyntaxData) {
      let node = PostfixUnaryExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSpecializeExprSyntax(_ data: SyntaxData) {
      let node = SpecializeExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplStringSegmentSyntax(_ data: SyntaxData) {
      let node = StringSegmentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplExpressionSegmentSyntax(_ data: SyntaxData) {
      let node = ExpressionSegmentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplStringLiteralExprSyntax(_ data: SyntaxData) {
      let node = StringLiteralExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplRegexLiteralExprSyntax(_ data: SyntaxData) {
      let node = RegexLiteralExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplKeyPathExprSyntax(_ data: SyntaxData) {
      let node = KeyPathExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplKeyPathComponentListSyntax(_ data: SyntaxData) {
      let node = KeyPathComponentListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplKeyPathComponentSyntax(_ data: SyntaxData) {
      let node = KeyPathComponentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplKeyPathPropertyComponentSyntax(_ data: SyntaxData) {
      let node = KeyPathPropertyComponentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplKeyPathSubscriptComponentSyntax(_ data: SyntaxData) {
      let node = KeyPathSubscriptComponentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplKeyPathOptionalComponentSyntax(_ data: SyntaxData) {
      let node = KeyPathOptionalComponentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplOldKeyPathExprSyntax(_ data: SyntaxData) {
      let node = OldKeyPathExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplKeyPathBaseExprSyntax(_ data: SyntaxData) {
      let node = KeyPathBaseExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplObjcNamePieceSyntax(_ data: SyntaxData) {
      let node = ObjcNamePieceSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplObjcNameSyntax(_ data: SyntaxData) {
      let node = ObjcNameSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplObjcKeyPathExprSyntax(_ data: SyntaxData) {
      let node = ObjcKeyPathExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplObjcSelectorExprSyntax(_ data: SyntaxData) {
      let node = ObjcSelectorExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPostfixIfConfigExprSyntax(_ data: SyntaxData) {
      let node = PostfixIfConfigExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplEditorPlaceholderExprSyntax(_ data: SyntaxData) {
      let node = EditorPlaceholderExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplObjectLiteralExprSyntax(_ data: SyntaxData) {
      let node = ObjectLiteralExprSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplYieldExprListSyntax(_ data: SyntaxData) {
      let node = YieldExprListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplYieldExprListElementSyntax(_ data: SyntaxData) {
      let node = YieldExprListElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTypeInitializerClauseSyntax(_ data: SyntaxData) {
      let node = TypeInitializerClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTypealiasDeclSyntax(_ data: SyntaxData) {
      let node = TypealiasDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAssociatedtypeDeclSyntax(_ data: SyntaxData) {
      let node = AssociatedtypeDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplFunctionParameterListSyntax(_ data: SyntaxData) {
      let node = FunctionParameterListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplParameterClauseSyntax(_ data: SyntaxData) {
      let node = ParameterClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplReturnClauseSyntax(_ data: SyntaxData) {
      let node = ReturnClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplFunctionSignatureSyntax(_ data: SyntaxData) {
      let node = FunctionSignatureSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplIfConfigClauseSyntax(_ data: SyntaxData) {
      let node = IfConfigClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplIfConfigClauseListSyntax(_ data: SyntaxData) {
      let node = IfConfigClauseListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplIfConfigDeclSyntax(_ data: SyntaxData) {
      let node = IfConfigDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundErrorDeclSyntax(_ data: SyntaxData) {
      let node = PoundErrorDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundWarningDeclSyntax(_ data: SyntaxData) {
      let node = PoundWarningDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundSourceLocationSyntax(_ data: SyntaxData) {
      let node = PoundSourceLocationSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundSourceLocationArgsSyntax(_ data: SyntaxData) {
      let node = PoundSourceLocationArgsSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDeclModifierDetailSyntax(_ data: SyntaxData) {
      let node = DeclModifierDetailSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDeclModifierSyntax(_ data: SyntaxData) {
      let node = DeclModifierSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplInheritedTypeSyntax(_ data: SyntaxData) {
      let node = InheritedTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplInheritedTypeListSyntax(_ data: SyntaxData) {
      let node = InheritedTypeListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTypeInheritanceClauseSyntax(_ data: SyntaxData) {
      let node = TypeInheritanceClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplClassDeclSyntax(_ data: SyntaxData) {
      let node = ClassDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplActorDeclSyntax(_ data: SyntaxData) {
      let node = ActorDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplStructDeclSyntax(_ data: SyntaxData) {
      let node = StructDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplProtocolDeclSyntax(_ data: SyntaxData) {
      let node = ProtocolDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplExtensionDeclSyntax(_ data: SyntaxData) {
      let node = ExtensionDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMemberDeclBlockSyntax(_ data: SyntaxData) {
      let node = MemberDeclBlockSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMemberDeclListSyntax(_ data: SyntaxData) {
      let node = MemberDeclListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMemberDeclListItemSyntax(_ data: SyntaxData) {
      let node = MemberDeclListItemSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSourceFileSyntax(_ data: SyntaxData) {
      let node = SourceFileSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplInitializerClauseSyntax(_ data: SyntaxData) {
      let node = InitializerClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplFunctionParameterSyntax(_ data: SyntaxData) {
      let node = FunctionParameterSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplModifierListSyntax(_ data: SyntaxData) {
      let node = ModifierListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplFunctionDeclSyntax(_ data: SyntaxData) {
      let node = FunctionDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplInitializerDeclSyntax(_ data: SyntaxData) {
      let node = InitializerDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDeinitializerDeclSyntax(_ data: SyntaxData) {
      let node = DeinitializerDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSubscriptDeclSyntax(_ data: SyntaxData) {
      let node = SubscriptDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAccessLevelModifierSyntax(_ data: SyntaxData) {
      let node = AccessLevelModifierSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAccessPathComponentSyntax(_ data: SyntaxData) {
      let node = AccessPathComponentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAccessPathSyntax(_ data: SyntaxData) {
      let node = AccessPathSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplImportDeclSyntax(_ data: SyntaxData) {
      let node = ImportDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAccessorParameterSyntax(_ data: SyntaxData) {
      let node = AccessorParameterSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAccessorDeclSyntax(_ data: SyntaxData) {
      let node = AccessorDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAccessorListSyntax(_ data: SyntaxData) {
      let node = AccessorListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAccessorBlockSyntax(_ data: SyntaxData) {
      let node = AccessorBlockSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPatternBindingSyntax(_ data: SyntaxData) {
      let node = PatternBindingSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPatternBindingListSyntax(_ data: SyntaxData) {
      let node = PatternBindingListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplVariableDeclSyntax(_ data: SyntaxData) {
      let node = VariableDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplEnumCaseElementSyntax(_ data: SyntaxData) {
      let node = EnumCaseElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplEnumCaseElementListSyntax(_ data: SyntaxData) {
      let node = EnumCaseElementListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplEnumCaseDeclSyntax(_ data: SyntaxData) {
      let node = EnumCaseDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplEnumDeclSyntax(_ data: SyntaxData) {
      let node = EnumDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplOperatorDeclSyntax(_ data: SyntaxData) {
      let node = OperatorDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDesignatedTypeListSyntax(_ data: SyntaxData) {
      let node = DesignatedTypeListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDesignatedTypeElementSyntax(_ data: SyntaxData) {
      let node = DesignatedTypeElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplOperatorPrecedenceAndTypesSyntax(_ data: SyntaxData) {
      let node = OperatorPrecedenceAndTypesSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPrecedenceGroupDeclSyntax(_ data: SyntaxData) {
      let node = PrecedenceGroupDeclSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPrecedenceGroupAttributeListSyntax(_ data: SyntaxData) {
      let node = PrecedenceGroupAttributeListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPrecedenceGroupRelationSyntax(_ data: SyntaxData) {
      let node = PrecedenceGroupRelationSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPrecedenceGroupNameListSyntax(_ data: SyntaxData) {
      let node = PrecedenceGroupNameListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPrecedenceGroupNameElementSyntax(_ data: SyntaxData) {
      let node = PrecedenceGroupNameElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPrecedenceGroupAssignmentSyntax(_ data: SyntaxData) {
      let node = PrecedenceGroupAssignmentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPrecedenceGroupAssociativitySyntax(_ data: SyntaxData) {
      let node = PrecedenceGroupAssociativitySyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTokenListSyntax(_ data: SyntaxData) {
      let node = TokenListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplNonEmptyTokenListSyntax(_ data: SyntaxData) {
      let node = NonEmptyTokenListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCustomAttributeSyntax(_ data: SyntaxData) {
      let node = CustomAttributeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAttributeSyntax(_ data: SyntaxData) {
      let node = AttributeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAttributeListSyntax(_ data: SyntaxData) {
      let node = AttributeListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSpecializeAttributeSpecListSyntax(_ data: SyntaxData) {
      let node = SpecializeAttributeSpecListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAvailabilityEntrySyntax(_ data: SyntaxData) {
      let node = AvailabilityEntrySyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplLabeledSpecializeEntrySyntax(_ data: SyntaxData) {
      let node = LabeledSpecializeEntrySyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTargetFunctionEntrySyntax(_ data: SyntaxData) {
      let node = TargetFunctionEntrySyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplNamedAttributeStringArgumentSyntax(_ data: SyntaxData) {
      let node = NamedAttributeStringArgumentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDeclNameSyntax(_ data: SyntaxData) {
      let node = DeclNameSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplImplementsAttributeArgumentsSyntax(_ data: SyntaxData) {
      let node = ImplementsAttributeArgumentsSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplObjCSelectorPieceSyntax(_ data: SyntaxData) {
      let node = ObjCSelectorPieceSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplObjCSelectorSyntax(_ data: SyntaxData) {
      let node = ObjCSelectorSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDifferentiableAttributeArgumentsSyntax(_ data: SyntaxData) {
      let node = DifferentiableAttributeArgumentsSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDifferentiabilityParamsClauseSyntax(_ data: SyntaxData) {
      let node = DifferentiabilityParamsClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDifferentiabilityParamsSyntax(_ data: SyntaxData) {
      let node = DifferentiabilityParamsSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDifferentiabilityParamListSyntax(_ data: SyntaxData) {
      let node = DifferentiabilityParamListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDifferentiabilityParamSyntax(_ data: SyntaxData) {
      let node = DifferentiabilityParamSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDerivativeRegistrationAttributeArgumentsSyntax(_ data: SyntaxData) {
      let node = DerivativeRegistrationAttributeArgumentsSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplQualifiedDeclNameSyntax(_ data: SyntaxData) {
      let node = QualifiedDeclNameSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplFunctionDeclNameSyntax(_ data: SyntaxData) {
      let node = FunctionDeclNameSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplBackDeployAttributeSpecListSyntax(_ data: SyntaxData) {
      let node = BackDeployAttributeSpecListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplBackDeployVersionListSyntax(_ data: SyntaxData) {
      let node = BackDeployVersionListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplBackDeployVersionArgumentSyntax(_ data: SyntaxData) {
      let node = BackDeployVersionArgumentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplOpaqueReturnTypeOfAttributeArgumentsSyntax(_ data: SyntaxData) {
      let node = OpaqueReturnTypeOfAttributeArgumentsSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplConventionAttributeArgumentsSyntax(_ data: SyntaxData) {
      let node = ConventionAttributeArgumentsSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplConventionWitnessMethodAttributeArgumentsSyntax(_ data: SyntaxData) {
      let node = ConventionWitnessMethodAttributeArgumentsSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplLabeledStmtSyntax(_ data: SyntaxData) {
      let node = LabeledStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplContinueStmtSyntax(_ data: SyntaxData) {
      let node = ContinueStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplWhileStmtSyntax(_ data: SyntaxData) {
      let node = WhileStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDeferStmtSyntax(_ data: SyntaxData) {
      let node = DeferStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplExpressionStmtSyntax(_ data: SyntaxData) {
      let node = ExpressionStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSwitchCaseListSyntax(_ data: SyntaxData) {
      let node = SwitchCaseListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplRepeatWhileStmtSyntax(_ data: SyntaxData) {
      let node = RepeatWhileStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplGuardStmtSyntax(_ data: SyntaxData) {
      let node = GuardStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplWhereClauseSyntax(_ data: SyntaxData) {
      let node = WhereClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplForInStmtSyntax(_ data: SyntaxData) {
      let node = ForInStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSwitchStmtSyntax(_ data: SyntaxData) {
      let node = SwitchStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCatchClauseListSyntax(_ data: SyntaxData) {
      let node = CatchClauseListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDoStmtSyntax(_ data: SyntaxData) {
      let node = DoStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplReturnStmtSyntax(_ data: SyntaxData) {
      let node = ReturnStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplYieldStmtSyntax(_ data: SyntaxData) {
      let node = YieldStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplYieldListSyntax(_ data: SyntaxData) {
      let node = YieldListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplFallthroughStmtSyntax(_ data: SyntaxData) {
      let node = FallthroughStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplBreakStmtSyntax(_ data: SyntaxData) {
      let node = BreakStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCaseItemListSyntax(_ data: SyntaxData) {
      let node = CaseItemListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCatchItemListSyntax(_ data: SyntaxData) {
      let node = CatchItemListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplConditionElementSyntax(_ data: SyntaxData) {
      let node = ConditionElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAvailabilityConditionSyntax(_ data: SyntaxData) {
      let node = AvailabilityConditionSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMatchingPatternConditionSyntax(_ data: SyntaxData) {
      let node = MatchingPatternConditionSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplOptionalBindingConditionSyntax(_ data: SyntaxData) {
      let node = OptionalBindingConditionSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplUnavailabilityConditionSyntax(_ data: SyntaxData) {
      let node = UnavailabilityConditionSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplHasSymbolConditionSyntax(_ data: SyntaxData) {
      let node = HasSymbolConditionSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplConditionElementListSyntax(_ data: SyntaxData) {
      let node = ConditionElementListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDeclarationStmtSyntax(_ data: SyntaxData) {
      let node = DeclarationStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplThrowStmtSyntax(_ data: SyntaxData) {
      let node = ThrowStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplIfStmtSyntax(_ data: SyntaxData) {
      let node = IfStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplElseIfContinuationSyntax(_ data: SyntaxData) {
      let node = ElseIfContinuationSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplElseBlockSyntax(_ data: SyntaxData) {
      let node = ElseBlockSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSwitchCaseSyntax(_ data: SyntaxData) {
      let node = SwitchCaseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSwitchDefaultLabelSyntax(_ data: SyntaxData) {
      let node = SwitchDefaultLabelSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCaseItemSyntax(_ data: SyntaxData) {
      let node = CaseItemSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCatchItemSyntax(_ data: SyntaxData) {
      let node = CatchItemSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSwitchCaseLabelSyntax(_ data: SyntaxData) {
      let node = SwitchCaseLabelSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCatchClauseSyntax(_ data: SyntaxData) {
      let node = CatchClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPoundAssertStmtSyntax(_ data: SyntaxData) {
      let node = PoundAssertStmtSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplGenericWhereClauseSyntax(_ data: SyntaxData) {
      let node = GenericWhereClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplGenericRequirementListSyntax(_ data: SyntaxData) {
      let node = GenericRequirementListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplGenericRequirementSyntax(_ data: SyntaxData) {
      let node = GenericRequirementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSameTypeRequirementSyntax(_ data: SyntaxData) {
      let node = SameTypeRequirementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplLayoutRequirementSyntax(_ data: SyntaxData) {
      let node = LayoutRequirementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplGenericParameterListSyntax(_ data: SyntaxData) {
      let node = GenericParameterListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplGenericParameterSyntax(_ data: SyntaxData) {
      let node = GenericParameterSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPrimaryAssociatedTypeListSyntax(_ data: SyntaxData) {
      let node = PrimaryAssociatedTypeListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPrimaryAssociatedTypeSyntax(_ data: SyntaxData) {
      let node = PrimaryAssociatedTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplGenericParameterClauseSyntax(_ data: SyntaxData) {
      let node = GenericParameterClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplConformanceRequirementSyntax(_ data: SyntaxData) {
      let node = ConformanceRequirementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPrimaryAssociatedTypeClauseSyntax(_ data: SyntaxData) {
      let node = PrimaryAssociatedTypeClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplSimpleTypeIdentifierSyntax(_ data: SyntaxData) {
      let node = SimpleTypeIdentifierSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMemberTypeIdentifierSyntax(_ data: SyntaxData) {
      let node = MemberTypeIdentifierSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplClassRestrictionTypeSyntax(_ data: SyntaxData) {
      let node = ClassRestrictionTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplArrayTypeSyntax(_ data: SyntaxData) {
      let node = ArrayTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplDictionaryTypeSyntax(_ data: SyntaxData) {
      let node = DictionaryTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplMetatypeTypeSyntax(_ data: SyntaxData) {
      let node = MetatypeTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplOptionalTypeSyntax(_ data: SyntaxData) {
      let node = OptionalTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplConstrainedSugarTypeSyntax(_ data: SyntaxData) {
      let node = ConstrainedSugarTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplImplicitlyUnwrappedOptionalTypeSyntax(_ data: SyntaxData) {
      let node = ImplicitlyUnwrappedOptionalTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCompositionTypeElementSyntax(_ data: SyntaxData) {
      let node = CompositionTypeElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCompositionTypeElementListSyntax(_ data: SyntaxData) {
      let node = CompositionTypeElementListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplCompositionTypeSyntax(_ data: SyntaxData) {
      let node = CompositionTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplPackExpansionTypeSyntax(_ data: SyntaxData) {
      let node = PackExpansionTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTupleTypeElementSyntax(_ data: SyntaxData) {
      let node = TupleTypeElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTupleTypeElementListSyntax(_ data: SyntaxData) {
      let node = TupleTypeElementListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTupleTypeSyntax(_ data: SyntaxData) {
      let node = TupleTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplFunctionTypeSyntax(_ data: SyntaxData) {
      let node = FunctionTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAttributedTypeSyntax(_ data: SyntaxData) {
      let node = AttributedTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplGenericArgumentListSyntax(_ data: SyntaxData) {
      let node = GenericArgumentListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplGenericArgumentSyntax(_ data: SyntaxData) {
      let node = GenericArgumentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplGenericArgumentClauseSyntax(_ data: SyntaxData) {
      let node = GenericArgumentClauseSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplNamedOpaqueReturnTypeSyntax(_ data: SyntaxData) {
      let node = NamedOpaqueReturnTypeSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTypeAnnotationSyntax(_ data: SyntaxData) {
      let node = TypeAnnotationSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplEnumCasePatternSyntax(_ data: SyntaxData) {
      let node = EnumCasePatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplIsTypePatternSyntax(_ data: SyntaxData) {
      let node = IsTypePatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplOptionalPatternSyntax(_ data: SyntaxData) {
      let node = OptionalPatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplIdentifierPatternSyntax(_ data: SyntaxData) {
      let node = IdentifierPatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAsTypePatternSyntax(_ data: SyntaxData) {
      let node = AsTypePatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTuplePatternSyntax(_ data: SyntaxData) {
      let node = TuplePatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplWildcardPatternSyntax(_ data: SyntaxData) {
      let node = WildcardPatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTuplePatternElementSyntax(_ data: SyntaxData) {
      let node = TuplePatternElementSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplExpressionPatternSyntax(_ data: SyntaxData) {
      let node = ExpressionPatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplTuplePatternElementListSyntax(_ data: SyntaxData) {
      let node = TuplePatternElementListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplValueBindingPatternSyntax(_ data: SyntaxData) {
      let node = ValueBindingPatternSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAvailabilitySpecListSyntax(_ data: SyntaxData) {
      let node = AvailabilitySpecListSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAvailabilityArgumentSyntax(_ data: SyntaxData) {
      let node = AvailabilityArgumentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAvailabilityLabeledArgumentSyntax(_ data: SyntaxData) {
      let node = AvailabilityLabeledArgumentSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplAvailabilityVersionRestrictionSyntax(_ data: SyntaxData) {
      let node = AvailabilityVersionRestrictionSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }

  /// Implementation detail of doVisit(_:_:). Do not call directly.
  private func visitImplVersionTupleSyntax(_ data: SyntaxData) {
      let node = VersionTupleSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
  }


  private func visit(_ data: SyntaxData) {
    switch data.raw.kind {
    case .token:
      let node = TokenSyntax(data)
      _ = visit(node)
      // No children to visit.
      visitPost(node)
    case .unknown:
      let node = UnknownSyntax(data)
      let needsChildren = (visit(node) == .visitChildren)
      // Avoid calling into visitChildren if possible.
      if needsChildren && !node.raw.layoutView!.children.isEmpty {
        visitChildren(node)
      }
      visitPost(node)
    // The implementation of every generated case goes into its own function. This
    // circumvents an issue where the compiler allocates stack space for every
    // case statement next to each other in debug builds, causing it to allocate
    // ~50KB per call to this function. rdar://55929175
    case .unknownDecl:
      visitImplUnknownDeclSyntax(data)
    case .unknownExpr:
      visitImplUnknownExprSyntax(data)
    case .unknownStmt:
      visitImplUnknownStmtSyntax(data)
    case .unknownType:
      visitImplUnknownTypeSyntax(data)
    case .unknownPattern:
      visitImplUnknownPatternSyntax(data)
    case .missing:
      visitImplMissingSyntax(data)
    case .missingDecl:
      visitImplMissingDeclSyntax(data)
    case .missingExpr:
      visitImplMissingExprSyntax(data)
    case .missingStmt:
      visitImplMissingStmtSyntax(data)
    case .missingType:
      visitImplMissingTypeSyntax(data)
    case .missingPattern:
      visitImplMissingPatternSyntax(data)
    case .codeBlockItem:
      visitImplCodeBlockItemSyntax(data)
    case .codeBlockItemList:
      visitImplCodeBlockItemListSyntax(data)
    case .codeBlock:
      visitImplCodeBlockSyntax(data)
    case .unexpectedNodes:
      visitImplUnexpectedNodesSyntax(data)
    case .inOutExpr:
      visitImplInOutExprSyntax(data)
    case .poundColumnExpr:
      visitImplPoundColumnExprSyntax(data)
    case .tupleExprElementList:
      visitImplTupleExprElementListSyntax(data)
    case .arrayElementList:
      visitImplArrayElementListSyntax(data)
    case .dictionaryElementList:
      visitImplDictionaryElementListSyntax(data)
    case .stringLiteralSegments:
      visitImplStringLiteralSegmentsSyntax(data)
    case .tryExpr:
      visitImplTryExprSyntax(data)
    case .awaitExpr:
      visitImplAwaitExprSyntax(data)
    case .moveExpr:
      visitImplMoveExprSyntax(data)
    case .declNameArgument:
      visitImplDeclNameArgumentSyntax(data)
    case .declNameArgumentList:
      visitImplDeclNameArgumentListSyntax(data)
    case .declNameArguments:
      visitImplDeclNameArgumentsSyntax(data)
    case .identifierExpr:
      visitImplIdentifierExprSyntax(data)
    case .superRefExpr:
      visitImplSuperRefExprSyntax(data)
    case .nilLiteralExpr:
      visitImplNilLiteralExprSyntax(data)
    case .discardAssignmentExpr:
      visitImplDiscardAssignmentExprSyntax(data)
    case .assignmentExpr:
      visitImplAssignmentExprSyntax(data)
    case .sequenceExpr:
      visitImplSequenceExprSyntax(data)
    case .exprList:
      visitImplExprListSyntax(data)
    case .poundLineExpr:
      visitImplPoundLineExprSyntax(data)
    case .poundFileExpr:
      visitImplPoundFileExprSyntax(data)
    case .poundFileIDExpr:
      visitImplPoundFileIDExprSyntax(data)
    case .poundFilePathExpr:
      visitImplPoundFilePathExprSyntax(data)
    case .poundFunctionExpr:
      visitImplPoundFunctionExprSyntax(data)
    case .poundDsohandleExpr:
      visitImplPoundDsohandleExprSyntax(data)
    case .symbolicReferenceExpr:
      visitImplSymbolicReferenceExprSyntax(data)
    case .prefixOperatorExpr:
      visitImplPrefixOperatorExprSyntax(data)
    case .binaryOperatorExpr:
      visitImplBinaryOperatorExprSyntax(data)
    case .arrowExpr:
      visitImplArrowExprSyntax(data)
    case .infixOperatorExpr:
      visitImplInfixOperatorExprSyntax(data)
    case .floatLiteralExpr:
      visitImplFloatLiteralExprSyntax(data)
    case .tupleExpr:
      visitImplTupleExprSyntax(data)
    case .arrayExpr:
      visitImplArrayExprSyntax(data)
    case .dictionaryExpr:
      visitImplDictionaryExprSyntax(data)
    case .tupleExprElement:
      visitImplTupleExprElementSyntax(data)
    case .arrayElement:
      visitImplArrayElementSyntax(data)
    case .dictionaryElement:
      visitImplDictionaryElementSyntax(data)
    case .integerLiteralExpr:
      visitImplIntegerLiteralExprSyntax(data)
    case .booleanLiteralExpr:
      visitImplBooleanLiteralExprSyntax(data)
    case .unresolvedTernaryExpr:
      visitImplUnresolvedTernaryExprSyntax(data)
    case .ternaryExpr:
      visitImplTernaryExprSyntax(data)
    case .memberAccessExpr:
      visitImplMemberAccessExprSyntax(data)
    case .unresolvedIsExpr:
      visitImplUnresolvedIsExprSyntax(data)
    case .isExpr:
      visitImplIsExprSyntax(data)
    case .unresolvedAsExpr:
      visitImplUnresolvedAsExprSyntax(data)
    case .asExpr:
      visitImplAsExprSyntax(data)
    case .typeExpr:
      visitImplTypeExprSyntax(data)
    case .closureCaptureItem:
      visitImplClosureCaptureItemSyntax(data)
    case .closureCaptureItemList:
      visitImplClosureCaptureItemListSyntax(data)
    case .closureCaptureSignature:
      visitImplClosureCaptureSignatureSyntax(data)
    case .closureParam:
      visitImplClosureParamSyntax(data)
    case .closureParamList:
      visitImplClosureParamListSyntax(data)
    case .closureSignature:
      visitImplClosureSignatureSyntax(data)
    case .closureExpr:
      visitImplClosureExprSyntax(data)
    case .unresolvedPatternExpr:
      visitImplUnresolvedPatternExprSyntax(data)
    case .multipleTrailingClosureElement:
      visitImplMultipleTrailingClosureElementSyntax(data)
    case .multipleTrailingClosureElementList:
      visitImplMultipleTrailingClosureElementListSyntax(data)
    case .functionCallExpr:
      visitImplFunctionCallExprSyntax(data)
    case .subscriptExpr:
      visitImplSubscriptExprSyntax(data)
    case .optionalChainingExpr:
      visitImplOptionalChainingExprSyntax(data)
    case .forcedValueExpr:
      visitImplForcedValueExprSyntax(data)
    case .postfixUnaryExpr:
      visitImplPostfixUnaryExprSyntax(data)
    case .specializeExpr:
      visitImplSpecializeExprSyntax(data)
    case .stringSegment:
      visitImplStringSegmentSyntax(data)
    case .expressionSegment:
      visitImplExpressionSegmentSyntax(data)
    case .stringLiteralExpr:
      visitImplStringLiteralExprSyntax(data)
    case .regexLiteralExpr:
      visitImplRegexLiteralExprSyntax(data)
    case .keyPathExpr:
      visitImplKeyPathExprSyntax(data)
    case .keyPathComponentList:
      visitImplKeyPathComponentListSyntax(data)
    case .keyPathComponent:
      visitImplKeyPathComponentSyntax(data)
    case .keyPathPropertyComponent:
      visitImplKeyPathPropertyComponentSyntax(data)
    case .keyPathSubscriptComponent:
      visitImplKeyPathSubscriptComponentSyntax(data)
    case .keyPathOptionalComponent:
      visitImplKeyPathOptionalComponentSyntax(data)
    case .oldKeyPathExpr:
      visitImplOldKeyPathExprSyntax(data)
    case .keyPathBaseExpr:
      visitImplKeyPathBaseExprSyntax(data)
    case .objcNamePiece:
      visitImplObjcNamePieceSyntax(data)
    case .objcName:
      visitImplObjcNameSyntax(data)
    case .objcKeyPathExpr:
      visitImplObjcKeyPathExprSyntax(data)
    case .objcSelectorExpr:
      visitImplObjcSelectorExprSyntax(data)
    case .postfixIfConfigExpr:
      visitImplPostfixIfConfigExprSyntax(data)
    case .editorPlaceholderExpr:
      visitImplEditorPlaceholderExprSyntax(data)
    case .objectLiteralExpr:
      visitImplObjectLiteralExprSyntax(data)
    case .yieldExprList:
      visitImplYieldExprListSyntax(data)
    case .yieldExprListElement:
      visitImplYieldExprListElementSyntax(data)
    case .typeInitializerClause:
      visitImplTypeInitializerClauseSyntax(data)
    case .typealiasDecl:
      visitImplTypealiasDeclSyntax(data)
    case .associatedtypeDecl:
      visitImplAssociatedtypeDeclSyntax(data)
    case .functionParameterList:
      visitImplFunctionParameterListSyntax(data)
    case .parameterClause:
      visitImplParameterClauseSyntax(data)
    case .returnClause:
      visitImplReturnClauseSyntax(data)
    case .functionSignature:
      visitImplFunctionSignatureSyntax(data)
    case .ifConfigClause:
      visitImplIfConfigClauseSyntax(data)
    case .ifConfigClauseList:
      visitImplIfConfigClauseListSyntax(data)
    case .ifConfigDecl:
      visitImplIfConfigDeclSyntax(data)
    case .poundErrorDecl:
      visitImplPoundErrorDeclSyntax(data)
    case .poundWarningDecl:
      visitImplPoundWarningDeclSyntax(data)
    case .poundSourceLocation:
      visitImplPoundSourceLocationSyntax(data)
    case .poundSourceLocationArgs:
      visitImplPoundSourceLocationArgsSyntax(data)
    case .declModifierDetail:
      visitImplDeclModifierDetailSyntax(data)
    case .declModifier:
      visitImplDeclModifierSyntax(data)
    case .inheritedType:
      visitImplInheritedTypeSyntax(data)
    case .inheritedTypeList:
      visitImplInheritedTypeListSyntax(data)
    case .typeInheritanceClause:
      visitImplTypeInheritanceClauseSyntax(data)
    case .classDecl:
      visitImplClassDeclSyntax(data)
    case .actorDecl:
      visitImplActorDeclSyntax(data)
    case .structDecl:
      visitImplStructDeclSyntax(data)
    case .protocolDecl:
      visitImplProtocolDeclSyntax(data)
    case .extensionDecl:
      visitImplExtensionDeclSyntax(data)
    case .memberDeclBlock:
      visitImplMemberDeclBlockSyntax(data)
    case .memberDeclList:
      visitImplMemberDeclListSyntax(data)
    case .memberDeclListItem:
      visitImplMemberDeclListItemSyntax(data)
    case .sourceFile:
      visitImplSourceFileSyntax(data)
    case .initializerClause:
      visitImplInitializerClauseSyntax(data)
    case .functionParameter:
      visitImplFunctionParameterSyntax(data)
    case .modifierList:
      visitImplModifierListSyntax(data)
    case .functionDecl:
      visitImplFunctionDeclSyntax(data)
    case .initializerDecl:
      visitImplInitializerDeclSyntax(data)
    case .deinitializerDecl:
      visitImplDeinitializerDeclSyntax(data)
    case .subscriptDecl:
      visitImplSubscriptDeclSyntax(data)
    case .accessLevelModifier:
      visitImplAccessLevelModifierSyntax(data)
    case .accessPathComponent:
      visitImplAccessPathComponentSyntax(data)
    case .accessPath:
      visitImplAccessPathSyntax(data)
    case .importDecl:
      visitImplImportDeclSyntax(data)
    case .accessorParameter:
      visitImplAccessorParameterSyntax(data)
    case .accessorDecl:
      visitImplAccessorDeclSyntax(data)
    case .accessorList:
      visitImplAccessorListSyntax(data)
    case .accessorBlock:
      visitImplAccessorBlockSyntax(data)
    case .patternBinding:
      visitImplPatternBindingSyntax(data)
    case .patternBindingList:
      visitImplPatternBindingListSyntax(data)
    case .variableDecl:
      visitImplVariableDeclSyntax(data)
    case .enumCaseElement:
      visitImplEnumCaseElementSyntax(data)
    case .enumCaseElementList:
      visitImplEnumCaseElementListSyntax(data)
    case .enumCaseDecl:
      visitImplEnumCaseDeclSyntax(data)
    case .enumDecl:
      visitImplEnumDeclSyntax(data)
    case .operatorDecl:
      visitImplOperatorDeclSyntax(data)
    case .designatedTypeList:
      visitImplDesignatedTypeListSyntax(data)
    case .designatedTypeElement:
      visitImplDesignatedTypeElementSyntax(data)
    case .operatorPrecedenceAndTypes:
      visitImplOperatorPrecedenceAndTypesSyntax(data)
    case .precedenceGroupDecl:
      visitImplPrecedenceGroupDeclSyntax(data)
    case .precedenceGroupAttributeList:
      visitImplPrecedenceGroupAttributeListSyntax(data)
    case .precedenceGroupRelation:
      visitImplPrecedenceGroupRelationSyntax(data)
    case .precedenceGroupNameList:
      visitImplPrecedenceGroupNameListSyntax(data)
    case .precedenceGroupNameElement:
      visitImplPrecedenceGroupNameElementSyntax(data)
    case .precedenceGroupAssignment:
      visitImplPrecedenceGroupAssignmentSyntax(data)
    case .precedenceGroupAssociativity:
      visitImplPrecedenceGroupAssociativitySyntax(data)
    case .tokenList:
      visitImplTokenListSyntax(data)
    case .nonEmptyTokenList:
      visitImplNonEmptyTokenListSyntax(data)
    case .customAttribute:
      visitImplCustomAttributeSyntax(data)
    case .attribute:
      visitImplAttributeSyntax(data)
    case .attributeList:
      visitImplAttributeListSyntax(data)
    case .specializeAttributeSpecList:
      visitImplSpecializeAttributeSpecListSyntax(data)
    case .availabilityEntry:
      visitImplAvailabilityEntrySyntax(data)
    case .labeledSpecializeEntry:
      visitImplLabeledSpecializeEntrySyntax(data)
    case .targetFunctionEntry:
      visitImplTargetFunctionEntrySyntax(data)
    case .namedAttributeStringArgument:
      visitImplNamedAttributeStringArgumentSyntax(data)
    case .declName:
      visitImplDeclNameSyntax(data)
    case .implementsAttributeArguments:
      visitImplImplementsAttributeArgumentsSyntax(data)
    case .objCSelectorPiece:
      visitImplObjCSelectorPieceSyntax(data)
    case .objCSelector:
      visitImplObjCSelectorSyntax(data)
    case .differentiableAttributeArguments:
      visitImplDifferentiableAttributeArgumentsSyntax(data)
    case .differentiabilityParamsClause:
      visitImplDifferentiabilityParamsClauseSyntax(data)
    case .differentiabilityParams:
      visitImplDifferentiabilityParamsSyntax(data)
    case .differentiabilityParamList:
      visitImplDifferentiabilityParamListSyntax(data)
    case .differentiabilityParam:
      visitImplDifferentiabilityParamSyntax(data)
    case .derivativeRegistrationAttributeArguments:
      visitImplDerivativeRegistrationAttributeArgumentsSyntax(data)
    case .qualifiedDeclName:
      visitImplQualifiedDeclNameSyntax(data)
    case .functionDeclName:
      visitImplFunctionDeclNameSyntax(data)
    case .backDeployAttributeSpecList:
      visitImplBackDeployAttributeSpecListSyntax(data)
    case .backDeployVersionList:
      visitImplBackDeployVersionListSyntax(data)
    case .backDeployVersionArgument:
      visitImplBackDeployVersionArgumentSyntax(data)
    case .opaqueReturnTypeOfAttributeArguments:
      visitImplOpaqueReturnTypeOfAttributeArgumentsSyntax(data)
    case .conventionAttributeArguments:
      visitImplConventionAttributeArgumentsSyntax(data)
    case .conventionWitnessMethodAttributeArguments:
      visitImplConventionWitnessMethodAttributeArgumentsSyntax(data)
    case .labeledStmt:
      visitImplLabeledStmtSyntax(data)
    case .continueStmt:
      visitImplContinueStmtSyntax(data)
    case .whileStmt:
      visitImplWhileStmtSyntax(data)
    case .deferStmt:
      visitImplDeferStmtSyntax(data)
    case .expressionStmt:
      visitImplExpressionStmtSyntax(data)
    case .switchCaseList:
      visitImplSwitchCaseListSyntax(data)
    case .repeatWhileStmt:
      visitImplRepeatWhileStmtSyntax(data)
    case .guardStmt:
      visitImplGuardStmtSyntax(data)
    case .whereClause:
      visitImplWhereClauseSyntax(data)
    case .forInStmt:
      visitImplForInStmtSyntax(data)
    case .switchStmt:
      visitImplSwitchStmtSyntax(data)
    case .catchClauseList:
      visitImplCatchClauseListSyntax(data)
    case .doStmt:
      visitImplDoStmtSyntax(data)
    case .returnStmt:
      visitImplReturnStmtSyntax(data)
    case .yieldStmt:
      visitImplYieldStmtSyntax(data)
    case .yieldList:
      visitImplYieldListSyntax(data)
    case .fallthroughStmt:
      visitImplFallthroughStmtSyntax(data)
    case .breakStmt:
      visitImplBreakStmtSyntax(data)
    case .caseItemList:
      visitImplCaseItemListSyntax(data)
    case .catchItemList:
      visitImplCatchItemListSyntax(data)
    case .conditionElement:
      visitImplConditionElementSyntax(data)
    case .availabilityCondition:
      visitImplAvailabilityConditionSyntax(data)
    case .matchingPatternCondition:
      visitImplMatchingPatternConditionSyntax(data)
    case .optionalBindingCondition:
      visitImplOptionalBindingConditionSyntax(data)
    case .unavailabilityCondition:
      visitImplUnavailabilityConditionSyntax(data)
    case .hasSymbolCondition:
      visitImplHasSymbolConditionSyntax(data)
    case .conditionElementList:
      visitImplConditionElementListSyntax(data)
    case .declarationStmt:
      visitImplDeclarationStmtSyntax(data)
    case .throwStmt:
      visitImplThrowStmtSyntax(data)
    case .ifStmt:
      visitImplIfStmtSyntax(data)
    case .elseIfContinuation:
      visitImplElseIfContinuationSyntax(data)
    case .elseBlock:
      visitImplElseBlockSyntax(data)
    case .switchCase:
      visitImplSwitchCaseSyntax(data)
    case .switchDefaultLabel:
      visitImplSwitchDefaultLabelSyntax(data)
    case .caseItem:
      visitImplCaseItemSyntax(data)
    case .catchItem:
      visitImplCatchItemSyntax(data)
    case .switchCaseLabel:
      visitImplSwitchCaseLabelSyntax(data)
    case .catchClause:
      visitImplCatchClauseSyntax(data)
    case .poundAssertStmt:
      visitImplPoundAssertStmtSyntax(data)
    case .genericWhereClause:
      visitImplGenericWhereClauseSyntax(data)
    case .genericRequirementList:
      visitImplGenericRequirementListSyntax(data)
    case .genericRequirement:
      visitImplGenericRequirementSyntax(data)
    case .sameTypeRequirement:
      visitImplSameTypeRequirementSyntax(data)
    case .layoutRequirement:
      visitImplLayoutRequirementSyntax(data)
    case .genericParameterList:
      visitImplGenericParameterListSyntax(data)
    case .genericParameter:
      visitImplGenericParameterSyntax(data)
    case .primaryAssociatedTypeList:
      visitImplPrimaryAssociatedTypeListSyntax(data)
    case .primaryAssociatedType:
      visitImplPrimaryAssociatedTypeSyntax(data)
    case .genericParameterClause:
      visitImplGenericParameterClauseSyntax(data)
    case .conformanceRequirement:
      visitImplConformanceRequirementSyntax(data)
    case .primaryAssociatedTypeClause:
      visitImplPrimaryAssociatedTypeClauseSyntax(data)
    case .simpleTypeIdentifier:
      visitImplSimpleTypeIdentifierSyntax(data)
    case .memberTypeIdentifier:
      visitImplMemberTypeIdentifierSyntax(data)
    case .classRestrictionType:
      visitImplClassRestrictionTypeSyntax(data)
    case .arrayType:
      visitImplArrayTypeSyntax(data)
    case .dictionaryType:
      visitImplDictionaryTypeSyntax(data)
    case .metatypeType:
      visitImplMetatypeTypeSyntax(data)
    case .optionalType:
      visitImplOptionalTypeSyntax(data)
    case .constrainedSugarType:
      visitImplConstrainedSugarTypeSyntax(data)
    case .implicitlyUnwrappedOptionalType:
      visitImplImplicitlyUnwrappedOptionalTypeSyntax(data)
    case .compositionTypeElement:
      visitImplCompositionTypeElementSyntax(data)
    case .compositionTypeElementList:
      visitImplCompositionTypeElementListSyntax(data)
    case .compositionType:
      visitImplCompositionTypeSyntax(data)
    case .packExpansionType:
      visitImplPackExpansionTypeSyntax(data)
    case .tupleTypeElement:
      visitImplTupleTypeElementSyntax(data)
    case .tupleTypeElementList:
      visitImplTupleTypeElementListSyntax(data)
    case .tupleType:
      visitImplTupleTypeSyntax(data)
    case .functionType:
      visitImplFunctionTypeSyntax(data)
    case .attributedType:
      visitImplAttributedTypeSyntax(data)
    case .genericArgumentList:
      visitImplGenericArgumentListSyntax(data)
    case .genericArgument:
      visitImplGenericArgumentSyntax(data)
    case .genericArgumentClause:
      visitImplGenericArgumentClauseSyntax(data)
    case .namedOpaqueReturnType:
      visitImplNamedOpaqueReturnTypeSyntax(data)
    case .typeAnnotation:
      visitImplTypeAnnotationSyntax(data)
    case .enumCasePattern:
      visitImplEnumCasePatternSyntax(data)
    case .isTypePattern:
      visitImplIsTypePatternSyntax(data)
    case .optionalPattern:
      visitImplOptionalPatternSyntax(data)
    case .identifierPattern:
      visitImplIdentifierPatternSyntax(data)
    case .asTypePattern:
      visitImplAsTypePatternSyntax(data)
    case .tuplePattern:
      visitImplTuplePatternSyntax(data)
    case .wildcardPattern:
      visitImplWildcardPatternSyntax(data)
    case .tuplePatternElement:
      visitImplTuplePatternElementSyntax(data)
    case .expressionPattern:
      visitImplExpressionPatternSyntax(data)
    case .tuplePatternElementList:
      visitImplTuplePatternElementListSyntax(data)
    case .valueBindingPattern:
      visitImplValueBindingPatternSyntax(data)
    case .availabilitySpecList:
      visitImplAvailabilitySpecListSyntax(data)
    case .availabilityArgument:
      visitImplAvailabilityArgumentSyntax(data)
    case .availabilityLabeledArgument:
      visitImplAvailabilityLabeledArgumentSyntax(data)
    case .availabilityVersionRestriction:
      visitImplAvailabilityVersionRestrictionSyntax(data)
    case .versionTuple:
      visitImplVersionTupleSyntax(data)
    }
  }

  private func visitChildren<SyntaxType: SyntaxProtocol>(_ node: SyntaxType) {
    let syntaxNode = Syntax(node)
    for childRaw in NonNilRawSyntaxChildren(syntaxNode, viewMode: viewMode) {
      let childData = SyntaxData(childRaw, parent: syntaxNode)
      visit(childData)
    }
  }
}
