// Copyright © 2021 Saleem Abdulrasool <compnerd@compnerd.org>
// SPDX-License-Identifier: Apache-2.0

#include "swift_installer.hh"
#include "logging.hh"
#include "scoped_raii.hh"

#include <comip.h>
#include <comdef.h>

#include <algorithm>
#include <cassert>
#include <ciso646>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

// NOTE(compnerd) `Unknwn.h` must be included before `Setup.Configuration.h` as
// the header is not fully self-contained.
#include <Unknwn.h>
#include <Setup.Configuration.h>

_COM_SMARTPTR_TYPEDEF(IEnumSetupInstances, __uuidof(IEnumSetupInstances));
_COM_SMARTPTR_TYPEDEF(ISetupConfiguration, __uuidof(ISetupConfiguration));
_COM_SMARTPTR_TYPEDEF(ISetupConfiguration2, __uuidof(ISetupConfiguration2));
_COM_SMARTPTR_TYPEDEF(ISetupHelper, __uuidof(ISetupHelper));
_COM_SMARTPTR_TYPEDEF(ISetupInstance, __uuidof(ISetupInstance));
_COM_SMARTPTR_TYPEDEF(ISetupInstance2, __uuidof(ISetupInstance2));
_COM_SMARTPTR_TYPEDEF(ISetupPackageReference, __uuidof(ISetupPackageReference));

namespace {
std::string contents(const std::filesystem::path &path) noexcept {
  std::ostringstream buffer;
  std::ifstream stream(path);
  if (!stream)
    return {};
  buffer << stream.rdbuf();
  return buffer.str();
}

template <typename CharType_>
void trim(std::basic_string<CharType_> &string) noexcept {
  string.erase(std::remove_if(std::begin(string), std::end(string),
                              [](CharType_ ch) { return !std::isprint(ch); }),
               std::end(string));
}
}

// This is technically a misnomer.  We are not looking for the Windows SDK but
// rather the Universal CRT SDK.
//
// The Windows SDK installation is found by querying:
//  HKLM\SOFTWARE\[Wow6432Node]\Microsoft\Microsoft SDKs\Windows\v10.0\InstallationFolder
// We can identify the SDK version using:
//  HKLM\SOFTWARE\[Wow6432Node]\Microsoft\Microsoft SDKs\Windows\v10.0\ProductVersion
//
// We currently only query:
//  HKLM\SOFTWARE\Microsoft\Windows Kits\Installed Roots\KitsRoot10
// which gives us the Universal CRT installation root.
//
// FIXME(compnerd) we should support additional installation configurations by
// also querying the HKCU hive.
namespace winsdk {
static const wchar_t kits_root_key[] = L"KitsRoot10";
static const wchar_t kits_installed_roots_keypath[] =
    L"SOFTWARE\\Microsoft\\Windows Kits\\Installed Roots";

std::filesystem::path install_root() noexcept {
  DWORD cbData = 0;

  if (FAILED(RegGetValueW(HKEY_LOCAL_MACHINE, kits_installed_roots_keypath,
                          kits_root_key, RRF_RT_REG_SZ, nullptr, nullptr,
                          &cbData)))
    return {};

  if (cbData == 0)
    return {};

  std::vector<wchar_t> buffer;
  buffer.resize(cbData);

  if (FAILED(RegGetValueW(HKEY_LOCAL_MACHINE, kits_installed_roots_keypath,
                          kits_root_key, RRF_RT_REG_SZ, nullptr, buffer.data(),
                          &cbData)))
    return {};

  return std::filesystem::path(buffer.data());
}

std::vector<std::wstring> available_versions() noexcept {
  HKEY hKey;

  if (FAILED(RegOpenKeyExW(HKEY_LOCAL_MACHINE, kits_installed_roots_keypath,
                           0, KEY_READ, &hKey)))
    return {};

  windows::raii::hkey key{hKey};

  DWORD cSubKeys;
  DWORD cbMaxSubKeyLen;
  if (FAILED(RegQueryInfoKeyW(hKey, nullptr, nullptr, nullptr, &cSubKeys,
                              &cbMaxSubKeyLen, nullptr, nullptr, nullptr,
                              nullptr, nullptr, nullptr)))
    return {};

  std::vector<wchar_t> buffer;
  buffer.resize(static_cast<size_t>(cbMaxSubKeyLen) + 1);

  std::vector<std::wstring> versions;
  for (DWORD dwIndex = 0; dwIndex < cSubKeys; ++dwIndex) {
    DWORD cchName = cbMaxSubKeyLen + 1;

    // TODO(compnerd) handle error
    (void)RegEnumKeyExW(hKey, dwIndex, buffer.data(), &cchName, nullptr,
                        nullptr, nullptr, nullptr);

    versions.emplace_back(buffer.data());
  }
  return versions;
}
}

