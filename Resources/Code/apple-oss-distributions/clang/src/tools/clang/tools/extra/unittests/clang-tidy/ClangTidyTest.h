//===--- ClangTidyTest.h - clang-tidy ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_UNITTESTS_CLANG_TIDY_CLANGTIDYTEST_H
#define LLVM_CLANG_TOOLS_EXTRA_UNITTESTS_CLANG_TIDY_CLANGTIDYTEST_H

#include "ClangTidy.h"
#include "ClangTidyDiagnosticConsumer.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendActions.h"
#include "clang/Tooling/Refactoring.h"
#include "clang/Tooling/Tooling.h"
#include <map>
#include <memory>

namespace clang {
namespace tidy {
namespace test {

class TestClangTidyAction : public ASTFrontendAction {
public:
  TestClangTidyAction(SmallVectorImpl<std::unique_ptr<ClangTidyCheck>> &Checks,
                      ast_matchers::MatchFinder &Finder,
                      ClangTidyContext &Context)
      : Checks(Checks), Finder(Finder), Context(Context) {}

private:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &Compiler,
                                                 StringRef File) override {
    Context.setSourceManager(&Compiler.getSourceManager());
    Context.setCurrentFile(File);
    Context.setASTContext(&Compiler.getASTContext());

    for (auto &Check : Checks) {
      Check->registerMatchers(&Finder);
      Check->registerPPCallbacks(Compiler);
    }
    return Finder.newASTConsumer();
  }

  SmallVectorImpl<std::unique_ptr<ClangTidyCheck>> &Checks;
  ast_matchers::MatchFinder &Finder;
  ClangTidyContext &Context;
};

template <typename Check, typename... Checks> struct CheckFactory {
  static void
  createChecks(ClangTidyContext *Context,
               SmallVectorImpl<std::unique_ptr<ClangTidyCheck>> &Result) {
    CheckFactory<Check>::createChecks(Context, Result);
    CheckFactory<Checks...>::createChecks(Context, Result);
  }
};

template <typename Check> struct CheckFactory<Check> {
  static void
  createChecks(ClangTidyContext *Context,
               SmallVectorImpl<std::unique_ptr<ClangTidyCheck>> &Result) {
    Result.emplace_back(llvm::make_unique<Check>(
        "test-check-" + std::to_string(Result.size()), Context));
  }
};

template <typename... CheckList>
std::string
runCheckOnCode(StringRef Code, std::vector<ClangTidyError> *Errors = nullptr,
               const Twine &Filename = "input.cc",
               ArrayRef<std::string> ExtraArgs = None,
               const ClangTidyOptions &ExtraOptions = ClangTidyOptions(),
               std::map<StringRef, StringRef> PathsToContent =
                   std::map<StringRef, StringRef>()) {
  ClangTidyOptions Options = ExtraOptions;
  Options.Checks = "*";
  ClangTidyContext Context(llvm::make_unique<DefaultOptionsProvider>(
      ClangTidyGlobalOptions(), Options));
  ClangTidyDiagnosticConsumer DiagConsumer(Context);

  std::vector<std::string> ArgCXX11(1, "clang-tidy");
  ArgCXX11.push_back("-fsyntax-only");
  ArgCXX11.push_back("-std=c++11");
  ArgCXX11.push_back("-Iinclude");
  ArgCXX11.insert(ArgCXX11.end(), ExtraArgs.begin(), ExtraArgs.end());
  ArgCXX11.push_back(Filename.str());

  ast_matchers::MatchFinder Finder;
  llvm::IntrusiveRefCntPtr<vfs::InMemoryFileSystem> InMemoryFileSystem(
      new vfs::InMemoryFileSystem);
  llvm::IntrusiveRefCntPtr<FileManager> Files(
      new FileManager(FileSystemOptions(), InMemoryFileSystem));

  SmallVector<std::unique_ptr<ClangTidyCheck>, 1> Checks;
  CheckFactory<CheckList...>::createChecks(&Context, Checks);
  tooling::ToolInvocation Invocation(
      ArgCXX11, new TestClangTidyAction(Checks, Finder, Context), Files.get());
  InMemoryFileSystem->addFile(Filename, 0,
                              llvm::MemoryBuffer::getMemBuffer(Code));
  for (const auto &FileContent : PathsToContent) {
    InMemoryFileSystem->addFile(
        Twine("include/") + FileContent.first, 0,
        llvm::MemoryBuffer::getMemBuffer(FileContent.second));
  }
  Invocation.setDiagnosticConsumer(&DiagConsumer);
  if (!Invocation.run()) {
    std::string ErrorText;
    for (const auto &Error : Context.getErrors()) {
      ErrorText += Error.Message.Message + "\n";
    }
    llvm::report_fatal_error(ErrorText);
  }

  DiagConsumer.finish();
  tooling::Replacements Fixes;
  for (const ClangTidyError &Error : Context.getErrors())
    Fixes.insert(Error.Fix.begin(), Error.Fix.end());
  if (Errors)
    *Errors = Context.getErrors();
  return tooling::applyAllReplacements(Code, Fixes);
}

#define EXPECT_NO_CHANGES(Check, Code)                                         \
  EXPECT_EQ(Code, runCheckOnCode<Check>(Code))

} // namespace test
} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_UNITTESTS_CLANG_TIDY_CLANGTIDYTEST_H
