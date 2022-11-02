//===-- TypeSystem.cpp ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

//
//  TypeSystem.cpp
//  lldb
//
//  Created by Ryan Brown on 3/29/15.
//
//

#include "lldb/Symbol/TypeSystem.h"

#include <set>

#include "lldb/Utility/Status.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Symbol/CompilerType.h"
#include "lldb/Target/Language.h"

using namespace lldb_private;
using namespace lldb;

/// A 64-bit SmallBitVector is only small up to 64-7 bits, and the
/// setBitsInMask interface wants to write full bytes.
static const size_t g_num_small_bitvector_bits = 64 - 8;
static_assert(eNumLanguageTypes < g_num_small_bitvector_bits,
              "Languages bit vector is no longer small on 64 bit systems");
LanguageSet::LanguageSet() : bitvector(eNumLanguageTypes, 0) {}

llvm::Optional<LanguageType> LanguageSet::GetSingularLanguage() {
  if (bitvector.count() == 1)
    return (LanguageType)bitvector.find_first();
  return {};
}

void LanguageSet::Insert(LanguageType language) { bitvector.set(language); }
size_t LanguageSet::Size() const { return bitvector.count(); }
bool LanguageSet::Empty() const { return bitvector.none(); }
bool LanguageSet::operator[](unsigned i) const { return bitvector[i]; }

TypeSystem::TypeSystem(LLVMCastKind kind) : m_kind(kind), m_sym_file(nullptr) {}

TypeSystem::~TypeSystem() {}

static lldb::TypeSystemSP CreateInstanceHelper(lldb::LanguageType language,
                                               Module *module, Target *target,
                                               const char *compiler_options) {
  uint32_t i = 0;
  TypeSystemCreateInstance create_callback;
  while ((create_callback = PluginManager::GetTypeSystemCreateCallbackAtIndex(
              i++)) != nullptr) {
    lldb::TypeSystemSP type_system_sp =
        create_callback(language, module, target, compiler_options);
    if (type_system_sp)
      return type_system_sp;
  }

  return lldb::TypeSystemSP();
}

lldb::TypeSystemSP TypeSystem::CreateInstance(lldb::LanguageType language,
                                              Module *module) {
  return CreateInstanceHelper(language, module, nullptr, nullptr);
}

lldb::TypeSystemSP TypeSystem::CreateInstance(lldb::LanguageType language,
                                              Target *target,
                                              const char *compiler_options) {
  return CreateInstanceHelper(language, nullptr, target, compiler_options);
}

lldb::TypeSystemSP TypeSystem::CreateInstance(lldb::LanguageType language,
                                              Target *target) {
  return CreateInstanceHelper(language, nullptr, target, nullptr);
}

bool TypeSystem::IsAnonymousType(lldb::opaque_compiler_type_t type) {
  return false;
}

CompilerType TypeSystem::GetArrayType(lldb::opaque_compiler_type_t type,
                                      uint64_t size) {
  return CompilerType();
}

CompilerType
TypeSystem::GetLValueReferenceType(lldb::opaque_compiler_type_t type) {
  return CompilerType();
}

CompilerType
TypeSystem::GetRValueReferenceType(lldb::opaque_compiler_type_t type) {
  return CompilerType();
}

CompilerType TypeSystem::AddConstModifier(lldb::opaque_compiler_type_t type) {
  return CompilerType();
}

CompilerType
TypeSystem::AddVolatileModifier(lldb::opaque_compiler_type_t type) {
  return CompilerType();
}

CompilerType
TypeSystem::AddRestrictModifier(lldb::opaque_compiler_type_t type) {
  return CompilerType();
}

CompilerType TypeSystem::CreateTypedef(lldb::opaque_compiler_type_t type,
                                       const char *name,
                                       const CompilerDeclContext &decl_ctx) {
  return CompilerType();
}

CompilerType TypeSystem::GetBuiltinTypeByName(ConstString name) {
  return CompilerType();
}

CompilerType TypeSystem::GetTypeForFormatters(void *type) {
  return CompilerType(this, type);
}

size_t TypeSystem::GetNumTemplateArguments(lldb::opaque_compiler_type_t type) {
  return 0;
}

TemplateArgumentKind
TypeSystem::GetTemplateArgumentKind(opaque_compiler_type_t type, size_t idx) {
  return eTemplateArgumentKindNull;
}

CompilerType TypeSystem::GetTypeTemplateArgument(opaque_compiler_type_t type,
                                                 size_t idx) {
  return CompilerType();
}

