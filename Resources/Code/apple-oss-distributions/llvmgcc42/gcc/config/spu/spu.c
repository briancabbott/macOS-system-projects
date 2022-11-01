/* Copyright (C) 2006 Free Software Foundation, Inc.

   This file is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free
   Software Foundation; either version 2 of the License, or (at your option) 
   any later version.

   This file is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License
   along with this file; see the file COPYING.  If not, write to the Free
   Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "insn-attr.h"
#include "flags.h"
#include "recog.h"
#include "obstack.h"
#include "tree.h"
#include "expr.h"
#include "optabs.h"
#include "except.h"
#include "function.h"
#include "output.h"
#include "basic-block.h"
#include "integrate.h"
#include "toplev.h"
#include "ggc.h"
#include "hashtab.h"
#include "tm_p.h"
#include "target.h"
#include "target-def.h"
#include "langhooks.h"
#include "reload.h"
#include "cfglayout.h"
#include "sched-int.h"
#include "params.h"
#include "assert.h"
#include "c-tree.h"
#include "c-common.h"
#include "machmode.h"
#include "tree-gimple.h"
#include "tm-constrs.h"
#include "spu-builtins.h"

/* LLVM LOCAL begin */
#ifdef ENABLE_LLVM
#undef INSN_SCHEDULING
#endif
/* LLVM LOCAL end */

/* Builtin types, data and prototypes. */
struct spu_builtin_range
{
  int low, high;
};

static struct spu_builtin_range spu_builtin_range[] = {
  {-0x40ll, 0x7fll},		/* SPU_BTI_7     */
  {-0x40ll, 0x3fll},		/* SPU_BTI_S7    */
  {0ll, 0x7fll},		/* SPU_BTI_U7    */
  {-0x200ll, 0x1ffll},		/* SPU_BTI_S10   */
  {-0x2000ll, 0x1fffll},	/* SPU_BTI_S10_4 */
  {0ll, 0x3fffll},		/* SPU_BTI_U14   */
  {-0x8000ll, 0xffffll},	/* SPU_BTI_16    */
  {-0x8000ll, 0x7fffll},	/* SPU_BTI_S16   */
  {-0x20000ll, 0x1ffffll},	/* SPU_BTI_S16_2 */
  {0ll, 0xffffll},		/* SPU_BTI_U16   */
  {0ll, 0x3ffffll},		/* SPU_BTI_U16_2 */
  {0ll, 0x3ffffll},		/* SPU_BTI_U18   */
};


/*  Target specific attribute specifications.  */
char regs_ever_allocated[FIRST_PSEUDO_REGISTER];

/*  Prototypes and external defs.  */
static void spu_init_builtins (void);
static bool spu_scalar_mode_supported_p (enum machine_mode mode);
static bool spu_vector_mode_supported_p (enum machine_mode mode);
static rtx adjust_operand (rtx op, HOST_WIDE_INT * start);
static rtx get_pic_reg (void);
static int need_to_save_reg (int regno, int saving);
static rtx frame_emit_store (int regno, rtx addr, HOST_WIDE_INT offset);
static rtx frame_emit_load (int regno, rtx addr, HOST_WIDE_INT offset);
static rtx frame_emit_add_imm (rtx dst, rtx src, HOST_WIDE_INT imm,
			       rtx scratch);
/* LLVM LOCAL begin */
#ifdef INSN_SCHEDULING
static void emit_nop_for_insn (rtx insn);
static bool insn_clobbers_hbr (rtx insn);
static void spu_emit_branch_hint (rtx before, rtx branch, rtx target,
				  int distance);
static rtx get_branch_target (rtx branch);
#endif
/* LLVM LOCAL end */
static void insert_branch_hints (void);
static void insert_nops (void);
static void spu_machine_dependent_reorg (void);
static int spu_sched_issue_rate (void);
static int spu_sched_variable_issue (FILE * dump, int verbose, rtx insn,
				     int can_issue_more);
static int get_pipe (rtx insn);
static int spu_sched_adjust_priority (rtx insn, int pri);
static int spu_sched_adjust_cost (rtx insn, rtx link, rtx dep_insn, int cost);
static tree spu_handle_fndecl_attribute (tree * node, tree name, tree args,
					 int flags,
					 bool *no_add_attrs);
static tree spu_handle_vector_attribute (tree * node, tree name, tree args,
					 int flags,
					 bool *no_add_attrs);
static int spu_naked_function_p (tree func);
static bool spu_pass_by_reference (int *cum, enum machine_mode mode,
				   tree type, bool named);
static tree spu_build_builtin_va_list (void);
static tree spu_gimplify_va_arg_expr (tree valist, tree type, tree * pre_p,
				      tree * post_p);
static int regno_aligned_for_load (int regno);
static int store_with_one_insn_p (rtx mem);
static int reg_align (rtx reg);
static int mem_is_padded_component_ref (rtx x);
static bool spu_assemble_integer (rtx x, unsigned int size, int aligned_p);
static void spu_asm_globalize_label (FILE * file, const char *name);
static bool spu_rtx_costs (rtx x, int code, int outer_code, int *total);
static bool spu_function_ok_for_sibcall (tree decl, tree exp);
static void spu_init_libfuncs (void);
static bool spu_return_in_memory (tree type, tree fntype);
static void fix_range (const char *);
static void spu_encode_section_info (tree, rtx, int);
/* LLVM LOCAL begin */
#ifdef INSN_SCHEDULING
static tree spu_builtin_mul_widen_even (tree);
static tree spu_builtin_mul_widen_odd (tree);
#endif
/* LLVM LOCAL end */
static tree spu_builtin_mask_for_load (void);

extern const char *reg_names[];
rtx spu_compare_op0, spu_compare_op1;

enum spu_immediate {
  SPU_NONE,
  SPU_IL,
  SPU_ILA,
  SPU_ILH,
  SPU_ILHU,
  SPU_ORI,
  SPU_ORHI,
  SPU_ORBI,
  SPU_IOHL
};
enum immediate_class
{
  IC_POOL,			/* constant pool */
  IC_IL1,			/* one il* instruction */
  IC_IL2,			/* both ilhu and iohl instructions */
  IC_IL1s,			/* one il* instruction */
  IC_IL2s,			/* both ilhu and iohl instructions */
  IC_FSMBI,			/* the fsmbi instruction */
  IC_CPAT,			/* one of the c*d instructions */
};

static enum spu_immediate which_immediate_load (HOST_WIDE_INT val);
static enum spu_immediate which_logical_immediate (HOST_WIDE_INT val);
static int cpat_info(unsigned char *arr, int size, int *prun, int *pstart);
static enum immediate_class classify_immediate (rtx op,
						enum machine_mode mode);

/* Built in types.  */
tree spu_builtin_types[SPU_BTI_MAX];

/*  TARGET overrides.  */

#undef TARGET_INIT_BUILTINS
#define TARGET_INIT_BUILTINS spu_init_builtins

#undef TARGET_EXPAND_BUILTIN
#define TARGET_EXPAND_BUILTIN spu_expand_builtin

#undef TARGET_EH_RETURN_FILTER_MODE
#define TARGET_EH_RETURN_FILTER_MODE spu_eh_return_filter_mode

/* The .8byte directive doesn't seem to work well for a 32 bit
   architecture. */
#undef TARGET_ASM_UNALIGNED_DI_OP
#define TARGET_ASM_UNALIGNED_DI_OP NULL

#undef TARGET_RTX_COSTS
#define TARGET_RTX_COSTS spu_rtx_costs

#undef TARGET_ADDRESS_COST
#define TARGET_ADDRESS_COST hook_int_rtx_0

#undef TARGET_SCHED_ISSUE_RATE
#define TARGET_SCHED_ISSUE_RATE spu_sched_issue_rate

#undef TARGET_SCHED_VARIABLE_ISSUE
#define TARGET_SCHED_VARIABLE_ISSUE spu_sched_variable_issue

#undef TARGET_SCHED_ADJUST_PRIORITY
#define TARGET_SCHED_ADJUST_PRIORITY spu_sched_adjust_priority

#undef TARGET_SCHED_ADJUST_COST
#define TARGET_SCHED_ADJUST_COST spu_sched_adjust_cost

const struct attribute_spec spu_attribute_table[];
#undef  TARGET_ATTRIBUTE_TABLE
#define TARGET_ATTRIBUTE_TABLE spu_attribute_table

#undef TARGET_ASM_INTEGER
#define TARGET_ASM_INTEGER spu_assemble_integer

#undef TARGET_SCALAR_MODE_SUPPORTED_P
#define TARGET_SCALAR_MODE_SUPPORTED_P	spu_scalar_mode_supported_p

#undef TARGET_VECTOR_MODE_SUPPORTED_P
#define TARGET_VECTOR_MODE_SUPPORTED_P	spu_vector_mode_supported_p

#undef TARGET_FUNCTION_OK_FOR_SIBCALL
#define TARGET_FUNCTION_OK_FOR_SIBCALL spu_function_ok_for_sibcall

#undef TARGET_ASM_GLOBALIZE_LABEL
#define TARGET_ASM_GLOBALIZE_LABEL spu_asm_globalize_label

#undef TARGET_PASS_BY_REFERENCE
#define TARGET_PASS_BY_REFERENCE spu_pass_by_reference

#undef TARGET_MUST_PASS_IN_STACK
#define TARGET_MUST_PASS_IN_STACK must_pass_in_stack_var_size

#undef TARGET_BUILD_BUILTIN_VA_LIST
#define TARGET_BUILD_BUILTIN_VA_LIST spu_build_builtin_va_list

#undef TARGET_SETUP_INCOMING_VARARGS
#define TARGET_SETUP_INCOMING_VARARGS spu_setup_incoming_varargs

#undef TARGET_MACHINE_DEPENDENT_REORG
#define TARGET_MACHINE_DEPENDENT_REORG spu_machine_dependent_reorg

#undef TARGET_GIMPLIFY_VA_ARG_EXPR
#define TARGET_GIMPLIFY_VA_ARG_EXPR spu_gimplify_va_arg_expr

#undef TARGET_DEFAULT_TARGET_FLAGS
#define TARGET_DEFAULT_TARGET_FLAGS (TARGET_DEFAULT)

#undef TARGET_INIT_LIBFUNCS
#define TARGET_INIT_LIBFUNCS spu_init_libfuncs

#undef TARGET_RETURN_IN_MEMORY
#define TARGET_RETURN_IN_MEMORY spu_return_in_memory

#undef  TARGET_ENCODE_SECTION_INFO
#define TARGET_ENCODE_SECTION_INFO spu_encode_section_info

#undef TARGET_VECTORIZE_BUILTIN_MUL_WIDEN_EVEN
#define TARGET_VECTORIZE_BUILTIN_MUL_WIDEN_EVEN spu_builtin_mul_widen_even

#undef TARGET_VECTORIZE_BUILTIN_MUL_WIDEN_ODD
#define TARGET_VECTORIZE_BUILTIN_MUL_WIDEN_ODD spu_builtin_mul_widen_odd

#undef TARGET_VECTORIZE_BUILTIN_MASK_FOR_LOAD
#define TARGET_VECTORIZE_BUILTIN_MASK_FOR_LOAD spu_builtin_mask_for_load

struct gcc_target targetm = TARGET_INITIALIZER;

/* Sometimes certain combinations of command options do not make sense
   on a particular target machine.  You can define a macro
   OVERRIDE_OPTIONS to take account of this. This macro, if defined, is
   executed once just after all the command options have been parsed.  */
void
spu_override_options (void)
{
  /* Override some of the default param values.  With so many registers
     larger values are better for these params.  */
  if (MAX_UNROLLED_INSNS == 100)
    MAX_UNROLLED_INSNS = 250;
  if (MAX_PENDING_LIST_LENGTH == 32)
    MAX_PENDING_LIST_LENGTH = 128;

  flag_omit_frame_pointer = 1;

  if (align_functions < 8)
    align_functions = 8;

  if (spu_fixed_range_string)
    fix_range (spu_fixed_range_string);
}

/* Handle an attribute requiring a FUNCTION_DECL; arguments as in
   struct attribute_spec.handler.  */

/*  Table of machine attributes.  */
const struct attribute_spec spu_attribute_table[] =
{
  /* { name, min_len, max_len, decl_req, type_req, fn_type_req, handler } */
  { "naked",          0, 0, true,  false, false, spu_handle_fndecl_attribute },
  { "spu_vector",     0, 0, false, true,  false, spu_handle_vector_attribute },
  { NULL,             0, 0, false, false, false, NULL }
};

/* True if MODE is valid for the target.  By "valid", we mean able to
   be manipulated in non-trivial ways.  In particular, this means all
   the arithmetic is supported.  */
static bool
spu_scalar_mode_supported_p (enum machine_mode mode)
{
  switch (mode)
    {
    case QImode:
    case HImode:
    case SImode:
    case SFmode:
    case DImode:
    case TImode:
    case DFmode:
      return true;

    default:
      return false;
    }
}

/* Similarly for vector modes.  "Supported" here is less strict.  At
   least some operations are supported; need to check optabs or builtins
   for further details.  */
static bool
spu_vector_mode_supported_p (enum machine_mode mode)
{
  switch (mode)
    {
    case V16QImode:
    case V8HImode:
    case V4SImode:
    case V2DImode:
    case V4SFmode:
    case V2DFmode:
      return true;

    default:
      return false;
    }
}

/* GCC assumes that in a paradoxical SUBREG the inner mode occupies the
   least significant bytes of the outer mode.  This function returns
   TRUE for the SUBREG's where this is correct.  */
int
valid_subreg (rtx op)
{
  enum machine_mode om = GET_MODE (op);
  enum machine_mode im = GET_MODE (SUBREG_REG (op));
  return om != VOIDmode && im != VOIDmode
    && (GET_MODE_SIZE (im) == GET_MODE_SIZE (om)
	|| (GET_MODE_SIZE (im) <= 4 && GET_MODE_SIZE (om) <= 4));
}

/* When insv and ext[sz]v ar passed a TI SUBREG, we want to strip it off
   and adjust the start offset.  */
static rtx
adjust_operand (rtx op, HOST_WIDE_INT * start)
{
  enum machine_mode mode;
  int op_size;
  /* Strip any SUBREG */
  if (GET_CODE (op) == SUBREG)
    {
      if (start)
	*start -=
	  GET_MODE_BITSIZE (GET_MODE (op)) -
	  GET_MODE_BITSIZE (GET_MODE (SUBREG_REG (op)));
      op = SUBREG_REG (op);
    }
  /* If it is smaller than SI, assure a SUBREG */
  op_size = GET_MODE_BITSIZE (GET_MODE (op));
  if (op_size < 32)
    {
      if (start)
	*start += 32 - op_size;
      op_size = 32;
    }
  /* If it is not a MODE_INT (and/or it is smaller than SI) add a SUBREG. */
  mode = mode_for_size (op_size, MODE_INT, 0);
  if (mode != GET_MODE (op))
    op = gen_rtx_SUBREG (mode, op, 0);
  return op;
}

void
spu_expand_extv (rtx ops[], int unsignedp)
{
  HOST_WIDE_INT width = INTVAL (ops[2]);
  HOST_WIDE_INT start = INTVAL (ops[3]);
  HOST_WIDE_INT src_size, dst_size;
  enum machine_mode src_mode, dst_mode;
  rtx dst = ops[0], src = ops[1];
  rtx s;

  dst = adjust_operand (ops[0], 0);
  dst_mode = GET_MODE (dst);
  dst_size = GET_MODE_BITSIZE (GET_MODE (dst));

  src = adjust_operand (src, &start);
  src_mode = GET_MODE (src);
  src_size = GET_MODE_BITSIZE (GET_MODE (src));

  if (start > 0)
    {
      s = gen_reg_rtx (src_mode);
      switch (src_mode)
	{
	case SImode:
	  emit_insn (gen_ashlsi3 (s, src, GEN_INT (start)));
	  break;
	case DImode:
	  emit_insn (gen_ashldi3 (s, src, GEN_INT (start)));
	  break;
	case TImode:
	  emit_insn (gen_ashlti3 (s, src, GEN_INT (start)));
	  break;
	default:
	  abort ();
	}
      src = s;
    }

  if (width < src_size)
    {
      rtx pat;
      int icode;
      switch (src_mode)
	{
	case SImode:
	  icode = unsignedp ? CODE_FOR_lshrsi3 : CODE_FOR_ashrsi3;
	  break;
	case DImode:
	  icode = unsignedp ? CODE_FOR_lshrdi3 : CODE_FOR_ashrdi3;
	  break;
	case TImode:
	  icode = unsignedp ? CODE_FOR_lshrti3 : CODE_FOR_ashrti3;
	  break;
	default:
	  abort ();
	}
      s = gen_reg_rtx (src_mode);
      pat = GEN_FCN (icode) (s, src, GEN_INT (src_size - width));
      emit_insn (pat);
      src = s;
    }

  convert_move (dst, src, unsignedp);
}

void
spu_expand_insv (rtx ops[])
{
  HOST_WIDE_INT width = INTVAL (ops[1]);
  HOST_WIDE_INT start = INTVAL (ops[2]);
  HOST_WIDE_INT maskbits;
  enum machine_mode dst_mode, src_mode;
  rtx dst = ops[0], src = ops[3];
  int dst_size, src_size;
  rtx mask;
  rtx shift_reg;
  int shift;


  if (GET_CODE (ops[0]) == MEM)
    dst = gen_reg_rtx (TImode);
  else
    dst = adjust_operand (dst, &start);
  dst_mode = GET_MODE (dst);
  dst_size = GET_MODE_BITSIZE (GET_MODE (dst));

  if (CONSTANT_P (src))
    {
      enum machine_mode m =
	(width <= 32 ? SImode : width <= 64 ? DImode : TImode);
      src = force_reg (m, convert_to_mode (m, src, 0));
    }
  src = adjust_operand (src, 0);
  src_mode = GET_MODE (src);
  src_size = GET_MODE_BITSIZE (GET_MODE (src));

  mask = gen_reg_rtx (dst_mode);
  shift_reg = gen_reg_rtx (dst_mode);
  shift = dst_size - start - width;

  /* It's not safe to use subreg here because the compiler assumes
     that the SUBREG_REG is right justified in the SUBREG. */
  convert_move (shift_reg, src, 1);

  if (shift > 0)
    {
      switch (dst_mode)
	{
	case SImode:
	  emit_insn (gen_ashlsi3 (shift_reg, shift_reg, GEN_INT (shift)));
	  break;
	case DImode:
	  emit_insn (gen_ashldi3 (shift_reg, shift_reg, GEN_INT (shift)));
	  break;
	case TImode:
	  emit_insn (gen_ashlti3 (shift_reg, shift_reg, GEN_INT (shift)));
	  break;
	default:
	  abort ();
	}
    }
  else if (shift < 0)
    abort ();

  switch (dst_size)
    {
    case 32:
      maskbits = (-1ll << (32 - width - start));
      if (start)
	maskbits += (1ll << (32 - start));
      emit_move_insn (mask, GEN_INT (maskbits));
      break;
    case 64:
      maskbits = (-1ll << (64 - width - start));
      if (start)
	maskbits += (1ll << (64 - start));
      emit_move_insn (mask, GEN_INT (maskbits));
      break;
    case 128:
      {
	unsigned char arr[16];
	int i = start / 8;
	memset (arr, 0, sizeof (arr));
	arr[i] = 0xff >> (start & 7);
	for (i++; i <= (start + width - 1) / 8; i++)
	  arr[i] = 0xff;
	arr[i - 1] &= 0xff << (7 - ((start + width - 1) & 7));
	emit_move_insn (mask, array_to_constant (TImode, arr));
      }
      break;
    default:
      abort ();
    }
  if (GET_CODE (ops[0]) == MEM)
    {
      rtx aligned = gen_reg_rtx (SImode);
      rtx low = gen_reg_rtx (SImode);
      rtx addr = gen_reg_rtx (SImode);
      rtx rotl = gen_reg_rtx (SImode);
      rtx mask0 = gen_reg_rtx (TImode);
      rtx mem;

      emit_move_insn (addr, XEXP (ops[0], 0));
      emit_insn (gen_andsi3 (aligned, addr, GEN_INT (-16)));
      emit_insn (gen_andsi3 (low, addr, GEN_INT (15)));
      emit_insn (gen_negsi2 (rotl, low));
      emit_insn (gen_rotqby_ti (shift_reg, shift_reg, rotl));
      emit_insn (gen_rotqmby_ti (mask0, mask, rotl));
      mem = change_address (ops[0], TImode, aligned);
      set_mem_alias_set (mem, 0);
      emit_move_insn (dst, mem);
      emit_insn (gen_selb (dst, dst, shift_reg, mask0));
      emit_move_insn (mem, dst);
      if (start + width > MEM_ALIGN (ops[0]))
	{
	  rtx shl = gen_reg_rtx (SImode);
	  rtx mask1 = gen_reg_rtx (TImode);
	  rtx dst1 = gen_reg_rtx (TImode);
	  rtx mem1;
	  emit_insn (gen_subsi3 (shl, GEN_INT (16), low));
	  emit_insn (gen_shlqby_ti (mask1, mask, shl));
	  mem1 = adjust_address (mem, TImode, 16);
	  set_mem_alias_set (mem1, 0);
	  emit_move_insn (dst1, mem1);
	  emit_insn (gen_selb (dst1, dst1, shift_reg, mask1));
	  emit_move_insn (mem1, dst1);
	}
    }
  else
    emit_insn (gen_selb (dst, dst, shift_reg, mask));
}


