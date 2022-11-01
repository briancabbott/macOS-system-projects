//===-- DNB.h ---------------------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  Created by Greg Clayton on 3/23/07.
//
//===----------------------------------------------------------------------===//

#ifndef __DNB_h__
#define __DNB_h__

#include "DNBDefs.h"
#include <mach/thread_info.h>
#include <string>

#define DNB_EXPORT __attribute__((visibility("default")))

#ifndef CPU_TYPE_ARM64
#define CPU_TYPE_ARM64 ((cpu_type_t) 12 | 0x01000000)
#endif

typedef bool (*DNBShouldCancelCallback) (void *);

void            DNBInitialize ();
void            DNBTerminate ();

nub_bool_t      DNBSetArchitecture      (const char *arch);

//----------------------------------------------------------------------
// Process control
//----------------------------------------------------------------------
nub_process_t   DNBProcessLaunch        (const char *path, 
                                         char const *argv[], 
                                         const char *envp[], 
                                         const char *working_directory, // NULL => dont' change, non-NULL => set working directory for inferior to this
                                         const char *stdin_path,
                                         const char *stdout_path,
                                         const char *stderr_path,
                                         bool no_stdio, 
                                         nub_launch_flavor_t launch_flavor, 
                                         int disable_aslr,
                                         const char *event_data,
                                         char *err_str, 
                                         size_t err_len);

nub_process_t   DNBProcessAttach        (nub_process_t pid, struct timespec *timeout, char *err_str, size_t err_len);
nub_process_t   DNBProcessAttachByName  (const char *name, struct timespec *timeout, char *err_str, size_t err_len);
nub_process_t   DNBProcessAttachWait    (const char *wait_name, nub_launch_flavor_t launch_flavor, bool ignore_existing, struct timespec *timeout, useconds_t interval, char *err_str, size_t err_len, DNBShouldCancelCallback should_cancel = NULL, void *callback_data = NULL);
// Resume a process with exact instructions on what to do with each thread:
// - If no thread actions are supplied (actions is NULL or num_actions is zero),
//   then all threads are continued.
// - If any thread actions are supplied, then each thread will do as it is told
//   by the action. A default actions for any threads that don't have an
//   explicit thread action can be made by making a thread action with a tid of
//   INVALID_NUB_THREAD. If there is no default action, those threads will
//   remain stopped.
nub_bool_t      DNBProcessResume        (nub_process_t pid, const DNBThreadResumeAction *actions, size_t num_actions) DNB_EXPORT;
nub_bool_t      DNBProcessHalt          (nub_process_t pid) DNB_EXPORT;
nub_bool_t      DNBProcessDetach        (nub_process_t pid) DNB_EXPORT;
nub_bool_t      DNBProcessSignal        (nub_process_t pid, int signal) DNB_EXPORT;
nub_bool_t      DNBProcessKill          (nub_process_t pid) DNB_EXPORT;
nub_bool_t      DNBProcessSendEvent     (nub_process_t pid, const char *event) DNB_EXPORT;
nub_size_t      DNBProcessMemoryRead    (nub_process_t pid, nub_addr_t addr, nub_size_t size, void *buf) DNB_EXPORT;
nub_size_t      DNBProcessMemoryWrite   (nub_process_t pid, nub_addr_t addr, nub_size_t size, const void *buf) DNB_EXPORT;
nub_addr_t      DNBProcessMemoryAllocate    (nub_process_t pid, nub_size_t size, uint32_t permissions) DNB_EXPORT;
nub_bool_t      DNBProcessMemoryDeallocate  (nub_process_t pid, nub_addr_t addr) DNB_EXPORT;
int             DNBProcessMemoryRegionInfo  (nub_process_t pid, nub_addr_t addr, DNBRegionInfo *region_info) DNB_EXPORT;
std::string     DNBProcessGetProfileData (nub_process_t pid, DNBProfileDataScanType scanType) DNB_EXPORT;
nub_bool_t      DNBProcessSetEnableAsyncProfiling   (nub_process_t pid, nub_bool_t enable, uint64_t interval_usec, DNBProfileDataScanType scan_type) DNB_EXPORT;

