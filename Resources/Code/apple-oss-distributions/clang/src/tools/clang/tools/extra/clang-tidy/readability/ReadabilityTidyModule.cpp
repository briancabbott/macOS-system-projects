//===--- ReadabilityTidyModule.cpp - clang-tidy ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "../ClangTidy.h"
#include "../ClangTidyModule.h"
#include "../ClangTidyModuleRegistry.h"
#include "BracesAroundStatementsCheck.h"
#include "ContainerSizeEmptyCheck.h"
#include "ElseAfterReturnCheck.h"
#include "FunctionSizeCheck.h"
#include "IdentifierNamingCheck.h"
#include "ImplicitBoolCastCheck.h"
#include "InconsistentDeclarationParameterNameCheck.h"
#include "NamedParameterCheck.h"
#include "RedundantSmartptrGetCheck.h"
#include "RedundantStringCStrCheck.h"
#include "SimplifyBooleanExprCheck.h"
#include "UniqueptrDeleteReleaseCheck.h"

namespace clang {
namespace tidy {
namespace readability {

class ReadabilityModule : public ClangTidyModule {
public:
  void addCheckFactories(ClangTidyCheckFactories &CheckFactories) override {
    CheckFactories.registerCheck<BracesAroundStatementsCheck>(
        "readability-braces-around-statements");
    CheckFactories.registerCheck<ContainerSizeEmptyCheck>(
        "readability-container-size-empty");
    CheckFactories.registerCheck<ElseAfterReturnCheck>(
        "readability-else-after-return");
    CheckFactories.registerCheck<FunctionSizeCheck>(
        "readability-function-size");
    CheckFactories.registerCheck<IdentifierNamingCheck>(
        "readability-identifier-naming");
    CheckFactories.registerCheck<ImplicitBoolCastCheck>(
        "readability-implicit-bool-cast");
    CheckFactories.registerCheck<InconsistentDeclarationParameterNameCheck>(
        "readability-inconsistent-declaration-parameter-name");
    CheckFactories.registerCheck<UniqueptrDeleteReleaseCheck>(
        "readability-uniqueptr-delete-release");
    CheckFactories.registerCheck<readability::NamedParameterCheck>(
        "readability-named-parameter");
    CheckFactories.registerCheck<RedundantSmartptrGetCheck>(
        "readability-redundant-smartptr-get");
    CheckFactories.registerCheck<RedundantStringCStrCheck>(
        "readability-redundant-string-cstr");
    CheckFactories.registerCheck<SimplifyBooleanExprCheck>(
        "readability-simplify-boolean-expr");
  }
};

// Register the ReadabilityModule using this statically initialized variable.
static ClangTidyModuleRegistry::Add<ReadabilityModule>
    X("readability-module", "Adds readability-related checks.");

} // namespace readability

// This anchor is used to force the linker to link in the generated object file
// and thus register the ReadabilityModule.
volatile int ReadabilityModuleAnchorSource = 0;

} // namespace tidy
} // namespace clang