int
spu_expand_block_move (rtx ops[])
{
  HOST_WIDE_INT bytes, align, offset;
  rtx src, dst, sreg, dreg, target;
  int i;
  if (GET_CODE (ops[2]) != CONST_INT
      || GET_CODE (ops[3]) != CONST_INT
      || INTVAL (ops[2]) > (HOST_WIDE_INT) (MOVE_RATIO * 8))
    return 0;

  bytes = INTVAL (ops[2]);
  align = INTVAL (ops[3]);

  if (bytes <= 0)
    return 1;

  dst = ops[0];
  src = ops[1];

  if (align == 16)
    {
      for (offset = 0; offset + 16 <= bytes; offset += 16)
	{
	  dst = adjust_address (ops[0], V16QImode, offset);
	  src = adjust_address (ops[1], V16QImode, offset);
	  emit_move_insn (dst, src);
	}
      if (offset < bytes)
	{
	  rtx mask;
	  unsigned char arr[16] = { 0 };
	  for (i = 0; i < bytes - offset; i++)
	    arr[i] = 0xff;
	  dst = adjust_address (ops[0], V16QImode, offset);
	  src = adjust_address (ops[1], V16QImode, offset);
	  mask = gen_reg_rtx (V16QImode);
	  sreg = gen_reg_rtx (V16QImode);
	  dreg = gen_reg_rtx (V16QImode);
	  target = gen_reg_rtx (V16QImode);
	  emit_move_insn (mask, array_to_constant (V16QImode, arr));
	  emit_move_insn (dreg, dst);
	  emit_move_insn (sreg, src);
	  emit_insn (gen_selb (target, dreg, sreg, mask));
	  emit_move_insn (dst, target);
	}
      return 1;
    }
  return 0;
}

enum spu_comp_code
{ SPU_EQ, SPU_GT, SPU_GTU };


int spu_comp_icode[8][3] = {
  {CODE_FOR_ceq_qi, CODE_FOR_cgt_qi, CODE_FOR_clgt_qi},
  {CODE_FOR_ceq_hi, CODE_FOR_cgt_hi, CODE_FOR_clgt_hi},
  {CODE_FOR_ceq_si, CODE_FOR_cgt_si, CODE_FOR_clgt_si},
  {CODE_FOR_ceq_di, CODE_FOR_cgt_di, CODE_FOR_clgt_di},
  {CODE_FOR_ceq_ti, CODE_FOR_cgt_ti, CODE_FOR_clgt_ti},
  {CODE_FOR_ceq_sf, CODE_FOR_cgt_sf, 0},
  {0, 0, 0},
  {CODE_FOR_ceq_vec, 0, 0},
};

/* Generate a compare for CODE.  Return a brand-new rtx that represents
   the result of the compare.   GCC can figure this out too if we don't
   provide all variations of compares, but GCC always wants to use
   WORD_MODE, we can generate better code in most cases if we do it
   ourselves.  */
void
spu_emit_branch_or_set (int is_set, enum rtx_code code, rtx operands[])
{
  int reverse_compare = 0;
  int reverse_test = 0;
  rtx compare_result;
  rtx comp_rtx;
  rtx target = operands[0];
  enum machine_mode comp_mode;
  enum machine_mode op_mode;
  enum spu_comp_code scode;
  int index;

  /* When spu_compare_op1 is a CONST_INT change (X >= C) to (X > C-1),
     and so on, to keep the constant in operand 1. */
  if (GET_CODE (spu_compare_op1) == CONST_INT)
    {
      HOST_WIDE_INT val = INTVAL (spu_compare_op1) - 1;
      if (trunc_int_for_mode (val, GET_MODE (spu_compare_op0)) == val)
	switch (code)
	  {
	  case GE:
	    spu_compare_op1 = GEN_INT (val);
	    code = GT;
	    break;
	  case LT:
	    spu_compare_op1 = GEN_INT (val);
	    code = LE;
	    break;
	  case GEU:
	    spu_compare_op1 = GEN_INT (val);
	    code = GTU;
	    break;
	  case LTU:
	    spu_compare_op1 = GEN_INT (val);
	    code = LEU;
	    break;
	  default:
	    break;
	  }
    }

  switch (code)
    {
    case GE:
      reverse_compare = 1;
      reverse_test = 1;
      scode = SPU_GT;
      break;
    case LE:
      reverse_compare = 0;
      reverse_test = 1;
      scode = SPU_GT;
      break;
    case LT:
      reverse_compare = 1;
      reverse_test = 0;
      scode = SPU_GT;
      break;
    case GEU:
      reverse_compare = 1;
      reverse_test = 1;
      scode = SPU_GTU;
      break;
    case LEU:
      reverse_compare = 0;
      reverse_test = 1;
      scode = SPU_GTU;
      break;
    case LTU:
      reverse_compare = 1;
      reverse_test = 0;
      scode = SPU_GTU;
      break;
    case NE:
      reverse_compare = 0;
      reverse_test = 1;
      scode = SPU_EQ;
      break;

    case EQ:
      scode = SPU_EQ;
      break;
    case GT:
      scode = SPU_GT;
      break;
    case GTU:
      scode = SPU_GTU;
      break;
    default:
      scode = SPU_EQ;
      break;
    }

  comp_mode = SImode;
  op_mode = GET_MODE (spu_compare_op0);

  switch (op_mode)
    {
    case QImode:
      index = 0;
      comp_mode = QImode;
      break;
    case HImode:
      index = 1;
      comp_mode = HImode;
      break;
    case SImode:
      index = 2;
      break;
    case DImode:
      index = 3;
      break;
    case TImode:
      index = 4;
      break;
    case SFmode:
      index = 5;
      break;
    case DFmode:
      index = 6;
      break;
    case V16QImode:
    case V8HImode:
    case V4SImode:
    case V2DImode:
    case V4SFmode:
    case V2DFmode:
      index = 7;
      break;
    default:
      abort ();
    }

  if (GET_MODE (spu_compare_op1) == DFmode)
    {
      rtx reg = gen_reg_rtx (DFmode);
      if (!flag_unsafe_math_optimizations
	  || (scode != SPU_GT && scode != SPU_EQ))
	abort ();
      if (reverse_compare)
	emit_insn (gen_subdf3 (reg, spu_compare_op1, spu_compare_op0));
      else
	emit_insn (gen_subdf3 (reg, spu_compare_op0, spu_compare_op1));
      reverse_compare = 0;
      spu_compare_op0 = reg;
      spu_compare_op1 = CONST0_RTX (DFmode);
    }

  if (is_set == 0 && spu_compare_op1 == const0_rtx
      && (GET_MODE (spu_compare_op0) == SImode
	  || GET_MODE (spu_compare_op0) == HImode) && scode == SPU_EQ)
    {
      /* Don't need to set a register with the result when we are 
         comparing against zero and branching. */
      reverse_test = !reverse_test;
      compare_result = spu_compare_op0;
    }
  else
    {
      compare_result = gen_reg_rtx (comp_mode);

      if (reverse_compare)
	{
	  rtx t = spu_compare_op1;
	  spu_compare_op1 = spu_compare_op0;
	  spu_compare_op0 = t;
	}

      if (spu_comp_icode[index][scode] == 0)
	abort ();

      if (!(*insn_data[spu_comp_icode[index][scode]].operand[1].predicate)
	  (spu_compare_op0, op_mode))
	spu_compare_op0 = force_reg (op_mode, spu_compare_op0);
      if (!(*insn_data[spu_comp_icode[index][scode]].operand[2].predicate)
	  (spu_compare_op1, op_mode))
	spu_compare_op1 = force_reg (op_mode, spu_compare_op1);
      comp_rtx = GEN_FCN (spu_comp_icode[index][scode]) (compare_result,
							 spu_compare_op0,
							 spu_compare_op1);
      if (comp_rtx == 0)
	abort ();
      emit_insn (comp_rtx);

    }

  if (is_set == 0)
    {
      rtx bcomp;
      rtx loc_ref;

      /* We don't have branch on QI compare insns, so we convert the
         QI compare result to a HI result. */
      if (comp_mode == QImode)
	{
	  rtx old_res = compare_result;
	  compare_result = gen_reg_rtx (HImode);
	  comp_mode = HImode;
	  emit_insn (gen_extendqihi2 (compare_result, old_res));
	}

      if (reverse_test)
	bcomp = gen_rtx_EQ (comp_mode, compare_result, const0_rtx);
      else
	bcomp = gen_rtx_NE (comp_mode, compare_result, const0_rtx);

      loc_ref = gen_rtx_LABEL_REF (VOIDmode, target);
      emit_jump_insn (gen_rtx_SET (VOIDmode, pc_rtx,
				   gen_rtx_IF_THEN_ELSE (VOIDmode, bcomp,
							 loc_ref, pc_rtx)));
    }
  else if (is_set == 2)
    {
      int compare_size = GET_MODE_BITSIZE (comp_mode);
      int target_size = GET_MODE_BITSIZE (GET_MODE (target));
      enum machine_mode mode = mode_for_size (target_size, MODE_INT, 0);
      rtx select_mask;
      rtx op_t = operands[2];
      rtx op_f = operands[3];

      /* The result of the comparison can be SI, HI or QI mode.  Create a
         mask based on that result. */
      if (target_size > compare_size)
	{
	  select_mask = gen_reg_rtx (mode);
	  emit_insn (gen_extend_compare (select_mask, compare_result));
	}
      else if (target_size < compare_size)
	select_mask =
	  gen_rtx_SUBREG (mode, compare_result,
			  (compare_size - target_size) / BITS_PER_UNIT);
      else if (comp_mode != mode)
	select_mask = gen_rtx_SUBREG (mode, compare_result, 0);
      else
	select_mask = compare_result;

      if (GET_MODE (target) != GET_MODE (op_t)
	  || GET_MODE (target) != GET_MODE (op_f))
	abort ();

      if (reverse_test)
	emit_insn (gen_selb (target, op_t, op_f, select_mask));
      else
	emit_insn (gen_selb (target, op_f, op_t, select_mask));
    }
  else
    {
      if (reverse_test)
	emit_insn (gen_rtx_SET (VOIDmode, compare_result,
				gen_rtx_NOT (comp_mode, compare_result)));
      if (GET_MODE (target) == SImode && GET_MODE (compare_result) == HImode)
	emit_insn (gen_extendhisi2 (target, compare_result));
      else if (GET_MODE (target) == SImode
	       && GET_MODE (compare_result) == QImode)
	emit_insn (gen_extend_compare (target, compare_result));
      else
	emit_move_insn (target, compare_result);
    }
}

HOST_WIDE_INT
const_double_to_hwint (rtx x)
{
  HOST_WIDE_INT val;
  REAL_VALUE_TYPE rv;
  if (GET_MODE (x) == SFmode)
    {
      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_SINGLE (rv, val);
    }
  else if (GET_MODE (x) == DFmode)
    {
      long l[2];
      REAL_VALUE_FROM_CONST_DOUBLE (rv, x);
      REAL_VALUE_TO_TARGET_DOUBLE (rv, l);
      val = l[0];
      val = (val << 32) | (l[1] & 0xffffffff);
    }
  else
    abort ();
  return val;
}

rtx
hwint_to_const_double (enum machine_mode mode, HOST_WIDE_INT v)
{
  long tv[2];
  REAL_VALUE_TYPE rv;
  gcc_assert (mode == SFmode || mode == DFmode);

  if (mode == SFmode)
    tv[0] = (v << 32) >> 32;
  else if (mode == DFmode)
    {
      tv[1] = (v << 32) >> 32;
      tv[0] = v >> 32;
    }
  real_from_target (&rv, tv, mode);
  return CONST_DOUBLE_FROM_REAL_VALUE (rv, mode);
}

void
print_operand_address (FILE * file, register rtx addr)
{
  rtx reg;
  rtx offset;

  if (GET_CODE (addr) == AND
      && GET_CODE (XEXP (addr, 1)) == CONST_INT
      && INTVAL (XEXP (addr, 1)) == -16)
    addr = XEXP (addr, 0);

  switch (GET_CODE (addr))
    {
    case REG:
      fprintf (file, "0(%s)", reg_names[REGNO (addr)]);
      break;

    case PLUS:
      reg = XEXP (addr, 0);
      offset = XEXP (addr, 1);
      if (GET_CODE (offset) == REG)
	{
	  fprintf (file, "%s,%s", reg_names[REGNO (reg)],
		   reg_names[REGNO (offset)]);
	}
      else if (GET_CODE (offset) == CONST_INT)
	{
	  fprintf (file, HOST_WIDE_INT_PRINT_DEC "(%s)",
		   INTVAL (offset), reg_names[REGNO (reg)]);
	}
      else
	abort ();
      break;

    case CONST:
    case LABEL_REF:
    case SYMBOL_REF:
    case CONST_INT:
      output_addr_const (file, addr);
      break;

    default:
      debug_rtx (addr);
      abort ();
    }
}

void
print_operand (FILE * file, rtx x, int code)
{
  enum machine_mode mode = GET_MODE (x);
  HOST_WIDE_INT val;
  unsigned char arr[16];
  int xcode = GET_CODE (x);
  int i, info;
  if (GET_MODE (x) == VOIDmode)
    switch (code)
      {
      case 'L':			/* 128 bits, signed */
      case 'm':			/* 128 bits, signed */
      case 'T':			/* 128 bits, signed */
      case 't':			/* 128 bits, signed */
	mode = TImode;
	break;
      case 'K':			/* 64 bits, signed */
      case 'k':			/* 64 bits, signed */
      case 'D':			/* 64 bits, signed */
      case 'd':			/* 64 bits, signed */
	mode = DImode;
	break;
      case 'J':			/* 32 bits, signed */
      case 'j':			/* 32 bits, signed */
      case 's':			/* 32 bits, signed */
      case 'S':			/* 32 bits, signed */
	mode = SImode;
	break;
      }
  switch (code)
    {

    case 'j':			/* 32 bits, signed */
    case 'k':			/* 64 bits, signed */
    case 'm':			/* 128 bits, signed */
      if (xcode == CONST_INT
	  || xcode == CONST_DOUBLE || xcode == CONST_VECTOR)
	{
	  gcc_assert (logical_immediate_p (x, mode));
	  constant_to_array (mode, x, arr);
	  val = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
	  val = trunc_int_for_mode (val, SImode);
	  switch (which_logical_immediate (val))
	  {
	  case SPU_ORI:
	    break;
	  case SPU_ORHI:
	    fprintf (file, "h");
	    break;
	  case SPU_ORBI:
	    fprintf (file, "b");
	    break;
	  default:
	    gcc_unreachable();
	  }
	}
      else
	gcc_unreachable();
      return;

    case 'J':			/* 32 bits, signed */
    case 'K':			/* 64 bits, signed */
    case 'L':			/* 128 bits, signed */
      if (xcode == CONST_INT
	  || xcode == CONST_DOUBLE || xcode == CONST_VECTOR)
	{
	  gcc_assert (logical_immediate_p (x, mode)
		      || iohl_immediate_p (x, mode));
	  constant_to_array (mode, x, arr);
	  val = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
	  val = trunc_int_for_mode (val, SImode);
	  switch (which_logical_immediate (val))
	  {
	  case SPU_ORI:
	  case SPU_IOHL:
	    break;
	  case SPU_ORHI:
	    val = trunc_int_for_mode (val, HImode);
	    break;
	  case SPU_ORBI:
	    val = trunc_int_for_mode (val, QImode);
	    break;
	  default:
	    gcc_unreachable();
	  }
	  fprintf (file, HOST_WIDE_INT_PRINT_DEC, val);
	}
      else
	gcc_unreachable();
      return;

    case 't':			/* 128 bits, signed */
    case 'd':			/* 64 bits, signed */
    case 's':			/* 32 bits, signed */
      if (CONSTANT_P (x))
	{
	  enum immediate_class c = classify_immediate (x, mode);
	  switch (c)
	    {
	    case IC_IL1:
	      constant_to_array (mode, x, arr);
	      val = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
	      val = trunc_int_for_mode (val, SImode);
	      switch (which_immediate_load (val))
		{
		case SPU_IL:
		  break;
		case SPU_ILA:
		  fprintf (file, "a");
		  break;
		case SPU_ILH:
		  fprintf (file, "h");
		  break;
		case SPU_ILHU:
		  fprintf (file, "hu");
		  break;
		default:
		  gcc_unreachable ();
		}
	      break;
	    case IC_CPAT:
	      constant_to_array (mode, x, arr);
	      cpat_info (arr, GET_MODE_SIZE (mode), &info, 0);
	      if (info == 1)
		fprintf (file, "b");
	      else if (info == 2)
		fprintf (file, "h");
	      else if (info == 4)
		fprintf (file, "w");
	      else if (info == 8)
		fprintf (file, "d");
	      break;
	    case IC_IL1s:
	      if (xcode == CONST_VECTOR)
		{
		  x = CONST_VECTOR_ELT (x, 0);
		  xcode = GET_CODE (x);
		}
	      if (xcode == SYMBOL_REF || xcode == LABEL_REF || xcode == CONST)
		fprintf (file, "a");
	      else if (xcode == HIGH)
		fprintf (file, "hu");
	      break;
	    case IC_FSMBI:
	    case IC_IL2:
	    case IC_IL2s:
	    case IC_POOL:
	      abort ();
	    }
	}
      else
	gcc_unreachable ();
      return;

    case 'T':			/* 128 bits, signed */
    case 'D':			/* 64 bits, signed */
    case 'S':			/* 32 bits, signed */
      if (CONSTANT_P (x))
	{
	  enum immediate_class c = classify_immediate (x, mode);
	  switch (c)
	    {
	    case IC_IL1:
	      constant_to_array (mode, x, arr);
	      val = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
	      val = trunc_int_for_mode (val, SImode);
	      switch (which_immediate_load (val))
		{
		case SPU_IL:
		case SPU_ILA:
		  break;
		case SPU_ILH:
		case SPU_ILHU:
		  val = trunc_int_for_mode (((arr[0] << 8) | arr[1]), HImode);
		  break;
		default:
		  gcc_unreachable ();
		}
	      fprintf (file, HOST_WIDE_INT_PRINT_DEC, val);
	      break;
	    case IC_FSMBI:
	      constant_to_array (mode, x, arr);
	      val = 0;
	      for (i = 0; i < 16; i++)
		{
		  val <<= 1;
		  val |= arr[i] & 1;
		}
	      print_operand (file, GEN_INT (val), 0);
	      break;
	    case IC_CPAT:
	      constant_to_array (mode, x, arr);
	      cpat_info (arr, GET_MODE_SIZE (mode), 0, &info);
	      fprintf (file, HOST_WIDE_INT_PRINT_DEC, (HOST_WIDE_INT)info);
	      break;
	    case IC_IL1s:
	      if (xcode == CONST_VECTOR)
		{
		  x = CONST_VECTOR_ELT (x, 0);
		  xcode = GET_CODE (x);
		}
	      if (xcode == HIGH)
		{
		  output_addr_const (file, XEXP (x, 0));
		  fprintf (file, "@h");
		}
	      else
		output_addr_const (file, x);
	      break;
	    case IC_IL2:
	    case IC_IL2s:
	    case IC_POOL:
	      abort ();
	    }
	}
      else
	gcc_unreachable ();
      return;

    case 'C':
      if (xcode == CONST_INT)
	{
	  /* Only 4 least significant bits are relevant for generate
	     control word instructions. */
	  fprintf (file, HOST_WIDE_INT_PRINT_DEC, INTVAL (x) & 15);
	  return;
	}
      break;

    case 'M':			/* print code for c*d */
      if (GET_CODE (x) == CONST_INT)
	switch (INTVAL (x))
	  {
	  case 1:
	    fprintf (file, "b");
	    break;
	  case 2:
	    fprintf (file, "h");
	    break;
	  case 4:
	    fprintf (file, "w");
	    break;
	  case 8:
	    fprintf (file, "d");
	    break;
	  default:
	    gcc_unreachable();
	  }
      else
	gcc_unreachable();
      return;

    case 'N':			/* Negate the operand */
      if (xcode == CONST_INT)
	fprintf (file, HOST_WIDE_INT_PRINT_DEC, -INTVAL (x));
      else if (xcode == CONST_VECTOR)
	fprintf (file, HOST_WIDE_INT_PRINT_DEC,
		 -INTVAL (CONST_VECTOR_ELT (x, 0)));
      return;

    case 'I':			/* enable/disable interrupts */
      if (xcode == CONST_INT)
	fprintf (file, "%s",  INTVAL (x) == 0 ? "d" : "e");
      return;

    case 'b':			/* branch modifiers */
      if (xcode == REG)
	fprintf (file, "%s", GET_MODE (x) == HImode ? "h" : "");
      else if (COMPARISON_P (x))
	fprintf (file, "%s", xcode == NE ? "n" : "");
      return;

    case 'i':			/* indirect call */
      if (xcode == MEM)
	{
	  if (GET_CODE (XEXP (x, 0)) == REG)
	    /* Used in indirect function calls. */
	    fprintf (file, "%s", reg_names[REGNO (XEXP (x, 0))]);
	  else
	    output_address (XEXP (x, 0));
	}
      return;

    case 'p':			/* load/store */
      if (xcode == MEM)
	{
	  x = XEXP (x, 0);
	  xcode = GET_CODE (x);
	}
      if (xcode == AND)
	{
	  x = XEXP (x, 0);
	  xcode = GET_CODE (x);
	}
      if (xcode == REG)
	fprintf (file, "d");
      else if (xcode == CONST_INT)
	fprintf (file, "a");
      else if (xcode == CONST || xcode == SYMBOL_REF || xcode == LABEL_REF)
	fprintf (file, "r");
      else if (xcode == PLUS || xcode == LO_SUM)
	{
	  if (GET_CODE (XEXP (x, 1)) == REG)
	    fprintf (file, "x");
	  else
	    fprintf (file, "d");
	}
      return;

    case 0:
      if (xcode == REG)
	fprintf (file, "%s", reg_names[REGNO (x)]);
      else if (xcode == MEM)
	output_address (XEXP (x, 0));
      else if (xcode == CONST_VECTOR)
	print_operand (file, CONST_VECTOR_ELT (x, 0), 0);
      else
	output_addr_const (file, x);
      return;

    default:
      output_operand_lossage ("invalid %%xn code");
    }
  gcc_unreachable ();
}

