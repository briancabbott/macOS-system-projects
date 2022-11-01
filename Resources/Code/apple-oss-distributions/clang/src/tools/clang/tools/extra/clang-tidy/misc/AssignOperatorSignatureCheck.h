//===--- AssignOperatorSignatureCheck.h - clang-tidy ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_ASSIGNOPERATORSIGNATURECHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_ASSIGNOPERATORSIGNATURECHECK_H

#include "../ClangTidy.h"

namespace clang {
namespace tidy {
namespace misc {

/// Finds declarations of assign operators with the wrong return and/or argument
/// types.
///
///   * The return type must be `Class&`.
///   * Works with move-assign and assign by value.
///   * Private and deleted operators are ignored.
class AssignOperatorSignatureCheck : public ClangTidyCheck {
public:
  AssignOperatorSignatureCheck(StringRef Name, ClangTidyContext *Context)
      : ClangTidyCheck(Name, Context) {}
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
};

} // namespace misc
} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_MISC_ASSIGNOPERATORSIGNATURECHECK_H
