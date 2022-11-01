//===- tapi/Core/Registry.h - TAPI Registry ---------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief The TAPI registry keeps track of the supported file formats.
///
//===----------------------------------------------------------------------===//

#ifndef TAPI_CORE_REGISTRY_H
#define TAPI_CORE_REGISTRY_H

#include "tapi/Core/ArchitectureSet.h"
#include "tapi/Core/InterfaceFile.h"
#include "tapi/Core/LLVM.h"
#include "tapi/Defines.h"
#include "llvm/BinaryFormat/Magic.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"

using llvm::file_magic;
using llvm::Error;
using llvm::Expected;

TAPI_NAMESPACE_INTERNAL_BEGIN

class Registry;

enum class ReadFlags {
  Header,
  Symbols,
  ObjCMetadata,
  All,
};

/// Abstract Reader class - all readers need to inherit from this class and
/// implement the interface.
class Reader {
public:
  virtual ~Reader() = default;
  virtual bool canRead(file_magic fileType, MemoryBufferRef bufferRef,
                       FileType types = FileType::All) const = 0;
  virtual Expected<FileType> getFileType(file_magic magic,
                                         MemoryBufferRef bufferRef) const = 0;
  virtual Expected<std::unique_ptr<InterfaceFile>>
  readFile(std::unique_ptr<MemoryBuffer> memBuffer, ReadFlags readFlags,
           ArchitectureSet arches) const = 0;
};

/// Abstract Writer class - all writers need to inherit from this class and
/// implement the interface.
class Writer {
public:
  virtual ~Writer() = default;
  virtual bool canWrite(const InterfaceFile *file,
                        VersionedFileType fileType) const = 0;
  virtual Error writeFile(raw_ostream &os, const InterfaceFile *file,
                          VersionedFileType fileType) const = 0;
};

class Registry {
public:
  bool canRead(MemoryBufferRef memBuffer, FileType types = FileType::All) const;
  Expected<FileType> getFileType(MemoryBufferRef memBuffer) const;
  bool canWrite(const InterfaceFile *file, VersionedFileType fileType) const;

  Expected<std::unique_ptr<InterfaceFile>>
  readFile(std::unique_ptr<MemoryBuffer> memBuffer,
           ReadFlags readFlags = ReadFlags::All,
           ArchitectureSet arches = ArchitectureSet::All()) const;
  Error writeFile(const std::string &path, const InterfaceFile *file,
                  VersionedFileType fileType) const;
  Error writeFile(raw_ostream &os, const InterfaceFile *file,
                  VersionedFileType fileType) const;

  void add(std::unique_ptr<Reader> reader) {
    _readers.emplace_back(std::move(reader));
  }

  void add(std::unique_ptr<Writer> writer) {
    _writers.emplace_back(std::move(writer));
  }

  void addBinaryReaders();
  void addYAMLReaders();
  void addYAMLWriters();
  void addDiagnosticReader();

private:
  std::vector<std::unique_ptr<Reader>> _readers;
  std::vector<std::unique_ptr<Writer>> _writers;
};

TAPI_NAMESPACE_INTERNAL_END

#endif // TAPI_CORE_REGISTRY_H
