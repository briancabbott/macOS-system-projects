//===- RedundantStringCStrCheck.cpp - Check for redundant c_str calls -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file implements a check for redundant calls of c_str() on strings.
//
//===----------------------------------------------------------------------===//

#include "RedundantStringCStrCheck.h"
#include "clang/Lex/Lexer.h"

namespace clang {

using namespace ast_matchers;

namespace {

template <typename T>
StringRef getText(const ast_matchers::MatchFinder::MatchResult &Result,
                  T const &Node) {
  return Lexer::getSourceText(
      CharSourceRange::getTokenRange(Node.getSourceRange()),
      *Result.SourceManager, Result.Context->getLangOpts());
}

// Return true if expr needs to be put in parens when it is an argument of a
// prefix unary operator, e.g. when it is a binary or ternary operator
// syntactically.
bool needParensAfterUnaryOperator(const Expr &ExprNode) {
  if (isa<clang::BinaryOperator>(&ExprNode) ||
      isa<clang::ConditionalOperator>(&ExprNode)) {
    return true;
  }
  if (const auto *Op = dyn_cast<CXXOperatorCallExpr>(&ExprNode)) {
    return Op->getNumArgs() == 2 && Op->getOperator() != OO_PlusPlus &&
           Op->getOperator() != OO_MinusMinus && Op->getOperator() != OO_Call &&
           Op->getOperator() != OO_Subscript;
  }
  return false;
}

// Format a pointer to an expression: prefix with '*' but simplify
// when it already begins with '&'.  Return empty string on failure.
std::string
formatDereference(const ast_matchers::MatchFinder::MatchResult &Result,
                  const Expr &ExprNode) {
  if (const auto *Op = dyn_cast<clang::UnaryOperator>(&ExprNode)) {
    if (Op->getOpcode() == UO_AddrOf) {
      // Strip leading '&'.
      return getText(Result, *Op->getSubExpr()->IgnoreParens());
    }
  }
  StringRef Text = getText(Result, ExprNode);
  if (Text.empty())
    return std::string();
  // Add leading '*'.
  if (needParensAfterUnaryOperator(ExprNode)) {
    return (llvm::Twine("*(") + Text + ")").str();
  }
  return (llvm::Twine("*") + Text).str();
}

const char StringConstructor[] =
    "::std::basic_string<char, std::char_traits<char>, std::allocator<char> >"
    "::basic_string";

const char StringCStrMethod[] =
    "::std::basic_string<char, std::char_traits<char>, std::allocator<char> >"
    "::c_str";

} // end namespace

namespace tidy {
namespace readability {

void RedundantStringCStrCheck::registerMatchers(
    ast_matchers::MatchFinder *Finder) {
  // Only register the matchers for C++; the functionality currently does not
  // provide any benefit to other languages, despite being benign.
  if (!getLangOpts().CPlusPlus)
    return;

  Finder->addMatcher(
      cxxConstructExpr(
          hasDeclaration(cxxMethodDecl(hasName(StringConstructor))),
          argumentCountIs(2),
          // The first argument must have the form x.c_str() or p->c_str()
          // where the method is string::c_str().  We can use the copy
          // constructor of string instead (or the compiler might share
          // the string object).
          hasArgument(0, cxxMemberCallExpr(
                             callee(memberExpr().bind("member")),
                             callee(cxxMethodDecl(hasName(StringCStrMethod))),
                             on(expr().bind("arg")))
                             .bind("call")),
          // The second argument is the alloc object which must not be
          // present explicitly.
          hasArgument(1, cxxDefaultArgExpr())),
      this);
  Finder->addMatcher(
      cxxConstructExpr(
          // Implicit constructors of these classes are overloaded
          // wrt. string types and they internally make a StringRef
          // referring to the argument.  Passing a string directly to
          // them is preferred to passing a char pointer.
          hasDeclaration(
              cxxMethodDecl(anyOf(hasName("::llvm::StringRef::StringRef"),
                                  hasName("::llvm::Twine::Twine")))),
          argumentCountIs(1),
          // The only argument must have the form x.c_str() or p->c_str()
          // where the method is string::c_str().  StringRef also has
          // a constructor from string which is more efficient (avoids
          // strlen), so we can construct StringRef from the string
          // directly.
          hasArgument(0, cxxMemberCallExpr(
                             callee(memberExpr().bind("member")),
                             callee(cxxMethodDecl(hasName(StringCStrMethod))),
                             on(expr().bind("arg")))
                             .bind("call"))),
      this);
}

void RedundantStringCStrCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Call = Result.Nodes.getStmtAs<CallExpr>("call");
  const auto *Arg = Result.Nodes.getStmtAs<Expr>("arg");
  bool Arrow = Result.Nodes.getStmtAs<MemberExpr>("member")->isArrow();
  // Replace the "call" node with the "arg" node, prefixed with '*'
  // if the call was using '->' rather than '.'.
  std::string ArgText =
      Arrow ? formatDereference(Result, *Arg) : getText(Result, *Arg).str();
  if (ArgText.empty())
    return;

  diag(Call->getLocStart(), "redundant call to `c_str()`")
      << FixItHint::CreateReplacement(Call->getSourceRange(), ArgText);
}

} // namespace readability
} // namespace tidy
} // namespace clang
