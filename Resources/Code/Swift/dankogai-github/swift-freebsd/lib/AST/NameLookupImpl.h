//===--- NameLookupImpl.h - Helpers used to implement lookup ----*- C++ -*-===//
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

#ifndef SWIFT_AST_NAME_LOOKUP_IMPL_H
#define SWIFT_AST_NAME_LOOKUP_IMPL_H

#include "swift/AST/NameLookup.h"
#include "swift/AST/ASTVisitor.h"

namespace swift {
namespace namelookup {

/// Performs a qualified lookup into the given module and, if necessary, its
/// reexports, observing proper shadowing rules.
void
lookupVisibleDeclsInModule(Module *M, Module::AccessPathTy accessPath,
                           SmallVectorImpl<ValueDecl *> &decls,
                           NLKind lookupKind,
                           ResolutionKind resolutionKind,
                           LazyResolver *typeResolver,
                           const DeclContext *moduleScopeContext,
                           ArrayRef<Module::ImportedModule> extraImports = {});

/// Searches through statements and patterns for local variable declarations.
class FindLocalVal : public StmtVisitor<FindLocalVal> {
  friend class ASTVisitor<FindLocalVal>;

  const SourceManager &SM;
  SourceLoc Loc;
  VisibleDeclConsumer &Consumer;

public:
  FindLocalVal(const SourceManager &SM, SourceLoc Loc,
               VisibleDeclConsumer &Consumer)
      : SM(SM), Loc(Loc), Consumer(Consumer) {}

  void checkValueDecl(ValueDecl *D, DeclVisibilityKind Reason) {
    Consumer.foundDecl(D, Reason);
  }

  void checkPattern(const Pattern *Pat, DeclVisibilityKind Reason) {
    switch (Pat->getKind()) {
    case PatternKind::Tuple:
      for (auto &field : cast<TuplePattern>(Pat)->getElements())
        checkPattern(field.getPattern(), Reason);
      return;
    case PatternKind::Paren:
    case PatternKind::Typed:
    case PatternKind::Var:
      return checkPattern(Pat->getSemanticsProvidingPattern(), Reason);
    case PatternKind::Named:
      return checkValueDecl(cast<NamedPattern>(Pat)->getDecl(), Reason);

    case PatternKind::NominalType: {
      for (auto &elt : cast<NominalTypePattern>(Pat)->getElements())
        checkPattern(elt.getSubPattern(), Reason);
      return;
    }
    case PatternKind::EnumElement: {
      auto *OP = cast<EnumElementPattern>(Pat);
      if (OP->hasSubPattern())
        checkPattern(OP->getSubPattern(), Reason);
      return;
    }
    case PatternKind::OptionalSome:
      checkPattern(cast<OptionalSomePattern>(Pat)->getSubPattern(), Reason);
      return;

    case PatternKind::Is: {
      auto *isPat = cast<IsPattern>(Pat);
      if (isPat->hasSubPattern())
        checkPattern(isPat->getSubPattern(), Reason);
      return;
    }

    // Handle non-vars.
    case PatternKind::Bool:
    case PatternKind::Expr:
    case PatternKind::Any:
      return;
    }
  }

  void checkGenericParams(GenericParamList *Params,
                          DeclVisibilityKind Reason) {
    if (!Params)
      return;

    for (auto P : *Params)
      checkValueDecl(P, Reason);
  }

  void checkSourceFile(const SourceFile &SF) {
    for (Decl *D : SF.Decls)
      if (TopLevelCodeDecl *TLCD = dyn_cast<TopLevelCodeDecl>(D))
        visitBraceStmt(TLCD->getBody(), /*isTopLevel=*/true);
  }

private:
  bool isReferencePointInRange(SourceRange R) {
    return SM.rangeContainsTokenLoc(R, Loc);
  }

  void visitBreakStmt(BreakStmt *) {}
  void visitContinueStmt(ContinueStmt *) {}
  void visitFallthroughStmt(FallthroughStmt *) {}
  void visitFailStmt(FailStmt *) {}
  void visitReturnStmt(ReturnStmt *) {}
  void visitThrowStmt(ThrowStmt *) {}
  void visitDeferStmt(DeferStmt *DS) {
    // Nothing in the defer is visible.
  }

