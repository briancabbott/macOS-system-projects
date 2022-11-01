#ifndef __GDB_I386_MACOSX_THREAD_STATUS_H__
#define __GDB_I386_MACOSX_THREAD_STATUS_H__

#define GDB_i386_THREAD_STATE 1
#define GDB_i386_THREAD_FPSTATE 2 /* Equivalent to Mach's i386_FLOAT_STATE */

#define GDB_i386_FP_NO 0        /* No floating point. */
#define GDB_i386_FP_SOFT 1      /* Software FP emulator. */
#define GDB_i386_FP_287 2       /* 80287 */
#define GDB_i386_FP_387 3       /* 80387 or 80486 */
#define GDB_i386_FP_SSE2 4      /* P4 Streaming SIMD 2 Extensions (includes MMX and SSE) - corresponds to Mach's FP_FXSR */

#define GDB_i386_FP_387_STATE_SIZE 108
#define GDB_i386_FP_SSE2_STATE_SIZE 512

struct gdb_i386_thread_state
{
  unsigned int eax;
  unsigned int ebx;
  unsigned int ecx;
  unsigned int edx;
  unsigned int edi;
  unsigned int esi;
  unsigned int ebp;
  unsigned int esp;
  unsigned int ss;
  unsigned int efl;
  unsigned int eip;
  unsigned int cs;
  unsigned int ds;
  unsigned int es;
  unsigned int fs;
  unsigned int gs;
};

typedef struct gdb_i386_thread_state gdb_i386_thread_state_t;

/* This structure is a copy of the struct i386_float_state definition
   in /usr/include/mach/i386/thread_status.h -- it must be identical for
   the call to thread_get_state and thread_set_state.  */

struct gdb_i386_thread_fpstate
{
  unsigned int fpkind;
  unsigned int initialized;
  unsigned char hw_fu_state[GDB_i386_FP_SSE2_STATE_SIZE];
  unsigned int exc_status;
};

typedef struct gdb_i386_thread_fpstate gdb_i386_thread_fpstate_t;

#define GDB_i386_THREAD_STATE_COUNT \
    (sizeof (struct gdb_i386_thread_state) / sizeof (unsigned int))

/* Yes, the THREAD_FPSTATE_COUNT includes the fpkind et al struct members in 
   there - the kernel wants to see this # match the definition of
   i386_FLOAT_STATE_COUNT in /usr/include/mach/i386/thread_status.h */

#define GDB_i386_THREAD_FPSTATE_COUNT \
    (sizeof (struct gdb_i386_thread_fpstate) / sizeof (unsigned int))

#endif /* __GDB_I386_MACOSX_THREAD_STATUS_H__ */
