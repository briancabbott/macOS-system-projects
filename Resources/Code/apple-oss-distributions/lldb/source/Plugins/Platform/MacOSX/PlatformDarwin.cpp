//===-- PlatformDarwin.cpp --------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "lldb/lldb-python.h"

#include "PlatformDarwin.h"

// C Includes
// C++ Includes
// Other libraries and framework includes
// Project includes
#include "lldb/Breakpoint/BreakpointLocation.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/Error.h"
#include "lldb/Core/Log.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/ModuleSpec.h"
#include "lldb/Core/Timer.h"
#include "lldb/Host/Host.h"
#include "lldb/Host/Symbols.h"
#include "lldb/Symbol/ObjectFile.h"
#include "lldb/Symbol/SymbolFile.h"
#include "lldb/Symbol/SymbolVendor.h"
#include "lldb/Target/Target.h"

using namespace lldb;
using namespace lldb_private;
    

//------------------------------------------------------------------
/// Default Constructor
//------------------------------------------------------------------
PlatformDarwin::PlatformDarwin (bool is_host) :
    PlatformPOSIX(is_host),  // This is the local host platform
    m_developer_directory (),
    m_dispatch_queue_offsets_addr (LLDB_INVALID_ADDRESS),
    m_libdispatch_offsets()
{
}

//------------------------------------------------------------------
/// Destructor.
///
/// The destructor is virtual since this class is designed to be
/// inherited from by the plug-in instance.
//------------------------------------------------------------------
PlatformDarwin::~PlatformDarwin()
{
}

FileSpecList
PlatformDarwin::LocateExecutableScriptingResources (Target *target,
                                                    Module &module)
{
    FileSpecList file_list;
    if (target && target->GetDebugger().GetScriptLanguage() == eScriptLanguagePython)
    {
        // NB some extensions might be meaningful and should not be stripped - "this.binary.file"
        // should not lose ".file" but GetFileNameStrippingExtension() will do precisely that.
        // Ideally, we should have a per-platform list of extensions (".exe", ".app", ".dSYM", ".framework")
        // which should be stripped while leaving "this.binary.file" as-is.
        FileSpec module_spec = module.GetFileSpec();
        
        if (module_spec)
        {
            SymbolVendor *symbols = module.GetSymbolVendor ();
            if (symbols)
            {
                SymbolFile *symfile = symbols->GetSymbolFile();
                if (symfile)
                {
                    ObjectFile *objfile = symfile->GetObjectFile();
                    if (objfile)
                    {
                        FileSpec symfile_spec (objfile->GetFileSpec());
                        if (symfile_spec && symfile_spec.Exists())
                        {
                            while (module_spec.GetFilename())
                            {
                                std::string module_basename (module_spec.GetFilename().GetCString());

                                // FIXME: for Python, we cannot allow certain characters in module
                                // filenames we import. Theoretically, different scripting languages may
                                // have different sets of forbidden tokens in filenames, and that should
                                // be dealt with by each ScriptInterpreter. For now, we just replace dots
                                // with underscores, but if we ever support anything other than Python
                                // we will need to rework this
                                std::replace(module_basename.begin(), module_basename.end(), '.', '_');
                                std::replace(module_basename.begin(), module_basename.end(), ' ', '_');
                                std::replace(module_basename.begin(), module_basename.end(), '-', '_');
                                

                                StreamString path_string;
                                // for OSX we are going to be in .dSYM/Contents/Resources/DWARF/<basename>
                                // let us go to .dSYM/Contents/Resources/Python/<basename>.py and see if the file exists
                                path_string.Printf("%s/../Python/%s.py",symfile_spec.GetDirectory().GetCString(), module_basename.c_str());
                                FileSpec script_fspec(path_string.GetData(), true);
                                if (script_fspec.Exists())
                                {
                                    file_list.Append (script_fspec);
                                    break;
                                }
                                
                                // If we didn't find the python file, then keep
                                // stripping the extensions and try again
                                ConstString filename_no_extension (module_spec.GetFileNameStrippingExtension());
                                if (module_spec.GetFilename() == filename_no_extension)
                                    break;
                                
                                module_spec.GetFilename() = filename_no_extension;
                            }
                        }
                    }
                }
            }
        }
    }
    return file_list;
}

Error
PlatformDarwin::ResolveExecutable (const FileSpec &exe_file,
                                   const ArchSpec &exe_arch,
                                   lldb::ModuleSP &exe_module_sp,
                                   const FileSpecList *module_search_paths_ptr)
{
    Error error;
    // Nothing special to do here, just use the actual file and architecture

    char exe_path[PATH_MAX];
    FileSpec resolved_exe_file (exe_file);
    
    if (IsHost())
    {
        // If we have "ls" as the exe_file, resolve the executable loation based on
        // the current path variables
        if (!resolved_exe_file.Exists())
        {
            exe_file.GetPath (exe_path, sizeof(exe_path));
            resolved_exe_file.SetFile(exe_path, true);
        }

        if (!resolved_exe_file.Exists())
            resolved_exe_file.ResolveExecutableLocation ();

        // Resolve any executable within a bundle on MacOSX
        Host::ResolveExecutableInBundle (resolved_exe_file);
        
        if (resolved_exe_file.Exists())
            error.Clear();
        else
        {
            exe_file.GetPath (exe_path, sizeof(exe_path));
            error.SetErrorStringWithFormat ("unable to find executable for '%s'", exe_path);
        }
    }
    else
    {
        if (m_remote_platform_sp)
        {
            error = m_remote_platform_sp->ResolveExecutable (exe_file, 
                                                             exe_arch,
                                                             exe_module_sp,
                                                             module_search_paths_ptr);
        }
        else
        {
            // We may connect to a process and use the provided executable (Don't use local $PATH).

            // Resolve any executable within a bundle on MacOSX
            Host::ResolveExecutableInBundle (resolved_exe_file);

            if (resolved_exe_file.Exists())
                error.Clear();
            else
                error.SetErrorStringWithFormat("the platform is not currently connected, and '%s' doesn't exist in the system root.", resolved_exe_file.GetFilename().AsCString(""));
        }
    }
    

    if (error.Success())
    {
        ModuleSpec module_spec (resolved_exe_file, exe_arch);
        if (module_spec.GetArchitecture().IsValid())
        {
            error = ModuleList::GetSharedModule (module_spec, 
                                                 exe_module_sp, 
                                                 module_search_paths_ptr,
                                                 NULL, 
                                                 NULL);
        
            if (error.Fail() || exe_module_sp.get() == NULL || exe_module_sp->GetObjectFile() == NULL)
            {
                exe_module_sp.reset();
                error.SetErrorStringWithFormat ("'%s' doesn't contain the architecture %s",
                                                exe_file.GetPath().c_str(),
                                                exe_arch.GetArchitectureName());
            }
        }
        else
        {
            // No valid architecture was specified, ask the platform for
            // the architectures that we should be using (in the correct order)
            // and see if we can find a match that way
            StreamString arch_names;
            for (uint32_t idx = 0; GetSupportedArchitectureAtIndex (idx, module_spec.GetArchitecture()); ++idx)
            {
                error = GetSharedModule (module_spec, 
                                         exe_module_sp, 
                                         module_search_paths_ptr,
                                         NULL, 
                                         NULL);
                // Did we find an executable using one of the 
                if (error.Success())
                {
                    if (exe_module_sp && exe_module_sp->GetObjectFile())
                        break;
                    else
                        error.SetErrorToGenericError();
                }
                
                if (idx > 0)
                    arch_names.PutCString (", ");
                arch_names.PutCString (module_spec.GetArchitecture().GetArchitectureName());
            }
            
            if (error.Fail() || !exe_module_sp)
            {
                error.SetErrorStringWithFormat ("'%s' doesn't contain any '%s' platform architectures: %s",
                                                exe_file.GetPath().c_str(),
                                                GetPluginName().GetCString(),
                                                arch_names.GetString().c_str());
            }
        }
    }

    return error;
}