extern char call_used_regs[];
extern char regs_ever_live[];

/* For PIC mode we've reserved PIC_OFFSET_TABLE_REGNUM, which is a
   caller saved register.  For leaf functions it is more efficient to
   use a volatile register because we won't need to save and restore the
   pic register.  This routine is only valid after register allocation
   is completed, so we can pick an unused register.  */
static rtx
get_pic_reg (void)
{
  rtx pic_reg = pic_offset_table_rtx;
  if (!reload_completed && !reload_in_progress)
    abort ();
  return pic_reg;
}

/* Split constant addresses to handle cases that are too large.  Also, add in
   the pic register when in PIC mode. */
int
spu_split_immediate (rtx * ops)
{
  enum machine_mode mode = GET_MODE (ops[0]);
  enum immediate_class c = classify_immediate (ops[1], mode);

  switch (c)
    {
    case IC_IL2:
      {
	unsigned char arrhi[16];
	unsigned char arrlo[16];
	rtx to, hi, lo;
	int i;
	constant_to_array (mode, ops[1], arrhi);
	to = no_new_pseudos ? ops[0] : gen_reg_rtx (mode);
	for (i = 0; i < 16; i += 4)
	  {
	    arrlo[i + 2] = arrhi[i + 2];
	    arrlo[i + 3] = arrhi[i + 3];
	    arrlo[i + 0] = arrlo[i + 1] = 0;
	    arrhi[i + 2] = arrhi[i + 3] = 0;
	  }
	hi = array_to_constant (mode, arrhi);
	lo = array_to_constant (mode, arrlo);
	emit_move_insn (to, hi);
	emit_insn (gen_rtx_SET
		   (VOIDmode, ops[0], gen_rtx_IOR (mode, to, lo)));
	return 1;
      }
    case IC_POOL:
      if (reload_in_progress || reload_completed)
	{
	  rtx mem = force_const_mem (mode, ops[1]);
	  if (TARGET_LARGE_MEM)
	    {
	      rtx addr = gen_rtx_REG (Pmode, REGNO (ops[0]));
	      emit_move_insn (addr, XEXP (mem, 0));
	      mem = replace_equiv_address (mem, addr);
	    }
	  emit_move_insn (ops[0], mem);
	  return 1;
	}
      break;
    case IC_IL1s:
    case IC_IL2s:
      if (reload_completed && GET_CODE (ops[1]) != HIGH)
	{
	  if (c == IC_IL2s)
	    {
	      emit_insn (gen_high (ops[0], ops[1]));
	      emit_insn (gen_low (ops[0], ops[0], ops[1]));
	    }
	  else if (flag_pic)
	    emit_insn (gen_pic (ops[0], ops[1]));
	  if (flag_pic)
	    {
	      rtx pic_reg = get_pic_reg ();
	      emit_insn (gen_addsi3 (ops[0], ops[0], pic_reg));
	      current_function_uses_pic_offset_table = 1;
	    }
	  return flag_pic || c == IC_IL2s;
	}
      break;
    case IC_IL1:
    case IC_FSMBI:
    case IC_CPAT:
      break;
    }
  return 0;
}

/* SAVING is TRUE when we are generating the actual load and store
   instructions for REGNO.  When determining the size of the stack
   needed for saving register we must allocate enough space for the
   worst case, because we don't always have the information early enough
   to not allocate it.  But we can at least eliminate the actual loads
   and stores during the prologue/epilogue.  */
static int
need_to_save_reg (int regno, int saving)
{
  if (regs_ever_live[regno] && !call_used_regs[regno])
    return 1;
  if (flag_pic
      && regno == PIC_OFFSET_TABLE_REGNUM
      && (!saving || current_function_uses_pic_offset_table)
      && (!saving
	  || !current_function_is_leaf || regs_ever_live[LAST_ARG_REGNUM]))
    return 1;
  return 0;
}

/* This function is only correct starting with local register
   allocation */
int
spu_saved_regs_size (void)
{
  int reg_save_size = 0;
  int regno;

  for (regno = FIRST_PSEUDO_REGISTER - 1; regno >= 0; --regno)
    if (need_to_save_reg (regno, 0))
      reg_save_size += 0x10;
  return reg_save_size;
}

static rtx
frame_emit_store (int regno, rtx addr, HOST_WIDE_INT offset)
{
  rtx reg = gen_rtx_REG (V4SImode, regno);
  rtx mem =
    gen_frame_mem (V4SImode, gen_rtx_PLUS (Pmode, addr, GEN_INT (offset)));
  return emit_insn (gen_movv4si (mem, reg));
}

static rtx
frame_emit_load (int regno, rtx addr, HOST_WIDE_INT offset)
{
  rtx reg = gen_rtx_REG (V4SImode, regno);
  rtx mem =
    gen_frame_mem (V4SImode, gen_rtx_PLUS (Pmode, addr, GEN_INT (offset)));
  return emit_insn (gen_movv4si (reg, mem));
}

/* This happens after reload, so we need to expand it.  */
static rtx
frame_emit_add_imm (rtx dst, rtx src, HOST_WIDE_INT imm, rtx scratch)
{
  rtx insn;
  if (satisfies_constraint_K (GEN_INT (imm)))
    {
      insn = emit_insn (gen_addsi3 (dst, src, GEN_INT (imm)));
    }
  else
    {
      insn = emit_insn (gen_movsi (scratch, gen_int_mode (imm, SImode)));
      REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_MAYBE_DEAD, const0_rtx,
					    REG_NOTES (insn));
      insn = emit_insn (gen_addsi3 (dst, src, scratch));
      if (REGNO (src) == REGNO (scratch))
	abort ();
    }
  if (REGNO (dst) == REGNO (scratch))
    REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_MAYBE_DEAD, const0_rtx,
					  REG_NOTES (insn));
  return insn;
}

/* Return nonzero if this function is known to have a null epilogue.  */

int
direct_return (void)
{
  if (reload_completed)
    {
      if (cfun->static_chain_decl == 0
	  && (spu_saved_regs_size ()
	      + get_frame_size ()
	      + current_function_outgoing_args_size
	      + current_function_pretend_args_size == 0)
	  && current_function_is_leaf)
	return 1;
    }
  return 0;
}

/*
   The stack frame looks like this:
         +-------------+
         |  incoming   | 
      AP |    args     | 
         +-------------+
         | $lr save    |
         +-------------+
 prev SP | back chain  | 
         +-------------+
         |  var args   | 
         |  reg save   | current_function_pretend_args_size bytes
         +-------------+
         |    ...      | 
         | saved regs  | spu_saved_regs_size() bytes
         +-------------+
         |    ...      | 
      FP |   vars      | get_frame_size()  bytes
         +-------------+
         |    ...      | 
         |  outgoing   | 
         |    args     | current_function_outgoing_args_size bytes
         +-------------+
         | $lr of next |
         |   frame     | 
         +-------------+
      SP | back chain  | 
         +-------------+

*/
void
spu_expand_prologue (void)
{
  HOST_WIDE_INT size = get_frame_size (), offset, regno;
  HOST_WIDE_INT total_size;
  HOST_WIDE_INT saved_regs_size;
  rtx sp_reg = gen_rtx_REG (Pmode, STACK_POINTER_REGNUM);
  rtx scratch_reg_0, scratch_reg_1;
  rtx insn, real;

  /* A NOTE_INSN_DELETED is supposed to be at the start and end of
     the "toplevel" insn chain.  */
  emit_note (NOTE_INSN_DELETED);

  if (flag_pic && optimize == 0)
    current_function_uses_pic_offset_table = 1;

  if (spu_naked_function_p (current_function_decl))
    return;

  scratch_reg_0 = gen_rtx_REG (SImode, LAST_ARG_REGNUM + 1);
  scratch_reg_1 = gen_rtx_REG (SImode, LAST_ARG_REGNUM + 2);

  saved_regs_size = spu_saved_regs_size ();
  total_size = size + saved_regs_size
    + current_function_outgoing_args_size
    + current_function_pretend_args_size;

  if (!current_function_is_leaf
      || current_function_calls_alloca || total_size > 0)
    total_size += STACK_POINTER_OFFSET;

  /* Save this first because code after this might use the link
     register as a scratch register. */
  if (!current_function_is_leaf)
    {
      insn = frame_emit_store (LINK_REGISTER_REGNUM, sp_reg, 16);
      RTX_FRAME_RELATED_P (insn) = 1;
    }

  if (total_size > 0)
    {
      offset = -current_function_pretend_args_size;
      for (regno = 0; regno < FIRST_PSEUDO_REGISTER; ++regno)
	if (need_to_save_reg (regno, 1))
	  {
	    offset -= 16;
	    insn = frame_emit_store (regno, sp_reg, offset);
	    RTX_FRAME_RELATED_P (insn) = 1;
	  }
    }

  if (flag_pic && current_function_uses_pic_offset_table)
    {
      rtx pic_reg = get_pic_reg ();
      insn = emit_insn (gen_load_pic_offset (pic_reg, scratch_reg_0));
      REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_MAYBE_DEAD, const0_rtx,
					    REG_NOTES (insn));
      insn = emit_insn (gen_subsi3 (pic_reg, pic_reg, scratch_reg_0));
      REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_MAYBE_DEAD, const0_rtx,
					    REG_NOTES (insn));
    }

  if (total_size > 0)
    {
      if (flag_stack_check)
	{
	  /* We compare against total_size-1 because
	     ($sp >= total_size) <=> ($sp > total_size-1) */
	  rtx scratch_v4si = gen_rtx_REG (V4SImode, REGNO (scratch_reg_0));
	  rtx sp_v4si = gen_rtx_REG (V4SImode, STACK_POINTER_REGNUM);
	  rtx size_v4si = spu_const (V4SImode, total_size - 1);
	  if (!satisfies_constraint_K (GEN_INT (total_size - 1)))
	    {
	      emit_move_insn (scratch_v4si, size_v4si);
	      size_v4si = scratch_v4si;
	    }
	  emit_insn (gen_cgt_v4si (scratch_v4si, sp_v4si, size_v4si));
	  emit_insn (gen_vec_extractv4si
		     (scratch_reg_0, scratch_v4si, GEN_INT (1)));
	  emit_insn (gen_spu_heq (scratch_reg_0, GEN_INT (0)));
	}

      /* Adjust the stack pointer, and make sure scratch_reg_0 contains
         the value of the previous $sp because we save it as the back
         chain. */
      if (total_size <= 2000)
	{
	  /* In this case we save the back chain first. */
	  insn = frame_emit_store (STACK_POINTER_REGNUM, sp_reg, -total_size);
	  insn =
	    frame_emit_add_imm (sp_reg, sp_reg, -total_size, scratch_reg_0);
	}
      else if (satisfies_constraint_K (GEN_INT (-total_size)))
	{
	  insn = emit_move_insn (scratch_reg_0, sp_reg);
	  insn =
	    emit_insn (gen_addsi3 (sp_reg, sp_reg, GEN_INT (-total_size)));
	}
      else
	{
	  insn = emit_move_insn (scratch_reg_0, sp_reg);
	  insn =
	    frame_emit_add_imm (sp_reg, sp_reg, -total_size, scratch_reg_1);
	}
      RTX_FRAME_RELATED_P (insn) = 1;
      real = gen_addsi3 (sp_reg, sp_reg, GEN_INT (-total_size));
      REG_NOTES (insn) =
	gen_rtx_EXPR_LIST (REG_FRAME_RELATED_EXPR, real, REG_NOTES (insn));

      if (total_size > 2000)
	{
	  /* Save the back chain ptr */
	  insn = frame_emit_store (REGNO (scratch_reg_0), sp_reg, 0);
	}

      if (frame_pointer_needed)
	{
	  rtx fp_reg = gen_rtx_REG (Pmode, HARD_FRAME_POINTER_REGNUM);
	  HOST_WIDE_INT fp_offset = STACK_POINTER_OFFSET
	    + current_function_outgoing_args_size;
	  /* Set the new frame_pointer */
	  insn = frame_emit_add_imm (fp_reg, sp_reg, fp_offset, scratch_reg_0);
	  RTX_FRAME_RELATED_P (insn) = 1;
	  real = gen_addsi3 (fp_reg, sp_reg, GEN_INT (fp_offset));
	  REG_NOTES (insn) = 
	    gen_rtx_EXPR_LIST (REG_FRAME_RELATED_EXPR,
			       real, REG_NOTES (insn));
	}
    }

  emit_note (NOTE_INSN_DELETED);
}

void
spu_expand_epilogue (bool sibcall_p)
{
  int size = get_frame_size (), offset, regno;
  HOST_WIDE_INT saved_regs_size, total_size;
  rtx sp_reg = gen_rtx_REG (Pmode, STACK_POINTER_REGNUM);
  rtx jump, scratch_reg_0;

  /* A NOTE_INSN_DELETED is supposed to be at the start and end of
     the "toplevel" insn chain.  */
  emit_note (NOTE_INSN_DELETED);

  if (spu_naked_function_p (current_function_decl))
    return;

  scratch_reg_0 = gen_rtx_REG (SImode, LAST_ARG_REGNUM + 1);

  saved_regs_size = spu_saved_regs_size ();
  total_size = size + saved_regs_size
    + current_function_outgoing_args_size
    + current_function_pretend_args_size;

  if (!current_function_is_leaf
      || current_function_calls_alloca || total_size > 0)
    total_size += STACK_POINTER_OFFSET;

  if (total_size > 0)
    {
      if (current_function_calls_alloca)
	/* Load it from the back chain because our save_stack_block and
	   restore_stack_block do nothing. */
	frame_emit_load (STACK_POINTER_REGNUM, sp_reg, 0);
      else
	frame_emit_add_imm (sp_reg, sp_reg, total_size, scratch_reg_0);


      if (saved_regs_size > 0)
	{
	  offset = -current_function_pretend_args_size;
	  for (regno = 0; regno < FIRST_PSEUDO_REGISTER; ++regno)
	    if (need_to_save_reg (regno, 1))
	      {
		offset -= 0x10;
		frame_emit_load (regno, sp_reg, offset);
	      }
	}
    }

  if (!current_function_is_leaf)
    frame_emit_load (LINK_REGISTER_REGNUM, sp_reg, 16);

  if (!sibcall_p)
    {
      emit_insn (gen_rtx_USE
		 (VOIDmode, gen_rtx_REG (SImode, LINK_REGISTER_REGNUM)));
      jump = emit_jump_insn (gen__return ());
      emit_barrier_after (jump);
    }

  emit_note (NOTE_INSN_DELETED);
}

rtx
spu_return_addr (int count, rtx frame ATTRIBUTE_UNUSED)
{
  if (count != 0)
    return 0;
  /* This is inefficient because it ends up copying to a save-register
     which then gets saved even though $lr has already been saved.  But
     it does generate better code for leaf functions and we don't need
     to use RETURN_ADDRESS_POINTER_REGNUM to get it working.  It's only
     used for __builtin_return_address anyway, so maybe we don't care if
     it's inefficient. */
  return get_hard_reg_initial_val (Pmode, LINK_REGISTER_REGNUM);
}


/* Given VAL, generate a constant appropriate for MODE.
   If MODE is a vector mode, every element will be VAL.
   For TImode, VAL will be zero extended to 128 bits. */
rtx
spu_const (enum machine_mode mode, HOST_WIDE_INT val)
{
  rtx inner;
  rtvec v;
  int units, i;

  gcc_assert (GET_MODE_CLASS (mode) == MODE_INT
	      || GET_MODE_CLASS (mode) == MODE_FLOAT
	      || GET_MODE_CLASS (mode) == MODE_VECTOR_INT
	      || GET_MODE_CLASS (mode) == MODE_VECTOR_FLOAT);

  if (GET_MODE_CLASS (mode) == MODE_INT)
    return immed_double_const (val, 0, mode);

  /* val is the bit representation of the float */
  if (GET_MODE_CLASS (mode) == MODE_FLOAT)
    return hwint_to_const_double (mode, val);

  if (GET_MODE_CLASS (mode) == MODE_VECTOR_INT)
    inner = immed_double_const (val, 0, GET_MODE_INNER (mode));
  else 
    inner = hwint_to_const_double (GET_MODE_INNER (mode), val);

  units = GET_MODE_NUNITS (mode);

  v = rtvec_alloc (units);

  for (i = 0; i < units; ++i)
    RTVEC_ELT (v, i) = inner;

  return gen_rtx_CONST_VECTOR (mode, v);
}

/* branch hint stuff */

/* The hardware requires 8 insns between a hint and the branch it
   effects.  This variable describes how many rtl instructions the
   compiler needs to see before inserting a hint.  (FIXME: We should
   accept less and insert nops to enforce it because hinting is always
   profitable for performance, but we do need to be careful of code
   size.) */
int spu_hint_dist = (8 * 4);

/* An array of these is used to propagate hints to predecessor blocks. */
struct spu_bb_info
{
  rtx prop_jump;		/* propagated from another block */
  basic_block bb;		/* the original block. */
};

/* LLVM LOCAL begin */
#ifdef INSN_SCHEDULING
/* The special $hbr register is used to prevent the insn scheduler from
   moving hbr insns across instructions which invalidate them.  It
   should only be used in a clobber, and this function searches for
   insns which clobber it.  */
static bool
insn_clobbers_hbr (rtx insn)
{
  if (INSN_P (insn) && GET_CODE (PATTERN (insn)) == PARALLEL)
    {
      rtx parallel = PATTERN (insn);
      rtx clobber;
      int j;
      for (j = XVECLEN (parallel, 0) - 1; j >= 0; j--)
	{
	  clobber = XVECEXP (parallel, 0, j);
	  if (GET_CODE (clobber) == CLOBBER
	      && GET_CODE (XEXP (clobber, 0)) == REG
	      && REGNO (XEXP (clobber, 0)) == HBR_REGNUM)
	    return 1;
	}
    }
  return 0;
}

static void
spu_emit_branch_hint (rtx before, rtx branch, rtx target, int distance)
{
  rtx branch_label;
  rtx hint, insn, prev, next;

  if (before == 0 || branch == 0 || target == 0)
    return;

  if (distance > 600)
    return;


  branch_label = gen_label_rtx ();
  LABEL_NUSES (branch_label)++;
  LABEL_PRESERVE_P (branch_label) = 1;
  insn = emit_label_before (branch_label, branch);
  branch_label = gen_rtx_LABEL_REF (VOIDmode, branch_label);

  /* If the previous insn is pipe0, make the hbr dual issue with it.  If
     the current insn is pipe0, dual issue with it. */
  prev = prev_active_insn (before);
  if (prev && get_pipe (prev) == 0)
    hint = emit_insn_before (gen_hbr (branch_label, target), before);
  else if (get_pipe (before) == 0 && distance > spu_hint_dist)
    {
      next = next_active_insn (before);
      hint = emit_insn_after (gen_hbr (branch_label, target), before);
      if (next)
	PUT_MODE (next, TImode);
    }
  else
    {
      hint = emit_insn_before (gen_hbr (branch_label, target), before);
      PUT_MODE (hint, TImode);
    }
  recog_memoized (hint);
}

/* Returns 0 if we don't want a hint for this branch.  Otherwise return
   the rtx for the branch target. */
static rtx
get_branch_target (rtx branch)
{
  if (GET_CODE (branch) == JUMP_INSN)
    {
      rtx set, src;

      /* Return statements */
      if (GET_CODE (PATTERN (branch)) == RETURN)
	return gen_rtx_REG (SImode, LINK_REGISTER_REGNUM);

      /* jump table */
      if (GET_CODE (PATTERN (branch)) == ADDR_VEC
	  || GET_CODE (PATTERN (branch)) == ADDR_DIFF_VEC)
	return 0;

      set = single_set (branch);
      src = SET_SRC (set);
      if (GET_CODE (SET_DEST (set)) != PC)
	abort ();

      if (GET_CODE (src) == IF_THEN_ELSE)
	{
	  rtx lab = 0;
	  rtx note = find_reg_note (branch, REG_BR_PROB, 0);
	  if (note)
	    {
	      /* If the more probable case is not a fall through, then
	         try a branch hint.  */
	      HOST_WIDE_INT prob = INTVAL (XEXP (note, 0));
	      if (prob > (REG_BR_PROB_BASE * 6 / 10)
		  && GET_CODE (XEXP (src, 1)) != PC)
		lab = XEXP (src, 1);
	      else if (prob < (REG_BR_PROB_BASE * 4 / 10)
		       && GET_CODE (XEXP (src, 2)) != PC)
		lab = XEXP (src, 2);
	    }
	  if (lab)
	    {
	      if (GET_CODE (lab) == RETURN)
		return gen_rtx_REG (SImode, LINK_REGISTER_REGNUM);
	      return lab;
	    }
	  return 0;
	}

      return src;
    }
  else if (GET_CODE (branch) == CALL_INSN)
    {
      rtx call;
      /* All of our call patterns are in a PARALLEL and the CALL is
         the first pattern in the PARALLEL. */
      if (GET_CODE (PATTERN (branch)) != PARALLEL)
	abort ();
      call = XVECEXP (PATTERN (branch), 0, 0);
      if (GET_CODE (call) == SET)
	call = SET_SRC (call);
      if (GET_CODE (call) != CALL)
	abort ();
      return XEXP (XEXP (call, 0), 0);
    }
  return 0;
}
#endif
/* LLVM LOCAL end */