  void checkStmtCondition(const StmtCondition &Cond) {
    for (auto entry : Cond)
      if (auto *P = entry.getPatternOrNull())
        if (!isReferencePointInRange(entry.getSourceRange()))
          checkPattern(P, DeclVisibilityKind::LocalVariable);
  }

  void visitIfStmt(IfStmt *S) {
    if (!isReferencePointInRange(S->getSourceRange()))
      return;

    if (!S->getElseStmt() ||
        !isReferencePointInRange(S->getElseStmt()->getSourceRange())) {
      checkStmtCondition(S->getCond());
    }

    visit(S->getThenStmt());
    if (S->getElseStmt())
      visit(S->getElseStmt());
  }
  void visitGuardStmt(GuardStmt *S) {
    if (SM.isBeforeInBuffer(Loc, S->getStartLoc()))
      return;

    // Names in the guard aren't visible until after the body.
    if (!isReferencePointInRange(S->getBody()->getSourceRange()))
      checkStmtCondition(S->getCond());

    visit(S->getBody());
  }

  void visitIfConfigStmt(IfConfigStmt * S) {
    // Active members are attached to the enclosing declaration, so there's no
    // need to walk anything within.
  }
  void visitWhileStmt(WhileStmt *S) {
    if (!isReferencePointInRange(S->getSourceRange()))
      return;

    checkStmtCondition(S->getCond());
    visit(S->getBody());
  }
  void visitRepeatWhileStmt(RepeatWhileStmt *S) {
    visit(S->getBody());
  }
  void visitDoStmt(DoStmt *S) {
    visit(S->getBody());
  }

  void visitForStmt(ForStmt *S) {
    if (!isReferencePointInRange(S->getSourceRange()))
      return;
    visit(S->getBody());
    for (Decl *D : S->getInitializerVarDecls()) {
      if (ValueDecl *VD = dyn_cast<ValueDecl>(D))
        checkValueDecl(VD, DeclVisibilityKind::LocalVariable);
    }
  }
  void visitForEachStmt(ForEachStmt *S) {
    if (!isReferencePointInRange(S->getSourceRange()))
      return;
    visit(S->getBody());
    checkPattern(S->getPattern(), DeclVisibilityKind::LocalVariable);
  }

  void visitBraceStmt(BraceStmt *S, bool isTopLevelCode = false) {
    if (isTopLevelCode) {
      if (SM.isBeforeInBuffer(Loc, S->getStartLoc()))
        return;
    } else {
      if (!isReferencePointInRange(S->getSourceRange()))
        return;
    }

    for (auto elem : S->getElements()) {
      if (Stmt *S = elem.dyn_cast<Stmt*>())
        visit(S);
    }
    for (auto elem : S->getElements()) {
      if (Decl *D = elem.dyn_cast<Decl*>()) {
        if (ValueDecl *VD = dyn_cast<ValueDecl>(D))
          checkValueDecl(VD, DeclVisibilityKind::LocalVariable);
      }
    }
  }
  
  void visitSwitchStmt(SwitchStmt *S) {
    if (!isReferencePointInRange(S->getSourceRange()))
      return;
    for (CaseStmt *C : S->getCases()) {
      visit(C);
    }
  }

  void visitCaseStmt(CaseStmt *S) {
    if (!isReferencePointInRange(S->getSourceRange()))
      return;
    for (const auto &CLI : S->getCaseLabelItems()) {
      auto *P = CLI.getPattern();
      if (!isReferencePointInRange(P->getSourceRange()))
        checkPattern(P, DeclVisibilityKind::LocalVariable);
    }
    visit(S->getBody());
  }

  void visitDoCatchStmt(DoCatchStmt *S) {
    if (!isReferencePointInRange(S->getSourceRange()))
      return;
    visit(S->getBody());
    visitCatchClauses(S->getCatches());
  }
  void visitCatchClauses(ArrayRef<CatchStmt*> clauses) {
    // TODO: some sort of binary search?
    for (auto clause : clauses) {
      visitCatchStmt(clause);
    }
  }
  void visitCatchStmt(CatchStmt *S) {
    if (!isReferencePointInRange(S->getSourceRange()))
      return;
    // Names in the pattern aren't visible until after the pattern.
    if (!isReferencePointInRange(S->getErrorPattern()->getSourceRange()))
      checkPattern(S->getErrorPattern(), DeclVisibilityKind::LocalVariable);
    visit(S->getBody());
  }
  
};

} // end namespace namelookup
} // end namespace swift

#endif