GenericKind TypeSystem::GetGenericArgumentKind(void *type, size_t idx) {
  return eNullGenericKindType;
}

CompilerType TypeSystem::GetGenericArgumentType(void *type, size_t idx) {
  return CompilerType();
}

llvm::Optional<CompilerType::IntegralTemplateArgument>
TypeSystem::GetIntegralTemplateArgument(opaque_compiler_type_t type,
                                        size_t idx) {
  return llvm::None;
}

LazyBool TypeSystem::ShouldPrintAsOneLiner(void *type, ValueObject *valobj) {
  return eLazyBoolCalculate;
}

bool TypeSystem::IsMeaninglessWithoutDynamicResolution(void *type) {
  return false;
}

void TypeSystem::DiagnoseWarnings(Process &process, Module &module) const {}

Status TypeSystem::IsCompatible() {
  // Assume a language is compatible. Override this virtual function
  // in your TypeSystem plug-in if version checking is desired.
  return Status();
}

ConstString TypeSystem::GetDisplayTypeName(void *type,
                                           const SymbolContext *sc) {
  return GetTypeName(type);
}

ConstString TypeSystem::GetMangledTypeName(void *type) {
  return GetTypeName(type);
}

ConstString TypeSystem::DeclGetMangledName(void *opaque_decl) {
  return ConstString();
}

CompilerDeclContext TypeSystem::DeclGetDeclContext(void *opaque_decl) {
  return CompilerDeclContext();
}

CompilerType TypeSystem::DeclGetFunctionReturnType(void *opaque_decl) {
  return CompilerType();
}

size_t TypeSystem::DeclGetFunctionNumArguments(void *opaque_decl) { return 0; }

CompilerType TypeSystem::DeclGetFunctionArgumentType(void *opaque_decl,
                                                     size_t arg_idx) {
  return CompilerType();
}

std::vector<CompilerDecl>
TypeSystem::DeclContextFindDeclByName(void *opaque_decl_ctx, ConstString name,
                                      bool ignore_imported_decls) {
  return std::vector<CompilerDecl>();
}

#pragma mark TypeSystemMap

TypeSystemMap::TypeSystemMap()
    : m_mutex(), m_map(), m_clear_in_progress(false) {}

TypeSystemMap::~TypeSystemMap() {}

void TypeSystemMap::operator=(const TypeSystemMap &rhs) { m_map = rhs.m_map; }

void TypeSystemMap::Clear() {
  collection map;
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    map = m_map;
    m_clear_in_progress = true;
  }
  std::set<TypeSystem *> visited;
  for (auto pair : map) {
    TypeSystem *type_system = pair.second.get();
    if (type_system && !visited.count(type_system)) {
      visited.insert(type_system);
      type_system->Finalize();
    }
  }
  map.clear();
  {
    std::lock_guard<std::mutex> guard(m_mutex);
    m_map.clear();
    m_clear_in_progress = false;
  }
}

void TypeSystemMap::ForEach(std::function<bool(TypeSystem *)> const &callback) {
  std::lock_guard<std::mutex> guard(m_mutex);
  // Use a std::set so we only call the callback once for each unique
  // TypeSystem instance
  std::set<TypeSystem *> visited;
  for (auto pair : m_map) {
    TypeSystem *type_system = pair.second.get();
    if (type_system && !visited.count(type_system)) {
      visited.insert(type_system);
      if (!callback(type_system))
        break;
    }
  }
}

