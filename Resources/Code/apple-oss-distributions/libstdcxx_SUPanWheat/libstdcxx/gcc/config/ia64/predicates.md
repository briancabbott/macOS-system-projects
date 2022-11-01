;; Predicate definitions for IA-64.
;; Copyright (C) 2004, 2005 Free Software Foundation, Inc.
;;
;; This file is part of GCC.
;;
;; GCC is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.
;;
;; GCC is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with GCC; see the file COPYING.  If not, write to
;; the Free Software Foundation, 59 Temple Place - Suite 330,
;; Boston, MA 02111-1307, USA.

;; True if OP is a valid operand for the MEM of a CALL insn.
(define_predicate "call_operand"
  (ior (match_code "symbol_ref")
       (match_operand 0 "register_operand")))

;; True if OP refers to any kind of symbol.
;; For roughly the same reasons that pmode_register_operand exists, this
;; predicate ignores its mode argument.
(define_special_predicate "symbolic_operand" 
   (match_code "symbol_ref,const,label_ref"))

;; True if OP is a SYMBOL_REF which refers to a function.
(define_predicate "function_operand"
  (and (match_code "symbol_ref")
       (match_test "SYMBOL_REF_FUNCTION_P (op)")))

;; True if OP refers to a symbol, and is appropriate for a GOT load.
(define_predicate "got_symbolic_operand" 
  (match_operand 0 "symbolic_operand" "")
{
  switch (GET_CODE (op))
    {
    case LABEL_REF:
      return true;

    case SYMBOL_REF:
      /* This sort of load should not be used for things in sdata.  */
      return !SYMBOL_REF_SMALL_ADDR_P (op);

    case CONST:
      /* Accept only (plus (symbol_ref) (const_int)).  */
      op = XEXP (op, 0);
      if (GET_CODE (op) != PLUS
	  || GET_CODE (XEXP (op, 0)) != SYMBOL_REF
          || GET_CODE (XEXP (op, 1)) != CONST_INT)
        return false;

      /* Ok if we're not using GOT entries at all.  */
      if (TARGET_NO_PIC || TARGET_AUTO_PIC)
        return true;

      /* The low 14 bits of the constant have been forced to zero
	 by ia64_expand_load_address, so that we do not use up so
	 many GOT entries.  Prevent cse from undoing this.  */
      op = XEXP (op, 1);
      return (INTVAL (op) & 0x3fff) == 0;

    default:
      abort ();
    }
})

;; True if OP refers to a symbol in the sdata section.
(define_predicate "sdata_symbolic_operand" 
  (match_code "symbol_ref,const")
{
  HOST_WIDE_INT offset = 0, size = 0;

  switch (GET_CODE (op))
    {
    case CONST:
      op = XEXP (op, 0);
      if (GET_CODE (op) != PLUS
	  || GET_CODE (XEXP (op, 0)) != SYMBOL_REF
	  || GET_CODE (XEXP (op, 1)) != CONST_INT)
	return false;
      offset = INTVAL (XEXP (op, 1));
      op = XEXP (op, 0);
      /* FALLTHRU */

    case SYMBOL_REF:
      if (CONSTANT_POOL_ADDRESS_P (op))
	{
	  size = GET_MODE_SIZE (get_pool_mode (op));
	  if (size > ia64_section_threshold)
	    return false;
	}
      else
	{
	  tree t;

	  if (!SYMBOL_REF_LOCAL_P (op) || !SYMBOL_REF_SMALL_P (op))
	    return false;

	  /* Note that in addition to DECLs, we can get various forms
	     of constants here.  */
	  t = SYMBOL_REF_DECL (op);
	  if (DECL_P (t))
	    t = DECL_SIZE_UNIT (t);
	  else
	    t = TYPE_SIZE_UNIT (TREE_TYPE (t));
	  if (t && host_integerp (t, 0))
	    {
	      size = tree_low_cst (t, 0);
	      if (size < 0)
		size = 0;
	    }
	}

      /* Deny the stupid user trick of addressing outside the object.  Such
	 things quickly result in GPREL22 relocation overflows.  Of course,
	 they're also highly undefined.  From a pure pedant's point of view
	 they deserve a slap on the wrist (such as provided by a relocation
	 overflow), but that just leads to bugzilla noise.  */
      return (offset >= 0 && offset <= size);

    default:
      abort ();
    }
})

