//===- lib/Core/Path.cpp - Path Operating System Concept --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Modified path support to handle frameworks.
///
//===----------------------------------------------------------------------===//

#include "tapi/Core/Path.h"
#include "tapi/Core/FileManager.h"
#include "tapi/Core/LLVM.h"
#include "tapi/Core/Utils.h"
#include "tapi/Defines.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace llvm;

TAPI_NAMESPACE_INTERNAL_BEGIN

/// \brief Replace extension considering frameworks.
void replace_extension(SmallVectorImpl<char> &path, const Twine &extension) {
  StringRef p(path.begin(), path.size());
  auto parentPath = sys::path::parent_path(p);
  auto filename = sys::path::filename(p);

  if (!parentPath.endswith(filename.str() + ".framework")) {
    sys::path::replace_extension(path, extension);
    return;
  }

  SmallString<8> ext_storage;
  StringRef ext = extension.toStringRef(ext_storage);

  // Append '.' if needed.
  if (!ext.empty() && ext[0] != '.')
    path.push_back('.');

  // Append extension.
  path.append(ext.begin(), ext.end());
}

llvm::Expected<PathSeq>
enumerateFiles(FileManager &fm, StringRef path,
               const std::function<bool(StringRef)> &func) {
  PathSeq files;
  std::error_code ec;
  auto &fs = *fm.getVirtualFileSystem();
  for (llvm::vfs::recursive_directory_iterator i(fs, path, ec), ie; i != ie;
       i.increment(ec)) {
    if (ec)
      return errorCodeToError(ec);

    // Skip files that not exist. This usually happens for broken symlinks.
    if (fs.status(i->path()) == std::errc::no_such_file_or_directory)
      continue;

    auto path = i->path();
    if (func(path))
      files.emplace_back(path);
  }

  return files;
}

Expected<PathSeq> enumerateHeaderFiles(FileManager &fm, StringRef path) {
  return enumerateFiles(fm, path, isHeaderFile);
}

TAPI_NAMESPACE_INTERNAL_END