llvm::Expected<TypeSystem &>
TypeSystemMap::GetTypeSystemForLanguage(lldb::LanguageType language,
                                        Module *module, bool can_create) {
  llvm::Error error = llvm::Error::success();
  assert(!error); // Check the success value when assertions are enabled
  std::lock_guard<std::mutex> guard(m_mutex);
  if (m_clear_in_progress) {
    error = llvm::make_error<llvm::StringError>(
        "Unable to get TypeSystem because TypeSystemMap is being cleared",
        llvm::inconvertibleErrorCode());
  } else {
    collection::iterator pos = m_map.find(language);
    if (pos != m_map.end()) {
      auto *type_system = pos->second.get();
      if (type_system) {
        llvm::consumeError(std::move(error));
        return *type_system;
      }
      error = llvm::make_error<llvm::StringError>(
          "TypeSystem for language " +
              llvm::toStringRef(Language::GetNameForLanguageType(language)) +
              " doesn't exist",
          llvm::inconvertibleErrorCode());
      return std::move(error);
    }

    for (const auto &pair : m_map) {
      if (pair.second && pair.second->SupportsLanguage(language)) {
        // Add a new mapping for "language" to point to an already existing
        // TypeSystem that supports this language
        m_map[language] = pair.second;
        if (pair.second.get()) {
          llvm::consumeError(std::move(error));
          return *pair.second.get();
        }
        error = llvm::make_error<llvm::StringError>(
            "TypeSystem for language " +
                llvm::toStringRef(Language::GetNameForLanguageType(language)) +
                " doesn't exist",
            llvm::inconvertibleErrorCode());
        return std::move(error);
      }
    }

    if (!can_create) {
      error = llvm::make_error<llvm::StringError>(
          "Unable to find type system for language " +
              llvm::toStringRef(Language::GetNameForLanguageType(language)),
          llvm::inconvertibleErrorCode());
    } else {
      // Cache even if we get a shared pointer that contains a null type system
      // back
      auto type_system_sp = TypeSystem::CreateInstance(language, module);
      m_map[language] = type_system_sp;
      if (type_system_sp.get()) {
        llvm::consumeError(std::move(error));
        return *type_system_sp.get();
      }
      error = llvm::make_error<llvm::StringError>(
          "TypeSystem for language " +
              llvm::toStringRef(Language::GetNameForLanguageType(language)) +
              " doesn't exist",
          llvm::inconvertibleErrorCode());
    }
  }

  return std::move(error);
}

llvm::Expected<TypeSystem &>
TypeSystemMap::GetTypeSystemForLanguage(lldb::LanguageType language,
                                        Target *target, bool can_create,
                                        const char *compiler_options) {
  llvm::Error error = llvm::Error::success();
  assert(!error); // Check the success value when assertions are enabled
  std::lock_guard<std::mutex> guard(m_mutex);
  if (m_clear_in_progress) {
    error = llvm::make_error<llvm::StringError>(
        "Unable to get TypeSystem because TypeSystemMap is being cleared",
        llvm::inconvertibleErrorCode());
  } else {
    collection::iterator pos = m_map.find(language);
    if (pos != m_map.end()) {
      auto *type_system = pos->second.get();
      if (type_system) {
        llvm::consumeError(std::move(error));
        return *type_system;
      }
      error = llvm::make_error<llvm::StringError>(
          "TypeSystem for language " +
              llvm::toStringRef(Language::GetNameForLanguageType(language)) +
              " doesn't exist",
          llvm::inconvertibleErrorCode());
      return std::move(error);
    }

    for (const auto &pair : m_map) {
      if (pair.second && pair.second->SupportsLanguage(language)) {
        // Add a new mapping for "language" to point to an already existing
        // TypeSystem that supports this language
        m_map[language] = pair.second;
        if (pair.second.get()) {
          llvm::consumeError(std::move(error));
          return *pair.second.get();
        }
        error = llvm::make_error<llvm::StringError>(
            "TypeSystem for language " +
                llvm::toStringRef(Language::GetNameForLanguageType(language)) +
                " doesn't exist",
            llvm::inconvertibleErrorCode());
        return std::move(error);
      }
    }

    if (!can_create) {
      error = llvm::make_error<llvm::StringError>(
          "Unable to find type system for language " +
              llvm::toStringRef(Language::GetNameForLanguageType(language)),
          llvm::inconvertibleErrorCode());
    } else {
      // Cache even if we get a shared pointer that contains a null type system
      // back
      auto type_system_sp = TypeSystem::CreateInstance(language, target,
                                                       compiler_options);
      m_map[language] = type_system_sp;
      if (type_system_sp.get()) {
        llvm::consumeError(std::move(error));
        return *type_system_sp.get();
      }
      error = llvm::make_error<llvm::StringError>(
          "TypeSystem for language " +
              llvm::toStringRef(Language::GetNameForLanguageType(language)) +
              " doesn't exist",
          llvm::inconvertibleErrorCode());
    }
  }

  return std::move(error);
}

void TypeSystemMap::RemoveTypeSystemsForLanguage(lldb::LanguageType language) {
  std::lock_guard<std::mutex> guard(m_mutex);
  collection::iterator pos = m_map.find(language);
  // If we are clearning the map, we don't need to remove this individual item.
  // It will go away soon enough.
  if (!m_clear_in_progress) {
    if (pos != m_map.end())
      m_map.erase(pos);
  }
}