static void
insert_branch_hints (void)
{
/* LLVM LOCAL begin */
#ifdef INSN_SCHEDULING
  struct spu_bb_info *spu_bb_info;
  rtx branch, insn, next;
  rtx branch_target = 0;
  int branch_addr = 0, insn_addr, head_addr;
  basic_block bb;
  unsigned int j;

  spu_bb_info =
    (struct spu_bb_info *) xcalloc (last_basic_block + 1,
				    sizeof (struct spu_bb_info));

  /* We need exact insn addresses and lengths.  */
  shorten_branches (get_insns ());

  FOR_EACH_BB_REVERSE (bb)
  {
    head_addr = INSN_ADDRESSES (INSN_UID (BB_HEAD (bb)));
    branch = 0;
    if (spu_bb_info[bb->index].prop_jump)
      {
	branch = spu_bb_info[bb->index].prop_jump;
	branch_target = get_branch_target (branch);
	branch_addr = INSN_ADDRESSES (INSN_UID (branch));
      }
    /* Search from end of a block to beginning.   In this loop, find
       jumps which need a branch and emit them only when:
       - it's an indirect branch and we're at the insn which sets
       the register  
       - we're at an insn that will invalidate the hint. e.g., a
       call, another hint insn, inline asm that clobbers $hbr, and
       some inlined operations (divmodsi4).  Don't consider jumps
       because they are only at the end of a block and are
       considered when we are deciding whether to propagate
       - we're getting too far away from the branch.  The hbr insns
       only have a signed 10 bit offset
       We go back as far as possible so the branch will be considered
       for propagation when we get to the beginning of the block.  */
    next = 0;
    for (insn = BB_END (bb); insn; insn = PREV_INSN (insn))
      {
	if (INSN_P (insn))
	  {
	    insn_addr = INSN_ADDRESSES (INSN_UID (insn));
	    if (branch && next
		&& ((GET_CODE (branch_target) == REG
		     && set_of (branch_target, insn) != NULL_RTX)
		    || insn_clobbers_hbr (insn)
		    || branch_addr - insn_addr > 600))
	      {
		int next_addr = INSN_ADDRESSES (INSN_UID (next));
		if (insn != BB_END (bb)
		    && branch_addr - next_addr >= spu_hint_dist)
		  {
		    if (dump_file)
		      fprintf (dump_file,
			       "hint for %i in block %i before %i\n",
			       INSN_UID (branch), bb->index, INSN_UID (next));
		    spu_emit_branch_hint (next, branch, branch_target,
					  branch_addr - next_addr);
		  }
		branch = 0;
	      }

	    /* JUMP_P will only be true at the end of a block.  When
	       branch is already set it means we've previously decided
	       to propagate a hint for that branch into this block. */
	    if (CALL_P (insn) || (JUMP_P (insn) && !branch))
	      {
		branch = 0;
		if ((branch_target = get_branch_target (insn)))
		  {
		    branch = insn;
		    branch_addr = insn_addr;
		  }
	      }

	    /* When a branch hint is emitted it will be inserted
	       before "next".  Make sure next is the beginning of a
	       cycle to minimize impact on the scheduled insns. */
	    if (GET_MODE (insn) == TImode)
	      next = insn;
	  }
	if (insn == BB_HEAD (bb))
	  break;
      }

    if (branch)
      {
	/* If we haven't emitted a hint for this branch yet, it might
	   be profitable to emit it in one of the predecessor blocks,
	   especially for loops.  */
	rtx bbend;
	basic_block prev = 0, prop = 0, prev2 = 0;
	int loop_exit = 0, simple_loop = 0;
	int next_addr = 0;
	if (next)
	  next_addr = INSN_ADDRESSES (INSN_UID (next));

	for (j = 0; j < EDGE_COUNT (bb->preds); j++)
	  if (EDGE_PRED (bb, j)->flags & EDGE_FALLTHRU)
	    prev = EDGE_PRED (bb, j)->src;
	  else
	    prev2 = EDGE_PRED (bb, j)->src;

	for (j = 0; j < EDGE_COUNT (bb->succs); j++)
	  if (EDGE_SUCC (bb, j)->flags & EDGE_LOOP_EXIT)
	    loop_exit = 1;
	  else if (EDGE_SUCC (bb, j)->dest == bb)
	    simple_loop = 1;

	/* If this branch is a loop exit then propagate to previous
	   fallthru block. This catches the cases when it is a simple
	   loop or when there is an initial branch into the loop. */
	if (prev && loop_exit && prev->loop_depth <= bb->loop_depth)
	  prop = prev;

	/* If there is only one adjacent predecessor.  Don't propagate
	   outside this loop.  This loop_depth test isn't perfect, but
	   I'm not sure the loop_father member is valid at this point.  */
	else if (prev && single_pred_p (bb)
		 && prev->loop_depth == bb->loop_depth)
	  prop = prev;

	/* If this is the JOIN block of a simple IF-THEN then
	   propogate the hint to the HEADER block. */
	else if (prev && prev2
		 && EDGE_COUNT (bb->preds) == 2
		 && EDGE_COUNT (prev->preds) == 1
		 && EDGE_PRED (prev, 0)->src == prev2
		 && prev2->loop_depth == bb->loop_depth
		 && GET_CODE (branch_target) != REG)
	  prop = prev;

	/* Don't propagate when:
	   - this is a simple loop and the hint would be too far
	   - this is not a simple loop and there are 16 insns in
	   this block already
	   - the predecessor block ends in a branch that will be
	   hinted
	   - the predecessor block ends in an insn that invalidates
	   the hint */
	if (prop
	    && prop->index >= 0
	    && (bbend = BB_END (prop))
	    && branch_addr - INSN_ADDRESSES (INSN_UID (bbend)) <
	    (simple_loop ? 600 : 16 * 4) && get_branch_target (bbend) == 0
	    && (JUMP_P (bbend) || !insn_clobbers_hbr (bbend)))
	  {
	    if (dump_file)
	      fprintf (dump_file, "propagate from %i to %i (loop depth %i) "
		       "for %i (loop_exit %i simple_loop %i dist %i)\n",
		       bb->index, prop->index, bb->loop_depth,
		       INSN_UID (branch), loop_exit, simple_loop,
		       branch_addr - INSN_ADDRESSES (INSN_UID (bbend)));

	    spu_bb_info[prop->index].prop_jump = branch;
	    spu_bb_info[prop->index].bb = bb;
	  }
	else if (next && branch_addr - next_addr >= spu_hint_dist)
	  {
	    if (dump_file)
	      fprintf (dump_file, "hint for %i in block %i before %i\n",
		       INSN_UID (branch), bb->index, INSN_UID (next));
	    spu_emit_branch_hint (next, branch, branch_target,
				  branch_addr - next_addr);
	  }
	branch = 0;
      }
  }
  free (spu_bb_info);
#endif
/* LLVM LOCAL end */
}

/* LLVM LOCAL begin */
#ifdef INSN_SCHEDULING
/* Emit a nop for INSN such that the two will dual issue.  This assumes
   INSN is 8-byte aligned.  When INSN is inline asm we emit an lnop.
   We check for TImode to handle a MULTI1 insn which has dual issued its
   first instruction.  get_pipe returns -1 for MULTI0, inline asm, or
   ADDR_VEC insns. */
static void
emit_nop_for_insn (rtx insn)
{
  int p;
  rtx new_insn;
  p = get_pipe (insn);
  if (p == 1 && GET_MODE (insn) == TImode)
    {
      new_insn = emit_insn_before (gen_nopn (GEN_INT (127)), insn);
      PUT_MODE (new_insn, TImode);
      PUT_MODE (insn, VOIDmode);
    }
  else
    new_insn = emit_insn_after (gen_lnop (), insn);
}
#endif
/* LLVM LOCAL end */

/* Insert nops in basic blocks to meet dual issue alignment
   requirements. */
static void
insert_nops (void)
{
/* LLVM LOCAL begin */
#ifdef INSN_SCHEDULING
  rtx insn, next_insn, prev_insn;
  int length;
  int addr;

  /* This sets up INSN_ADDRESSES. */
  shorten_branches (get_insns ());

  /* Keep track of length added by nops. */
  length = 0;

  prev_insn = 0;
  for (insn = get_insns (); insn; insn = next_insn)
    {
      next_insn = next_active_insn (insn);
      addr = INSN_ADDRESSES (INSN_UID (insn));
      if (GET_MODE (insn) == TImode
	  && next_insn
	  && GET_MODE (next_insn) != TImode
	  && ((addr + length) & 7) != 0)
	{
	  /* prev_insn will always be set because the first insn is
	     always 8-byte aligned. */
	  emit_nop_for_insn (prev_insn);
	  length += 4;
	}
      prev_insn = insn;
    }
#endif
/* LLVM LOCAL end */
}

static void
spu_machine_dependent_reorg (void)
{
  if (optimize > 0)
    {
      if (TARGET_BRANCH_HINTS)
	insert_branch_hints ();
      insert_nops ();
    }
}


/* Insn scheduling routines, primarily for dual issue. */
static int
spu_sched_issue_rate (void)
{
  return 2;
}

static int
spu_sched_variable_issue (FILE * dump ATTRIBUTE_UNUSED,
			  int verbose ATTRIBUTE_UNUSED, rtx insn,
			  int can_issue_more)
{
  if (GET_CODE (PATTERN (insn)) != USE
      && GET_CODE (PATTERN (insn)) != CLOBBER
      && get_pipe (insn) != -2)
    can_issue_more--;
  return can_issue_more;
}

static int
get_pipe (rtx insn)
{
  enum attr_type t;
  /* Handle inline asm */
  if (INSN_CODE (insn) == -1)
    return -1;
  t = get_attr_type (insn);
  switch (t)
    {
    case TYPE_CONVERT:
      return -2;
    case TYPE_MULTI0:
      return -1;

    case TYPE_FX2:
    case TYPE_FX3:
    case TYPE_SPR:
    case TYPE_NOP:
    case TYPE_FXB:
    case TYPE_FPD:
    case TYPE_FP6:
    case TYPE_FP7:
    case TYPE_IPREFETCH:
      return 0;

    case TYPE_LNOP:
    case TYPE_SHUF:
    case TYPE_LOAD:
    case TYPE_STORE:
    case TYPE_BR:
    case TYPE_MULTI1:
    case TYPE_HBR:
      return 1;
    default:
      abort ();
    }
}

static int
spu_sched_adjust_priority (rtx insn, int pri)
{
  int p = get_pipe (insn);
  /* Schedule UNSPEC_CONVERT's early so they have less effect on
   * scheduling.  */
  if (GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER
      || p == -2)
    return pri + 100; 
  /* Schedule pipe0 insns early for greedier dual issue. */
  if (p != 1)
    return pri + 50;
  return pri;
}

/* INSN is dependent on DEP_INSN. */
static int
spu_sched_adjust_cost (rtx insn, rtx link ATTRIBUTE_UNUSED,
		       rtx dep_insn ATTRIBUTE_UNUSED, int cost)
{
/* LLVM LOCAL begin */
#ifdef INSN_SCHEDULING
  if (GET_CODE (insn) == CALL_INSN)
    return cost - 2;
  /* The dfa scheduler sets cost to 0 for all anti-dependencies and the
     scheduler makes every insn in a block anti-dependent on the final
     jump_insn.  We adjust here so higher cost insns will get scheduled
     earlier. */
  if (GET_CODE (insn) == JUMP_INSN && REG_NOTE_KIND (link) == REG_DEP_ANTI)
    return INSN_COST (dep_insn) - 3;
  return cost;
#else
  /* If INSN_SCHEDULING is not defined, this function is merely a stub, so
     return something reasonable to make the compiler happy. */
  return cost;
#endif
/* LLVM LOCAL end */
}

/* Create a CONST_DOUBLE from a string.  */
struct rtx_def *
spu_float_const (const char *string, enum machine_mode mode)
{
  REAL_VALUE_TYPE value;
  value = REAL_VALUE_ATOF (string, mode);
  return CONST_DOUBLE_FROM_REAL_VALUE (value, mode);
}

/* Given a (CONST (PLUS (SYMBOL_REF) (CONST_INT))) return TRUE when the
   CONST_INT fits constraint 'K', i.e., is small. */
int
legitimate_const (rtx x, int aligned)
{
  /* We can never know if the resulting address fits in 18 bits and can be
     loaded with ila.  Instead we should use the HI and LO relocations to
     load a 32 bit address. */
  rtx sym, cst;

  gcc_assert (GET_CODE (x) == CONST);

  if (GET_CODE (XEXP (x, 0)) != PLUS)
    return 0;
  sym = XEXP (XEXP (x, 0), 0);
  cst = XEXP (XEXP (x, 0), 1);
  if (GET_CODE (sym) != SYMBOL_REF || GET_CODE (cst) != CONST_INT)
    return 0;
  if (aligned && ((INTVAL (cst) & 15) != 0 || !ALIGNED_SYMBOL_REF_P (sym)))
    return 0;
  return satisfies_constraint_K (cst);
}

int
spu_constant_address_p (rtx x)
{
  return (GET_CODE (x) == LABEL_REF || GET_CODE (x) == SYMBOL_REF
	  || GET_CODE (x) == CONST_INT || GET_CODE (x) == CONST
	  || GET_CODE (x) == HIGH);
}

static enum spu_immediate
which_immediate_load (HOST_WIDE_INT val)
{
  gcc_assert (val == trunc_int_for_mode (val, SImode));

  if (val >= -0x8000 && val <= 0x7fff)
    return SPU_IL;
  if (val >= 0 && val <= 0x3ffff)
    return SPU_ILA;
  if ((val & 0xffff) == ((val >> 16) & 0xffff))
    return SPU_ILH;
  if ((val & 0xffff) == 0)
    return SPU_ILHU;

  return SPU_NONE;
}

/* Return true when OP can be loaded by one of the il instructions, or
   when flow2 is not completed and OP can be loaded using ilhu and iohl. */
int
immediate_load_p (rtx op, enum machine_mode mode)
{
  if (CONSTANT_P (op))
    {
      enum immediate_class c = classify_immediate (op, mode);
      return c == IC_IL1 || (!flow2_completed && c == IC_IL2);
    }
  return 0;
}

/* Return true if the first SIZE bytes of arr is a constant that can be
   generated with cbd, chd, cwd or cdd.  When non-NULL, PRUN and PSTART
   represent the size and offset of the instruction to use. */
static int
cpat_info(unsigned char *arr, int size, int *prun, int *pstart)
{
  int cpat, run, i, start;
  cpat = 1;
  run = 0;
  start = -1;
  for (i = 0; i < size && cpat; i++)
    if (arr[i] != i+16)
      { 
	if (!run)
	  {
	    start = i;
	    if (arr[i] == 3)
	      run = 1;
	    else if (arr[i] == 2 && arr[i+1] == 3)
	      run = 2;
	    else if (arr[i] == 0)
	      {
		while (arr[i+run] == run && i+run < 16)
		  run++;
		if (run != 4 && run != 8)
		  cpat = 0;
	      }
	    else
	      cpat = 0;
	    if ((i & (run-1)) != 0)
	      cpat = 0;
	    i += run;
	  }
	else
	  cpat = 0;
      }
  if (cpat && (run || size < 16))
    {
      if (run == 0)
	run = 1;
      if (prun)
	*prun = run;
      if (pstart)
	*pstart = start == -1 ? 16-run : start;
      return 1;
    }
  return 0;
}

/* OP is a CONSTANT_P.  Determine what instructions can be used to load
   it into a register.  MODE is only valid when OP is a CONST_INT. */
static enum immediate_class
classify_immediate (rtx op, enum machine_mode mode)
{
  HOST_WIDE_INT val;
  unsigned char arr[16];
  int i, j, repeated, fsmbi;

  gcc_assert (CONSTANT_P (op));

  if (GET_MODE (op) != VOIDmode)
    mode = GET_MODE (op);

  /* A V4SI const_vector with all identical symbols is ok. */
  if (mode == V4SImode
      && GET_CODE (op) == CONST_VECTOR
      && GET_CODE (CONST_VECTOR_ELT (op, 0)) != CONST_INT
      && GET_CODE (CONST_VECTOR_ELT (op, 0)) != CONST_DOUBLE
      && CONST_VECTOR_ELT (op, 0) == CONST_VECTOR_ELT (op, 1)
      && CONST_VECTOR_ELT (op, 1) == CONST_VECTOR_ELT (op, 2)
      && CONST_VECTOR_ELT (op, 2) == CONST_VECTOR_ELT (op, 3))
    op = CONST_VECTOR_ELT (op, 0);

  switch (GET_CODE (op))
    {
    case SYMBOL_REF:
    case LABEL_REF:
      return TARGET_LARGE_MEM ? IC_IL2s : IC_IL1s;

    case CONST:
      return TARGET_LARGE_MEM
	|| !legitimate_const (op, 0) ? IC_IL2s : IC_IL1s;

    case HIGH:
      return IC_IL1s;

    case CONST_VECTOR:
      for (i = 0; i < GET_MODE_NUNITS (mode); i++)
	if (GET_CODE (CONST_VECTOR_ELT (op, i)) != CONST_INT
	    && GET_CODE (CONST_VECTOR_ELT (op, i)) != CONST_DOUBLE)
	  return IC_POOL;
      /* Fall through. */

    case CONST_INT:
    case CONST_DOUBLE:
      constant_to_array (mode, op, arr);

      /* Check that each 4-byte slot is identical. */
      repeated = 1;
      for (i = 4; i < 16; i += 4)
	for (j = 0; j < 4; j++)
	  if (arr[j] != arr[i + j])
	    repeated = 0;

      if (repeated)
	{
	  val = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
	  val = trunc_int_for_mode (val, SImode);

	  if (which_immediate_load (val) != SPU_NONE)
	    return IC_IL1;
	}

      /* Any mode of 2 bytes or smaller can be loaded with an il
         instruction. */
      gcc_assert (GET_MODE_SIZE (mode) > 2);

      fsmbi = 1;
      for (i = 0; i < 16 && fsmbi; i++)
	if (arr[i] != 0 && arr[i] != 0xff)
	  fsmbi = 0;
      if (fsmbi)
	return IC_FSMBI;

      if (cpat_info (arr, GET_MODE_SIZE (mode), 0, 0))
	return IC_CPAT;

      if (repeated)
	return IC_IL2;

      return IC_POOL;
    default:
      break;
    }
  gcc_unreachable ();
}

static enum spu_immediate
which_logical_immediate (HOST_WIDE_INT val)
{
  gcc_assert (val == trunc_int_for_mode (val, SImode));

  if (val >= -0x200 && val <= 0x1ff)
    return SPU_ORI;
  if (val >= 0 && val <= 0xffff)
    return SPU_IOHL;
  if ((val & 0xffff) == ((val >> 16) & 0xffff))
    {
      val = trunc_int_for_mode (val, HImode);
      if (val >= -0x200 && val <= 0x1ff)
	return SPU_ORHI;
      if ((val & 0xff) == ((val >> 8) & 0xff))
	{
	  val = trunc_int_for_mode (val, QImode);
	  if (val >= -0x200 && val <= 0x1ff)
	    return SPU_ORBI;
	}
    }
  return SPU_NONE;
}

int
logical_immediate_p (rtx op, enum machine_mode mode)
{
  HOST_WIDE_INT val;
  unsigned char arr[16];
  int i, j;

  gcc_assert (GET_CODE (op) == CONST_INT || GET_CODE (op) == CONST_DOUBLE
	      || GET_CODE (op) == CONST_VECTOR);

  if (GET_MODE (op) != VOIDmode)
    mode = GET_MODE (op);

  constant_to_array (mode, op, arr);

  /* Check that bytes are repeated. */
  for (i = 4; i < 16; i += 4)
    for (j = 0; j < 4; j++)
      if (arr[j] != arr[i + j])
	return 0;

  val = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
  val = trunc_int_for_mode (val, SImode);

  i = which_logical_immediate (val);
  return i != SPU_NONE && i != SPU_IOHL;
}

int
iohl_immediate_p (rtx op, enum machine_mode mode)
{
  HOST_WIDE_INT val;
  unsigned char arr[16];
  int i, j;

  gcc_assert (GET_CODE (op) == CONST_INT || GET_CODE (op) == CONST_DOUBLE
	      || GET_CODE (op) == CONST_VECTOR);

  if (GET_MODE (op) != VOIDmode)
    mode = GET_MODE (op);

  constant_to_array (mode, op, arr);

  /* Check that bytes are repeated. */
  for (i = 4; i < 16; i += 4)
    for (j = 0; j < 4; j++)
      if (arr[j] != arr[i + j])
	return 0;

  val = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
  val = trunc_int_for_mode (val, SImode);

  return val >= 0 && val <= 0xffff;
}

int
arith_immediate_p (rtx op, enum machine_mode mode,
		   HOST_WIDE_INT low, HOST_WIDE_INT high)
{
  HOST_WIDE_INT val;
  unsigned char arr[16];
  int bytes, i, j;

  gcc_assert (GET_CODE (op) == CONST_INT || GET_CODE (op) == CONST_DOUBLE
	      || GET_CODE (op) == CONST_VECTOR);

  if (GET_MODE (op) != VOIDmode)
    mode = GET_MODE (op);

  constant_to_array (mode, op, arr);

  if (VECTOR_MODE_P (mode))
    mode = GET_MODE_INNER (mode);

  bytes = GET_MODE_SIZE (mode);
  mode = mode_for_size (GET_MODE_BITSIZE (mode), MODE_INT, 0);

  /* Check that bytes are repeated. */
  for (i = bytes; i < 16; i += bytes)
    for (j = 0; j < bytes; j++)
      if (arr[j] != arr[i + j])
	return 0;

  val = arr[0];
  for (j = 1; j < bytes; j++)
    val = (val << 8) | arr[j];

  val = trunc_int_for_mode (val, mode);

  return val >= low && val <= high;
}

/* We accept:
   - any 32 bit constant (SImode, SFmode)
   - any constant that can be generated with fsmbi (any mode)
   - a 64 bit constant where the high and low bits are identical
     (DImode, DFmode)
   - a 128 bit constant where the four 32 bit words match. */
