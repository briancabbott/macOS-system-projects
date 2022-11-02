//===--- ToolChains.h - Platform-specific ToolChain logic -------*- C++ -*-===//
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

#ifndef SWIFT_DRIVER_TOOLCHAINS_H
#define SWIFT_DRIVER_TOOLCHAINS_H

#include "swift/Basic/LLVM.h"
#include "swift/Driver/ToolChain.h"
#include "llvm/Support/Compiler.h"

namespace swift {
namespace driver {
namespace toolchains {

class LLVM_LIBRARY_VISIBILITY Darwin : public ToolChain {
protected:
  std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const LinkJobAction &job,
                      const JobContext &context) const override;

  std::string findProgramRelativeToSwiftImpl(StringRef name) const override;

public:
  Darwin(const Driver &D, const llvm::Triple &Triple) : ToolChain(D, Triple) {}
  ~Darwin() = default;

};

#if defined(SWIFT_ENABLE_TARGET_LINUX)

class LLVM_LIBRARY_VISIBILITY Linux : public ToolChain {
protected:
  std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const AutolinkExtractJobAction &job,
                      const JobContext &context) const override;
  std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const LinkJobAction &job,
                      const JobContext &context) const override;

public:
  Linux(const Driver &D, const llvm::Triple &Triple) : ToolChain(D, Triple) {}
  ~Linux() = default;
};

#endif // SWIFT_ENABLE_TARGET_LINUX

#if defined(SWIFT_ENABLE_TARGET_FREEBSD)

class LLVM_LIBRARY_VISIBILITY FreeBSD : public ToolChain {
protected:
  std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const AutolinkExtractJobAction &job,
                      const JobContext &context) const override;
  std::pair<const char *, llvm::opt::ArgStringList>
  constructInvocation(const LinkJobAction &job,
                      const JobContext &context) const override;

public:
  FreeBSD(const Driver &D, const llvm::Triple &Triple) : ToolChain(D, Triple) {}
  ~FreeBSD() = default;
};

#endif // SWIFT_ENABLE_TARGET_FREEBSD

} // end namespace toolchains
} // end namespace driver
} // end namespace swift

#endif