;; Like nonimmediate_operand, but don't allow MEMs that try to use a
;; POST_MODIFY with a REG as displacement.
(define_predicate "destination_operand"
  (and (match_operand 0 "nonimmediate_operand")
       (match_test "GET_CODE (op) != MEM
		    || GET_CODE (XEXP (op, 0)) != POST_MODIFY
		    || GET_CODE (XEXP (XEXP (XEXP (op, 0), 1), 1)) != REG")))

;; Like memory_operand, but don't allow post-increments.
(define_predicate "not_postinc_memory_operand"
  (and (match_operand 0 "memory_operand")
       (match_test "GET_RTX_CLASS (GET_CODE (XEXP (op, 0))) != RTX_AUTOINC")))

;; True if OP is a general operand, excluding tls symbolic operands.
(define_predicate "move_operand"
  (and (match_operand 0 "general_operand")
       (not (match_test 
	     "GET_CODE (op) == SYMBOL_REF && SYMBOL_REF_TLS_MODEL (op)"))))

;; True if OP is a register operand that is (or could be) a GR reg.
(define_predicate "gr_register_operand"
  (match_operand 0 "register_operand")
{
  unsigned int regno;
  if (GET_CODE (op) == SUBREG)
    op = SUBREG_REG (op);

  regno = REGNO (op);
  return (regno >= FIRST_PSEUDO_REGISTER || GENERAL_REGNO_P (regno));
})

;; True if OP is a register operand that is (or could be) an FR reg.
(define_predicate "fr_register_operand"
  (match_operand 0 "register_operand")
{
  unsigned int regno;
  if (GET_CODE (op) == SUBREG)
    op = SUBREG_REG (op);

  regno = REGNO (op);
  return (regno >= FIRST_PSEUDO_REGISTER || FR_REGNO_P (regno));
})

;; True if OP is a register operand that is (or could be) a GR/FR reg.
(define_predicate "grfr_register_operand"
  (match_operand 0 "register_operand")
{
  unsigned int regno;
  if (GET_CODE (op) == SUBREG)
    op = SUBREG_REG (op);

  regno = REGNO (op);
  return (regno >= FIRST_PSEUDO_REGISTER
	  || GENERAL_REGNO_P (regno)
	  || FR_REGNO_P (regno));
})

;; True if OP is a nonimmediate operand that is (or could be) a GR reg.
(define_predicate "gr_nonimmediate_operand"
  (match_operand 0 "nonimmediate_operand")
{
  unsigned int regno;

  if (GET_CODE (op) == MEM)
    return true;
  if (GET_CODE (op) == SUBREG)
    op = SUBREG_REG (op);

  regno = REGNO (op);
  return (regno >= FIRST_PSEUDO_REGISTER || GENERAL_REGNO_P (regno));
})

;; True if OP is a nonimmediate operand that is (or could be) a FR reg.
(define_predicate "fr_nonimmediate_operand"
  (match_operand 0 "nonimmediate_operand")
{
  unsigned int regno;

  if (GET_CODE (op) == MEM)
    return true;
  if (GET_CODE (op) == SUBREG)
    op = SUBREG_REG (op);

  regno = REGNO (op);
  return (regno >= FIRST_PSEUDO_REGISTER || FR_REGNO_P (regno));
})

;; True if OP is a nonimmediate operand that is (or could be) a GR/FR reg.
(define_predicate "grfr_nonimmediate_operand"
  (match_operand 0 "nonimmediate_operand")
{
  unsigned int regno;

  if (GET_CODE (op) == MEM)
    return true;
  if (GET_CODE (op) == SUBREG)
    op = SUBREG_REG (op);

  regno = REGNO (op);
  return (regno >= FIRST_PSEUDO_REGISTER
	  || GENERAL_REGNO_P (regno)
	  || FR_REGNO_P (regno));
})

;; True if OP is a GR register operand, or zero.
(define_predicate "gr_reg_or_0_operand"
  (ior (match_operand 0 "gr_register_operand")
       (and (match_code "const_int,const_double,const_vector")
	    (match_test "op == CONST0_RTX (GET_MODE (op))"))))

;; True if OP is a GR register operand, or a 5 bit immediate operand.
(define_predicate "gr_reg_or_5bit_operand"
  (ior (match_operand 0 "gr_register_operand")
       (and (match_code "const_int")
	    (match_test "INTVAL (op) >= 0 && INTVAL (op) < 32"))))

;; True if OP is a GR register operand, or a 6 bit immediate operand.
(define_predicate "gr_reg_or_6bit_operand"
  (ior (match_operand 0 "gr_register_operand")
       (and (match_code "const_int")
	    (match_test "CONST_OK_FOR_M (INTVAL (op))"))))

;; True if OP is a GR register operand, or an 8 bit immediate operand.
(define_predicate "gr_reg_or_8bit_operand"
  (ior (match_operand 0 "gr_register_operand")
       (and (match_code "const_int")
	    (match_test "CONST_OK_FOR_K (INTVAL (op))"))))

;; True if OP is a GR/FR register operand, or an 8 bit immediate operand.
(define_predicate "grfr_reg_or_8bit_operand"
  (ior (match_operand 0 "grfr_register_operand")
       (and (match_code "const_int")
	    (match_test "CONST_OK_FOR_K (INTVAL (op))"))))

;; True if OP is a register operand, or an 8 bit adjusted immediate operand.
(define_predicate "gr_reg_or_8bit_adjusted_operand"
  (ior (match_operand 0 "gr_register_operand")
       (and (match_code "const_int")
	    (match_test "CONST_OK_FOR_L (INTVAL (op))"))))

;; True if OP is a register operand, or is valid for both an 8 bit
;; immediate and an 8 bit adjusted immediate operand.  This is necessary
;; because when we emit a compare, we don't know what the condition will be,
;; so we need the union of the immediates accepted by GT and LT.
(define_predicate "gr_reg_or_8bit_and_adjusted_operand"
  (ior (match_operand 0 "gr_register_operand")
       (and (match_code "const_int")
	    (match_test "CONST_OK_FOR_K (INTVAL (op))
                         && CONST_OK_FOR_L (INTVAL (op))"))))

;; True if OP is a register operand, or a 14 bit immediate operand.
(define_predicate "gr_reg_or_14bit_operand"
  (ior (match_operand 0 "gr_register_operand")
       (and (match_code "const_int")
	    (match_test "CONST_OK_FOR_I (INTVAL (op))"))))

;;  True if OP is a register operand, or a 22 bit immediate operand.
(define_predicate "gr_reg_or_22bit_operand"
  (ior (match_operand 0 "gr_register_operand")
       (and (match_code "const_int")
	    (match_test "CONST_OK_FOR_J (INTVAL (op))"))))

;; True if OP is a 7 bit immediate operand.
(define_predicate "dshift_count_operand"
  (and (match_code "const_int")
       (match_test "INTVAL (op) >= 0 && INTVAL (op) < 128")))

;; True if OP is a 6 bit immediate operand.
(define_predicate "shift_count_operand"
  (and (match_code "const_int")
       (match_test "CONST_OK_FOR_M (INTVAL (op))")))

;; True if OP is a 5 bit immediate operand.
(define_predicate "shift_32bit_count_operand"
   (and (match_code "const_int")
        (match_test "INTVAL (op) >= 0 && INTVAL (op) < 32")))

;; True if OP is one of the immediate values 2, 4, 8, or 16.
(define_predicate "shladd_operand"
  (and (match_code "const_int")
       (match_test "INTVAL (op) == 2 || INTVAL (op) == 4 ||
	            INTVAL (op) == 8 || INTVAL (op) == 16")))

;; True if OP is one of the immediate values 1, 2, 3, or 4.
(define_predicate "shladd_log2_operand"
  (and (match_code "const_int")
       (match_test "INTVAL (op) >= 1 && INTVAL (op) <= 4")))

;; True if OP is one of the immediate values  -16, -8, -4, -1, 1, 4, 8, 16.
(define_predicate "fetchadd_operand"
  (and (match_code "const_int")
       (match_test "INTVAL (op) == -16 || INTVAL (op) == -8 ||
                    INTVAL (op) == -4  || INTVAL (op) == -1 ||
                    INTVAL (op) == 1   || INTVAL (op) == 4  ||
                    INTVAL (op) == 8   || INTVAL (op) == 16")))

;; True if OP is 0..3.
(define_predicate "const_int_2bit_operand"
  (and (match_code "const_int")
        (match_test "INTVAL (op) >= 0 && INTVAL (op) <= 3")))

;; True if OP is a floating-point constant zero, one, or a register.
(define_predicate "fr_reg_or_fp01_operand"
  (ior (match_operand 0 "fr_register_operand")
       (and (match_code "const_double")
	    (match_test "CONST_DOUBLE_OK_FOR_G (op)"))))

;; Like fr_reg_or_fp01_operand, but don't allow any SUBREGs.
(define_predicate "xfreg_or_fp01_operand"
  (and (match_operand 0 "fr_reg_or_fp01_operand")
       (not (match_code "subreg"))))

;; True if OP is a constant zero, or a register.
(define_predicate "fr_reg_or_0_operand"
  (ior (match_operand 0 "fr_register_operand")
       (and (match_code "const_double,const_vector")
	    (match_test "op == CONST0_RTX (GET_MODE (op))"))))

;; True if this is a comparison operator, which accepts a normal 8-bit
;; signed immediate operand.
(define_predicate "normal_comparison_operator"
  (match_code "eq,ne,gt,le,gtu,leu"))

;; True if this is a comparison operator, which accepts an adjusted 8-bit
;; signed immediate operand.
(define_predicate "adjusted_comparison_operator"
  (match_code "lt,ge,ltu,geu"))

;; True if this is a signed inequality operator.
(define_predicate "signed_inequality_operator"
  (match_code "ge,gt,le,lt"))

;; True if this operator is valid for predication.
(define_predicate "predicate_operator"
  (match_code "eq,ne"))

;; True if this operator can be used in a conditional operation.
(define_predicate "condop_operator"
  (match_code "plus,minus,ior,xor,and"))

;; These three are hardware registers that can only be addressed in
;; DImode.  It's not strictly necessary to test mode == DImode here,
;; but it makes decent insurance against someone writing a
;; match_operand wrong.

;; True if this is the ar.lc register.
(define_predicate "ar_lc_reg_operand"
  (and (match_code "reg")
       (match_test "mode == DImode && REGNO (op) == AR_LC_REGNUM")))

;; True if this is the ar.ccv register.
(define_predicate "ar_ccv_reg_operand"
  (and (match_code "reg")
       (match_test "mode == DImode && REGNO (op) == AR_CCV_REGNUM")))

;; True if this is the ar.pfs register.
(define_predicate "ar_pfs_reg_operand"
  (and (match_code "reg")
       (match_test "mode == DImode && REGNO (op) == AR_PFS_REGNUM")))

;; True if OP is valid as a base register in a reg + offset address.
;; ??? Should I copy the flag_omit_frame_pointer and cse_not_expected
;; checks from pa.c basereg_operand as well?  Seems to be OK without them
;; in test runs.
(define_predicate "basereg_operand"
  (match_operand 0 "register_operand")
{
  if (GET_CODE (op) == SUBREG)
    op = SUBREG_REG (op);
  return REG_POINTER (op);
})