int
spu_legitimate_constant_p (rtx x)
{
  int i;
  /* V4SI with all identical symbols is valid. */
  if (GET_MODE (x) == V4SImode
      && (GET_CODE (CONST_VECTOR_ELT (x, 0)) == SYMBOL_REF
	  || GET_CODE (CONST_VECTOR_ELT (x, 0)) == LABEL_REF
	  || GET_CODE (CONST_VECTOR_ELT (x, 0)) == CONST
	  || GET_CODE (CONST_VECTOR_ELT (x, 0)) == HIGH))
    return CONST_VECTOR_ELT (x, 0) == CONST_VECTOR_ELT (x, 1)
	   && CONST_VECTOR_ELT (x, 1) == CONST_VECTOR_ELT (x, 2)
	   && CONST_VECTOR_ELT (x, 2) == CONST_VECTOR_ELT (x, 3);

  if (VECTOR_MODE_P (GET_MODE (x)))
    for (i = 0; i < GET_MODE_NUNITS (GET_MODE (x)); i++)
      if (GET_CODE (CONST_VECTOR_ELT (x, i)) != CONST_INT
	  && GET_CODE (CONST_VECTOR_ELT (x, i)) != CONST_DOUBLE)
	return 0;
  return 1;
}

/* Valid address are:
   - symbol_ref, label_ref, const
   - reg
   - reg + const, where either reg or const is 16 byte aligned
   - reg + reg, alignment doesn't matter
  The alignment matters in the reg+const case because lqd and stqd
  ignore the 4 least significant bits of the const.  (TODO: It might be
  preferable to allow any alignment and fix it up when splitting.) */
int
spu_legitimate_address (enum machine_mode mode ATTRIBUTE_UNUSED,
			rtx x, int reg_ok_strict)
{
  if (mode == TImode && GET_CODE (x) == AND
      && GET_CODE (XEXP (x, 1)) == CONST_INT
      && INTVAL (XEXP (x, 1)) == (HOST_WIDE_INT) -16)
    x = XEXP (x, 0);
  switch (GET_CODE (x))
    {
    case SYMBOL_REF:
    case LABEL_REF:
      return !TARGET_LARGE_MEM;

    case CONST:
      return !TARGET_LARGE_MEM && legitimate_const (x, 0);

    case CONST_INT:
      return INTVAL (x) >= 0 && INTVAL (x) <= 0x3ffff;

    case SUBREG:
      x = XEXP (x, 0);
      gcc_assert (GET_CODE (x) == REG);

    case REG:
      return INT_REG_OK_FOR_BASE_P (x, reg_ok_strict);

    case PLUS:
    case LO_SUM:
      {
	rtx op0 = XEXP (x, 0);
	rtx op1 = XEXP (x, 1);
	if (GET_CODE (op0) == SUBREG)
	  op0 = XEXP (op0, 0);
	if (GET_CODE (op1) == SUBREG)
	  op1 = XEXP (op1, 0);
	/* We can't just accept any aligned register because CSE can
	   change it to a register that is not marked aligned and then
	   recog will fail.   So we only accept frame registers because
	   they will only be changed to other frame registers. */
	if (GET_CODE (op0) == REG
	    && INT_REG_OK_FOR_BASE_P (op0, reg_ok_strict)
	    && GET_CODE (op1) == CONST_INT
	    && INTVAL (op1) >= -0x2000
	    && INTVAL (op1) <= 0x1fff
	    && (REGNO_PTR_FRAME_P (REGNO (op0)) || (INTVAL (op1) & 15) == 0))
	  return 1;
	if (GET_CODE (op0) == REG
	    && INT_REG_OK_FOR_BASE_P (op0, reg_ok_strict)
	    && GET_CODE (op1) == REG
	    && INT_REG_OK_FOR_INDEX_P (op1, reg_ok_strict))
	  return 1;
      }
      break;

    default:
      break;
    }
  return 0;
}

/* When the address is reg + const_int, force the const_int into a
   register.  */
rtx
spu_legitimize_address (rtx x, rtx oldx ATTRIBUTE_UNUSED,
			enum machine_mode mode)
{
  rtx op0, op1;
  /* Make sure both operands are registers.  */
  if (GET_CODE (x) == PLUS)
    {
      op0 = XEXP (x, 0);
      op1 = XEXP (x, 1);
      if (ALIGNED_SYMBOL_REF_P (op0))
	{
	  op0 = force_reg (Pmode, op0);
	  mark_reg_pointer (op0, 128);
	}
      else if (GET_CODE (op0) != REG)
	op0 = force_reg (Pmode, op0);
      if (ALIGNED_SYMBOL_REF_P (op1))
	{
	  op1 = force_reg (Pmode, op1);
	  mark_reg_pointer (op1, 128);
	}
      else if (GET_CODE (op1) != REG)
	op1 = force_reg (Pmode, op1);
      x = gen_rtx_PLUS (Pmode, op0, op1);
      if (spu_legitimate_address (mode, x, 0))
	return x;
    }
  return NULL_RTX;
}

/* Handle an attribute requiring a FUNCTION_DECL; arguments as in
   struct attribute_spec.handler.  */
static tree
spu_handle_fndecl_attribute (tree * node,
			     tree name,
			     tree args ATTRIBUTE_UNUSED,
			     int flags ATTRIBUTE_UNUSED, bool * no_add_attrs)
{
  if (TREE_CODE (*node) != FUNCTION_DECL)
    {
      warning (0, "`%s' attribute only applies to functions",
	       IDENTIFIER_POINTER (name));
      *no_add_attrs = true;
    }

  return NULL_TREE;
}

/* Handle the "vector" attribute.  */
static tree
spu_handle_vector_attribute (tree * node, tree name,
			     tree args ATTRIBUTE_UNUSED,
			     int flags ATTRIBUTE_UNUSED, bool * no_add_attrs)
{
  tree type = *node, result = NULL_TREE;
  enum machine_mode mode;
  int unsigned_p;

  while (POINTER_TYPE_P (type)
	 || TREE_CODE (type) == FUNCTION_TYPE
	 || TREE_CODE (type) == METHOD_TYPE || TREE_CODE (type) == ARRAY_TYPE)
    type = TREE_TYPE (type);

  mode = TYPE_MODE (type);

  unsigned_p = TYPE_UNSIGNED (type);
  switch (mode)
    {
    case DImode:
      result = (unsigned_p ? unsigned_V2DI_type_node : V2DI_type_node);
      break;
    case SImode:
      result = (unsigned_p ? unsigned_V4SI_type_node : V4SI_type_node);
      break;
    case HImode:
      result = (unsigned_p ? unsigned_V8HI_type_node : V8HI_type_node);
      break;
    case QImode:
      result = (unsigned_p ? unsigned_V16QI_type_node : V16QI_type_node);
      break;
    case SFmode:
      result = V4SF_type_node;
      break;
    case DFmode:
      result = V2DF_type_node;
      break;
    default:
      break;
    }

  /* Propagate qualifiers attached to the element type
     onto the vector type.  */
  if (result && result != type && TYPE_QUALS (type))
    result = build_qualified_type (result, TYPE_QUALS (type));

  *no_add_attrs = true;		/* No need to hang on to the attribute.  */

  if (!result)
    warning (0, "`%s' attribute ignored", IDENTIFIER_POINTER (name));
  else
    *node = reconstruct_complex_type (*node, result);

  return NULL_TREE;
}

/* Return non-zero if FUNC is a naked function.  */
static int
spu_naked_function_p (tree func)
{
  tree a;

  if (TREE_CODE (func) != FUNCTION_DECL)
    abort ();

  a = lookup_attribute ("naked", DECL_ATTRIBUTES (func));
  return a != NULL_TREE;
}

int
spu_initial_elimination_offset (int from, int to)
{
  int saved_regs_size = spu_saved_regs_size ();
  int sp_offset = 0;
  if (!current_function_is_leaf || current_function_outgoing_args_size
      || get_frame_size () || saved_regs_size)
    sp_offset = STACK_POINTER_OFFSET;
  if (from == FRAME_POINTER_REGNUM && to == STACK_POINTER_REGNUM)
    return (sp_offset + current_function_outgoing_args_size);
  else if (from == FRAME_POINTER_REGNUM && to == HARD_FRAME_POINTER_REGNUM)
    return 0;
  else if (from == ARG_POINTER_REGNUM && to == STACK_POINTER_REGNUM)
    return sp_offset + current_function_outgoing_args_size
      + get_frame_size () + saved_regs_size + STACK_POINTER_OFFSET;
  else if (from == ARG_POINTER_REGNUM && to == HARD_FRAME_POINTER_REGNUM)
    return get_frame_size () + saved_regs_size + sp_offset;
  return 0;
}

rtx
spu_function_value (tree type, tree func ATTRIBUTE_UNUSED)
{
  enum machine_mode mode = TYPE_MODE (type);
  int byte_size = ((mode == BLKmode)
		   ? int_size_in_bytes (type) : GET_MODE_SIZE (mode));

  /* Make sure small structs are left justified in a register. */
  if ((mode == BLKmode || (type && AGGREGATE_TYPE_P (type)))
      && byte_size <= UNITS_PER_WORD * MAX_REGISTER_RETURN && byte_size > 0)
    {
      enum machine_mode smode;
      rtvec v;
      int i;
      int nregs = (byte_size + UNITS_PER_WORD - 1) / UNITS_PER_WORD;
      int n = byte_size / UNITS_PER_WORD;
      v = rtvec_alloc (nregs);
      for (i = 0; i < n; i++)
	{
	  RTVEC_ELT (v, i) = gen_rtx_EXPR_LIST (VOIDmode,
						gen_rtx_REG (TImode,
							     FIRST_RETURN_REGNUM
							     + i),
						GEN_INT (UNITS_PER_WORD * i));
	  byte_size -= UNITS_PER_WORD;
	}

      if (n < nregs)
	{
	  if (byte_size < 4)
	    byte_size = 4;
	  smode =
	    smallest_mode_for_size (byte_size * BITS_PER_UNIT, MODE_INT);
	  RTVEC_ELT (v, n) =
	    gen_rtx_EXPR_LIST (VOIDmode,
			       gen_rtx_REG (smode, FIRST_RETURN_REGNUM + n),
			       GEN_INT (UNITS_PER_WORD * n));
	}
      return gen_rtx_PARALLEL (mode, v);
    }
  return gen_rtx_REG (mode, FIRST_RETURN_REGNUM);
}

rtx
spu_function_arg (CUMULATIVE_ARGS cum,
		  enum machine_mode mode,
		  tree type, int named ATTRIBUTE_UNUSED)
{
  int byte_size;

  if (cum >= MAX_REGISTER_ARGS)
    return 0;

  byte_size = ((mode == BLKmode)
	       ? int_size_in_bytes (type) : GET_MODE_SIZE (mode));

  /* The ABI does not allow parameters to be passed partially in
     reg and partially in stack. */
  if ((cum + (byte_size + 15) / 16) > MAX_REGISTER_ARGS)
    return 0;

  /* Make sure small structs are left justified in a register. */
  if ((mode == BLKmode || (type && AGGREGATE_TYPE_P (type)))
      && byte_size < UNITS_PER_WORD && byte_size > 0)
    {
      enum machine_mode smode;
      rtx gr_reg;
      if (byte_size < 4)
	byte_size = 4;
      smode = smallest_mode_for_size (byte_size * BITS_PER_UNIT, MODE_INT);
      gr_reg = gen_rtx_EXPR_LIST (VOIDmode,
				  gen_rtx_REG (smode, FIRST_ARG_REGNUM + cum),
				  const0_rtx);
      return gen_rtx_PARALLEL (mode, gen_rtvec (1, gr_reg));
    }
  else
    return gen_rtx_REG (mode, FIRST_ARG_REGNUM + cum);
}

/* Variable sized types are passed by reference.  */
static bool
spu_pass_by_reference (CUMULATIVE_ARGS * cum ATTRIBUTE_UNUSED,
		       enum machine_mode mode ATTRIBUTE_UNUSED,
		       tree type, bool named ATTRIBUTE_UNUSED)
{
  return type && TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST;
}


/* Var args. */

/* Create and return the va_list datatype.

   On SPU, va_list is an array type equivalent to

      typedef struct __va_list_tag
        {
            void *__args __attribute__((__aligned(16)));
            void *__skip __attribute__((__aligned(16)));
            
        } va_list[1];

   where __args points to the arg that will be returned by the next
   va_arg(), and __skip points to the previous stack frame such that
   when __args == __skip we should advance __args by 32 bytes. */
static tree
spu_build_builtin_va_list (void)
{
  tree f_args, f_skip, record, type_decl;
  bool owp;

  record = (*lang_hooks.types.make_type) (RECORD_TYPE);

  type_decl =
    build_decl (TYPE_DECL, get_identifier ("__va_list_tag"), record);

  f_args = build_decl (FIELD_DECL, get_identifier ("__args"), ptr_type_node);
  f_skip = build_decl (FIELD_DECL, get_identifier ("__skip"), ptr_type_node);

  DECL_FIELD_CONTEXT (f_args) = record;
  DECL_ALIGN (f_args) = 128;
  DECL_USER_ALIGN (f_args) = 1;

  DECL_FIELD_CONTEXT (f_skip) = record;
  DECL_ALIGN (f_skip) = 128;
  DECL_USER_ALIGN (f_skip) = 1;

  TREE_CHAIN (record) = type_decl;
  TYPE_NAME (record) = type_decl;
  TYPE_FIELDS (record) = f_args;
  TREE_CHAIN (f_args) = f_skip;

  /* We know this is being padded and we want it too.  It is an internal
     type so hide the warnings from the user. */
  owp = warn_padded;
  warn_padded = false;

  layout_type (record);

  warn_padded = owp;

  /* The correct type is an array type of one element.  */
  return build_array_type (record, build_index_type (size_zero_node));
}

/* Implement va_start by filling the va_list structure VALIST.
   NEXTARG points to the first anonymous stack argument.

   The following global variables are used to initialize
   the va_list structure:

     current_function_args_info;
       the CUMULATIVE_ARGS for this function

     current_function_arg_offset_rtx:
       holds the offset of the first anonymous stack argument
       (relative to the virtual arg pointer).  */

void
spu_va_start (tree valist, rtx nextarg)
{
  tree f_args, f_skip;
  tree args, skip, t;

  f_args = TYPE_FIELDS (TREE_TYPE (va_list_type_node));
  f_skip = TREE_CHAIN (f_args);

  valist = build_va_arg_indirect_ref (valist);
  args =
    build3 (COMPONENT_REF, TREE_TYPE (f_args), valist, f_args, NULL_TREE);
  skip =
    build3 (COMPONENT_REF, TREE_TYPE (f_skip), valist, f_skip, NULL_TREE);

  /* Find the __args area.  */
  t = make_tree (TREE_TYPE (args), nextarg);
  if (current_function_pretend_args_size > 0)
    t = build2 (PLUS_EXPR, TREE_TYPE (args), t,
		build_int_cst (integer_type_node, -STACK_POINTER_OFFSET));
  t = build2 (MODIFY_EXPR, TREE_TYPE (args), args, t);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);

  /* Find the __skip area.  */
  t = make_tree (TREE_TYPE (skip), virtual_incoming_args_rtx);
  t = build2 (PLUS_EXPR, TREE_TYPE (skip), t,
	      build_int_cst (integer_type_node,
			     (current_function_pretend_args_size
			      - STACK_POINTER_OFFSET)));
  t = build2 (MODIFY_EXPR, TREE_TYPE (skip), skip, t);
  TREE_SIDE_EFFECTS (t) = 1;
  expand_expr (t, const0_rtx, VOIDmode, EXPAND_NORMAL);
}

/* Gimplify va_arg by updating the va_list structure 
   VALIST as required to retrieve an argument of type
   TYPE, and returning that argument. 
   
   ret = va_arg(VALIST, TYPE);

   generates code equivalent to:
   
    paddedsize = (sizeof(TYPE) + 15) & -16;
    if (VALIST.__args + paddedsize > VALIST.__skip
	&& VALIST.__args <= VALIST.__skip)
      addr = VALIST.__skip + 32;
    else
      addr = VALIST.__args;
    VALIST.__args = addr + paddedsize;
    ret = *(TYPE *)addr;
 */
static tree
spu_gimplify_va_arg_expr (tree valist, tree type, tree * pre_p,
			  tree * post_p ATTRIBUTE_UNUSED)
{
  tree f_args, f_skip;
  tree args, skip;
  HOST_WIDE_INT size, rsize;
  tree paddedsize, addr, tmp;
  bool pass_by_reference_p;

  f_args = TYPE_FIELDS (TREE_TYPE (va_list_type_node));
  f_skip = TREE_CHAIN (f_args);

  valist = build1 (INDIRECT_REF, TREE_TYPE (TREE_TYPE (valist)), valist);
  args =
    build3 (COMPONENT_REF, TREE_TYPE (f_args), valist, f_args, NULL_TREE);
  skip =
    build3 (COMPONENT_REF, TREE_TYPE (f_skip), valist, f_skip, NULL_TREE);

  addr = create_tmp_var (ptr_type_node, "va_arg");
  DECL_POINTER_ALIAS_SET (addr) = get_varargs_alias_set ();

  /* if an object is dynamically sized, a pointer to it is passed
     instead of the object itself. */
  pass_by_reference_p = spu_pass_by_reference (NULL, TYPE_MODE (type), type,
					       false);
  if (pass_by_reference_p)
    type = build_pointer_type (type);
  size = int_size_in_bytes (type);
  rsize = ((size + UNITS_PER_WORD - 1) / UNITS_PER_WORD) * UNITS_PER_WORD;

  /* build conditional expression to calculate addr. The expression
     will be gimplified later. */
  paddedsize = fold_convert (ptr_type_node, size_int (rsize));
  tmp = build2 (PLUS_EXPR, ptr_type_node, args, paddedsize);
  tmp = build2 (TRUTH_AND_EXPR, boolean_type_node,
		build2 (GT_EXPR, boolean_type_node, tmp, skip),
		build2 (LE_EXPR, boolean_type_node, args, skip));

  tmp = build3 (COND_EXPR, ptr_type_node, tmp,
		build2 (PLUS_EXPR, ptr_type_node, skip,
			fold_convert (ptr_type_node, size_int (32))), args);

  tmp = build2 (MODIFY_EXPR, ptr_type_node, addr, tmp);
  gimplify_and_add (tmp, pre_p);

  /* update VALIST.__args */
  tmp = build2 (PLUS_EXPR, ptr_type_node, addr, paddedsize);
  tmp = build2 (MODIFY_EXPR, TREE_TYPE (args), args, tmp);
  gimplify_and_add (tmp, pre_p);

  addr = fold_convert (build_pointer_type (type), addr);

  if (pass_by_reference_p)
    addr = build_va_arg_indirect_ref (addr);

  return build_va_arg_indirect_ref (addr);
}

/* Save parameter registers starting with the register that corresponds
   to the first unnamed parameters.  If the first unnamed parameter is
   in the stack then save no registers.  Set pretend_args_size to the
   amount of space needed to save the registers. */
void
spu_setup_incoming_varargs (CUMULATIVE_ARGS * cum, enum machine_mode mode,
			    tree type, int *pretend_size, int no_rtl)
{
  if (!no_rtl)
    {
      rtx tmp;
      int regno;
      int offset;
      int ncum = *cum;

      /* cum currently points to the last named argument, we want to
         start at the next argument. */
      FUNCTION_ARG_ADVANCE (ncum, mode, type, 1);

      offset = -STACK_POINTER_OFFSET;
      for (regno = ncum; regno < MAX_REGISTER_ARGS; regno++)
	{
	  tmp = gen_frame_mem (V4SImode,
			       plus_constant (virtual_incoming_args_rtx,
					      offset));
	  emit_move_insn (tmp,
			  gen_rtx_REG (V4SImode, FIRST_ARG_REGNUM + regno));
	  offset += 16;
	}
      *pretend_size = offset + STACK_POINTER_OFFSET;
    }
}

void
spu_conditional_register_usage (void)
{
  if (flag_pic)
    {
      fixed_regs[PIC_OFFSET_TABLE_REGNUM] = 1;
      call_used_regs[PIC_OFFSET_TABLE_REGNUM] = 1;
    }
  global_regs[INTR_REGNUM] = 1;
}

/* This is called to decide when we can simplify a load instruction.  We
   must only return true for registers which we know will always be
   aligned.  Taking into account that CSE might replace this reg with
   another one that has not been marked aligned.  
   So this is really only true for frame, stack and virtual registers,
   which we know are always aligned and should not be adversely effected
   by CSE.  */
static int
regno_aligned_for_load (int regno)
{
  return regno == FRAME_POINTER_REGNUM
    || regno == HARD_FRAME_POINTER_REGNUM
    || regno == STACK_POINTER_REGNUM
    || (regno >= FIRST_VIRTUAL_REGISTER && regno <= LAST_VIRTUAL_REGISTER);
}

/* Return TRUE when mem is known to be 16-byte aligned. */
int
aligned_mem_p (rtx mem)
{
  if (MEM_ALIGN (mem) >= 128)
    return 1;
  if (GET_MODE_SIZE (GET_MODE (mem)) >= 16)
    return 1;
  if (GET_CODE (XEXP (mem, 0)) == PLUS)
    {
      rtx p0 = XEXP (XEXP (mem, 0), 0);
      rtx p1 = XEXP (XEXP (mem, 0), 1);
      if (regno_aligned_for_load (REGNO (p0)))
	{
	  if (GET_CODE (p1) == REG && regno_aligned_for_load (REGNO (p1)))
	    return 1;
	  if (GET_CODE (p1) == CONST_INT && (INTVAL (p1) & 15) == 0)
	    return 1;
	}
    }
  else if (GET_CODE (XEXP (mem, 0)) == REG)
    {
      if (regno_aligned_for_load (REGNO (XEXP (mem, 0))))
	return 1;
    }
  else if (ALIGNED_SYMBOL_REF_P (XEXP (mem, 0)))
    return 1;
  else if (GET_CODE (XEXP (mem, 0)) == CONST)
    {
      rtx p0 = XEXP (XEXP (XEXP (mem, 0), 0), 0);
      rtx p1 = XEXP (XEXP (XEXP (mem, 0), 0), 1);
      if (GET_CODE (p0) == SYMBOL_REF
	  && GET_CODE (p1) == CONST_INT && (INTVAL (p1) & 15) == 0)
	return 1;
    }
  return 0;
}

