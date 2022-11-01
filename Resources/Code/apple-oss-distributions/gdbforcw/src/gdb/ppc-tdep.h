/* Target-dependent code for GDB, the GNU debugger.
   Copyright 2000, 2001, 2002, 2003
   Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#ifndef PPC_TDEP_H
#define PPC_TDEP_H

struct gdbarch;
struct frame_info;
struct value;
struct regcache;
struct type;

/* From ppc-linux-tdep.c... */
CORE_ADDR ppc_linux_frame_saved_pc (struct frame_info *fi);
void ppc_linux_init_extra_frame_info (int fromleaf, struct frame_info *);
int ppc_linux_frameless_function_invocation (struct frame_info *);
void ppc_linux_frame_init_saved_regs (struct frame_info *);
CORE_ADDR ppc_linux_frame_chain (struct frame_info *);
enum return_value_convention ppc_sysv_abi_return_value (struct gdbarch *gdbarch,
							struct type *valtype,
							struct regcache *regcache,
							void *readbuf,
							const void *writebuf);
enum return_value_convention ppc_sysv_abi_broken_return_value (struct gdbarch *gdbarch,
							       struct type *valtype,
							       struct regcache *regcache,
							       void *readbuf,
							       const void *writebuf);
CORE_ADDR ppc_darwin_abi_push_dummy_call (struct gdbarch *gdbarch,
					  CORE_ADDR func_addr,
					  struct regcache *regcache,
					  CORE_ADDR bp_addr, int nargs,
					  struct value **args, CORE_ADDR sp,
					  int struct_return,
					  CORE_ADDR struct_addr);
CORE_ADDR ppc64_darwin_abi_push_dummy_call (struct gdbarch *gdbarch,
					    CORE_ADDR func_addr,
					    struct regcache *regcache,
					    CORE_ADDR bp_addr, int nargs,
					    struct value **args, CORE_ADDR sp,
					    int struct_return,
					    CORE_ADDR struct_addr);
CORE_ADDR ppc_sysv_abi_push_dummy_call (struct gdbarch *gdbarch,
					CORE_ADDR func_addr,
					struct regcache *regcache,
					CORE_ADDR bp_addr, int nargs,
					struct value **args, CORE_ADDR sp,
					int struct_return,
					CORE_ADDR struct_addr);
CORE_ADDR ppc64_sysv_abi_push_dummy_call (struct gdbarch *gdbarch,
					  CORE_ADDR func_addr,
					  struct regcache *regcache,
					  CORE_ADDR bp_addr, int nargs,
					  struct value **args, CORE_ADDR sp,
					  int struct_return,
					  CORE_ADDR struct_addr);
CORE_ADDR ppc64_sysv_abi_adjust_breakpoint_address (struct gdbarch *gdbarch,
						    CORE_ADDR bpaddr);
int ppc_linux_memory_remove_breakpoint (CORE_ADDR addr, char *contents_cache);
struct link_map_offsets *ppc_linux_svr4_fetch_link_map_offsets (void);
void ppc_linux_supply_gregset (char *buf);
void ppc_linux_supply_fpregset (char *buf);

enum return_value_convention ppc64_sysv_abi_return_value (struct gdbarch *gdbarch,
							  struct type *valtype,
							  struct regcache *regcache,
							  void *readbuf,
							  const void *writebuf);
enum return_value_convention ppc64_darwin_abi_return_value (struct gdbarch *gdbarch,
							    struct type *valtype,
							    struct regcache *regcache,
							    void *readbuf,
							    const void *writebuf);
enum return_value_convention ppc_darwin_abi_return_value (struct gdbarch *gdbarch,
							  struct type *valtype,
							  struct regcache *regcache,
							  void *readbuf,
							  const void *writebuf);

/* From rs6000-tdep.c... */
int ppc64_sysv_abi_use_struct_convention (int gcc_p, struct type *value_type);
void ppc64_sysv_abi_extract_return_value (struct type *valtype,
					  struct regcache *regbuf,
					  void *valbuf);
void ppc64_sysv_abi_store_return_value (struct type *valtype,
					struct regcache *regbuf,
					const void *valbuf);
CORE_ADDR rs6000_frame_saved_pc (struct frame_info *fi);
void rs6000_init_extra_frame_info (int fromleaf, struct frame_info *);
int rs6000_frameless_function_invocation (struct frame_info *);
void rs6000_frame_init_saved_regs (struct frame_info *);
CORE_ADDR rs6000_frame_chain (struct frame_info *);
int altivec_register_p (int regno);
void rs6000_pop_frame (void);
void rs6000_info_powerpc_command (char *args, int from_tty);
extern struct cmd_list_element *info_powerpc_cmdlist;
CORE_ADDR rs6000_fetch_pointer_argument (struct frame_info *frame, int argi, struct type *type);



/* From ppc-sysv-tdep.c */
CORE_ADDR ppc_sysv_abi_push_arguments (int, struct value **,
				       CORE_ADDR, int, CORE_ADDR);
CORE_ADDR ppc_darwin_abi_push_arguments (int, struct value **,
					 CORE_ADDR, int,  CORE_ADDR);


/* Return non-zero when the architecture has an FPU (or at least when
   the ABI is using the FPU).  */
int ppc_floating_point_unit_p (struct gdbarch *gdbarch);

/* Private data that this module attaches to struct gdbarch. */

struct gdbarch_tdep
  {
    /* APPLE LOCAL: We use the wordsize to be the size of the 
       GPR registers that are actually passed in the ABI.  So on 32 bit
       PPC, wordsize is 4 & only the lower 4 bytes of the register are
       passed (even though from assembly you can set & get the full 64 bits
       or the registers on a G5.)  */
    int wordsize;              /* size in bytes of fixed-point word */
    int *regoff;               /* byte offsets in register arrays */
    const struct reg *regs;    /* from current variant */
    int ppc_gp0_regnum;		/* GPR register 0 */
    int ppc_gplast_regnum;	/* GPR register 31 */
    int ppc_toc_regnum;		/* TOC register */
    int ppc_ps_regnum;	        /* Processor (or machine) status (%msr) */
    int ppc_cr_regnum;		/* Condition register */
    int ppc_lr_regnum;		/* Link register */
    int ppc_ctr_regnum;		/* Count register */
    int ppc_xer_regnum;		/* Integer exception register */
    int ppc_fpscr_regnum;	/* Floating point status and condition
    				   register */
    int ppc_mq_regnum;		/* Multiply/Divide extension register */
    int ppc_vr0_regnum;		/* First AltiVec register */
    int ppc_vrsave_regnum;	/* Last AltiVec register */
    int ppc_ev0_regnum;         /* First ev register */
    int ppc_ev31_regnum;        /* Last ev register */
    int lr_frame_offset;	/* Offset to ABI specific location where
                                   link register is saved.  */
};

#endif