Error
PlatformDarwin::ResolveSymbolFile (Target &target,
                                   const ModuleSpec &sym_spec,
                                   FileSpec &sym_file)
{
    Error error;
    sym_file = sym_spec.GetSymbolFileSpec();
    if (sym_file.Exists())
    {
        if (sym_file.GetFileType() == FileSpec::eFileTypeDirectory)
        {
            sym_file = Symbols::FindSymbolFileInBundle (sym_file,
                                                        sym_spec.GetUUIDPtr(),
                                                        sym_spec.GetArchitecturePtr());
        }
    }
    else
    {
        if (sym_spec.GetUUID().IsValid())
        {
            
        }
    }
    return error;
    
}

static lldb_private::Error
MakeCacheFolderForFile (const FileSpec& module_cache_spec)
{
    FileSpec module_cache_folder = module_cache_spec.CopyByRemovingLastPathComponent();
    return Host::MakeDirectory(module_cache_folder.GetPath().c_str(), eFilePermissionsDirectoryDefault);
}

static lldb_private::Error
BringInRemoteFile (Platform* platform,
                   const lldb_private::ModuleSpec &module_spec,
                   const FileSpec& module_cache_spec)
{
    MakeCacheFolderForFile(module_cache_spec);
    Error err = platform->GetFile(module_spec.GetFileSpec(), module_cache_spec);
    return err;
}

lldb_private::Error
PlatformDarwin::GetSharedModuleWithLocalCache (const lldb_private::ModuleSpec &module_spec,
                                               lldb::ModuleSP &module_sp,
                                               const lldb_private::FileSpecList *module_search_paths_ptr,
                                               lldb::ModuleSP *old_module_sp_ptr,
                                               bool *did_create_ptr)
{

    Log *log(GetLogIfAnyCategoriesSet(LIBLLDB_LOG_PLATFORM));
    if (log)
        log->Printf("[%s] Trying to find module %s/%s - platform path %s/%s symbol path %s/%s",
                     (IsHost() ? "host" : "remote"),
                     module_spec.GetFileSpec().GetDirectory().AsCString(),
                     module_spec.GetFileSpec().GetFilename().AsCString(),
                     module_spec.GetPlatformFileSpec().GetDirectory().AsCString(),
                     module_spec.GetPlatformFileSpec().GetFilename().AsCString(),
                     module_spec.GetSymbolFileSpec().GetDirectory().AsCString(),
                     module_spec.GetSymbolFileSpec().GetFilename().AsCString());

    std::string cache_path(GetLocalCacheDirectory());
    std::string module_path (module_spec.GetFileSpec().GetPath());
    cache_path.append(module_path);
    FileSpec module_cache_spec(cache_path.c_str(),false);
    
    // if rsync is supported, always bring in the file - rsync will be very efficient
    // when files are the same on the local and remote end of the connection
    if (this->GetSupportsRSync())
    {
        Error err = BringInRemoteFile (this, module_spec, module_cache_spec);
        if (err.Fail())
            return err;
        if (module_cache_spec.Exists())
        {
            Log *log(GetLogIfAnyCategoriesSet(LIBLLDB_LOG_PLATFORM));
            if (log)
                log->Printf("[%s] module %s/%s was rsynced and is now there",
                             (IsHost() ? "host" : "remote"),
                             module_spec.GetFileSpec().GetDirectory().AsCString(),
                             module_spec.GetFileSpec().GetFilename().AsCString());
            ModuleSpec local_spec(module_cache_spec, module_spec.GetArchitecture());
            module_sp.reset(new Module(local_spec));
            module_sp->SetPlatformFileSpec(module_spec.GetFileSpec());
            return Error();
        }
    }

    if (module_spec.GetFileSpec().Exists() && !module_sp)
    {
        module_sp.reset(new Module(module_spec));
        return Error();
    }
    
    // try to find the module in the cache
    if (module_cache_spec.Exists())
    {
        // get the local and remote MD5 and compare
        if (m_remote_platform_sp)
        {
            // when going over the *slow* GDB remote transfer mechanism we first check
            // the hashes of the files - and only do the actual transfer if they differ
            uint64_t high_local,high_remote,low_local,low_remote;
            Host::CalculateMD5 (module_cache_spec, low_local, high_local);
            m_remote_platform_sp->CalculateMD5(module_spec.GetFileSpec(), low_remote, high_remote);
            if (low_local != low_remote || high_local != high_remote)
            {
                // bring in the remote file
                Log *log(GetLogIfAnyCategoriesSet(LIBLLDB_LOG_PLATFORM));
                if (log)
                    log->Printf("[%s] module %s/%s needs to be replaced from remote copy",
                                 (IsHost() ? "host" : "remote"),
                                 module_spec.GetFileSpec().GetDirectory().AsCString(),
                                 module_spec.GetFileSpec().GetFilename().AsCString());
                Error err = BringInRemoteFile (this, module_spec, module_cache_spec);
                if (err.Fail())
                    return err;
            }
        }
        
        ModuleSpec local_spec(module_cache_spec, module_spec.GetArchitecture());
        module_sp.reset(new Module(local_spec));
        module_sp->SetPlatformFileSpec(module_spec.GetFileSpec());
        Log *log(GetLogIfAnyCategoriesSet(LIBLLDB_LOG_PLATFORM));
            if (log)
                log->Printf("[%s] module %s/%s was found in the cache",
                             (IsHost() ? "host" : "remote"),
                             module_spec.GetFileSpec().GetDirectory().AsCString(),
                             module_spec.GetFileSpec().GetFilename().AsCString());
        return Error();
    }
    
    // bring in the remote module file
    if (log)
        log->Printf("[%s] module %s/%s needs to come in remotely",
                     (IsHost() ? "host" : "remote"),
                     module_spec.GetFileSpec().GetDirectory().AsCString(),
                     module_spec.GetFileSpec().GetFilename().AsCString());
    Error err = BringInRemoteFile (this, module_spec, module_cache_spec);
    if (err.Fail())
        return err;
    if (module_cache_spec.Exists())
    {
        Log *log(GetLogIfAnyCategoriesSet(LIBLLDB_LOG_PLATFORM));
        if (log)
            log->Printf("[%s] module %s/%s is now cached and fine",
                         (IsHost() ? "host" : "remote"),
                         module_spec.GetFileSpec().GetDirectory().AsCString(),
                         module_spec.GetFileSpec().GetFilename().AsCString());
        ModuleSpec local_spec(module_cache_spec, module_spec.GetArchitecture());
        module_sp.reset(new Module(local_spec));
        module_sp->SetPlatformFileSpec(module_spec.GetFileSpec());
        return Error();
    }
    else
        return Error("unable to obtain valid module file");
}