/* Encode symbol attributes (local vs. global, tls model) of a SYMBOL_REF
   into its SYMBOL_REF_FLAGS.  */
static void
spu_encode_section_info (tree decl, rtx rtl, int first)
{
  default_encode_section_info (decl, rtl, first);

  /* If a variable has a forced alignment to < 16 bytes, mark it with
     SYMBOL_FLAG_ALIGN1.  */
  if (TREE_CODE (decl) == VAR_DECL
      && DECL_USER_ALIGN (decl) && DECL_ALIGN (decl) < 128)
    SYMBOL_REF_FLAGS (XEXP (rtl, 0)) |= SYMBOL_FLAG_ALIGN1;
}

/* Return TRUE if we are certain the mem refers to a complete object
   which is both 16-byte aligned and padded to a 16-byte boundary.  This
   would make it safe to store with a single instruction. 
   We guarantee the alignment and padding for static objects by aligning
   all of them to 16-bytes. (DATA_ALIGNMENT and CONSTANT_ALIGNMENT.)
   FIXME: We currently cannot guarantee this for objects on the stack
   because assign_parm_setup_stack calls assign_stack_local with the
   alignment of the parameter mode and in that case the alignment never
   gets adjusted by LOCAL_ALIGNMENT. */
static int
store_with_one_insn_p (rtx mem)
{
  rtx addr = XEXP (mem, 0);
  if (GET_MODE (mem) == BLKmode)
    return 0;
  /* Only static objects. */
  if (GET_CODE (addr) == SYMBOL_REF)
    {
      /* We use the associated declaration to make sure the access is
         referring to the whole object.
         We check both MEM_EXPR and and SYMBOL_REF_DECL.  I'm not sure
         if it is necessary.  Will there be cases where one exists, and
         the other does not?  Will there be cases where both exist, but
         have different types?  */
      tree decl = MEM_EXPR (mem);
      if (decl
	  && TREE_CODE (decl) == VAR_DECL
	  && GET_MODE (mem) == TYPE_MODE (TREE_TYPE (decl)))
	return 1;
      decl = SYMBOL_REF_DECL (addr);
      if (decl
	  && TREE_CODE (decl) == VAR_DECL
	  && GET_MODE (mem) == TYPE_MODE (TREE_TYPE (decl)))
	return 1;
    }
  return 0;
}

int
spu_expand_mov (rtx * ops, enum machine_mode mode)
{
  if (GET_CODE (ops[0]) == SUBREG && !valid_subreg (ops[0]))
    abort ();

  if (GET_CODE (ops[1]) == SUBREG && !valid_subreg (ops[1]))
    {
      rtx from = SUBREG_REG (ops[1]);
      enum machine_mode imode = GET_MODE (from);

      gcc_assert (GET_MODE_CLASS (mode) == MODE_INT
		  && GET_MODE_CLASS (imode) == MODE_INT
		  && subreg_lowpart_p (ops[1]));

      if (GET_MODE_SIZE (imode) < 4)
	{
	  from = gen_rtx_SUBREG (SImode, from, 0);
	  imode = SImode;
	}

      if (GET_MODE_SIZE (mode) < GET_MODE_SIZE (imode))
	{
	  enum insn_code icode = trunc_optab->handlers[mode][imode].insn_code;
	  emit_insn (GEN_FCN (icode) (ops[0], from));
	}
      else
	emit_insn (gen_extend_insn (ops[0], from, mode, imode, 1));
      return 1;
    }

  /* At least one of the operands needs to be a register. */
  if ((reload_in_progress | reload_completed) == 0
      && !register_operand (ops[0], mode) && !register_operand (ops[1], mode))
    {
      rtx temp = force_reg (mode, ops[1]);
      emit_move_insn (ops[0], temp);
      return 1;
    }
  if (reload_in_progress || reload_completed)
    {
      if (CONSTANT_P (ops[1]))
	return spu_split_immediate (ops);
      return 0;
    }
  else
    {
      if (GET_CODE (ops[0]) == MEM)
	{
	  if (!spu_valid_move (ops))
	    {
	      emit_insn (gen_store (ops[0], ops[1], gen_reg_rtx (TImode),
				    gen_reg_rtx (TImode)));
	      return 1;
	    }
	}
      else if (GET_CODE (ops[1]) == MEM)
	{
	  if (!spu_valid_move (ops))
	    {
	      emit_insn (gen_load
			 (ops[0], ops[1], gen_reg_rtx (TImode),
			  gen_reg_rtx (SImode)));
	      return 1;
	    }
	}
      /* Catch the SImode immediates greater than 0x7fffffff, and sign
         extend them. */
      if (GET_CODE (ops[1]) == CONST_INT)
	{
	  HOST_WIDE_INT val = trunc_int_for_mode (INTVAL (ops[1]), mode);
	  if (val != INTVAL (ops[1]))
	    {
	      emit_move_insn (ops[0], GEN_INT (val));
	      return 1;
	    }
	}
    }
  return 0;
}

static int
reg_align (rtx reg)
{
  /* For now, only frame registers are known to be aligned at all times.
     We can't trust REGNO_POINTER_ALIGN because optimization will move
     registers around, potentially changing an "aligned" register in an
     address to an unaligned register, which would result in an invalid
     address. */
  int regno = REGNO (reg);
  return REGNO_PTR_FRAME_P (regno) ? REGNO_POINTER_ALIGN (regno) : 1;
}

void
spu_split_load (rtx * ops)
{
  enum machine_mode mode = GET_MODE (ops[0]);
  rtx addr, load, rot, mem, p0, p1;
  int rot_amt;

  addr = XEXP (ops[1], 0);

  rot = 0;
  rot_amt = 0;
  if (GET_CODE (addr) == PLUS)
    {
      /* 8 cases:
         aligned reg   + aligned reg     => lqx
         aligned reg   + unaligned reg   => lqx, rotqby
         aligned reg   + aligned const   => lqd
         aligned reg   + unaligned const => lqd, rotqbyi
         unaligned reg + aligned reg     => lqx, rotqby
         unaligned reg + unaligned reg   => lqx, a, rotqby (1 scratch)
         unaligned reg + aligned const   => lqd, rotqby
         unaligned reg + unaligned const -> not allowed by legitimate address
       */
      p0 = XEXP (addr, 0);
      p1 = XEXP (addr, 1);
      if (reg_align (p0) < 128)
	{
	  if (GET_CODE (p1) == REG && reg_align (p1) < 128)
	    {
	      emit_insn (gen_addsi3 (ops[3], p0, p1));
	      rot = ops[3];
	    }
	  else
	    rot = p0;
	}
      else
	{
	  if (GET_CODE (p1) == CONST_INT && (INTVAL (p1) & 15))
	    {
	      rot_amt = INTVAL (p1) & 15;
	      p1 = GEN_INT (INTVAL (p1) & -16);
	      addr = gen_rtx_PLUS (SImode, p0, p1);
	    }
	  else if (GET_CODE (p1) == REG && reg_align (p1) < 128)
	    rot = p1;
	}
    }
  else if (GET_CODE (addr) == REG)
    {
      if (reg_align (addr) < 128)
	rot = addr;
    }
  else if (GET_CODE (addr) == CONST)
    {
      if (GET_CODE (XEXP (addr, 0)) == PLUS
	  && ALIGNED_SYMBOL_REF_P (XEXP (XEXP (addr, 0), 0))
	  && GET_CODE (XEXP (XEXP (addr, 0), 1)) == CONST_INT)
	{
	  rot_amt = INTVAL (XEXP (XEXP (addr, 0), 1));
	  if (rot_amt & -16)
	    addr = gen_rtx_CONST (Pmode,
				  gen_rtx_PLUS (Pmode,
						XEXP (XEXP (addr, 0), 0),
						GEN_INT (rot_amt & -16)));
	  else
	    addr = XEXP (XEXP (addr, 0), 0);
	}
      else
	rot = addr;
    }
  else if (GET_CODE (addr) == CONST_INT)
    {
      rot_amt = INTVAL (addr);
      addr = GEN_INT (rot_amt & -16);
    }
  else if (!ALIGNED_SYMBOL_REF_P (addr))
    rot = addr;

  if (GET_MODE_SIZE (mode) < 4)
    rot_amt += GET_MODE_SIZE (mode) - 4;

  rot_amt &= 15;

  if (rot && rot_amt)
    {
      emit_insn (gen_addsi3 (ops[3], rot, GEN_INT (rot_amt)));
      rot = ops[3];
      rot_amt = 0;
    }

  load = ops[2];

  addr = gen_rtx_AND (SImode, copy_rtx (addr), GEN_INT (-16));
  mem = change_address (ops[1], TImode, addr);

  emit_insn (gen_movti (load, mem));

  if (rot)
    emit_insn (gen_rotqby_ti (load, load, rot));
  else if (rot_amt)
    emit_insn (gen_rotlti3 (load, load, GEN_INT (rot_amt * 8)));

  if (reload_completed)
    emit_move_insn (ops[0], gen_rtx_REG (GET_MODE (ops[0]), REGNO (load)));
  else
    emit_insn (gen_spu_convert (ops[0], load));
}

void
spu_split_store (rtx * ops)
{
  enum machine_mode mode = GET_MODE (ops[0]);
  rtx pat = ops[2];
  rtx reg = ops[3];
  rtx addr, p0, p1, p1_lo, smem;
  int aform;
  int scalar;

  addr = XEXP (ops[0], 0);

  if (GET_CODE (addr) == PLUS)
    {
      /* 8 cases:
         aligned reg   + aligned reg     => lqx, c?x, shuf, stqx
         aligned reg   + unaligned reg   => lqx, c?x, shuf, stqx
         aligned reg   + aligned const   => lqd, c?d, shuf, stqx
         aligned reg   + unaligned const => lqd, c?d, shuf, stqx
         unaligned reg + aligned reg     => lqx, c?x, shuf, stqx
         unaligned reg + unaligned reg   => lqx, c?x, shuf, stqx
         unaligned reg + aligned const   => lqd, c?d, shuf, stqx
         unaligned reg + unaligned const -> not allowed by legitimate address
       */
      aform = 0;
      p0 = XEXP (addr, 0);
      p1 = p1_lo = XEXP (addr, 1);
      if (GET_CODE (p0) == REG && GET_CODE (p1) == CONST_INT)
	{
	  p1_lo = GEN_INT (INTVAL (p1) & 15);
	  p1 = GEN_INT (INTVAL (p1) & -16);
	  addr = gen_rtx_PLUS (SImode, p0, p1);
	}
    }
  else if (GET_CODE (addr) == REG)
    {
      aform = 0;
      p0 = addr;
      p1 = p1_lo = const0_rtx;
    }
  else
    {
      aform = 1;
      p0 = gen_rtx_REG (SImode, STACK_POINTER_REGNUM);
      p1 = 0;			/* aform doesn't use p1 */
      p1_lo = addr;
      if (ALIGNED_SYMBOL_REF_P (addr))
	p1_lo = const0_rtx;
      else if (GET_CODE (addr) == CONST)
	{
	  if (GET_CODE (XEXP (addr, 0)) == PLUS
	      && ALIGNED_SYMBOL_REF_P (XEXP (XEXP (addr, 0), 0))
	      && GET_CODE (XEXP (XEXP (addr, 0), 1)) == CONST_INT)
	    {
	      HOST_WIDE_INT v = INTVAL (XEXP (XEXP (addr, 0), 1));
	      if ((v & -16) != 0)
		addr = gen_rtx_CONST (Pmode,
				      gen_rtx_PLUS (Pmode,
						    XEXP (XEXP (addr, 0), 0),
						    GEN_INT (v & -16)));
	      else
		addr = XEXP (XEXP (addr, 0), 0);
	      p1_lo = GEN_INT (v & 15);
	    }
	}
      else if (GET_CODE (addr) == CONST_INT)
	{
	  p1_lo = GEN_INT (INTVAL (addr) & 15);
	  addr = GEN_INT (INTVAL (addr) & -16);
	}
    }

  addr = gen_rtx_AND (SImode, copy_rtx (addr), GEN_INT (-16));

  scalar = store_with_one_insn_p (ops[0]);
  if (!scalar)
    {
      /* We could copy the flags from the ops[0] MEM to mem here,
         We don't because we want this load to be optimized away if
         possible, and copying the flags will prevent that in certain
         cases, e.g. consider the volatile flag. */

      rtx lmem = change_address (ops[0], TImode, copy_rtx (addr));
      set_mem_alias_set (lmem, 0);
      emit_insn (gen_movti (reg, lmem));

      if (!p0 || reg_align (p0) >= 128)
	p0 = stack_pointer_rtx;
      if (!p1_lo)
	p1_lo = const0_rtx;

      emit_insn (gen_cpat (pat, p0, p1_lo, GEN_INT (GET_MODE_SIZE (mode))));
      emit_insn (gen_shufb (reg, ops[1], reg, pat));
    }
  else if (reload_completed)
    {
      if (GET_CODE (ops[1]) == REG)
	emit_move_insn (reg, gen_rtx_REG (GET_MODE (reg), REGNO (ops[1])));
      else if (GET_CODE (ops[1]) == SUBREG)
	emit_move_insn (reg,
			gen_rtx_REG (GET_MODE (reg),
				     REGNO (SUBREG_REG (ops[1]))));
      else
	abort ();
    }
  else
    {
      if (GET_CODE (ops[1]) == REG)
	emit_insn (gen_spu_convert (reg, ops[1]));
      else if (GET_CODE (ops[1]) == SUBREG)
	emit_insn (gen_spu_convert (reg, SUBREG_REG (ops[1])));
      else
	abort ();
    }

  if (GET_MODE_SIZE (mode) < 4 && scalar)
    emit_insn (gen_shlqby_ti
	       (reg, reg, GEN_INT (4 - GET_MODE_SIZE (mode))));

  smem = change_address (ops[0], TImode, addr);
  /* We can't use the previous alias set because the memory has changed
     size and can potentially overlap objects of other types.  */
  set_mem_alias_set (smem, 0);

  emit_insn (gen_movti (smem, reg));
}

/* Return TRUE if X is MEM which is a struct member reference
   and the member can safely be loaded and stored with a single
   instruction because it is padded. */
static int
mem_is_padded_component_ref (rtx x)
{
  tree t = MEM_EXPR (x);
  tree r;
  if (!t || TREE_CODE (t) != COMPONENT_REF)
    return 0;
  t = TREE_OPERAND (t, 1);
  if (!t || TREE_CODE (t) != FIELD_DECL
      || DECL_ALIGN (t) < 128 || AGGREGATE_TYPE_P (TREE_TYPE (t)))
    return 0;
  /* Only do this for RECORD_TYPEs, not UNION_TYPEs. */
  r = DECL_FIELD_CONTEXT (t);
  if (!r || TREE_CODE (r) != RECORD_TYPE)
    return 0;
  /* Make sure they are the same mode */
  if (GET_MODE (x) != TYPE_MODE (TREE_TYPE (t)))
    return 0;
  /* If there are no following fields then the field alignment assures
     the structure is padded to the alignment which means this field is
     padded too.  */
  if (TREE_CHAIN (t) == 0)
    return 1;
  /* If the following field is also aligned then this field will be
     padded. */
  t = TREE_CHAIN (t);
  if (TREE_CODE (t) == FIELD_DECL && DECL_ALIGN (t) >= 128)
    return 1;
  return 0;
}

/* Parse the -mfixed-range= option string.  */
static void
fix_range (const char *const_str)
{
  int i, first, last;
  char *str, *dash, *comma;
  
  /* str must be of the form REG1'-'REG2{,REG1'-'REG} where REG1 and
     REG2 are either register names or register numbers.  The effect
     of this option is to mark the registers in the range from REG1 to
     REG2 as ``fixed'' so they won't be used by the compiler.  */
  
  i = strlen (const_str);
  str = (char *) alloca (i + 1);
  memcpy (str, const_str, i + 1);
  
  while (1)
    {
      dash = strchr (str, '-');
      if (!dash)
	{
	  warning (0, "value of -mfixed-range must have form REG1-REG2");
	  return;
	}
      *dash = '\0';
      comma = strchr (dash + 1, ',');
      if (comma)
	*comma = '\0';
      
      first = decode_reg_name (str);
      if (first < 0)
	{
	  warning (0, "unknown register name: %s", str);
	  return;
	}
      
      last = decode_reg_name (dash + 1);
      if (last < 0)
	{
	  warning (0, "unknown register name: %s", dash + 1);
	  return;
	}
      
      *dash = '-';
      
      if (first > last)
	{
	  warning (0, "%s-%s is an empty range", str, dash + 1);
	  return;
	}
      
      for (i = first; i <= last; ++i)
	fixed_regs[i] = call_used_regs[i] = 1;

      if (!comma)
	break;

      *comma = ',';
      str = comma + 1;
    }
}

int
spu_valid_move (rtx * ops)
{
  enum machine_mode mode = GET_MODE (ops[0]);
  if (!register_operand (ops[0], mode) && !register_operand (ops[1], mode))
    return 0;

  /* init_expr_once tries to recog against load and store insns to set
     the direct_load[] and direct_store[] arrays.  We always want to
     consider those loads and stores valid.  init_expr_once is called in
     the context of a dummy function which does not have a decl. */
  if (cfun->decl == 0)
    return 1;

  /* Don't allows loads/stores which would require more than 1 insn.
     During and after reload we assume loads and stores only take 1
     insn. */
  if (GET_MODE_SIZE (mode) < 16 && !reload_in_progress && !reload_completed)
    {
      if (GET_CODE (ops[0]) == MEM
	  && (GET_MODE_SIZE (mode) < 4
	      || !(store_with_one_insn_p (ops[0])
		   || mem_is_padded_component_ref (ops[0]))))
	return 0;
      if (GET_CODE (ops[1]) == MEM
	  && (GET_MODE_SIZE (mode) < 4 || !aligned_mem_p (ops[1])))
	return 0;
    }
  return 1;
}

/* Return TRUE if x is a CONST_INT, CONST_DOUBLE or CONST_VECTOR that
   can be generated using the fsmbi instruction. */
int
fsmbi_const_p (rtx x)
{
  if (CONSTANT_P (x))
    {
      /* We can always choose DImode for CONST_INT because the high bits
         of an SImode will always be all 1s, i.e., valid for fsmbi. */
      enum immediate_class c = classify_immediate (x, DImode);
      return c == IC_FSMBI;
    }
  return 0;
}

/* Return TRUE if x is a CONST_INT, CONST_DOUBLE or CONST_VECTOR that
   can be generated using the cbd, chd, cwd or cdd instruction. */
int
cpat_const_p (rtx x, enum machine_mode mode)
{
  if (CONSTANT_P (x))
    {
      enum immediate_class c = classify_immediate (x, mode);
      return c == IC_CPAT;
    }
  return 0;
}

rtx
gen_cpat_const (rtx * ops)
{
  unsigned char dst[16];
  int i, offset, shift, isize;
  if (GET_CODE (ops[3]) != CONST_INT
      || GET_CODE (ops[2]) != CONST_INT
      || (GET_CODE (ops[1]) != CONST_INT
	  && GET_CODE (ops[1]) != REG))
    return 0;
  if (GET_CODE (ops[1]) == REG
      && (!REG_POINTER (ops[1])
	  || REGNO_POINTER_ALIGN (ORIGINAL_REGNO (ops[1])) < 128))
    return 0;

  for (i = 0; i < 16; i++)
    dst[i] = i + 16;
  isize = INTVAL (ops[3]);
  if (isize == 1)
    shift = 3;
  else if (isize == 2)
    shift = 2;
  else
    shift = 0;
  offset = (INTVAL (ops[2]) +
	    (GET_CODE (ops[1]) ==
	     CONST_INT ? INTVAL (ops[1]) : 0)) & 15;
  for (i = 0; i < isize; i++)
    dst[offset + i] = i + shift;
  return array_to_constant (TImode, dst);
}

/* Convert a CONST_INT, CONST_DOUBLE, or CONST_VECTOR into a 16 byte
   array.  Use MODE for CONST_INT's.  When the constant's mode is smaller
   than 16 bytes, the value is repeated across the rest of the array. */