//----------------------------------------------------------------------
// Process status
//----------------------------------------------------------------------
nub_bool_t      DNBProcessIsAlive                       (nub_process_t pid) DNB_EXPORT;
nub_state_t     DNBProcessGetState                      (nub_process_t pid) DNB_EXPORT;
nub_bool_t      DNBProcessGetExitStatus                 (nub_process_t pid, int *status) DNB_EXPORT;
nub_bool_t      DNBProcessSetExitStatus                 (nub_process_t pid, int status) DNB_EXPORT;
const char *    DNBProcessGetExitInfo                   (nub_process_t pid) DNB_EXPORT;
nub_bool_t      DNBProcessSetExitInfo                   (nub_process_t pid, const char *info) DNB_EXPORT;
nub_size_t      DNBProcessGetNumThreads                 (nub_process_t pid) DNB_EXPORT;
nub_thread_t    DNBProcessGetCurrentThread              (nub_process_t pid) DNB_EXPORT;
nub_thread_t    DNBProcessGetCurrentThreadMachPort      (nub_process_t pid) DNB_EXPORT;
nub_thread_t    DNBProcessSetCurrentThread              (nub_process_t pid, nub_thread_t tid) DNB_EXPORT;
nub_thread_t    DNBProcessGetThreadAtIndex              (nub_process_t pid, nub_size_t thread_idx) DNB_EXPORT;
nub_bool_t      DNBProcessSyncThreadState               (nub_process_t pid, nub_thread_t tid) DNB_EXPORT;
nub_addr_t      DNBProcessGetSharedLibraryInfoAddress   (nub_process_t pid) DNB_EXPORT;
nub_bool_t      DNBProcessSharedLibrariesUpdated        (nub_process_t pid) DNB_EXPORT;
nub_size_t      DNBProcessGetSharedLibraryInfo          (nub_process_t pid, nub_bool_t only_changed, DNBExecutableImageInfo **image_infos) DNB_EXPORT;
nub_bool_t      DNBProcessSetNameToAddressCallback      (nub_process_t pid, DNBCallbackNameToAddress callback, void *baton) DNB_EXPORT;
nub_bool_t      DNBProcessSetSharedLibraryInfoCallback  (nub_process_t pid, DNBCallbackCopyExecutableImageInfos callback, void *baton) DNB_EXPORT;
nub_addr_t      DNBProcessLookupAddress                 (nub_process_t pid, const char *name, const char *shlib) DNB_EXPORT;
nub_size_t      DNBProcessGetAvailableSTDOUT            (nub_process_t pid, char *buf, nub_size_t buf_size) DNB_EXPORT;
nub_size_t      DNBProcessGetAvailableSTDERR            (nub_process_t pid, char *buf, nub_size_t buf_size) DNB_EXPORT;
nub_size_t      DNBProcessGetAvailableProfileData       (nub_process_t pid, char *buf, nub_size_t buf_size) DNB_EXPORT;
nub_size_t      DNBProcessGetStopCount                  (nub_process_t pid) DNB_EXPORT;
uint32_t        DNBProcessGetCPUType                    (nub_process_t pid) DNB_EXPORT; 

//----------------------------------------------------------------------
// Process executable and arguments
//----------------------------------------------------------------------
const char *    DNBProcessGetExecutablePath     (nub_process_t pid);
const char *    DNBProcessGetArgumentAtIndex    (nub_process_t pid, nub_size_t idx);
nub_size_t      DNBProcessGetArgumentCount      (nub_process_t pid);

//----------------------------------------------------------------------
// Process events
//----------------------------------------------------------------------
nub_event_t     DNBProcessWaitForEvents         (nub_process_t pid, nub_event_t event_mask, bool wait_for_set, struct timespec* timeout);
void            DNBProcessResetEvents           (nub_process_t pid, nub_event_t event_mask);

//----------------------------------------------------------------------
// Thread functions
//----------------------------------------------------------------------
const char *    DNBThreadGetName                (nub_process_t pid, nub_thread_t tid);
nub_bool_t      DNBThreadGetIdentifierInfo      (nub_process_t pid, nub_thread_t tid, thread_identifier_info_data_t *ident_info);
nub_state_t     DNBThreadGetState               (nub_process_t pid, nub_thread_t tid);
nub_bool_t      DNBThreadGetRegisterValueByID   (nub_process_t pid, nub_thread_t tid, uint32_t set, uint32_t reg, DNBRegisterValue *value);
nub_bool_t      DNBThreadSetRegisterValueByID   (nub_process_t pid, nub_thread_t tid, uint32_t set, uint32_t reg, const DNBRegisterValue *value);
nub_size_t      DNBThreadGetRegisterContext     (nub_process_t pid, nub_thread_t tid, void *buf, size_t buf_len);
nub_size_t      DNBThreadSetRegisterContext     (nub_process_t pid, nub_thread_t tid, const void *buf, size_t buf_len);
uint32_t        DNBThreadSaveRegisterState      (nub_process_t pid, nub_thread_t tid);
nub_bool_t      DNBThreadRestoreRegisterState   (nub_process_t pid, nub_thread_t tid, uint32_t save_id);
nub_bool_t      DNBThreadGetRegisterValueByName (nub_process_t pid, nub_thread_t tid, uint32_t set, const char *name, DNBRegisterValue *value);
nub_bool_t      DNBThreadGetStopReason          (nub_process_t pid, nub_thread_t tid, DNBThreadStopInfo *stop_info);
const char *    DNBThreadGetInfo                (nub_process_t pid, nub_thread_t tid);
//----------------------------------------------------------------------
// Breakpoint functions
//----------------------------------------------------------------------
nub_bool_t      DNBBreakpointSet                (nub_process_t pid, nub_addr_t addr, nub_size_t size, nub_bool_t hardware);
nub_bool_t      DNBBreakpointClear              (nub_process_t pid, nub_addr_t addr);

//----------------------------------------------------------------------
// Watchpoint functions
//----------------------------------------------------------------------
nub_bool_t      DNBWatchpointSet                (nub_process_t pid, nub_addr_t addr, nub_size_t size, uint32_t watch_flags, nub_bool_t hardware);
nub_bool_t      DNBWatchpointClear              (nub_process_t pid, nub_addr_t addr);
uint32_t        DNBWatchpointGetNumSupportedHWP (nub_process_t pid); 

const DNBRegisterSetInfo *
                DNBGetRegisterSetInfo           (nub_size_t *num_reg_sets);
nub_bool_t      DNBGetRegisterInfoByName        (const char *reg_name, DNBRegisterInfo* info);

//----------------------------------------------------------------------
// Printf style formatting for printing values in the inferior memory
// space and registers.
//----------------------------------------------------------------------
nub_size_t      DNBPrintf (nub_process_t pid, nub_thread_t tid, nub_addr_t addr, FILE *file, const char *format);

//----------------------------------------------------------------------
// Other static nub information calls.
//----------------------------------------------------------------------
const char *    DNBStateAsString (nub_state_t state);
nub_bool_t      DNBResolveExecutablePath (const char *path, char *resolved_path, size_t resolved_path_size);

#endif
