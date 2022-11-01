//===-- PlatformMacOSX.h ----------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_PlatformMacOSX_h_
#define liblldb_PlatformMacOSX_h_

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "PlatformDarwin.h"

class PlatformMacOSX : public PlatformDarwin
{
public:

    //------------------------------------------------------------
    // Class functions
    //------------------------------------------------------------
    static lldb_private::Platform* 
    CreateInstance (bool force, const lldb_private::ArchSpec *arch);

    static void
    Initialize ();

    static void
    Terminate ();
    
    static lldb_private::ConstString
    GetPluginNameStatic (bool is_host);

    static const char *
    GetDescriptionStatic(bool is_host);
    
    //------------------------------------------------------------
    // Class Methods
    //------------------------------------------------------------
    PlatformMacOSX (bool is_host);

    virtual
    ~PlatformMacOSX();

    //------------------------------------------------------------
    // lldb_private::PluginInterface functions
    //------------------------------------------------------------
    virtual lldb_private::ConstString
    GetPluginName()
    {
        return GetPluginNameStatic (IsHost());
    }
    
    virtual uint32_t
    GetPluginVersion()
    {
        return 1;
    }
    
    virtual lldb_private::Error
    GetSharedModule (const lldb_private::ModuleSpec &module_spec,
                     lldb::ModuleSP &module_sp,
                     const lldb_private::FileSpecList *module_search_paths_ptr,
                     lldb::ModuleSP *old_module_sp_ptr,
                     bool *did_create_ptr);
    
    virtual const char *
    GetDescription ()
    {
        return GetDescriptionStatic (IsHost());
    }

    lldb_private::Error
    GetSymbolFile (const lldb_private::FileSpec &platform_file, 
                   const lldb_private::UUID *uuid_ptr,
                   lldb_private::FileSpec &local_file);
    
    virtual lldb_private::Error
    GetFile (const lldb_private::FileSpec& source,
             const lldb_private::FileSpec& destination)
    {
        return PlatformDarwin::GetFile (source,destination);
    }
    
    virtual lldb_private::Error
    GetFile (const lldb_private::FileSpec &platform_file, 
             const lldb_private::UUID *uuid_ptr,
             lldb_private::FileSpec &local_file);
    
    virtual bool
    GetSupportedArchitectureAtIndex (uint32_t idx, 
                                     lldb_private::ArchSpec &arch);

    virtual lldb_private::ConstString
    GetSDKDirectory (lldb_private::Target &target);

private:
    DISALLOW_COPY_AND_ASSIGN (PlatformMacOSX);

};

#endif  // liblldb_PlatformMacOSX_h_