void
constant_to_array (enum machine_mode mode, rtx x, unsigned char arr[16])
{
  HOST_WIDE_INT val;
  int i, j, first;

  memset (arr, 0, 16);
  mode = GET_MODE (x) != VOIDmode ? GET_MODE (x) : mode;
  if (GET_CODE (x) == CONST_INT
      || (GET_CODE (x) == CONST_DOUBLE
	  && (mode == SFmode || mode == DFmode)))
    {
      gcc_assert (mode != VOIDmode && mode != BLKmode);

      if (GET_CODE (x) == CONST_DOUBLE)
	val = const_double_to_hwint (x);
      else
	val = INTVAL (x);
      first = GET_MODE_SIZE (mode) - 1;
      for (i = first; i >= 0; i--)
	{
	  arr[i] = val & 0xff;
	  val >>= 8;
	}
      /* Splat the constant across the whole array. */
      for (j = 0, i = first + 1; i < 16; i++)
	{
	  arr[i] = arr[j];
	  j = (j == first) ? 0 : j + 1;
	}
    }
  else if (GET_CODE (x) == CONST_DOUBLE)
    {
      val = CONST_DOUBLE_LOW (x);
      for (i = 15; i >= 8; i--)
	{
	  arr[i] = val & 0xff;
	  val >>= 8;
	}
      val = CONST_DOUBLE_HIGH (x);
      for (i = 7; i >= 0; i--)
	{
	  arr[i] = val & 0xff;
	  val >>= 8;
	}
    }
  else if (GET_CODE (x) == CONST_VECTOR)
    {
      int units;
      rtx elt;
      mode = GET_MODE_INNER (mode);
      units = CONST_VECTOR_NUNITS (x);
      for (i = 0; i < units; i++)
	{
	  elt = CONST_VECTOR_ELT (x, i);
	  if (GET_CODE (elt) == CONST_INT || GET_CODE (elt) == CONST_DOUBLE)
	    {
	      if (GET_CODE (elt) == CONST_DOUBLE)
		val = const_double_to_hwint (elt);
	      else
		val = INTVAL (elt);
	      first = GET_MODE_SIZE (mode) - 1;
	      if (first + i * GET_MODE_SIZE (mode) > 16)
		abort ();
	      for (j = first; j >= 0; j--)
		{
		  arr[j + i * GET_MODE_SIZE (mode)] = val & 0xff;
		  val >>= 8;
		}
	    }
	}
    }
  else
    gcc_unreachable();
}

/* Convert a 16 byte array to a constant of mode MODE.  When MODE is
   smaller than 16 bytes, use the bytes that would represent that value
   in a register, e.g., for QImode return the value of arr[3].  */
rtx
array_to_constant (enum machine_mode mode, unsigned char arr[16])
{
  enum machine_mode inner_mode;
  rtvec v;
  int units, size, i, j, k;
  HOST_WIDE_INT val;

  if (GET_MODE_CLASS (mode) == MODE_INT
      && GET_MODE_BITSIZE (mode) <= HOST_BITS_PER_WIDE_INT)
    {
      j = GET_MODE_SIZE (mode);
      i = j < 4 ? 4 - j : 0;
      for (val = 0; i < j; i++)
	val = (val << 8) | arr[i];
      val = trunc_int_for_mode (val, mode);
      return GEN_INT (val);
    }

  if (mode == TImode)
    {
      HOST_WIDE_INT high;
      for (i = high = 0; i < 8; i++)
	high = (high << 8) | arr[i];
      for (i = 8, val = 0; i < 16; i++)
	val = (val << 8) | arr[i];
      return immed_double_const (val, high, TImode);
    }
  if (mode == SFmode)
    {
      val = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
      val = trunc_int_for_mode (val, SImode);
      return hwint_to_const_double (SFmode, val);
    }
  if (mode == DFmode)
    {
      val = (arr[0] << 24) | (arr[1] << 16) | (arr[2] << 8) | arr[3];
      val <<= 32;
      val |= (arr[4] << 24) | (arr[5] << 16) | (arr[6] << 8) | arr[7];
      return hwint_to_const_double (DFmode, val);
    }

  if (!VECTOR_MODE_P (mode))
    abort ();

  units = GET_MODE_NUNITS (mode);
  size = GET_MODE_UNIT_SIZE (mode);
  inner_mode = GET_MODE_INNER (mode);
  v = rtvec_alloc (units);

  for (k = i = 0; i < units; ++i)
    {
      val = 0;
      for (j = 0; j < size; j++, k++)
	val = (val << 8) | arr[k];

      if (GET_MODE_CLASS (inner_mode) == MODE_FLOAT)
	RTVEC_ELT (v, i) = hwint_to_const_double (inner_mode, val);
      else
	RTVEC_ELT (v, i) = GEN_INT (trunc_int_for_mode (val, inner_mode));
    }
  if (k > 16)
    abort ();

  return gen_rtx_CONST_VECTOR (mode, v);
}

static void
reloc_diagnostic (rtx x)
{
  tree loc_decl, decl = 0;
  const char *msg;
  if (!flag_pic || !(TARGET_WARN_RELOC || TARGET_ERROR_RELOC))
    return;

  if (GET_CODE (x) == SYMBOL_REF)
    decl = SYMBOL_REF_DECL (x);
  else if (GET_CODE (x) == CONST
	   && GET_CODE (XEXP (XEXP (x, 0), 0)) == SYMBOL_REF)
    decl = SYMBOL_REF_DECL (XEXP (XEXP (x, 0), 0));

  /* SYMBOL_REF_DECL is not necessarily a DECL. */
  if (decl && !DECL_P (decl))
    decl = 0;

  /* We use last_assemble_variable_decl to get line information.  It's
     not always going to be right and might not even be close, but will
     be right for the more common cases. */
  if (!last_assemble_variable_decl)
    loc_decl = decl;
  else
    loc_decl = last_assemble_variable_decl;

  /* The decl could be a string constant.  */
  if (decl && DECL_P (decl))
    msg = "%Jcreating run-time relocation for %qD";
  else
    msg = "creating run-time relocation";

  if (TARGET_WARN_RELOC)
    warning (0, msg, loc_decl, decl);
  else
    error (msg, loc_decl, decl);
}

/* Hook into assemble_integer so we can generate an error for run-time
   relocations.  The SPU ABI disallows them. */
static bool
spu_assemble_integer (rtx x, unsigned int size, int aligned_p)
{
  /* By default run-time relocations aren't supported, but we allow them
     in case users support it in their own run-time loader.  And we provide
     a warning for those users that don't.  */
  if ((GET_CODE (x) == SYMBOL_REF)
      || GET_CODE (x) == LABEL_REF || GET_CODE (x) == CONST)
    reloc_diagnostic (x);

  return default_assemble_integer (x, size, aligned_p);
}

static void
spu_asm_globalize_label (FILE * file, const char *name)
{
  fputs ("\t.global\t", file);
  assemble_name (file, name);
  fputs ("\n", file);
}

static bool
spu_rtx_costs (rtx x, int code, int outer_code ATTRIBUTE_UNUSED, int *total)
{
  enum machine_mode mode = GET_MODE (x);
  int cost = COSTS_N_INSNS (2);

  /* Folding to a CONST_VECTOR will use extra space but there might
     be only a small savings in cycles.  We'd like to use a CONST_VECTOR
     only if it allows us to fold away multiple insns.  Changing the cost
     of a CONST_VECTOR here (or in CONST_COSTS) doesn't help though
     because this cost will only be compared against a single insn. 
     if (code == CONST_VECTOR)
       return (LEGITIMATE_CONSTANT_P(x)) ? cost : COSTS_N_INSNS(6);
   */

  /* Use defaults for float operations.  Not accurate but good enough. */
  if (mode == DFmode)
    {
      *total = COSTS_N_INSNS (13);
      return true;
    }
  if (mode == SFmode)
    {
      *total = COSTS_N_INSNS (6);
      return true;
    }
  switch (code)
    {
    case CONST_INT:
      if (satisfies_constraint_K (x))
	*total = 0;
      else if (INTVAL (x) >= -0x80000000ll && INTVAL (x) <= 0xffffffffll)
	*total = COSTS_N_INSNS (1);
      else
	*total = COSTS_N_INSNS (3);
      return true;

    case CONST:
      *total = COSTS_N_INSNS (3);
      return true;

    case LABEL_REF:
    case SYMBOL_REF:
      *total = COSTS_N_INSNS (0);
      return true;

    case CONST_DOUBLE:
      *total = COSTS_N_INSNS (5);
      return true;

    case FLOAT_EXTEND:
    case FLOAT_TRUNCATE:
    case FLOAT:
    case UNSIGNED_FLOAT:
    case FIX:
    case UNSIGNED_FIX:
      *total = COSTS_N_INSNS (7);
      return true;

    case PLUS:
      if (mode == TImode)
	{
	  *total = COSTS_N_INSNS (9);
	  return true;
	}
      break;

    case MULT:
      cost =
	GET_CODE (XEXP (x, 0)) ==
	REG ? COSTS_N_INSNS (12) : COSTS_N_INSNS (7);
      if (mode == SImode && GET_CODE (XEXP (x, 0)) == REG)
	{
	  if (GET_CODE (XEXP (x, 1)) == CONST_INT)
	    {
	      HOST_WIDE_INT val = INTVAL (XEXP (x, 1));
	      cost = COSTS_N_INSNS (14);
	      if ((val & 0xffff) == 0)
		cost = COSTS_N_INSNS (9);
	      else if (val > 0 && val < 0x10000)
		cost = COSTS_N_INSNS (11);
	    }
	}
      *total = cost;
      return true;
    case DIV:
    case UDIV:
    case MOD:
    case UMOD:
      *total = COSTS_N_INSNS (20);
      return true;
    case ROTATE:
    case ROTATERT:
    case ASHIFT:
    case ASHIFTRT:
    case LSHIFTRT:
      *total = COSTS_N_INSNS (4);
      return true;
    case UNSPEC:
      if (XINT (x, 1) == UNSPEC_CONVERT)
	*total = COSTS_N_INSNS (0);
      else
	*total = COSTS_N_INSNS (4);
      return true;
    }
  /* Scale cost by mode size.  Except when initializing (cfun->decl == 0). */
  if (GET_MODE_CLASS (mode) == MODE_INT
      && GET_MODE_SIZE (mode) > GET_MODE_SIZE (SImode) && cfun && cfun->decl)
    cost = cost * (GET_MODE_SIZE (mode) / GET_MODE_SIZE (SImode))
      * (GET_MODE_SIZE (mode) / GET_MODE_SIZE (SImode));
  *total = cost;
  return true;
}

enum machine_mode
spu_eh_return_filter_mode (void)
{
  /* We would like this to be SImode, but sjlj exceptions seems to work
     only with word_mode. */
  return TImode;
}

/* Decide whether we can make a sibling call to a function.  DECL is the
   declaration of the function being targeted by the call and EXP is the
   CALL_EXPR representing the call.  */
static bool
spu_function_ok_for_sibcall (tree decl, tree exp ATTRIBUTE_UNUSED)
{
  return decl && !TARGET_LARGE_MEM;
}

/* We need to correctly update the back chain pointer and the Available
   Stack Size (which is in the second slot of the sp register.) */
void
spu_allocate_stack (rtx op0, rtx op1)
{
  HOST_WIDE_INT v;
  rtx chain = gen_reg_rtx (V4SImode);
  rtx stack_bot = gen_frame_mem (V4SImode, stack_pointer_rtx);
  rtx sp = gen_reg_rtx (V4SImode);
  rtx splatted = gen_reg_rtx (V4SImode);
  rtx pat = gen_reg_rtx (TImode);

  /* copy the back chain so we can save it back again. */
  emit_move_insn (chain, stack_bot);

  op1 = force_reg (SImode, op1);

  v = 0x1020300010203ll;
  emit_move_insn (pat, immed_double_const (v, v, TImode));
  emit_insn (gen_shufb (splatted, op1, op1, pat));

  emit_insn (gen_spu_convert (sp, stack_pointer_rtx));
  emit_insn (gen_subv4si3 (sp, sp, splatted));

  if (flag_stack_check)
    {
      rtx avail = gen_reg_rtx(SImode);
      rtx result = gen_reg_rtx(SImode);
      emit_insn (gen_vec_extractv4si (avail, sp, GEN_INT (1)));
      emit_insn (gen_cgt_si(result, avail, GEN_INT (-1)));
      emit_insn (gen_spu_heq (result, GEN_INT(0) ));
    }

  emit_insn (gen_spu_convert (stack_pointer_rtx, sp));

  emit_move_insn (stack_bot, chain);

  emit_move_insn (op0, virtual_stack_dynamic_rtx);
}

void
spu_restore_stack_nonlocal (rtx op0 ATTRIBUTE_UNUSED, rtx op1)
{
  static unsigned char arr[16] =
    { 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3 };
  rtx temp = gen_reg_rtx (SImode);
  rtx temp2 = gen_reg_rtx (SImode);
  rtx temp3 = gen_reg_rtx (V4SImode);
  rtx temp4 = gen_reg_rtx (V4SImode);
  rtx pat = gen_reg_rtx (TImode);
  rtx sp = gen_rtx_REG (V4SImode, STACK_POINTER_REGNUM);

  /* Restore the backchain from the first word, sp from the second.  */
  emit_move_insn (temp2, adjust_address_nv (op1, SImode, 0));
  emit_move_insn (temp, adjust_address_nv (op1, SImode, 4));

  emit_move_insn (pat, array_to_constant (TImode, arr));

  /* Compute Available Stack Size for sp */
  emit_insn (gen_subsi3 (temp, temp, stack_pointer_rtx));
  emit_insn (gen_shufb (temp3, temp, temp, pat));

  /* Compute Available Stack Size for back chain */
  emit_insn (gen_subsi3 (temp2, temp2, stack_pointer_rtx));
  emit_insn (gen_shufb (temp4, temp2, temp2, pat));
  emit_insn (gen_addv4si3 (temp4, sp, temp4));

  emit_insn (gen_addv4si3 (sp, sp, temp3));
  emit_move_insn (gen_frame_mem (V4SImode, stack_pointer_rtx), temp4);
}

static void
spu_init_libfuncs (void)
{
  set_optab_libfunc (smul_optab, DImode, "__muldi3");
  set_optab_libfunc (sdiv_optab, DImode, "__divdi3");
  set_optab_libfunc (smod_optab, DImode, "__moddi3");
  set_optab_libfunc (udiv_optab, DImode, "__udivdi3");
  set_optab_libfunc (umod_optab, DImode, "__umoddi3");
  set_optab_libfunc (udivmod_optab, DImode, "__udivmoddi4");
  set_optab_libfunc (ffs_optab, DImode, "__ffsdi2");
  set_optab_libfunc (clz_optab, DImode, "__clzdi2");
  set_optab_libfunc (ctz_optab, DImode, "__ctzdi2");
  set_optab_libfunc (popcount_optab, DImode, "__popcountdi2");
  set_optab_libfunc (parity_optab, DImode, "__paritydi2");

  set_conv_libfunc (ufloat_optab, DFmode, SImode, "__float_unssidf");
  set_conv_libfunc (ufloat_optab, DFmode, DImode, "__float_unsdidf");
}

/* Make a subreg, stripping any existing subreg.  We could possibly just
   call simplify_subreg, but in this case we know what we want. */
rtx
spu_gen_subreg (enum machine_mode mode, rtx x)
{
  if (GET_CODE (x) == SUBREG)
    x = SUBREG_REG (x);
  if (GET_MODE (x) == mode)
    return x;
  return gen_rtx_SUBREG (mode, x, 0);
}

static bool
spu_return_in_memory (tree type, tree fntype ATTRIBUTE_UNUSED)
{
  return (TYPE_MODE (type) == BLKmode
	  && ((type) == 0
	      || TREE_CODE (TYPE_SIZE (type)) != INTEGER_CST
	      || int_size_in_bytes (type) >
	      (MAX_REGISTER_RETURN * UNITS_PER_WORD)));
}

/* Create the built-in types and functions */

struct spu_builtin_description spu_builtins[] = {
#define DEF_BUILTIN(fcode, icode, name, type, params) \
  {fcode, icode, name, type, params, NULL_TREE},
#include "spu-builtins.def"
#undef DEF_BUILTIN
};

static void
spu_init_builtins (void)
{
  struct spu_builtin_description *d;
  unsigned int i;

  V16QI_type_node = build_vector_type (intQI_type_node, 16);
  V8HI_type_node = build_vector_type (intHI_type_node, 8);
  V4SI_type_node = build_vector_type (intSI_type_node, 4);
  V2DI_type_node = build_vector_type (intDI_type_node, 2);
  V4SF_type_node = build_vector_type (float_type_node, 4);
  V2DF_type_node = build_vector_type (double_type_node, 2);

  unsigned_V16QI_type_node = build_vector_type (unsigned_intQI_type_node, 16);
  unsigned_V8HI_type_node = build_vector_type (unsigned_intHI_type_node, 8);
  unsigned_V4SI_type_node = build_vector_type (unsigned_intSI_type_node, 4);
  unsigned_V2DI_type_node = build_vector_type (unsigned_intDI_type_node, 2);

  spu_builtin_types[SPU_BTI_QUADWORD] = V16QI_type_node;

  spu_builtin_types[SPU_BTI_7] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_S7] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_U7] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_S10] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_S10_4] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_U14] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_16] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_S16] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_S16_2] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_U16] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_U16_2] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_U18] = global_trees[TI_INTSI_TYPE];

  spu_builtin_types[SPU_BTI_INTQI] = global_trees[TI_INTQI_TYPE];
  spu_builtin_types[SPU_BTI_INTHI] = global_trees[TI_INTHI_TYPE];
  spu_builtin_types[SPU_BTI_INTSI] = global_trees[TI_INTSI_TYPE];
  spu_builtin_types[SPU_BTI_INTDI] = global_trees[TI_INTDI_TYPE];
  spu_builtin_types[SPU_BTI_UINTQI] = global_trees[TI_UINTQI_TYPE];
  spu_builtin_types[SPU_BTI_UINTHI] = global_trees[TI_UINTHI_TYPE];
  spu_builtin_types[SPU_BTI_UINTSI] = global_trees[TI_UINTSI_TYPE];
  spu_builtin_types[SPU_BTI_UINTDI] = global_trees[TI_UINTDI_TYPE];

  spu_builtin_types[SPU_BTI_FLOAT] = global_trees[TI_FLOAT_TYPE];
  spu_builtin_types[SPU_BTI_DOUBLE] = global_trees[TI_DOUBLE_TYPE];

  spu_builtin_types[SPU_BTI_VOID] = global_trees[TI_VOID_TYPE];

  spu_builtin_types[SPU_BTI_PTR] =
    build_pointer_type (build_qualified_type
			(void_type_node,
			 TYPE_QUAL_CONST | TYPE_QUAL_VOLATILE));

  /* For each builtin we build a new prototype.  The tree code will make
     sure nodes are shared. */
  for (i = 0, d = spu_builtins; i < NUM_SPU_BUILTINS; i++, d++)
    {
      tree p;
      char name[64];		/* build_function will make a copy. */
      int parm;

      if (d->name == 0)
	continue;

      /* find last parm */
      for (parm = 1; d->parm[parm] != SPU_BTI_END_OF_PARAMS; parm++)
	{
	}

      p = void_list_node;
      while (parm > 1)
	p = tree_cons (NULL_TREE, spu_builtin_types[d->parm[--parm]], p);

      p = build_function_type (spu_builtin_types[d->parm[0]], p);

      sprintf (name, "__builtin_%s", d->name);
      d->fndecl =
	builtin_function (name, p, END_BUILTINS + i, BUILT_IN_MD,
			  NULL, NULL_TREE);
      if (d->fcode == SPU_MASK_FOR_LOAD)
	TREE_READONLY (d->fndecl) = 1;	
    }
}

int
spu_safe_dma (HOST_WIDE_INT channel)
{
  return (channel >= 21 && channel <= 27);
}

void
spu_builtin_splats (rtx ops[])
{
  enum machine_mode mode = GET_MODE (ops[0]);
  if (GET_CODE (ops[1]) == CONST_INT || GET_CODE (ops[1]) == CONST_DOUBLE)
    {
      unsigned char arr[16];
      constant_to_array (GET_MODE_INNER (mode), ops[1], arr);
      emit_move_insn (ops[0], array_to_constant (mode, arr));
    }
  else if (GET_MODE (ops[0]) == V4SImode && CONSTANT_P (ops[1]))
    {
      rtvec v = rtvec_alloc (4);
      RTVEC_ELT (v, 0) = ops[1];
      RTVEC_ELT (v, 1) = ops[1];
      RTVEC_ELT (v, 2) = ops[1];
      RTVEC_ELT (v, 3) = ops[1];
      emit_move_insn (ops[0], gen_rtx_CONST_VECTOR (mode, v));
    }
  else
    {
      rtx reg = gen_reg_rtx (TImode);
      rtx shuf;
      if (GET_CODE (ops[1]) != REG
	  && GET_CODE (ops[1]) != SUBREG)
	ops[1] = force_reg (GET_MODE_INNER (mode), ops[1]);
      switch (mode)
	{
	case V2DImode:
	case V2DFmode:
	  shuf =
	    immed_double_const (0x0001020304050607ll, 0x1011121314151617ll,
				TImode);
	  break;
	case V4SImode:
	case V4SFmode:
	  shuf =
	    immed_double_const (0x0001020300010203ll, 0x0001020300010203ll,
				TImode);
	  break;
	case V8HImode:
	  shuf =
	    immed_double_const (0x0203020302030203ll, 0x0203020302030203ll,
				TImode);
	  break;
	case V16QImode:
	  shuf =
	    immed_double_const (0x0303030303030303ll, 0x0303030303030303ll,
				TImode);
	  break;
	default:
	  abort ();
	}
      emit_move_insn (reg, shuf);
      emit_insn (gen_shufb (ops[0], ops[1], ops[1], reg));
    }
}