Error
PlatformDarwin::GetSharedModule (const ModuleSpec &module_spec,
                                 ModuleSP &module_sp,
                                 const FileSpecList *module_search_paths_ptr,
                                 ModuleSP *old_module_sp_ptr,
                                 bool *did_create_ptr)
{
    Error error;
    module_sp.reset();
    
    if (IsRemote())
    {
        // If we have a remote platform always, let it try and locate
        // the shared module first.
        if (m_remote_platform_sp)
        {
            error = m_remote_platform_sp->GetSharedModule (module_spec,
                                                           module_sp,
                                                           module_search_paths_ptr,
                                                           old_module_sp_ptr,
                                                           did_create_ptr);
        }
    }
    
    if (!module_sp)
    {
        // Fall back to the local platform and find the file locally
        error = Platform::GetSharedModule (module_spec,
                                           module_sp,
                                           module_search_paths_ptr,
                                           old_module_sp_ptr,
                                           did_create_ptr);
        
        const FileSpec &platform_file = module_spec.GetFileSpec();
        if (!module_sp && module_search_paths_ptr && platform_file)
        {
            // We can try to pull off part of the file path up to the bundle
            // directory level and try any module search paths...
            FileSpec bundle_directory;
            if (Host::GetBundleDirectory (platform_file, bundle_directory))
            {
                if (platform_file == bundle_directory)
                {
                    ModuleSpec new_module_spec (module_spec);
                    new_module_spec.GetFileSpec() = bundle_directory;
                    if (Host::ResolveExecutableInBundle (new_module_spec.GetFileSpec()))
                    {
                        Error new_error (Platform::GetSharedModule (new_module_spec,
                                                                    module_sp,
                                                                    NULL,
                                                                    old_module_sp_ptr,
                                                                    did_create_ptr));
                        
                        if (module_sp)
                            return new_error;
                    }
                }
                else
                {
                    char platform_path[PATH_MAX];
                    char bundle_dir[PATH_MAX];
                    platform_file.GetPath (platform_path, sizeof(platform_path));
                    const size_t bundle_directory_len = bundle_directory.GetPath (bundle_dir, sizeof(bundle_dir));
                    char new_path[PATH_MAX];
                    size_t num_module_search_paths = module_search_paths_ptr->GetSize();
                    for (size_t i=0; i<num_module_search_paths; ++i)
                    {
                        const size_t search_path_len = module_search_paths_ptr->GetFileSpecAtIndex(i).GetPath(new_path, sizeof(new_path));
                        if (search_path_len < sizeof(new_path))
                        {
                            snprintf (new_path + search_path_len, sizeof(new_path) - search_path_len, "/%s", platform_path + bundle_directory_len);
                            FileSpec new_file_spec (new_path, false);
                            if (new_file_spec.Exists())
                            {
                                ModuleSpec new_module_spec (module_spec);
                                new_module_spec.GetFileSpec() = new_file_spec;
                                Error new_error (Platform::GetSharedModule (new_module_spec,
                                                                            module_sp,
                                                                            NULL,
                                                                            old_module_sp_ptr,
                                                                            did_create_ptr));
                                
                                if (module_sp)
                                {
                                    module_sp->SetPlatformFileSpec(new_file_spec);
                                    return new_error;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (module_sp)
        module_sp->SetPlatformFileSpec(module_spec.GetFileSpec());
    return error;
}

size_t
PlatformDarwin::GetSoftwareBreakpointTrapOpcode (Target &target, BreakpointSite *bp_site)
{
    const uint8_t *trap_opcode = NULL;
    uint32_t trap_opcode_size = 0;
    bool bp_is_thumb = false;
        
    llvm::Triple::ArchType machine = target.GetArchitecture().GetMachine();
    switch (machine)
    {
    case llvm::Triple::x86:
    case llvm::Triple::x86_64:
        {
            static const uint8_t g_i386_breakpoint_opcode[] = { 0xCC };
            trap_opcode = g_i386_breakpoint_opcode;
            trap_opcode_size = sizeof(g_i386_breakpoint_opcode);
        }
        break;

    case llvm::Triple::arm64:
        {
            // TODO: fix this with actual darwin breakpoint opcode for arm64.
            // right now debugging uses the Z packets with GDB remote so this
            // is not needed, but the size needs to be correct...
            static const uint8_t g_arm64_breakpoint_opcode[] = { 0xFE, 0xDE, 0xFF, 0xE7 };
            trap_opcode = g_arm64_breakpoint_opcode;
            trap_opcode_size = sizeof(g_arm64_breakpoint_opcode);
        }
        break;

    case llvm::Triple::thumb:
        bp_is_thumb = true; // Fall through...
    case llvm::Triple::arm:
        {
            static const uint8_t g_arm_breakpoint_opcode[] = { 0xFE, 0xDE, 0xFF, 0xE7 };
            static const uint8_t g_thumb_breakpooint_opcode[] = { 0xFE, 0xDE };

            // Auto detect arm/thumb if it wasn't explicitly specified
            if (!bp_is_thumb)
            {
                lldb::BreakpointLocationSP bp_loc_sp (bp_site->GetOwnerAtIndex (0));
                if (bp_loc_sp)
                    bp_is_thumb = bp_loc_sp->GetAddress().GetAddressClass () == eAddressClassCodeAlternateISA;
            }
            if (bp_is_thumb)
            {
                trap_opcode = g_thumb_breakpooint_opcode;
                trap_opcode_size = sizeof(g_thumb_breakpooint_opcode);
                break;
            }
            trap_opcode = g_arm_breakpoint_opcode;
            trap_opcode_size = sizeof(g_arm_breakpoint_opcode);
        }
        break;
        
    case llvm::Triple::ppc:
    case llvm::Triple::ppc64:
        {
            static const uint8_t g_ppc_breakpoint_opcode[] = { 0x7F, 0xC0, 0x00, 0x08 };
            trap_opcode = g_ppc_breakpoint_opcode;
            trap_opcode_size = sizeof(g_ppc_breakpoint_opcode);
        }
        break;
        
    default:
        assert(!"Unhandled architecture in PlatformDarwin::GetSoftwareBreakpointTrapOpcode()");
        break;
    }
    
    if (trap_opcode && trap_opcode_size)
    {
        if (bp_site->SetTrapOpcode(trap_opcode, trap_opcode_size))
            return trap_opcode_size;
    }
    return 0;

}

bool
PlatformDarwin::GetRemoteOSVersion ()
{
    if (m_remote_platform_sp)
        return m_remote_platform_sp->GetOSVersion (m_major_os_version, 
                                                   m_minor_os_version, 
                                                   m_update_os_version);
    return false;
}

bool
PlatformDarwin::GetRemoteOSBuildString (std::string &s)
{
    if (m_remote_platform_sp)
        return m_remote_platform_sp->GetRemoteOSBuildString (s);
    s.clear();
    return false;
}

bool
PlatformDarwin::GetRemoteOSKernelDescription (std::string &s)
{
    if (m_remote_platform_sp)
        return m_remote_platform_sp->GetRemoteOSKernelDescription (s);
    s.clear();
    return false;
}

// Remote Platform subclasses need to override this function
ArchSpec
PlatformDarwin::GetRemoteSystemArchitecture ()
{
    if (m_remote_platform_sp)
        return m_remote_platform_sp->GetRemoteSystemArchitecture ();
    return ArchSpec();
}


const char *
PlatformDarwin::GetHostname ()
{
    if (IsHost())
        return Platform::GetHostname();

    if (m_remote_platform_sp)
        return m_remote_platform_sp->GetHostname ();
    return NULL;
}

bool
PlatformDarwin::IsConnected () const
{
    if (IsHost())
        return true;
    else if (m_remote_platform_sp)
        return m_remote_platform_sp->IsConnected();
    return false;
}

Error
PlatformDarwin::ConnectRemote (Args& args)
{
    Error error;
    if (IsHost())
    {
        error.SetErrorStringWithFormat ("can't connect to the host platform '%s', always connected", GetPluginName().GetCString());
    }
    else
    {
        if (!m_remote_platform_sp)
            m_remote_platform_sp = Platform::Create ("remote-gdb-server", error);

        if (m_remote_platform_sp && error.Success())
            error = m_remote_platform_sp->ConnectRemote (args);
        else
            error.SetErrorString ("failed to create a 'remote-gdb-server' platform");
        
        if (error.Fail())
            m_remote_platform_sp.reset();
    }
    
    if (error.Success() && m_remote_platform_sp)
    {
        if (m_options.get())
        {
            OptionGroupOptions* options = m_options.get();
            OptionGroupPlatformRSync* m_rsync_options = (OptionGroupPlatformRSync*)options->GetGroupWithOption('r');
            OptionGroupPlatformSSH* m_ssh_options = (OptionGroupPlatformSSH*)options->GetGroupWithOption('s');
            OptionGroupPlatformCaching* m_cache_options = (OptionGroupPlatformCaching*)options->GetGroupWithOption('c');
            
            if (m_rsync_options->m_rsync)
            {
                SetSupportsRSync(true);
                SetRSyncOpts(m_rsync_options->m_rsync_opts.c_str());
                SetRSyncPrefix(m_rsync_options->m_rsync_prefix.c_str());
                SetIgnoresRemoteHostname(m_rsync_options->m_ignores_remote_hostname);
            }
            if (m_ssh_options->m_ssh)
            {
                SetSupportsSSH(true);
                SetSSHOpts(m_ssh_options->m_ssh_opts.c_str());
            }
            SetLocalCacheDirectory(m_cache_options->m_cache_dir.c_str());
        }
    }

    return error;
}

Error
PlatformDarwin::DisconnectRemote ()
{
    Error error;
    
    if (IsHost())
    {
        error.SetErrorStringWithFormat ("can't disconnect from the host platform '%s', always connected", GetPluginName().GetCString());
    }
    else
    {
        if (m_remote_platform_sp)
            error = m_remote_platform_sp->DisconnectRemote ();
        else
            error.SetErrorString ("the platform is not currently connected");
    }
    return error;
}


bool
PlatformDarwin::GetProcessInfo (lldb::pid_t pid, ProcessInstanceInfo &process_info)
{
    bool sucess = false;
    if (IsHost())
    {
        sucess = Platform::GetProcessInfo (pid, process_info);
    }
    else
    {
        if (m_remote_platform_sp)
            sucess = m_remote_platform_sp->GetProcessInfo (pid, process_info);
    }
    return sucess;
}



uint32_t
PlatformDarwin::FindProcesses (const ProcessInstanceInfoMatch &match_info,
                               ProcessInstanceInfoList &process_infos)
{
    uint32_t match_count = 0;
    if (IsHost())
    {
        // Let the base class figure out the host details
        match_count = Platform::FindProcesses (match_info, process_infos);
    }
    else
    {
        // If we are remote, we can only return results if we are connected
        if (m_remote_platform_sp)
            match_count = m_remote_platform_sp->FindProcesses (match_info, process_infos);
    }
    return match_count;    
}

Error
PlatformDarwin::LaunchProcess (ProcessLaunchInfo &launch_info)
{
    Error error;
    
    if (IsHost())
    {
        error = Platform::LaunchProcess (launch_info);
    }
    else
    {
        if (m_remote_platform_sp)
            error = m_remote_platform_sp->LaunchProcess (launch_info);
        else
            error.SetErrorString ("the platform is not currently connected");
    }
    return error;
}

lldb::ProcessSP
PlatformDarwin::DebugProcess (ProcessLaunchInfo &launch_info,
                              Debugger &debugger,
                              Target *target,       // Can be NULL, if NULL create a new target, else use existing one
                              Listener &listener,
                              Error &error)
{
    ProcessSP process_sp;
    
    if (IsHost())
    {
        process_sp = Platform::DebugProcess (launch_info, debugger, target, listener, error);
    }
    else
    {
        if (m_remote_platform_sp)
            process_sp = m_remote_platform_sp->DebugProcess (launch_info, debugger, target, listener, error);
        else
            error.SetErrorString ("the platform is not currently connected");
    }
    return process_sp;
    
}


lldb::ProcessSP
PlatformDarwin::Attach (ProcessAttachInfo &attach_info,
                        Debugger &debugger,
                        Target *target,
                        Listener &listener, 
                        Error &error)
{
    lldb::ProcessSP process_sp;
    
    if (IsHost())
    {
        if (target == NULL)
        {
            TargetSP new_target_sp;
            
            error = debugger.GetTargetList().CreateTarget (debugger,
                                                           NULL,
                                                           NULL, 
                                                           false,
                                                           NULL,
                                                           new_target_sp);
            target = new_target_sp.get();
        }
        else
            error.Clear();
    
        if (target && error.Success())
        {
            debugger.GetTargetList().SetSelectedTarget(target);

            process_sp = target->CreateProcess (listener, attach_info.GetProcessPluginName(), NULL);
            
            if (process_sp)
                error = process_sp->Attach (attach_info);
        }
    }
    else
    {
        if (m_remote_platform_sp)
            process_sp = m_remote_platform_sp->Attach (attach_info, debugger, target, listener, error);
        else
            error.SetErrorString ("the platform is not currently connected");
    }
    return process_sp;
}

const char *
PlatformDarwin::GetUserName (uint32_t uid)
{
    // Check the cache in Platform in case we have already looked this uid up
    const char *user_name = Platform::GetUserName(uid);
    if (user_name)
        return user_name;

    if (IsRemote() && m_remote_platform_sp)
        return m_remote_platform_sp->GetUserName(uid);
    return NULL;
}

const char *
PlatformDarwin::GetGroupName (uint32_t gid)
{
    const char *group_name = Platform::GetGroupName(gid);
    if (group_name)
        return group_name;

    if (IsRemote() && m_remote_platform_sp)
        return m_remote_platform_sp->GetGroupName(gid);
    return NULL;
}

bool
PlatformDarwin::ModuleIsExcludedForNonModuleSpecificSearches (lldb_private::Target &target, const lldb::ModuleSP &module_sp)
{
    if (!module_sp)
        return false;
        
    ObjectFile *obj_file = module_sp->GetObjectFile();
    if (!obj_file)
        return false;
    
    ObjectFile::Type obj_type = obj_file->GetType();
    if (obj_type == ObjectFile::eTypeDynamicLinker)
        return true;
    else
        return false;
}

std::string
PlatformDarwin::GetQueueNameForThreadQAddress (Process *process, addr_t thread_dispatch_qaddr)
{
    std::string dispatch_queue_name;
    if (thread_dispatch_qaddr == LLDB_INVALID_ADDRESS || thread_dispatch_qaddr == 0 || process == NULL)
        return "";

    ReadLibdispatchOffsets (process);
    if (m_libdispatch_offsets.IsValid ())
    {
        Error error;
        addr_t queue_addr = process->ReadPointerFromMemory (thread_dispatch_qaddr, error);
        if (error.Success())
        {
            if (m_libdispatch_offsets.dqo_version >= 4)
            {
                // libdispatch versions 4+, pointer to dispatch name is in the
                // queue structure.
                addr_t pointer_to_label_address = queue_addr + m_libdispatch_offsets.dqo_label;
                addr_t label_addr = process->ReadPointerFromMemory (pointer_to_label_address, error);
                if (error.Success())
                {
                    process->ReadCStringFromMemory (label_addr, dispatch_queue_name, error);
                }
            }
            else
            {
                // libdispatch versions 1-3, dispatch name is a fixed width char array
                // in the queue structure.
                addr_t label_addr = queue_addr + m_libdispatch_offsets.dqo_label;
                dispatch_queue_name.resize (m_libdispatch_offsets.dqo_label_size, '\0');
                size_t bytes_read = process->ReadMemory (label_addr, &dispatch_queue_name[0], m_libdispatch_offsets.dqo_label_size, error);
                if (bytes_read < m_libdispatch_offsets.dqo_label_size)
                    dispatch_queue_name.erase (bytes_read);
            }
        }
    }
    return dispatch_queue_name;
}

void
PlatformDarwin::ReadLibdispatchOffsetsAddress (Process *process)
{
    if (m_dispatch_queue_offsets_addr != LLDB_INVALID_ADDRESS)
        return;

    static ConstString g_dispatch_queue_offsets_symbol_name ("dispatch_queue_offsets");
    const Symbol *dispatch_queue_offsets_symbol = NULL;

    // libdispatch symbols were in libSystem.B.dylib up through Mac OS X 10.6 ("Snow Leopard")
    ModuleSpec libSystem_module_spec (FileSpec("libSystem.B.dylib", false));
    ModuleSP module_sp(process->GetTarget().GetImages().FindFirstModule (libSystem_module_spec));
    if (module_sp)
        dispatch_queue_offsets_symbol = module_sp->FindFirstSymbolWithNameAndType (g_dispatch_queue_offsets_symbol_name, eSymbolTypeData);
    
    // libdispatch symbols are in their own dylib as of Mac OS X 10.7 ("Lion") and later
    if (dispatch_queue_offsets_symbol == NULL)
    {
        ModuleSpec libdispatch_module_spec (FileSpec("libdispatch.dylib", false));
        module_sp = process->GetTarget().GetImages().FindFirstModule (libdispatch_module_spec);
        if (module_sp)
            dispatch_queue_offsets_symbol = module_sp->FindFirstSymbolWithNameAndType (g_dispatch_queue_offsets_symbol_name, eSymbolTypeData);
    }
    if (dispatch_queue_offsets_symbol)
        m_dispatch_queue_offsets_addr = dispatch_queue_offsets_symbol->GetAddress().GetLoadAddress(&process->GetTarget());
}

void
PlatformDarwin::ReadLibdispatchOffsets (Process *process)
{
    if (m_libdispatch_offsets.IsValid())
        return;

    ReadLibdispatchOffsetsAddress (process);

    uint8_t memory_buffer[sizeof (struct LibdispatchOffsets)];
    DataExtractor data (memory_buffer, 
                        sizeof(memory_buffer), 
                        process->GetTarget().GetArchitecture().GetByteOrder(), 
                        process->GetTarget().GetArchitecture().GetAddressByteSize());

    Error error;
    if (process->ReadMemory (m_dispatch_queue_offsets_addr, memory_buffer, sizeof(memory_buffer), error) == sizeof(memory_buffer))
    {
        lldb::offset_t data_offset = 0;

        // The struct LibdispatchOffsets is a series of uint16_t's - extract them all
        // in one big go.
        data.GetU16 (&data_offset, &m_libdispatch_offsets.dqo_version, sizeof (struct LibdispatchOffsets) / sizeof (uint16_t));
    }
}

lldb::queue_id_t
PlatformDarwin::GetQueueIDForThreadQAddress (Process *process, lldb::addr_t dispatch_qaddr)
{
    if (dispatch_qaddr == LLDB_INVALID_ADDRESS || dispatch_qaddr == 0 || process == NULL)
        return LLDB_INVALID_QUEUE_ID;

    Error error;
    uint32_t ptr_size = process->GetTarget().GetArchitecture().GetAddressByteSize();
    uint64_t this_thread_queue_id = process->ReadUnsignedIntegerFromMemory (dispatch_qaddr, ptr_size, LLDB_INVALID_QUEUE_ID, error);
    if (!error.Success())
        return LLDB_INVALID_QUEUE_ID;

    return this_thread_queue_id;
}


bool
PlatformDarwin::x86GetSupportedArchitectureAtIndex (uint32_t idx, ArchSpec &arch)
{
    if (idx == 0)
    {
        arch = Host::GetArchitecture (Host::eSystemDefaultArchitecture);
        return arch.IsValid();
    }
    else if (idx == 1)
    {
        ArchSpec platform_arch (Host::GetArchitecture (Host::eSystemDefaultArchitecture));
        ArchSpec platform_arch64 (Host::GetArchitecture (Host::eSystemDefaultArchitecture64));
        if (platform_arch.IsExactMatch(platform_arch64))
        {
            // This macosx platform supports both 32 and 64 bit. Since we already
            // returned the 64 bit arch for idx == 0, return the 32 bit arch 
            // for idx == 1
            arch = Host::GetArchitecture (Host::eSystemDefaultArchitecture32);
            return arch.IsValid();
        }
    }
    return false;
}

// The architecture selection rules for arm processors
// These cpu subtypes have distinct names (e.g. armv7f) but armv7 binaries run fine on an armv7f processor.

bool
PlatformDarwin::ARMGetSupportedArchitectureAtIndex (uint32_t idx, ArchSpec &arch)
{
    ArchSpec system_arch (GetSystemArchitecture());

    const ArchSpec::Core system_core = system_arch.GetCore();
    switch (system_core)
    {
    default:
        switch (idx)
        {
            case  0: arch.SetTriple ("arm64-apple-ios");    return true;
            case  1: arch.SetTriple ("armv7-apple-ios");    return true;
            case  2: arch.SetTriple ("armv7f-apple-ios");   return true;
            case  3: arch.SetTriple ("armv7k-apple-ios");   return true;
            case  4: arch.SetTriple ("armv7s-apple-ios");   return true;
            case  5: arch.SetTriple ("armv7m-apple-ios");   return true;
            case  6: arch.SetTriple ("armv7em-apple-ios");  return true;
            case  7: arch.SetTriple ("armv6m-apple-ios");   return true;
            case  8: arch.SetTriple ("armv6-apple-ios");    return true;
            case  9: arch.SetTriple ("armv5-apple-ios");    return true;
            case 10: arch.SetTriple ("armv4-apple-ios");    return true;
            case 11: arch.SetTriple ("arm-apple-ios");      return true;
            case 12: arch.SetTriple ("thumbv7-apple-ios");  return true;
            case 13: arch.SetTriple ("thumbv7f-apple-ios"); return true;
            case 14: arch.SetTriple ("thumbv7k-apple-ios"); return true;
            case 15: arch.SetTriple ("thumbv7s-apple-ios"); return true;
            case 16: arch.SetTriple ("thumbv7m-apple-ios"); return true;
            case 17: arch.SetTriple ("thumbv7em-apple-ios"); return true;
            case 18: arch.SetTriple ("thumbv6m-apple-ios"); return true;
            case 19: arch.SetTriple ("thumbv6-apple-ios");  return true;
            case 20: arch.SetTriple ("thumbv5-apple-ios");  return true;
            case 21: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 22: arch.SetTriple ("thumb-apple-ios");    return true;
            default: break;
        }
        break;

    case ArchSpec::eCore_arm_arm64:
        switch (idx)
        {
            case  0: arch.SetTriple ("arm64-apple-ios");   return true;
            case  1: arch.SetTriple ("armv7s-apple-ios");   return true;
            case  2: arch.SetTriple ("armv7f-apple-ios");   return true;
            case  3: arch.SetTriple ("armv7m-apple-ios");   return true;
            case  4: arch.SetTriple ("armv7em-apple-ios");  return true;
            case  5: arch.SetTriple ("armv7-apple-ios");    return true;
            case  6: arch.SetTriple ("armv6m-apple-ios");   return true;
            case  7: arch.SetTriple ("armv6-apple-ios");    return true;
            case  8: arch.SetTriple ("armv5-apple-ios");    return true;
            case  9: arch.SetTriple ("armv4-apple-ios");    return true;
            case 10: arch.SetTriple ("arm-apple-ios");      return true;
            case 11: arch.SetTriple ("thumbv7-apple-ios");  return true;
            case 12: arch.SetTriple ("thumbv7f-apple-ios"); return true;
            case 13: arch.SetTriple ("thumbv7k-apple-ios"); return true;
            case 14: arch.SetTriple ("thumbv7s-apple-ios"); return true;
            case 15: arch.SetTriple ("thumbv7m-apple-ios"); return true;
            case 16: arch.SetTriple ("thumbv7em-apple-ios"); return true;
            case 17: arch.SetTriple ("thumbv6m-apple-ios"); return true;
            case 18: arch.SetTriple ("thumbv6-apple-ios");  return true;
            case 19: arch.SetTriple ("thumbv5-apple-ios");  return true;
            case 20: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 21: arch.SetTriple ("thumb-apple-ios");    return true;
        default: break;
        }
        break;

    case ArchSpec::eCore_arm_armv7f:
        switch (idx)
        {
            case  0: arch.SetTriple ("armv7f-apple-ios");   return true;
            case  1: arch.SetTriple ("armv7-apple-ios");    return true;
            case  2: arch.SetTriple ("armv6m-apple-ios");   return true;
            case  3: arch.SetTriple ("armv6-apple-ios");    return true;
            case  4: arch.SetTriple ("armv5-apple-ios");    return true;
            case  5: arch.SetTriple ("armv4-apple-ios");    return true;
            case  6: arch.SetTriple ("arm-apple-ios");      return true;
            case  7: arch.SetTriple ("thumbv7f-apple-ios"); return true;
            case  8: arch.SetTriple ("thumbv7-apple-ios");  return true;
            case  9: arch.SetTriple ("thumbv6m-apple-ios"); return true;
            case 10: arch.SetTriple ("thumbv6-apple-ios");  return true;
            case 11: arch.SetTriple ("thumbv5-apple-ios");  return true;
            case 12: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 13: arch.SetTriple ("thumb-apple-ios");    return true;
            default: break;
        }
        break;

    case ArchSpec::eCore_arm_armv7k:
        switch (idx)
        {
            case  0: arch.SetTriple ("armv7k-apple-ios");   return true;
            case  1: arch.SetTriple ("armv7-apple-ios");    return true;
            case  2: arch.SetTriple ("armv6m-apple-ios");   return true;
            case  3: arch.SetTriple ("armv6-apple-ios");    return true;
            case  4: arch.SetTriple ("armv5-apple-ios");    return true;
            case  5: arch.SetTriple ("armv4-apple-ios");    return true;
            case  6: arch.SetTriple ("arm-apple-ios");      return true;
            case  7: arch.SetTriple ("thumbv7k-apple-ios"); return true;
            case  8: arch.SetTriple ("thumbv7-apple-ios");  return true;
            case  9: arch.SetTriple ("thumbv6m-apple-ios"); return true;
            case 10: arch.SetTriple ("thumbv6-apple-ios");  return true;
            case 11: arch.SetTriple ("thumbv5-apple-ios");  return true;
            case 12: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 13: arch.SetTriple ("thumb-apple-ios");    return true;
            default: break;
        }
        break;

    case ArchSpec::eCore_arm_armv7s:
        switch (idx)
        {
            case  0: arch.SetTriple ("armv7s-apple-ios");   return true;
            case  1: arch.SetTriple ("armv7-apple-ios");    return true;
            case  2: arch.SetTriple ("armv6m-apple-ios");   return true;
            case  3: arch.SetTriple ("armv6-apple-ios");    return true;
            case  4: arch.SetTriple ("armv5-apple-ios");    return true;
            case  5: arch.SetTriple ("armv4-apple-ios");    return true;
            case  6: arch.SetTriple ("arm-apple-ios");      return true;
            case  7: arch.SetTriple ("thumbv7s-apple-ios"); return true;
            case  8: arch.SetTriple ("thumbv7-apple-ios");  return true;
            case  9: arch.SetTriple ("thumbv6m-apple-ios"); return true;
            case 10: arch.SetTriple ("thumbv6-apple-ios");  return true;
            case 11: arch.SetTriple ("thumbv5-apple-ios");  return true;
            case 12: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 13: arch.SetTriple ("thumb-apple-ios");    return true;
            default: break;
        }
        break;

    case ArchSpec::eCore_arm_armv7m:
        switch (idx)
        {
            case  0: arch.SetTriple ("armv7m-apple-ios");   return true;
            case  1: arch.SetTriple ("armv7-apple-ios");    return true;
            case  2: arch.SetTriple ("armv6m-apple-ios");   return true;
            case  3: arch.SetTriple ("armv6-apple-ios");    return true;
            case  4: arch.SetTriple ("armv5-apple-ios");    return true;
            case  5: arch.SetTriple ("armv4-apple-ios");    return true;
            case  6: arch.SetTriple ("arm-apple-ios");      return true;
            case  7: arch.SetTriple ("thumbv7m-apple-ios"); return true;
            case  8: arch.SetTriple ("thumbv7-apple-ios");  return true;
            case  9: arch.SetTriple ("thumbv6m-apple-ios"); return true;
            case 10: arch.SetTriple ("thumbv6-apple-ios");  return true;
            case 11: arch.SetTriple ("thumbv5-apple-ios");  return true;
            case 12: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 13: arch.SetTriple ("thumb-apple-ios");    return true;
            default: break;
        }
        break;

    case ArchSpec::eCore_arm_armv7em:
        switch (idx)
        {
            case  0: arch.SetTriple ("armv7em-apple-ios");  return true;
            case  1: arch.SetTriple ("armv7-apple-ios");    return true;
            case  2: arch.SetTriple ("armv6m-apple-ios");   return true;
            case  3: arch.SetTriple ("armv6-apple-ios");    return true;
            case  4: arch.SetTriple ("armv5-apple-ios");    return true;
            case  5: arch.SetTriple ("armv4-apple-ios");    return true;
            case  6: arch.SetTriple ("arm-apple-ios");      return true;
            case  7: arch.SetTriple ("thumbv7em-apple-ios"); return true;
            case  8: arch.SetTriple ("thumbv7-apple-ios");  return true;
            case  9: arch.SetTriple ("thumbv6m-apple-ios"); return true;
            case 10: arch.SetTriple ("thumbv6-apple-ios");  return true;
            case 11: arch.SetTriple ("thumbv5-apple-ios");  return true;
            case 12: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 13: arch.SetTriple ("thumb-apple-ios");    return true;
            default: break;
        }
        break;

    case ArchSpec::eCore_arm_armv7:
        switch (idx)
        {
            case  0: arch.SetTriple ("armv7-apple-ios");    return true;
            case  1: arch.SetTriple ("armv6m-apple-ios");   return true;
            case  2: arch.SetTriple ("armv6-apple-ios");    return true;
            case  3: arch.SetTriple ("armv5-apple-ios");    return true;
            case  4: arch.SetTriple ("armv4-apple-ios");    return true;
            case  5: arch.SetTriple ("arm-apple-ios");      return true;
            case  6: arch.SetTriple ("thumbv7-apple-ios");  return true;
            case  7: arch.SetTriple ("thumbv6m-apple-ios"); return true;
            case  8: arch.SetTriple ("thumbv6-apple-ios");  return true;
            case  9: arch.SetTriple ("thumbv5-apple-ios");  return true;
            case 10: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 11: arch.SetTriple ("thumb-apple-ios");    return true;
            default: break;
        }
        break;

    case ArchSpec::eCore_arm_armv6m:
        switch (idx)
        {
            case 0: arch.SetTriple ("armv6m-apple-ios");   return true;
            case 1: arch.SetTriple ("armv6-apple-ios");    return true;
            case 2: arch.SetTriple ("armv5-apple-ios");    return true;
            case 3: arch.SetTriple ("armv4-apple-ios");    return true;
            case 4: arch.SetTriple ("arm-apple-ios");      return true;
            case 5: arch.SetTriple ("thumbv6m-apple-ios"); return true;
            case 6: arch.SetTriple ("thumbv6-apple-ios");  return true;
            case 7: arch.SetTriple ("thumbv5-apple-ios");  return true;
            case 8: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 9: arch.SetTriple ("thumb-apple-ios");    return true;
            default: break;
        }
        break;

    case ArchSpec::eCore_arm_armv6:
        switch (idx)
        {
            case 0: arch.SetTriple ("armv6-apple-ios");    return true;
            case 1: arch.SetTriple ("armv5-apple-ios");    return true;
            case 2: arch.SetTriple ("armv4-apple-ios");    return true;
            case 3: arch.SetTriple ("arm-apple-ios");      return true;
            case 4: arch.SetTriple ("thumbv6-apple-ios");  return true;
            case 5: arch.SetTriple ("thumbv5-apple-ios");  return true;
            case 6: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 7: arch.SetTriple ("thumb-apple-ios");    return true;
            default: break;
        }
        break;

    case ArchSpec::eCore_arm_armv5:
        switch (idx)
        {
            case 0: arch.SetTriple ("armv5-apple-ios");    return true;
            case 1: arch.SetTriple ("armv4-apple-ios");    return true;
            case 2: arch.SetTriple ("arm-apple-ios");      return true;
            case 3: arch.SetTriple ("thumbv5-apple-ios");  return true;
            case 4: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 5: arch.SetTriple ("thumb-apple-ios");    return true;
            default: break;
        }
        break;

    case ArchSpec::eCore_arm_armv4:
        switch (idx)
        {
            case 0: arch.SetTriple ("armv4-apple-ios");    return true;
            case 1: arch.SetTriple ("arm-apple-ios");      return true;
            case 2: arch.SetTriple ("thumbv4t-apple-ios"); return true;
            case 3: arch.SetTriple ("thumb-apple-ios");    return true;
            default: break;
        }
        break;
    }
    arch.Clear();
    return false;
}


const char *
PlatformDarwin::GetDeveloperDirectory()
{
    if (m_developer_directory.empty())
    {
        bool developer_dir_path_valid = false;
        char developer_dir_path[PATH_MAX];
        FileSpec temp_file_spec;
        if (Host::GetLLDBPath (ePathTypeLLDBShlibDir, temp_file_spec))
        {
            if (temp_file_spec.GetPath (developer_dir_path, sizeof(developer_dir_path)))
            {
                char *shared_frameworks = strstr (developer_dir_path, "/SharedFrameworks/LLDB.framework");
                if (shared_frameworks)
                {
                    ::snprintf (shared_frameworks, 
                                sizeof(developer_dir_path) - (shared_frameworks - developer_dir_path),
                                "/Developer");
                    developer_dir_path_valid = true;
                }
                else
                {
                    char *lib_priv_frameworks = strstr (developer_dir_path, "/Library/PrivateFrameworks/LLDB.framework");
                    if (lib_priv_frameworks)
                    {
                        *lib_priv_frameworks = '\0';
                        developer_dir_path_valid = true;
                    }
                }
            }
        }
        
        if (!developer_dir_path_valid)
        {
            std::string xcode_dir_path;
            const char *xcode_select_prefix_dir = getenv ("XCODE_SELECT_PREFIX_DIR");
            if (xcode_select_prefix_dir)
                xcode_dir_path.append (xcode_select_prefix_dir);
            xcode_dir_path.append ("/usr/share/xcode-select/xcode_dir_path");
            temp_file_spec.SetFile(xcode_dir_path.c_str(), false);
            size_t bytes_read = temp_file_spec.ReadFileContents(0, developer_dir_path, sizeof(developer_dir_path), NULL);
            if (bytes_read > 0)
            {
                developer_dir_path[bytes_read] = '\0';
                while (developer_dir_path[bytes_read-1] == '\r' ||
                       developer_dir_path[bytes_read-1] == '\n')
                    developer_dir_path[--bytes_read] = '\0';
                developer_dir_path_valid = true;
            }
        }
        
        if (!developer_dir_path_valid)
        {
            FileSpec xcode_select_cmd ("/usr/bin/xcode-select", false);
            if (xcode_select_cmd.Exists())
            {
                int exit_status = -1;
                int signo = -1;
                std::string command_output;
                Error error = Host::RunShellCommand ("/usr/bin/xcode-select --print-path", 
                                                     NULL,                                 // current working directory
                                                     &exit_status,
                                                     &signo,
                                                     &command_output,
                                                     2,                                     // short timeout
                                                     NULL);                                 // don't run in a shell
                if (error.Success() && exit_status == 0 && !command_output.empty())
                {
                    const char *cmd_output_ptr = command_output.c_str();
                    developer_dir_path[sizeof (developer_dir_path) - 1] = '\0';
                    size_t i;
                    for (i = 0; i < sizeof (developer_dir_path) - 1; i++)
                    {
                        if (cmd_output_ptr[i] == '\r' || cmd_output_ptr[i] == '\n' || cmd_output_ptr[i] == '\0')
                            break;
                        developer_dir_path[i] = cmd_output_ptr[i];
                    }
                    developer_dir_path[i] = '\0';

                    FileSpec devel_dir (developer_dir_path, false);
                    if (devel_dir.Exists() && devel_dir.IsDirectory())
                    {
                        developer_dir_path_valid = true;
                    }
                }
            }
        }

        if (developer_dir_path_valid)
        {
            temp_file_spec.SetFile (developer_dir_path, false);
            if (temp_file_spec.Exists())
            {
                m_developer_directory.assign (developer_dir_path);
                return m_developer_directory.c_str();
            }
        }
        // Assign a single NULL character so we know we tried to find the device
        // support directory and we don't keep trying to find it over and over.
        m_developer_directory.assign (1, '\0');
    }
    
    // We should have put a single NULL character into m_developer_directory
    // or it should have a valid path if the code gets here
    assert (m_developer_directory.empty() == false);
    if (m_developer_directory[0])
        return m_developer_directory.c_str();
    return NULL;
}


BreakpointSP
PlatformDarwin::SetThreadCreationBreakpoint (Target &target)
{
    BreakpointSP bp_sp;
    static const char *g_bp_names[] =
    {
        "start_wqthread",
        "_pthread_wqthread",
        "_pthread_start",
    };

    static const char *g_bp_modules[] =
    {
        "libsystem_c.dylib",
        "libSystem.B.dylib"
    };

    FileSpecList bp_modules;
    for (size_t i = 0; i < sizeof(g_bp_modules)/sizeof(const char *); i++)
    {
        const char *bp_module = g_bp_modules[i];
        bp_modules.Append(FileSpec(bp_module, false));
    }

    bool internal = true;
    bool hardware = false;
    LazyBool skip_prologue = eLazyBoolNo;
    bp_sp = target.CreateBreakpoint (&bp_modules,
                                     NULL,
                                     g_bp_names,
                                     sizeof(g_bp_names)/sizeof(const char *),
                                     eFunctionNameTypeFull,
                                     skip_prologue,
                                     internal,
                                     hardware);
    bp_sp->SetBreakpointKind("thread-creation");

    return bp_sp;
}

size_t
PlatformDarwin::GetEnvironment (StringList &env)
{
    if (IsRemote())
    {
        if (m_remote_platform_sp)
            return m_remote_platform_sp->GetEnvironment(env);
        return 0;
    }
    return Host::GetEnvironment(env);
}

int32_t
PlatformDarwin::GetResumeCountForLaunchInfo (ProcessLaunchInfo &launch_info)
{
    const char *shell = launch_info.GetShell();
    if (shell == NULL)
        return 1;
        
    const char *shell_name = strrchr (shell, '/');
    if (shell_name == NULL)
        shell_name = shell;
    else
        shell_name++;
    
    if (strcmp (shell_name, "sh") == 0)
    {
        // /bin/sh re-exec's itself as /bin/bash requiring another resume.
        // But it only does this if the COMMAND_MODE environment variable
        // is set to "legacy".
        char * const *envp = (char * const*)launch_info.GetEnvironmentEntries().GetConstArgumentVector();
        if (envp != NULL)
        {
            for (int i = 0; envp[i] != NULL; i++)
            {
                if (strcmp (envp[i], "COMMAND_MODE=legacy" ) == 0)
                    return 2;
            }
        }
        return 1;
    }
    else if (strcmp (shell_name, "csh") == 0
            || strcmp (shell_name, "tcsh") == 0
            || strcmp (shell_name, "zsh") == 0)
    {
        // csh and tcsh always seem to re-exec themselves.
        return 2;
    }
    else
        return 1;
}
