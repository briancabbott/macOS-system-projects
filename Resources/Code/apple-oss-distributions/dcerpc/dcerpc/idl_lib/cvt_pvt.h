/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portions of this software have been released under the following terms:
 *
 * (c) Copyright 1989-1993 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989-1993 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989-1993 DIGITAL EQUIPMENT CORPORATION
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 * permission to use, copy, modify, and distribute this file for any
 * purpose is hereby granted without fee, provided that the above
 * copyright notices and this notice appears in all source code copies,
 * and that none of the names of Open Software Foundation, Inc., Hewlett-
 * Packard Company or Digital Equipment Corporation be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission.  Neither Open Software
 * Foundation, Inc., Hewlett-Packard Company nor Digital
 * Equipment Corporation makes any representations about the suitability
 * of this software for any purpose.
 *
 * Copyright (c) 2007, Novell, Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Novell Inc. nor the names of its contributors
 *     may be used to endorse or promote products derived from this
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
**
**  NAME:
**
**      cvt_pvt.h
**
**  FACILITY:
**
**      IDL Stub Runtime Support
**
**  ABSTRACT:
**
**  Private include file for floating point conversion routines.
**
**  VERSION: DCE 1.0
**
*/

#ifndef CVT__PRIVATE
#define CVT__PRIVATE

#include <dce/idlbase.h>
#define C_TAB   	'\t'
#define C_BLANK 	' '
#define C_PLUS  	'+'
#define C_MINUS 	'-'
#define C_ASTERISK 	'*'
#define C_UNDERSCORE 	'_'
#define C_DECIMAL_POINT	'.'
#define C_DOT	 	'.'
#define C_ZERO  	'0'
#define C_ONE   	'1'
#define C_TWO   	'2'
#define C_THREE 	'3'
#define C_FOUR  	'4'
#define C_FIVE  	'5'
#define C_SIX   	'6'
#define C_SEVEN 	'7'
#define C_EIGHT 	'8'
#define C_NINE  	'9'
#define C_A     	'A'
#define C_B     	'B'
#define C_C     	'C'
#define C_D     	'D'
#define C_E     	'E'
#define C_F     	'F'
#define C_L     	'L'
#define C_R     	'R'
#define C_S     	'S'
#define C_T     	'T'
#define C_U     	'U'
#define C_W     	'W'
#define C_a     	'a'
#define C_b     	'b'
#define C_c     	'c'
#define C_d     	'd'
#define C_e     	'e'
#define C_f     	'f'
#define C_t     	't'

/*
UNPACKED REAL:

[0]: excess 2147483648 (2 ^ 31) binary exponent
[1]: mantissa: msb ------>
[2]: -------------------->
[3]: -------------------->
[4]: ----------------> lsb
[5]: 28 unused bits, invalid bit, infinity bit, zero bit, negative bit

All fraction bits are explicit and are normalized s.t. 0.5 <= fraction < 1.0

*/

typedef idl_ulong_int UNPACKED_REAL[6];
typedef UNPACKED_REAL *UNPACKED_REAL_PTR;

#define U_R_EXP 0
#define U_R_FLAGS 5

#define U_R_NEGATIVE 1
#define U_R_ZERO 2
#define U_R_INFINITY 4
#define U_R_INVALID  8
#define U_R_UNUSUAL (U_R_ZERO | U_R_INFINITY | U_R_INVALID)

#if defined(ultrix) && defined(vax)
#define U_R_BIAS 2147483648L
#else
#define U_R_BIAS 2147483648UL
#endif

extern idl_ulong_int vax_c[];

#define VAX_F_INVALID &vax_c[0]
#define VAX_D_INVALID &vax_c[0]
#define VAX_G_INVALID &vax_c[0]
#define VAX_H_INVALID &vax_c[0]

#define VAX_F_ZERO &vax_c[4]
#define VAX_D_ZERO &vax_c[4]
#define VAX_G_ZERO &vax_c[4]
#define VAX_H_ZERO &vax_c[4]

#define VAX_F_POS_HUGE &vax_c[8]
#define VAX_D_POS_HUGE &vax_c[8]
#define VAX_G_POS_HUGE &vax_c[8]
#define VAX_H_POS_HUGE &vax_c[8]

#define VAX_F_NEG_HUGE &vax_c[12]
#define VAX_D_NEG_HUGE &vax_c[12]
#define VAX_G_NEG_HUGE &vax_c[12]
#define VAX_H_NEG_HUGE &vax_c[12]

extern idl_ulong_int ieee_s[];

#define IEEE_S_INVALID ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[1] : &ieee_s[0])
#define IEEE_S_POS_ZERO ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[3] : &ieee_s[2])
#define IEEE_S_NEG_ZERO ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[5] : &ieee_s[4])
#define IEEE_S_POS_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[7] : &ieee_s[6])
#define IEEE_S_NEG_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[9] : &ieee_s[8])
#define IEEE_S_POS_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[11] : &ieee_s[10])
#define IEEE_S_NEG_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_s[13] : &ieee_s[12])

extern idl_ulong_int ieee_t[];

#define IEEE_T_INVALID ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[2] : &ieee_t[0])
#define IEEE_T_POS_ZERO ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[6] : &ieee_t[4])
#define IEEE_T_NEG_ZERO ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[10] : &ieee_t[8])
#define IEEE_T_POS_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[14] : &ieee_t[12])
#define IEEE_T_NEG_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[18] : &ieee_t[16])
#define IEEE_T_POS_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[22] : &ieee_t[20])
#define IEEE_T_NEG_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&ieee_t[26] : &ieee_t[24])

extern idl_ulong_int ibm_s[];

#define IBM_S_INVALID 	&ibm_s[0]
#define IBM_S_POS_ZERO 	&ibm_s[1]
#define IBM_S_NEG_ZERO 	&ibm_s[2]
#define IBM_S_POS_HUGE 	&ibm_s[3]
#define IBM_S_NEG_HUGE 	&ibm_s[4]
#define IBM_S_POS_INFINITY  &ibm_s[5]
#define IBM_S_NEG_INFINITY  &ibm_s[6]

extern idl_ulong_int ibm_l[];

#define IBM_L_INVALID 	&ibm_l[0]
#define IBM_L_POS_ZERO 	&ibm_l[2]
#define IBM_L_NEG_ZERO 	&ibm_l[4]
#define IBM_L_POS_HUGE 	&ibm_l[6]
#define IBM_L_NEG_HUGE 	&ibm_l[8]
#define IBM_L_POS_INFINITY  &ibm_l[10]
#define IBM_L_NEG_INFINITY  &ibm_l[12]

extern idl_ulong_int cray[];

#define CRAY_INVALID	&cray[0]
#define CRAY_POS_ZERO	&cray[2]
#define CRAY_NEG_ZERO	&cray[4]
#define CRAY_POS_HUGE	&cray[6]
#define CRAY_NEG_HUGE	&cray[8]
#define CRAY_POS_INFINITY  &cray[10]
#define CRAY_NEG_INFINITY  &cray[12]

extern idl_ulong_int int_c[];

#define INT_INVALID ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[1] : &int_c[0])
#define INT_ZERO ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[3] : &int_c[2])
#define INT_POS_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[5] : &int_c[4])
#define INT_NEG_HUGE ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[7] : &int_c[6])
#define INT_POS_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[9] : &int_c[8])
#define INT_NEG_INFINITY ((options & CVT_C_BIG_ENDIAN) ? \
	&int_c[11] : &int_c[10])

#endif
