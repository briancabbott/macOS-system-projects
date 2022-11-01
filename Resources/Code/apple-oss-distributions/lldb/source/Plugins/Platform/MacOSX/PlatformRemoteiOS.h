//===-- PlatformRemoteiOS.h ----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_PlatformRemoteiOS_h_
#define liblldb_PlatformRemoteiOS_h_

// C Includes
// C++ Includes
// Other libraries and framework includes
#include "lldb/Host/FileSpec.h"

// Project includes
#include "PlatformDarwin.h"

class PlatformRemoteiOS : public PlatformDarwin
{
public:

    //------------------------------------------------------------
    // Class Functions
    //------------------------------------------------------------
    static lldb_private::Platform* 
    CreateInstance (bool force, const lldb_private::ArchSpec *arch);

    static void
    Initialize ();

    static void
    Terminate ();
    
    static lldb_private::ConstString
    GetPluginNameStatic ();

    static const char *
    GetDescriptionStatic();
    
    //------------------------------------------------------------
    // Class Methods
    //------------------------------------------------------------
    PlatformRemoteiOS ();

    virtual
    ~PlatformRemoteiOS();

    //------------------------------------------------------------
    // lldb_private::PluginInterface functions
    //------------------------------------------------------------
    virtual lldb_private::ConstString
    GetPluginName()
    {
        return GetPluginNameStatic();
    }
    
    virtual uint32_t
    GetPluginVersion()
    {
        return 1;
    }

    //------------------------------------------------------------
    // lldb_private::Platform functions
    //------------------------------------------------------------
    virtual lldb_private::Error
    ResolveExecutable (const lldb_private::FileSpec &exe_file,
                       const lldb_private::ArchSpec &arch,
                       lldb::ModuleSP &module_sp,
                       const lldb_private::FileSpecList *module_search_paths_ptr);

    virtual const char *
    GetDescription ()
    {
        return GetDescriptionStatic();
    }

    virtual void
    GetStatus (lldb_private::Stream &strm);

    virtual lldb_private::Error
    GetSymbolFile (const lldb_private::FileSpec &platform_file, 
                   const lldb_private::UUID *uuid_ptr,
                   lldb_private::FileSpec &local_file);

    virtual lldb_private::Error
    GetSharedModule (const lldb_private::ModuleSpec &module_spec,
                     lldb::ModuleSP &module_sp,
                     const lldb_private::FileSpecList *module_search_paths_ptr,
                     lldb::ModuleSP *old_module_sp_ptr,
                     bool *did_create_ptr);

    virtual bool
    GetSupportedArchitectureAtIndex (uint32_t idx, 
                                     lldb_private::ArchSpec &arch);

protected:
    struct SDKDirectoryInfo
    {
        SDKDirectoryInfo (const lldb_private::FileSpec &sdk_dir_spec);
        lldb_private::FileSpec directory;
        lldb_private::ConstString build;
        uint32_t version_major;
        uint32_t version_minor;
        uint32_t version_update;
        bool user_cached;
    };
    typedef std::vector<SDKDirectoryInfo> SDKDirectoryInfoCollection;
    SDKDirectoryInfoCollection m_sdk_directory_infos;
    std::string m_device_support_directory;
    std::string m_device_support_directory_for_os_version;
    std::string m_build_update;
    uint32_t m_last_module_sdk_idx;

    bool
    UpdateSDKDirectoryInfosInNeeded();

    const char *
    GetDeviceSupportDirectory();

    const char *
    GetDeviceSupportDirectoryForOSVersion();

    const SDKDirectoryInfo *
    GetSDKDirectoryForLatestOSVersion ();

    const SDKDirectoryInfo *
    GetSDKDirectoryForCurrentOSVersion ();

    static lldb_private::FileSpec::EnumerateDirectoryResult
    GetContainedFilesIntoVectorOfStringsCallback (void *baton,
                                                  lldb_private::FileSpec::FileType file_type,
                                                  const lldb_private::FileSpec &file_spec);

    uint32_t
    FindFileInAllSDKs (const char *platform_file_path,
                       lldb_private::FileSpecList &file_list);

    bool
    GetFileInSDK (const char *platform_file_path,
                  uint32_t sdk_idx,
                  lldb_private::FileSpec &local_file);

    bool
    GetFileInSDKRoot (const char *platform_file_path,
                      const char *sdkroot_path,
                      bool symbols_dirs_only,
                      lldb_private::FileSpec &local_file);

    uint32_t
    FindFileInAllSDKs (const lldb_private::FileSpec &platform_file,
                       lldb_private::FileSpecList &file_list);

private:
    DISALLOW_COPY_AND_ASSIGN (PlatformRemoteiOS);

};

#endif  // liblldb_PlatformRemoteiOS_h_