void
spu_builtin_extract (rtx ops[])
{
  enum machine_mode mode;
  rtx rot, from, tmp;

  mode = GET_MODE (ops[1]);

  if (GET_CODE (ops[2]) == CONST_INT)
    {
      switch (mode)
	{
	case V16QImode:
	  emit_insn (gen_vec_extractv16qi (ops[0], ops[1], ops[2]));
	  break;
	case V8HImode:
	  emit_insn (gen_vec_extractv8hi (ops[0], ops[1], ops[2]));
	  break;
	case V4SFmode:
	  emit_insn (gen_vec_extractv4sf (ops[0], ops[1], ops[2]));
	  break;
	case V4SImode:
	  emit_insn (gen_vec_extractv4si (ops[0], ops[1], ops[2]));
	  break;
	case V2DImode:
	  emit_insn (gen_vec_extractv2di (ops[0], ops[1], ops[2]));
	  break;
	case V2DFmode:
	  emit_insn (gen_vec_extractv2df (ops[0], ops[1], ops[2]));
	  break;
	default:
	  abort ();
	}
      return;
    }

  from = spu_gen_subreg (TImode, ops[1]);
  rot = gen_reg_rtx (TImode);
  tmp = gen_reg_rtx (SImode);

  switch (mode)
    {
    case V16QImode:
      emit_insn (gen_addsi3 (tmp, ops[2], GEN_INT (-3)));
      break;
    case V8HImode:
      emit_insn (gen_addsi3 (tmp, ops[2], ops[2]));
      emit_insn (gen_addsi3 (tmp, tmp, GEN_INT (-2)));
      break;
    case V4SFmode:
    case V4SImode:
      emit_insn (gen_ashlsi3 (tmp, ops[2], GEN_INT (2)));
      break;
    case V2DImode:
    case V2DFmode:
      emit_insn (gen_ashlsi3 (tmp, ops[2], GEN_INT (3)));
      break;
    default:
      abort ();
    }
  emit_insn (gen_rotqby_ti (rot, from, tmp));

  emit_insn (gen_spu_convert (ops[0], rot));
}

void
spu_builtin_insert (rtx ops[])
{
  enum machine_mode mode = GET_MODE (ops[0]);
  enum machine_mode imode = GET_MODE_INNER (mode);
  rtx mask = gen_reg_rtx (TImode);
  rtx offset;

  if (GET_CODE (ops[3]) == CONST_INT)
    offset = GEN_INT (INTVAL (ops[3]) * GET_MODE_SIZE (imode));
  else
    {
      offset = gen_reg_rtx (SImode);
      emit_insn (gen_mulsi3
		 (offset, ops[3], GEN_INT (GET_MODE_SIZE (imode))));
    }
  emit_insn (gen_cpat
	     (mask, stack_pointer_rtx, offset,
	      GEN_INT (GET_MODE_SIZE (imode))));
  emit_insn (gen_shufb (ops[0], ops[1], ops[2], mask));
}

void
spu_builtin_promote (rtx ops[])
{
  enum machine_mode mode, imode;
  rtx rot, from, offset;
  HOST_WIDE_INT pos;

  mode = GET_MODE (ops[0]);
  imode = GET_MODE_INNER (mode);

  from = gen_reg_rtx (TImode);
  rot = spu_gen_subreg (TImode, ops[0]);

  emit_insn (gen_spu_convert (from, ops[1]));

  if (GET_CODE (ops[2]) == CONST_INT)
    {
      pos = -GET_MODE_SIZE (imode) * INTVAL (ops[2]);
      if (GET_MODE_SIZE (imode) < 4)
	pos += 4 - GET_MODE_SIZE (imode);
      offset = GEN_INT (pos & 15);
    }
  else
    {
      offset = gen_reg_rtx (SImode);
      switch (mode)
	{
	case V16QImode:
	  emit_insn (gen_subsi3 (offset, GEN_INT (3), ops[2]));
	  break;
	case V8HImode:
	  emit_insn (gen_subsi3 (offset, GEN_INT (1), ops[2]));
	  emit_insn (gen_addsi3 (offset, offset, offset));
	  break;
	case V4SFmode:
	case V4SImode:
	  emit_insn (gen_subsi3 (offset, GEN_INT (0), ops[2]));
	  emit_insn (gen_ashlsi3 (offset, offset, GEN_INT (2)));
	  break;
	case V2DImode:
	case V2DFmode:
	  emit_insn (gen_ashlsi3 (offset, ops[2], GEN_INT (3)));
	  break;
	default:
	  abort ();
	}
    }
  emit_insn (gen_rotqby_ti (rot, from, offset));
}

void
spu_initialize_trampoline (rtx tramp, rtx fnaddr, rtx cxt)
{
  rtx shuf = gen_reg_rtx (V4SImode);
  rtx insn = gen_reg_rtx (V4SImode);
  rtx shufc;
  rtx insnc;
  rtx mem;

  fnaddr = force_reg (SImode, fnaddr);
  cxt = force_reg (SImode, cxt);

  if (TARGET_LARGE_MEM)
    {
      rtx rotl = gen_reg_rtx (V4SImode);
      rtx mask = gen_reg_rtx (V4SImode);
      rtx bi = gen_reg_rtx (SImode);
      unsigned char shufa[16] = {
	2, 3, 0, 1, 18, 19, 16, 17,
	0, 1, 2, 3, 16, 17, 18, 19
      };
      unsigned char insna[16] = {
	0x41, 0, 0, 79,
	0x41, 0, 0, STATIC_CHAIN_REGNUM,
	0x60, 0x80, 0, 79,
	0x60, 0x80, 0, STATIC_CHAIN_REGNUM
      };

      shufc = force_reg (TImode, array_to_constant (TImode, shufa));
      insnc = force_reg (V4SImode, array_to_constant (V4SImode, insna));

      emit_insn (gen_shufb (shuf, fnaddr, cxt, shufc));
      emit_insn (gen_rotlv4si3 (rotl, shuf, spu_const (V4SImode, 7)));
      emit_insn (gen_movv4si (mask, spu_const (V4SImode, 0xffff << 7)));
      emit_insn (gen_selb (insn, insnc, rotl, mask));

      mem = memory_address (Pmode, tramp);
      emit_move_insn (gen_rtx_MEM (V4SImode, mem), insn);

      emit_move_insn (bi, GEN_INT (0x35000000 + (79 << 7)));
      mem = memory_address (Pmode, plus_constant (tramp, 16));
      emit_move_insn (gen_rtx_MEM (Pmode, mem), bi);
    }
  else
    {
      rtx scxt = gen_reg_rtx (SImode);
      rtx sfnaddr = gen_reg_rtx (SImode);
      unsigned char insna[16] = {
	0x42, 0, 0, STATIC_CHAIN_REGNUM,
	0x30, 0, 0, 0,
	0, 0, 0, 0,
	0, 0, 0, 0
      };

      shufc = gen_reg_rtx (TImode);
      insnc = force_reg (V4SImode, array_to_constant (V4SImode, insna));

      /* By or'ing all of cxt with the ila opcode we are assuming cxt
	 fits 18 bits and the last 4 are zeros.  This will be true if
	 the stack pointer is initialized to 0x3fff0 at program start,
	 otherwise the ila instruction will be garbage. */

      emit_insn (gen_ashlsi3 (scxt, cxt, GEN_INT (7)));
      emit_insn (gen_ashlsi3 (sfnaddr, fnaddr, GEN_INT (5)));
      emit_insn (gen_cpat
		 (shufc, stack_pointer_rtx, GEN_INT (4), GEN_INT (4)));
      emit_insn (gen_shufb (shuf, sfnaddr, scxt, shufc));
      emit_insn (gen_iorv4si3 (insn, insnc, shuf));

      mem = memory_address (Pmode, tramp);
      emit_move_insn (gen_rtx_MEM (V4SImode, mem), insn);

    }
  emit_insn (gen_sync ());
}

void
spu_expand_sign_extend (rtx ops[])
{
  unsigned char arr[16];
  rtx pat = gen_reg_rtx (TImode);
  rtx sign, c;
  int i, last;
  last = GET_MODE (ops[0]) == DImode ? 7 : 15;
  if (GET_MODE (ops[1]) == QImode)
    {
      sign = gen_reg_rtx (HImode);
      emit_insn (gen_extendqihi2 (sign, ops[1]));
      for (i = 0; i < 16; i++)
	arr[i] = 0x12;
      arr[last] = 0x13;
    }
  else
    {
      for (i = 0; i < 16; i++)
	arr[i] = 0x10;
      switch (GET_MODE (ops[1]))
	{
	case HImode:
	  sign = gen_reg_rtx (SImode);
	  emit_insn (gen_extendhisi2 (sign, ops[1]));
	  arr[last] = 0x03;
	  arr[last - 1] = 0x02;
	  break;
	case SImode:
	  sign = gen_reg_rtx (SImode);
	  emit_insn (gen_ashrsi3 (sign, ops[1], GEN_INT (31)));
	  for (i = 0; i < 4; i++)
	    arr[last - i] = 3 - i;
	  break;
	case DImode:
	  sign = gen_reg_rtx (SImode);
	  c = gen_reg_rtx (SImode);
	  emit_insn (gen_spu_convert (c, ops[1]));
	  emit_insn (gen_ashrsi3 (sign, c, GEN_INT (31)));
	  for (i = 0; i < 8; i++)
	    arr[last - i] = 7 - i;
	  break;
	default:
	  abort ();
	}
    }
  emit_move_insn (pat, array_to_constant (TImode, arr));
  emit_insn (gen_shufb (ops[0], ops[1], sign, pat));
}

/* expand vector initialization. If there are any constant parts,
   load constant parts first. Then load any non-constant parts.  */
void
spu_expand_vector_init (rtx target, rtx vals)
{
  enum machine_mode mode = GET_MODE (target);
  int n_elts = GET_MODE_NUNITS (mode);
  int n_var = 0;
  bool all_same = true;
  rtx first, x = NULL_RTX, first_constant = NULL_RTX;
  int i;

  first = XVECEXP (vals, 0, 0); 
  for (i = 0; i < n_elts; ++i)
    {
      x = XVECEXP (vals, 0, i);
      if (!CONSTANT_P (x))
	++n_var;
      else
	{
	  if (first_constant == NULL_RTX)
	    first_constant = x;
	}
      if (i > 0 && !rtx_equal_p (x, first))
	all_same = false;
    }

  /* if all elements are the same, use splats to repeat elements */
  if (all_same)
    {
      if (!CONSTANT_P (first)
	  && !register_operand (first, GET_MODE (x)))
	first = force_reg (GET_MODE (first), first);
      emit_insn (gen_spu_splats (target, first));
      return;
    }

  /* load constant parts */
  if (n_var != n_elts)
    {
      if (n_var == 0)
	{
	  emit_move_insn (target,
			  gen_rtx_CONST_VECTOR (mode, XVEC (vals, 0)));
	}
      else
	{
	  rtx constant_parts_rtx = copy_rtx (vals);

	  gcc_assert (first_constant != NULL_RTX);
	  /* fill empty slots with the first constant, this increases
	     our chance of using splats in the recursive call below. */
	  for (i = 0; i < n_elts; ++i)
	    if (!CONSTANT_P (XVECEXP (constant_parts_rtx, 0, i)))
	      XVECEXP (constant_parts_rtx, 0, i) = first_constant;

	  spu_expand_vector_init (target, constant_parts_rtx);
	}
    }

  /* load variable parts */
  if (n_var != 0)
    {
      rtx insert_operands[4];

      insert_operands[0] = target;
      insert_operands[2] = target;
      for (i = 0; i < n_elts; ++i)
	{
	  x = XVECEXP (vals, 0, i);
	  if (!CONSTANT_P (x))
	    {
	      if (!register_operand (x, GET_MODE (x)))
		x = force_reg (GET_MODE (x), x);
	      insert_operands[1] = x;
	      insert_operands[3] = GEN_INT (i);
	      spu_builtin_insert (insert_operands);
	    }
	}
    }
}

static rtx
spu_force_reg (enum machine_mode mode, rtx op)
{
  rtx x, r;
  if (GET_MODE (op) == VOIDmode || GET_MODE (op) == BLKmode)
    {
      if ((SCALAR_INT_MODE_P (mode) && GET_CODE (op) == CONST_INT)
	  || GET_MODE (op) == BLKmode)
	return force_reg (mode, convert_to_mode (mode, op, 0));
      abort ();
    }

  r = force_reg (GET_MODE (op), op);
  if (GET_MODE_SIZE (GET_MODE (op)) == GET_MODE_SIZE (mode))
    {
      x = simplify_gen_subreg (mode, r, GET_MODE (op), 0);
      if (x)
	return x;
    }

  x = gen_reg_rtx (mode);
  emit_insn (gen_spu_convert (x, r));
  return x;
}

static void
spu_check_builtin_parm (struct spu_builtin_description *d, rtx op, int p)
{
  HOST_WIDE_INT v = 0;
  int lsbits;
  /* Check the range of immediate operands. */
  if (p >= SPU_BTI_7 && p <= SPU_BTI_U18)
    {
      int range = p - SPU_BTI_7;
      if (!CONSTANT_P (op)
	  || (GET_CODE (op) == CONST_INT
	      && (INTVAL (op) < spu_builtin_range[range].low
		  || INTVAL (op) > spu_builtin_range[range].high)))
	error ("%s expects an integer literal in the range [%d, %d].",
	       d->name,
	       spu_builtin_range[range].low, spu_builtin_range[range].high);

      if (GET_CODE (op) == CONST
	  && (GET_CODE (XEXP (op, 0)) == PLUS
	      || GET_CODE (XEXP (op, 0)) == MINUS))
	{
	  v = INTVAL (XEXP (XEXP (op, 0), 1));
	  op = XEXP (XEXP (op, 0), 0);
	}
      else if (GET_CODE (op) == CONST_INT)
	v = INTVAL (op);

      switch (p)
	{
	case SPU_BTI_S10_4:
	  lsbits = 4;
	  break;
	case SPU_BTI_U16_2:
	  /* This is only used in lqa, and stqa.  Even though the insns
	     encode 16 bits of the address (all but the 2 least
	     significant), only 14 bits are used because it is masked to
	     be 16 byte aligned. */
	  lsbits = 4;
	  break;
	case SPU_BTI_S16_2:
	  /* This is used for lqr and stqr. */
	  lsbits = 2;
	  break;
	default:
	  lsbits = 0;
	}

      if (GET_CODE (op) == LABEL_REF
	  || (GET_CODE (op) == SYMBOL_REF
	      && SYMBOL_REF_FUNCTION_P (op))
	  || (INTVAL (op) & ((1 << lsbits) - 1)) != 0)
	warning (0, "%d least significant bits of %s are ignored.", lsbits,
		 d->name);
    }
}


static void
expand_builtin_args (struct spu_builtin_description *d, tree arglist,
		     rtx target, rtx ops[])
{
  enum insn_code icode = d->icode;
  int i = 0;

  /* Expand the arguments into rtl. */

  if (d->parm[0] != SPU_BTI_VOID)
    ops[i++] = target;

  for (; i < insn_data[icode].n_operands; i++)
    {
      tree arg = TREE_VALUE (arglist);
      if (arg == 0)
	abort ();
      ops[i] = expand_expr (arg, NULL_RTX, VOIDmode, 0);
      arglist = TREE_CHAIN (arglist);
    }
}

static rtx
spu_expand_builtin_1 (struct spu_builtin_description *d,
		      tree arglist, rtx target)
{
  rtx pat;
  rtx ops[8];
  enum insn_code icode = d->icode;
  enum machine_mode mode, tmode;
  int i, p;
  tree return_type;

  /* Set up ops[] with values from arglist. */
  expand_builtin_args (d, arglist, target, ops);

  /* Handle the target operand which must be operand 0. */
  i = 0;
  if (d->parm[0] != SPU_BTI_VOID)
    {

      /* We prefer the mode specified for the match_operand otherwise
         use the mode from the builtin function prototype. */
      tmode = insn_data[d->icode].operand[0].mode;
      if (tmode == VOIDmode)
	tmode = TYPE_MODE (spu_builtin_types[d->parm[0]]);

      /* Try to use target because not using it can lead to extra copies
         and when we are using all of the registers extra copies leads
         to extra spills.  */
      if (target && GET_CODE (target) == REG && GET_MODE (target) == tmode)
	ops[0] = target;
      else
	target = ops[0] = gen_reg_rtx (tmode);

      if (!(*insn_data[icode].operand[0].predicate) (ops[0], tmode))
	abort ();

      i++;
    }

  if (d->fcode == SPU_MASK_FOR_LOAD)
    {
      enum machine_mode mode = insn_data[icode].operand[1].mode;
      tree arg;
      rtx addr, op, pat;

      /* get addr */
      arg = TREE_VALUE (arglist);
      gcc_assert (TREE_CODE (TREE_TYPE (arg)) == POINTER_TYPE);
      op = expand_expr (arg, NULL_RTX, Pmode, EXPAND_NORMAL);
      addr = memory_address (mode, op);

      /* negate addr */
      op = gen_reg_rtx (GET_MODE (addr));
      emit_insn (gen_rtx_SET (VOIDmode, op,
                 gen_rtx_NEG (GET_MODE (addr), addr)));
      op = gen_rtx_MEM (mode, op);

      pat = GEN_FCN (icode) (target, op);
      if (!pat) 
        return 0;
      emit_insn (pat);
      return target;
    }   

  /* Ignore align_hint, but still expand it's args in case they have
     side effects. */
  if (icode == CODE_FOR_spu_align_hint)
    return 0;

  /* Handle the rest of the operands. */
  for (p = 1; i < insn_data[icode].n_operands; i++, p++)
    {
      if (insn_data[d->icode].operand[i].mode != VOIDmode)
	mode = insn_data[d->icode].operand[i].mode;
      else
	mode = TYPE_MODE (spu_builtin_types[d->parm[i]]);

      /* mode can be VOIDmode here for labels */

      /* For specific intrinsics with an immediate operand, e.g.,
         si_ai(), we sometimes need to convert the scalar argument to a
         vector argument by splatting the scalar. */
      if (VECTOR_MODE_P (mode)
	  && (GET_CODE (ops[i]) == CONST_INT
	      || GET_MODE_CLASS (GET_MODE (ops[i])) == MODE_INT
	      || GET_MODE_CLASS (GET_MODE (ops[i])) == MODE_FLOAT))
	{
	  if (GET_CODE (ops[i]) == CONST_INT)
	    ops[i] = spu_const (mode, INTVAL (ops[i]));
	  else
	    {
	      rtx reg = gen_reg_rtx (mode);
	      enum machine_mode imode = GET_MODE_INNER (mode);
	      if (!spu_nonmem_operand (ops[i], GET_MODE (ops[i])))
		ops[i] = force_reg (GET_MODE (ops[i]), ops[i]);
	      if (imode != GET_MODE (ops[i]))
		ops[i] = convert_to_mode (imode, ops[i],
					  TYPE_UNSIGNED (spu_builtin_types
							 [d->parm[i]]));
	      emit_insn (gen_spu_splats (reg, ops[i]));
	      ops[i] = reg;
	    }
	}

      if (!(*insn_data[icode].operand[i].predicate) (ops[i], mode))
	ops[i] = spu_force_reg (mode, ops[i]);

      spu_check_builtin_parm (d, ops[i], d->parm[p]);
    }

  switch (insn_data[icode].n_operands)
    {
    case 0:
      pat = GEN_FCN (icode) (0);
      break;
    case 1:
      pat = GEN_FCN (icode) (ops[0]);
      break;
    case 2:
      pat = GEN_FCN (icode) (ops[0], ops[1]);
      break;
    case 3:
      pat = GEN_FCN (icode) (ops[0], ops[1], ops[2]);
      break;
    case 4:
      pat = GEN_FCN (icode) (ops[0], ops[1], ops[2], ops[3]);
      break;
    case 5:
      pat = GEN_FCN (icode) (ops[0], ops[1], ops[2], ops[3], ops[4]);
      break;
    case 6:
      pat = GEN_FCN (icode) (ops[0], ops[1], ops[2], ops[3], ops[4], ops[5]);
      break;
    default:
      abort ();
    }

  if (!pat)
    abort ();

  if (d->type == B_CALL || d->type == B_BISLED)
    emit_call_insn (pat);
  else if (d->type == B_JUMP)
    {
      emit_jump_insn (pat);
      emit_barrier ();
    }
  else
    emit_insn (pat);

  return_type = spu_builtin_types[d->parm[0]];
  if (d->parm[0] != SPU_BTI_VOID
      && GET_MODE (target) != TYPE_MODE (return_type))
    {
      /* target is the return value.  It should always be the mode of
         the builtin function prototype. */
      target = spu_force_reg (TYPE_MODE (return_type), target);
    }

  return target;
}

rtx
spu_expand_builtin (tree exp,
		    rtx target,
		    rtx subtarget ATTRIBUTE_UNUSED,
		    enum machine_mode mode ATTRIBUTE_UNUSED,
		    int ignore ATTRIBUTE_UNUSED)
{
  tree fndecl = TREE_OPERAND (TREE_OPERAND (exp, 0), 0);
  unsigned int fcode = DECL_FUNCTION_CODE (fndecl) - END_BUILTINS;
  tree arglist = TREE_OPERAND (exp, 1);
  struct spu_builtin_description *d;

  if (fcode < NUM_SPU_BUILTINS)
    {
      d = &spu_builtins[fcode];

      return spu_expand_builtin_1 (d, arglist, target);
    }
  abort ();
}

/* LLVM LOCAL begin */
#ifdef INSN_SCHEDULING
/* Implement targetm.vectorize.builtin_mul_widen_even.  */
static tree
spu_builtin_mul_widen_even (tree type)
{
  switch (TYPE_MODE (type))
    {
    case V8HImode:
      if (TYPE_UNSIGNED (type))
	return spu_builtins[SPU_MULE_0].fndecl;
      else
	return spu_builtins[SPU_MULE_1].fndecl;
      break;
    default:
      return NULL_TREE;
    }
}

/* Implement targetm.vectorize.builtin_mul_widen_odd.  */
static tree
spu_builtin_mul_widen_odd (tree type)
{
  switch (TYPE_MODE (type))
    {
    case V8HImode:
      if (TYPE_UNSIGNED (type))
	return spu_builtins[SPU_MULO_1].fndecl;
      else
	return spu_builtins[SPU_MULO_0].fndecl; 
      break;
    default:
      return NULL_TREE;
    }
}
#endif /* INSN_SCHEDULING */
/* LLVM LOCAL end */

/* Implement targetm.vectorize.builtin_mask_for_load.  */
static tree
spu_builtin_mask_for_load (void)
{
  struct spu_builtin_description *d = &spu_builtins[SPU_MASK_FOR_LOAD];
  gcc_assert (d);
  return d->fndecl;
}
