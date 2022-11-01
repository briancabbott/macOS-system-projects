/* Definitions of target machine for GNU compiler, for IBM S/390.
   Copyright (C) 2000, 2002, 2003, 2004 Free Software Foundation, Inc.
   Contributed by Hartmut Penner (hpenner@de.ibm.com)

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

/* Declare functions in s390.c.  */

extern void optimization_options (int, int);
extern void override_options (void);
extern bool s390_can_eliminate (int, int);
extern HOST_WIDE_INT s390_initial_elimination_offset (int, int);
extern void s390_emit_prologue (void);
extern void s390_emit_epilogue (bool);
extern void s390_function_profiler (FILE *, int);
extern void s390_conditional_register_usage (void);

#ifdef RTX_CODE
extern int s390_extra_constraint_str (rtx, int, const char *);
extern int s390_const_ok_for_constraint_p (HOST_WIDE_INT, int, const char *);
extern int const0_operand (rtx, enum machine_mode);
extern int consttable_operand (rtx, enum machine_mode);
extern int larl_operand (rtx, enum machine_mode);
extern int s_operand (rtx, enum machine_mode);
extern int shift_count_operand (rtx, enum machine_mode);
extern int bras_sym_operand (rtx, enum machine_mode);
extern int load_multiple_operation (rtx, enum machine_mode);
extern int store_multiple_operation (rtx, enum machine_mode);
extern int s390_single_part (rtx, enum machine_mode, enum machine_mode, int);
extern unsigned HOST_WIDE_INT s390_extract_part (rtx, enum machine_mode, int);
extern bool s390_split_ok_p (rtx, rtx, enum machine_mode, int);
extern bool s390_offset_p (rtx, rtx, rtx);
extern int tls_symbolic_operand (rtx);

extern int s390_match_ccmode (rtx, enum machine_mode);
extern enum machine_mode s390_tm_ccmode (rtx, rtx, int);
extern enum machine_mode s390_select_ccmode (enum rtx_code, rtx, rtx);
extern void s390_canonicalize_comparison (enum rtx_code *, rtx *, rtx *);
extern int s390_comparison (rtx op, enum machine_mode mode);
extern int s390_alc_comparison (rtx op, enum machine_mode mode);
extern int s390_slb_comparison (rtx op, enum machine_mode mode);
extern rtx s390_emit_compare (enum rtx_code, rtx, rtx);
extern void s390_emit_jump (rtx, rtx);
extern int symbolic_reference_mentioned_p (rtx);
extern int tls_symbolic_reference_mentioned_p (rtx);
extern int legitimate_la_operand_p (rtx);
extern int preferred_la_operand_p (rtx, rtx);
extern int legitimate_pic_operand_p (rtx);
extern int legitimate_constant_p (rtx);
extern int legitimate_reload_constant_p (rtx);
extern int legitimate_address_p (enum machine_mode, rtx, int);
extern rtx legitimize_pic_address (rtx, rtx);
extern rtx legitimize_address (rtx, rtx, enum machine_mode);
extern rtx legitimize_reload_address (rtx, enum machine_mode, int, int);
extern enum reg_class s390_preferred_reload_class (rtx, enum reg_class);
extern enum reg_class s390_secondary_input_reload_class (enum reg_class,
							 enum machine_mode,
							 rtx);
extern enum reg_class s390_secondary_output_reload_class (enum reg_class,
							  enum machine_mode,
							  rtx);
extern int s390_plus_operand (rtx, enum machine_mode);
extern void s390_expand_plus_operand (rtx, rtx, rtx);
extern void emit_symbolic_move (rtx *);
extern void s390_load_address (rtx, rtx);
extern void s390_expand_movmem (rtx, rtx, rtx);
extern void s390_expand_clrmem (rtx, rtx);
extern void s390_expand_cmpmem (rtx, rtx, rtx, rtx);
extern bool s390_expand_addcc (enum rtx_code, rtx, rtx, rtx, rtx, rtx);
extern rtx s390_return_addr_rtx (int, rtx);
extern rtx s390_back_chain_rtx (void);
extern rtx s390_emit_call (rtx, rtx, rtx, rtx);
extern void s390_expand_logical_operator (enum rtx_code,
					  enum machine_mode, rtx *);
extern bool s390_logical_operator_ok_p (rtx *);
extern void s390_narrow_logical_operator (enum rtx_code, rtx *, rtx *);
extern void s390_split_access_reg (rtx, rtx *, rtx *);

extern bool s390_output_addr_const_extra (FILE*, rtx);
extern void print_operand_address (FILE *, rtx);
extern void print_operand (FILE *, rtx, int);
extern void s390_output_constant_pool (rtx, rtx);
extern void s390_output_pool_entry (rtx, enum machine_mode, unsigned int);
extern void s390_trampoline_template (FILE *);
extern void s390_initialize_trampoline (rtx, rtx, rtx);
extern rtx s390_gen_rtx_const_DI (int, int);
extern void s390_output_dwarf_dtprel (FILE*, int, rtx);
extern int s390_agen_dep_p (rtx, rtx);
extern rtx s390_load_got (void);
extern void s390_emit_tpf_eh_return (rtx);

#endif /* RTX_CODE */

#ifdef TREE_CODE
extern void s390_function_arg_advance (CUMULATIVE_ARGS *, enum machine_mode,
				       tree, int);
#ifdef RTX_CODE
extern rtx s390_function_arg (CUMULATIVE_ARGS *, enum machine_mode, tree, int);
extern rtx s390_function_value (tree, enum machine_mode);
extern void s390_va_start (tree, rtx);
#endif /* RTX_CODE */
#endif /* TREE_CODE */
