//===--- TypeTraits.h - clang-tidy-------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_UTILS_TYPETRAITS_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_UTILS_TYPETRAITS_H

#include "clang/AST/ASTContext.h"
#include "clang/AST/Type.h"

namespace clang {
namespace tidy {
namespace type_traits {

// \brief Returns true If \c Type is expensive to copy.
llvm::Optional<bool> isExpensiveToCopy(QualType Type, ASTContext &Context);

} // type_traits
} // namespace tidy
} // namespace clang

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_UTILS_TYPETRAITS_H
