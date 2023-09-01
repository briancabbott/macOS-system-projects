//===--- ImageInspectionWin32.cpp - Win32 image inspection ----------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#if !defined(__ELF__) && !defined(__MACH__)

#include "ImageInspection.h"

#if defined(__CYGWIN__)
#include <dlfcn.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <DbgHelp.h>
#endif

#include "swift/Threading/Mutex.h"

using namespace swift;

int swift::lookupSymbol(const void *address, SymbolInfo *info) {
#if defined(__CYGWIN__)
  Dl_info dlinfo;
  if (dladdr(address, &dlinfo) == 0) {
    return 0;
  }

  info->fileName = dlinfo.dli_fname;
  info->baseAddress = dlinfo.dli_fbase;
  info->symbolName = dli_info.dli_sname;
  info->symbolAddress = dli_saddr;
  return 1;
#elif defined(_WIN32)
  return _swift_win32_withDbgHelpLibrary([&] (HANDLE hProcess) {
    static const constexpr size_t kSymbolMaxNameLen = 1024;

    if (!hProcess) {
      return 0;
    }

    SYMBOL_INFO_PACKAGE symbol = {};
    symbol.si.SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol.si.MaxNameLen = MAX_SYM_NAME;
    if (SymFromAddr(hProcess, reinterpret_cast<const DWORD64>(address),
                    nullptr, &symbol.si) == FALSE) {
      return 0;
    }

    info->fileName = NULL;
    info->baseAddress = reinterpret_cast<void *>(symbol.si.ModBase);
    info->symbolName.reset(_strdup(symbol.si.Name));
    info->symbolAddress = reinterpret_cast<void *>(symbol.si.Address);

    return 1;
  });
#else
  return 0;
#endif // defined(__CYGWIN__) || defined(_WIN32)
}

#if defined(_WIN32)
static LazyMutex mutex;
static HANDLE dbgHelpHandle = nullptr;

void swift::_swift_win32_withDbgHelpLibrary(
  void (* body)(HANDLE hProcess, void *context), void *context) {
  mutex.withLock([=] () {
    // If we have not previously created a handle to use with the library, do so
    // now. This handle belongs to the Swift runtime and should not be closed by
    // `body` (or anybody else.)
    if (!dbgHelpHandle) {
      // Per the documentation for the Debug Help library, we should not use the
      // current process handle because other subsystems might also use it and
      // end up stomping on each other. So we'll try to duplicate that handle to
      // get a unique one that still fulfills the needs of the library. If that
      // fails (presumably because the current process doesn't have the
      // PROCESS_DUP_HANDLE access right) then fall back to using the original
      // process handle and hope nobody else is using it too.
      HANDLE currentProcess = GetCurrentProcess();
      if (!DuplicateHandle(currentProcess, currentProcess, currentProcess,
                           &dbgHelpHandle, 0, false, DUPLICATE_SAME_ACCESS)) {
        dbgHelpHandle = currentProcess;
      }
    }

    // If we have not previously initialized the Debug Help library, do so now.
    bool isDbgHelpInitialized = false;
    if (dbgHelpHandle) {
      isDbgHelpInitialized = SymInitialize(dbgHelpHandle, nullptr, true);
    }

    if (isDbgHelpInitialized) {
      // Set the library's options to what the Swift runtime generally expects.
      // If the options aren't going to change, we can skip the call and save a
      // few CPU cycles on the library call.
      constexpr const DWORD options = SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS;
      DWORD oldOptions = SymGetOptions();
      if (oldOptions != options) {
        SymSetOptions(options);
      }

      body(dbgHelpHandle, context);

      // Before returning, reset the library's options back to their previous
      // value. No need to call if the options didn't change because LazyMutex
      // is not recursive, so there shouldn't be an outer call expecting the
      // original options, and a subsequent call to this function will set them
      // to the defaults above.
      if (oldOptions != options) {
        SymSetOptions(oldOptions);
      }
    } else {
      body(nullptr, context);
    }
  });
}
#endif

#endif // !defined(__ELF__) && !defined(__MACH__)
