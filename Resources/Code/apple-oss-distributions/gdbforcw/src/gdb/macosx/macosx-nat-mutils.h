#ifndef __GDB_MACOSX_NAT_MUTILS_H__
#define __GDB_MACOSX_NAT_MUTILS_H__

#include "defs.h"
#include "memattr.h"

struct target_ops;

#if (defined __GNUC__)
#define __MACH_CHECK_FUNCTION __PRETTY_FUNCTION__
#else
#define __MACH_CHECK_FUNCTION ((__const char *) 0)
#endif

#define MACH_PROPAGATE_ERROR(ret) \
{ MACH_WARN_ERROR(ret); if ((ret) != KERN_SUCCESS) { return ret; } }

#define MACH_CHECK_ERROR(ret) \
mach_check_error (ret, __FILE__, __LINE__, __MACH_CHECK_FUNCTION);

#define MACH_WARN_ERROR(ret) \
mach_warn_error (ret, __FILE__, __LINE__, __MACH_CHECK_FUNCTION);

#define MACH_ERROR_STRING(ret) \
(mach_error_string (ret) ? mach_error_string (ret) : "[UNKNOWN]")

void gdb_check (const char *str, const char *file, unsigned int line,
                const char *func);
void gdb_check_fatal (const char *str, const char *file, unsigned int line,
                      const char *func);

unsigned int child_get_pagesize (void);

int
mach_xfer_memory (CORE_ADDR memaddr, char *myaddr,
                  int len, int write,
                  struct mem_attrib *attrib, struct target_ops *target);

void mach_check_error (kern_return_t ret, const char *file, unsigned int line,
                       const char *func);
void mach_warn_error (kern_return_t ret, const char *file, unsigned int line,
                      const char *func);

int macosx_port_valid (mach_port_t port);
int macosx_task_valid (task_t task);
int macosx_thread_valid (task_t task, thread_t thread);
int macosx_pid_valid (int pid);

thread_t macosx_primary_thread_of_task (task_t task);

kern_return_t macosx_msg_receive (mach_msg_header_t * msgin, size_t msgsize,
                                  unsigned long timeout, mach_port_t port);

int call_ptrace (int request, int pid, int arg3, int arg4);

CORE_ADDR macosx_allocate_space_in_inferior (int len);

#endif /* __GDB_MACOSX_NAT_MUTILS_H__ */
