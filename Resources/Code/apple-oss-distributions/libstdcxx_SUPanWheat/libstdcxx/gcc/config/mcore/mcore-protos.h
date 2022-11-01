/* Prototypes for exported functions defined in mcore.c
   Copyright (C) 2000, 2002, 2003, 2004 Free Software Foundation, Inc.
   Contributed by Nick Clifton (nickc@redhat.com)

   This file is part of GCC.

   GCC is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GCC is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GCC; see the file COPYING.  If not, write to
   the Free Software Foundation, 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

extern const char * mcore_output_jump_label_table	(void);
extern void         mcore_expand_prolog          	(void);
extern void         mcore_expand_epilog          	(void);
extern int          mcore_const_ok_for_inline    	(long);
extern int          mcore_num_ones               	(int);
extern int          mcore_num_zeros              	(int);
extern int          mcore_initial_elimination_offset	(int, int);
extern int          mcore_byte_offset            	(unsigned int);
extern int          mcore_halfword_offset        	(unsigned int);
extern int          mcore_const_trick_uses_not   	(long);
extern void         mcore_override_options       	(void);
extern int          mcore_dllexport_name_p       	(const char *);
extern int          mcore_dllimport_name_p       	(const char *);
extern int          mcore_naked_function_p       	(void);

#ifdef TREE_CODE
#ifdef HAVE_MACHINE_MODES
extern int          mcore_num_arg_regs           	(enum machine_mode, tree);
#endif /* HAVE_MACHINE_MODES */

#ifdef RTX_CODE
extern rtx          mcore_function_value         	(tree, tree);
#endif /* RTX_CODE */
#endif /* TREE_CODE */

#ifdef RTX_CODE

extern GTY(()) rtx arch_compare_op0;
extern GTY(()) rtx arch_compare_op1;

extern const char * mcore_output_bclri         		(rtx, int);
extern const char * mcore_output_bseti         		(rtx, int);
extern const char * mcore_output_cmov          		(rtx *, int, const char *);
extern char *       mcore_output_call          		(rtx *, int);
extern int          mcore_is_dead                	(rtx, rtx);
extern int          mcore_expand_insv            	(rtx *);
extern int          mcore_modify_comparison      	(RTX_CODE);
extern bool         mcore_expand_block_move      	(rtx *);
extern const char * mcore_output_andn          		(rtx, rtx *);
extern void         mcore_print_operand_address  	(FILE *, rtx);
extern void         mcore_print_operand          	(FILE *, rtx, int);
extern rtx          mcore_gen_compare_reg        	(RTX_CODE);
extern int          mcore_symbolic_address_p     	(rtx);
extern bool         mcore_r15_operand_p			(rtx);
extern enum reg_class mcore_secondary_reload_class	(enum reg_class, enum machine_mode, rtx);
extern enum reg_class mcore_reload_class 		(rtx, enum reg_class);
extern int          mcore_is_same_reg            	(rtx, rtx);
extern int          mcore_arith_S_operand         	(rtx);

#ifdef HAVE_MACHINE_MODES
extern const char * mcore_output_move          		(rtx, rtx *, enum machine_mode);
extern const char * mcore_output_movedouble    		(rtx *, enum machine_mode);
extern int          mcore_arith_reg_operand       	(rtx, enum machine_mode);
extern int          mcore_general_movsrc_operand  	(rtx, enum machine_mode);
extern int          mcore_general_movdst_operand  	(rtx, enum machine_mode);
extern int          mcore_reload_operand          	(rtx, enum machine_mode);
extern int          mcore_arith_J_operand         	(rtx, enum machine_mode);
extern int          mcore_arith_K_operand         	(rtx, enum machine_mode);
extern int          mcore_arith_K_operand_not_0   	(rtx, enum machine_mode);
extern int          mcore_arith_M_operand         	(rtx, enum machine_mode);
extern int          mcore_arith_K_S_operand       	(rtx, enum machine_mode);
extern int          mcore_arith_imm_operand       	(rtx, enum machine_mode);
extern int          mcore_arith_any_imm_operand   	(rtx, enum machine_mode);
extern int          mcore_arith_O_operand         	(rtx, enum machine_mode);
extern int          mcore_literal_K_operand       	(rtx, enum machine_mode);
extern int          mcore_addsub_operand          	(rtx, enum machine_mode);
extern int          mcore_compare_operand         	(rtx, enum machine_mode);
extern int          mcore_load_multiple_operation 	(rtx, enum machine_mode);
extern int          mcore_store_multiple_operation	(rtx, enum machine_mode);
extern int          mcore_call_address_operand    	(rtx, enum machine_mode);

#ifdef TREE_CODE
extern rtx          mcore_function_arg           	(CUMULATIVE_ARGS, enum machine_mode, tree, int);
#endif /* TREE_CODE */
#endif /* HAVE_MACHINE_MODES */
#endif /* RTX_CODE */