namespace msvc {
// Current Build Tools
static const wchar_t toolset_current_x86_x64[] =
    L"Microsoft.VisualStudio.Component.VC.Tools.x86.x64";
static const wchar_t toolset_current_arm[] =
    L"Microsoft.VisualStudio.Component.VC.Tools.ARM";
static const wchar_t toolset_current_arm64[] =
    L"Microsoft.VisualStudio.Component.VC.Tools.ARM64";
static const wchar_t toolset_current_arm64ec[] =
    L"Microsoft.VisualStudio.Component.VC.Tools.ARM64EC";

// VS2022 v143 Build Tools
static const wchar_t toolset_v143_x86_x64[] =
    L"Microsoft.VisualStudio.Component.VC.14.30.17.0.x86.x64";
static const wchar_t toolset_v143_arm[] =
    L"Microsoft.VisualStudio.Component.VC.14.30.17.0.ARM";
static const wchar_t toolset_v143_arm64[] =
    L"Microsoft.VisualStudio.Component.VC.14.30.17.0.ARM64";

// VS2019 v142 Build Tools
static const wchar_t toolset_v142_x86_x64[] =
    L"Microsoft.VisualStudio.Component.VC.14.29.16.11.x86.x64";
static const wchar_t toolset_v142_arm[] =
    L"Microsoft.VisualStudio.Component.VC.14.29.16.11.ARM";
static const wchar_t toolset_v142_arm64[] =
    L"Microsoft.VisualStudio.Component.VC.14.29.16.11.ARM64";

// VS2017 v141 Build Tools
static const wchar_t toolset_v141_x86_x64[] =
    L"Microsoft.VisualStudio.Component.VC.v141.x86.x64";
static const wchar_t toolset_v141_arm[] =
    L"Microsoft.VisualStudio.Component.VC.v141.ARM";
static const wchar_t toolset_v141_arm64[] =
    L"Microsoft.VisualStudio.Component.VC.v141.ARM64";

static const wchar_t *known_toolsets[] = {
  toolset_current_x86_x64,
  toolset_current_arm64ec,
  toolset_current_arm64,
  toolset_current_arm,

  toolset_v143_x86_x64,
  toolset_v143_arm64,
  toolset_v143_arm,

  toolset_v142_x86_x64,
  toolset_v142_arm64,
  toolset_v142_arm,

  toolset_v141_x86_x64,
  toolset_v141_arm64,
  toolset_v141_arm,
};

// The name is misleading.  This currently returns the default toolset in all
// VS2015+ installations.
std::vector<std::filesystem::path> available_toolsets() noexcept {
  windows::raii::com_initializer com;

  std::vector<std::filesystem::path> toolsets;

  ISetupConfigurationPtr configuration;
  if (FAILED(configuration.CreateInstance(__uuidof(SetupConfiguration))))
    return toolsets;

  ISetupConfiguration2Ptr configuration2;
  if (FAILED(configuration->QueryInterface(&configuration2)))
    return toolsets;

  IEnumSetupInstancesPtr instances;
  if (FAILED(configuration2->EnumAllInstances(&instances)))
    return toolsets;

  ULONG fetched;
  ISetupInstancePtr instance;
  while (SUCCEEDED(instances->Next(1, &instance, &fetched)) && fetched) {
    ISetupInstance2Ptr instance2;
    if (FAILED(instance->QueryInterface(&instance2)))
      continue;

    InstanceState state;
    if (FAILED(instance2->GetState(&state)))
      continue;

    // Ensure that the instance state matches
    //  eLocal: The instance installation path exists.
    //  eRegistered: A product is registered to the instance.
    if (~state & eLocal or ~state & eRegistered)
      continue;

    LPSAFEARRAY packages;
    if (FAILED(instance2->GetPackages(&packages)))
      continue;

    LONG lower, upper;
    if (FAILED(SafeArrayGetLBound(packages, 1, &lower)) ||
        FAILED(SafeArrayGetUBound(packages, 1, &upper)))
      continue;

    for (LONG index = 0, count = upper - lower + 1; index < count; ++index) {
      IUnknownPtr element;
      if (FAILED(SafeArrayGetElement(packages, &index, &element)))
        continue;

      ISetupPackageReferencePtr package;
      if (FAILED(element->QueryInterface(&package)))
        continue;

      _bstr_t package_id;
      if (FAILED(package->GetId(package_id.GetAddress())))
        continue;

      // Ensure that we are dealing with a (known) MSVC ToolSet
      if (std::none_of(std::begin(known_toolsets), std::end(known_toolsets),
                      [package_id = package_id.GetBSTR()](const wchar_t *id) {
                        return wcscmp(package_id, id) == 0;
                      }))
        continue;

      _bstr_t VSInstallDir;
      if (FAILED(instance2->GetInstallationPath(VSInstallDir.GetAddress())))
        continue;

      std::filesystem::path VCInstallDir{static_cast<wchar_t *>(VSInstallDir)};
      VCInstallDir.append("VC");

      std::string VCToolsVersion;

      // VSInstallDir\VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt
      // contains the default version of the v141 MSVC toolset on VS2019. Prefer
      // to use
      // VSInstallDir\VC\Auxiliary\Build\Microsoft.VCToolsVersion.v142.default.txt
      // which contains the default v142 version of the toolset on VS2019.
      for (const auto &file : {"Microsoft.VCToolsVersion.v143.default.txt",
                               "Microsoft.VCToolsVersion.v142.default.txt",
                               "Microsoft.VCToolsVersion.v141.default.txt",
                               "Microsoft.VCToolsVersion.default.txt"}) {
        std::filesystem::path path{VCInstallDir};
        path.append("Auxiliary");
        path.append("Build");
        path.append(file);

        VCToolsVersion = contents(path);
        // Strip any line ending characters from the contents of the file.
        trim(VCToolsVersion);
        if (!VCToolsVersion.empty())
          break;
      }

      if (VCToolsVersion.empty())
        continue;

      std::filesystem::path VCToolsInstallDir{VCInstallDir};
      VCToolsInstallDir.append("Tools");
      VCToolsInstallDir.append("MSVC");
      VCToolsInstallDir.append(VCToolsVersion);

      // FIXME(compnerd) should we actually just walk the directory structure
      // instead and populate all the toolsets?  That would match roughly what
      // we do with the UCRT currently.

      toolsets.push_back(VCToolsInstallDir);
    }
  }

  return toolsets;
}
}

