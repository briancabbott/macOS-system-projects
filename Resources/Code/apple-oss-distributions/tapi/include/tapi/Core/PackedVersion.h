//===- tapi/Core/PackedVersion.h - PackedVersion ----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief Defines the MachO Packed Version.
///
//===----------------------------------------------------------------------===//

#ifndef TAPI_CORE_PACKED_VERSION_H
#define TAPI_CORE_PACKED_VERSION_H

#include "tapi/Core/LLVM.h"
#include "tapi/Defines.h"
#include "tapi/PackedVersion32.h"
#include "tapi/tapi.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/VersionTuple.h"
#include "llvm/Support/raw_ostream.h"
#include <utility>

TAPI_NAMESPACE_INTERNAL_BEGIN

struct PackedVersion {
  uint32_t _version{0};

  constexpr PackedVersion() = default;
  constexpr PackedVersion(uint32_t version) : _version(version) {}
  PackedVersion(unsigned major, unsigned minor, unsigned subminor)
      : _version((major << 16) | ((minor & 0xff) << 8) | (subminor & 0xff)) {}
  PackedVersion(llvm::VersionTuple version) {
    _version = version.getMajor() << 16;
    if (auto minor = version.getMinor())
      _version |= (*minor & 0xff) << 8;
    if (auto subminor = version.getSubminor())
      _version |= *subminor & 0xff;
  }

  bool empty() const { return _version == 0; }

  /// \brief Retrieve the major version number.
  unsigned getMajor() const { return _version >> 16; }

  /// \brief Retrieve the minor version number, if provided.
  unsigned getMinor() const { return (_version >> 8) & 0xff; }

  /// \brief Retrieve the subminor version number, if provided.
  unsigned getSubminor() const { return _version & 0xff; }

  bool parse32(StringRef str);
  std::pair<bool, bool> parse64(StringRef str);

  bool operator<(const PackedVersion &rhs) const {
    return _version < rhs._version;
  }

  bool operator<=(const PackedVersion &rhs) const {
    return _version <= rhs._version;
  }

  bool operator==(const PackedVersion &rhs) const {
    return _version == rhs._version;
  }

  bool operator!=(const PackedVersion &rhs) const {
    return _version != rhs._version;
  }

  void print(raw_ostream &os) const;

  operator PackedVersion32() const {
    return {getMajor(), getMinor(), getSubminor()};
  }
};

inline raw_ostream &operator<<(raw_ostream &os, const PackedVersion &version) {
  version.print(os);
  return os;
}

const DiagnosticBuilder &operator<<(const DiagnosticBuilder &db,
                                    const PackedVersion &version);

TAPI_NAMESPACE_INTERNAL_END

#endif // TAPI_CORE_PACKED_VERSION_H