namespace msi {
std::wstring get_property(MSIHANDLE hInstall, std::wstring_view key) noexcept {
  DWORD size = 0;
  UINT status;

  status = MsiGetPropertyW(hInstall, key.data(), L"", &size);
  assert(status == ERROR_MORE_DATA);

  std::vector<wchar_t> buffer;
  buffer.resize(size + 1);

  size = buffer.capacity();
  status = MsiGetPropertyW(hInstall, key.data(), buffer.data(), &size);
  // TODO(compnerd) handle error

  return {buffer.data(), buffer.capacity()};
}
}

namespace {
bool replace_file(const std::filesystem::path &source,
                  const std::filesystem::path &destination,
                  std::error_code &error_code) {
  static const constexpr std::filesystem::copy_options options =
      std::filesystem::copy_options::overwrite_existing;

  // Copy file to ensure that we are on the same volume.
  UUID uuid;
  RPC_WSTR id;
  RPC_STATUS status;
  if (RPC_STATUS status = UuidCreate(&uuid); status == RPC_S_OK) {
  } else {
    error_code = std::error_code(status, std::system_category());
    return false;
  }

  if (RPC_STATUS status = UuidToStringW(&uuid, &id); status == RPC_S_OK) {
  } else {
    error_code = std::error_code(status, std::system_category());
    return false;
  }

  std::filesystem::path temp =
      std::filesystem::path(destination)
          .replace_filename(std::wstring(reinterpret_cast<LPCWSTR>(id)));

  RpcStringFreeW(&id);

  if (!std::filesystem::copy_file(source, temp, options, error_code))
    return false;

  // Rename file.
  HANDLE hFile = CreateFileW(temp.wstring().c_str(),
                             GENERIC_READ | DELETE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                             nullptr,
                             OPEN_EXISTING,
                             FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_POSIX_SEMANTICS,
                             nullptr);
  if (hFile == INVALID_HANDLE_VALUE) {
    error_code = std::error_code(GetLastError(), std::system_category());
    return false;
  }

  windows::raii::handle handle(hFile);

  std::wstring path = destination.wstring();
  std::vector<char> buffer(sizeof(FILE_RENAME_INFO) - sizeof(wchar_t) +
                           (path.size() * sizeof(std::wstring::value_type)));
  FILE_RENAME_INFO &rename_info =
      *reinterpret_cast<FILE_RENAME_INFO *>(buffer.data());
  rename_info.Flags = FILE_RENAME_FLAG_POSIX_SEMANTICS
                    | FILE_RENAME_FLAG_REPLACE_IF_EXISTS;
  rename_info.RootDirectory = 0;
  rename_info.FileNameLength = path.size() * sizeof(std::wstring::value_type);
  std::copy(path.begin(), path.end(), &rename_info.FileName[0]);

  SetLastError(ERROR_SUCCESS);
  if (!SetFileInformationByHandle(hFile, FileRenameInfoEx, &rename_info,
                                  buffer.size())) {
    error_code = std::error_code(GetLastError(), std::system_category());
    return false;
  }

  return true;
}
}

UINT SwiftInstaller_InstallAuxiliaryFiles(MSIHANDLE hInstall) {
  std::wstring data = msi::get_property(hInstall, L"CustomActionData");
  trim(data);

  std::filesystem::path SDKROOT{data};
  LOG(hInstall, info) << "SDKROOT: " << SDKROOT.string();

  // Copy SDK Module Maps
  std::filesystem::path UniversalCRTSdkDir = winsdk::install_root();
  LOG(hInstall, info) << "UniversalCRTSdkDir: " << UniversalCRTSdkDir;
  if (!UniversalCRTSdkDir.empty()) {
    // FIXME(compnerd) Technically we are using the UniversalCRTSdkDir here
    // instead of the WindowsSdkDir which would contain `um`.

    // FIXME(compnerd) we may end up in a state where the ucrt and Windows SDKs
    // do not match.  Users have reported cases where they somehow managed to
    // setup such a configuration.  We should split this up to explicitly
    // handle the UCRT and WinSDK paths separately.
    for (const auto &version : winsdk::available_versions()) {
      const struct {
        std::filesystem::path src;
        std::filesystem::path dst;
      } items[] = {
        { SDKROOT / "usr" / "share" / "ucrt.modulemap",
          UniversalCRTSdkDir / "Include" / version / "ucrt" / "module.modulemap" },
        { SDKROOT / "usr" / "share" / "winsdk.modulemap",
          UniversalCRTSdkDir / "Include" / version / "um" / "module.modulemap" },
      };

      for (const auto &item : items) {
        std::error_code ec;
        if (!replace_file(item.src, item.dst, ec)) {
          LOG(hInstall, error)
              << "unable to copy " << item.src << " to " << item.dst << ": "
              << ec.message();
          continue;
        }
        LOG(hInstall, info) << "Deployed " << item.dst;
      }
    }
  }

  // Copy MSVC Tools Module Maps
  for (const auto &VCToolsInstallDir : msvc::available_toolsets()) {
    const struct {
      std::filesystem::path src;
      std::filesystem::path dst;
    } items[] = {
      { SDKROOT / "usr" / "share" / "vcruntime.modulemap",
        VCToolsInstallDir / "include" / "module.modulemap" },
      { SDKROOT / "usr" / "share" / "vcruntime.apinotes",
        VCToolsInstallDir / "include" / "vcruntime.apinotes" },
    };

    for (const auto &item : items) {
      std::error_code ec;
      if (!replace_file(item.src, item.dst, ec)) {
        LOG(hInstall, error)
            << "unable to copy " << item.src << " to " << item.dst << ": "
            << ec.message();
        continue;
      }
      LOG(hInstall, info) << "Deployed " << item.dst;
    }
  }

  // TODO(compnerd) it would be ideal to record the files deployed here to the
  // `RemoveFile` table which would allow them to be cleaned up on removal.
  // This is tricky as we cannot modify the on-disk database.  The deferred
  // action is already executed post-InstallExecute which means that we can now
  // identify the cached MSI by:
  //
  //  std::wstring product_code = msi::get_property(hInstall, L"ProductCode");
  //
  //  DWORD size = 0;
  //  (void)MsiGetProductInfoW(product_code.c_str(),
  //                           INSTALLPROPERTY_LOCALPACKAGE, L"", &size);
  //  std::vector<wchar_t> buffer;
  //  buffer.resize(++size);
  //  (void)MsiGetProductInfoW(product_code.c_str(),
  //                           INSTALLPROPERTY_LOCALPACKAGE, buffer.data(),
  //                           &size);
  //
  // We can then create a new property for the location of the entry and a new
  // RemoveFile entry for cleaning up the file.  Note that we may have to tweak
  // things to get repairs to work properly with the tracking.
  //
  //  PMSIHANDLE database;
  //  (void)MsiOpenDatabaseW(buffer.data(), MSIDBOPEN_TRANSACT, &database);
  //
  //  static const wchar_t query[] =
  //      LR"SQL(
  //INSERT INTO `Property` (`Property`, `Value`)
  //  VALUES(?, ?);
  //INSERT INTO `RemoveFile` (`FileKey`, `Component_`, `FileName`, `DirProperty`, `InstallMode`
  //  VALUES (?, ?, ?, ?, ?);
  //      )SQL";
  //
  //  PMSIHANDLE view;
  //  (void)MsiDatabaseOpenViewW(database, query, &view);
  //
  //  std::hash<std::wstring> hasher;
  //
  //  std::wostringstream component;
  //  component << "cmp" << hasher(path.wstring());
  //
  //  std::wostringstream property;
  //  property << "prop" << hasher(path.wstring());
  //
  //  PMSIHANDLE record = MsiRecordCreate(7);
  //  (void)MsiRecordSetStringW(record, 1, property.str().c_str());
  //  (void)MsiRecordSetStringW(record, 2, path.parent_path().wstring().c_str());
  //  (void)MsiRecordSetStringW(record, 3, component.str().c_str());
  //  (void)MsiRecordSetStringW(record, 4, component.str().c_str());
  //  (void)MsiRecordSetStringW(record, 5, path.filename().wstring().c_str());
  //  (void)MsiRecordSetStringW(record, 6, property.str().c_str());
  //  (void)MsiRecordSetInteger(record, 7, 2);
  //
  //  (void)MsiViewExecute(view, record);
  //
  //  (void)MsiDatabaseCommit(database);
  //
  // Currently, this seems to fail with the commiting of the database, which is
  // still a mystery to me.  This relies on the clever usage of the
  // `EnsureTable` in the WiX definition to ensure that we do not need to create
  // the table.
  //
  // The error handling has been elided here for brevity's sake.

  return ERROR_SUCCESS;
}
