/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * ppp_comp.h - Definitions for doing PPP packet compression.
 *
 * Copyright (c) 1994 The Australian National University.
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, provided that the above copyright
 * notice appears in all copies.  This software is provided without any
 * warranty, express or implied. The Australian National University
 * makes no representations about the suitability of this software for
 * any purpose.
 *
 * IN NO EVENT SHALL THE AUSTRALIAN NATIONAL UNIVERSITY BE LIABLE TO ANY
 * PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
 * THE AUSTRALIAN NATIONAL UNIVERSITY HAVE BEEN ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 * THE AUSTRALIAN NATIONAL UNIVERSITY SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE AUSTRALIAN NATIONAL UNIVERSITY HAS NO
 * OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
 * OR MODIFICATIONS.
 *
 */

#ifndef _NET_PPP_COMP_H
#define _NET_PPP_COMP_H

/*
 * The following symbols control whether we include code for
 * various compression methods.
 */
#ifndef DO_BSD_COMPRESS
#define DO_BSD_COMPRESS	1	/* by default, include BSD-Compress */
#endif
#ifndef DO_DEFLATE
#define DO_DEFLATE	1	/* by default, include Deflate */
#endif
#define DO_PREDICTOR_1	0
#define DO_PREDICTOR_2	0

#ifdef KERNEL

/* Reference to a ppp compressor object */
typedef void * ppp_comp_ref;

/*
 * Structure giving methods for compression/decompression.
 */
struct ppp_comp_reg {
	int	compress_proto;	/* CCP compression protocol number */

	/* Allocate space for a compressor (transmit side) */
	void	*(*comp_alloc) __P((u_char *options, int opt_len));
	/* Free space used by a compressor */
	void	(*comp_free) __P((void *state));
	/* Initialize a compressor */
	int	(*comp_init) __P((void *state, u_char *options, int opt_len,
				  int unit, int hdrlen, int mtu, int debug));
	/* Reset a compressor */
	void	(*comp_reset) __P((void *state));
	/* Compress a packet */
	int	(*compress) __P((void *state, mbuf_t *m));
	/* Return compression statistics */
	void	(*comp_stat) __P((void *state, struct compstat *stats));

	/* Allocate space for a decompressor (receive side) */
	void	*(*decomp_alloc) __P((u_char *options, int opt_len));
	/* Free space used by a decompressor */
	void	(*decomp_free) __P((void *state));
	/* Initialize a decompressor */
	int	(*decomp_init) __P((void *state, u_char *options, int opt_len,
				    int unit, int hdrlen, int mru, int debug));
	/* Reset a decompressor */
	void	(*decomp_reset) __P((void *state));
	/* Decompress a packet. */
	int	(*decompress) __P((void *state, mbuf_t *m));
	/* Update state for an incompressible packet received */
	void	(*incomp) __P((void *state, mbuf_t m));
	/* Return decompression statistics */
	void	(*decomp_stat) __P((void *state, struct compstat *stats));
};

/*
 * Return values for decompress routine.
 * We need to make these distinctions so that we can disable certain
 * useful functionality, namely sending a CCP reset-request as a result
 * of an error detected after decompression.  This is to avoid infringing
 * a patent held by Motorola.
 * Don't you just lurve software patents.
 */
#define DECOMP_OK		0	/* everything went OK */
#define DECOMP_ERROR		1	/* error detected before decomp. */
#define DECOMP_FATALERROR	2	/* error detected after decomp. */

#define COMP_OK			0	/* everything went OK, packet is compressed */
#define COMP_NOTDONE		1	/* packet has not been compressed */

#endif /* KERNEL */

/*
 * CCP codes.
 */
#define CCP_CONFREQ	1
#define CCP_CONFACK	2
#define CCP_TERMREQ	5
#define CCP_TERMACK	6
#define CCP_RESETREQ	14
#define CCP_RESETACK	15

/*
 * Max # bytes for a CCP option
 */
#define CCP_MAX_OPTION_LENGTH	64

/*
 * Parts of a CCP packet.
 */
#define CCP_CODE(dp)		((dp)[0])
#define CCP_ID(dp)		((dp)[1])
#define CCP_LENGTH(dp)		(((dp)[2] << 8) + (dp)[3])
#define CCP_HDRLEN		4

#define CCP_OPT_CODE(dp)	((dp)[0])
#define CCP_OPT_LENGTH(dp)	((dp)[1])
#define CCP_OPT_MINLEN		2

/*
 * Definitions for BSD-Compress.
 */
#define CI_BSD_COMPRESS		21	/* config. option for BSD-Compress */
#define CILEN_BSD_COMPRESS	3	/* length of config. option */

/* Macros for handling the 3rd byte of the BSD-Compress config option. */
#define BSD_NBITS(x)		((x) & 0x1F)	/* number of bits requested */
#define BSD_VERSION(x)		((x) >> 5)	/* version of option format */
#define BSD_CURRENT_VERSION	1		/* current version number */
#define BSD_MAKE_OPT(v, n)	(((v) << 5) | (n))

#define BSD_MIN_BITS		9	/* smallest code size supported */
#define BSD_MAX_BITS		15	/* largest code size supported */

/*
 * Definitions for Deflate.
 */
#define CI_DEFLATE		26	/* config option for Deflate */
#define CI_DEFLATE_DRAFT	24	/* value used in original draft RFC */
#define CILEN_DEFLATE		4	/* length of its config option */

#define DEFLATE_MIN_SIZE	8
#define DEFLATE_MAX_SIZE	15
#define DEFLATE_METHOD_VAL	8
#define DEFLATE_SIZE(x)		(((x) >> 4) + DEFLATE_MIN_SIZE)
#define DEFLATE_METHOD(x)	((x) & 0x0F)
#define DEFLATE_MAKE_OPT(w)	((((w) - DEFLATE_MIN_SIZE) << 4) \
				 + DEFLATE_METHOD_VAL)
#define DEFLATE_CHK_SEQUENCE	0

/*
 * Definitions for MPPE.
 */

#define CI_MPPE			18	/* config. option for MPPE */
#define CILEN_MPPE		6	/* length of config. option */

#define MPPE_PAD                4       /* MPPE growth per frame */
#define MPPE_MAX_KEY_LEN        16      /* largest key length (128-bit) */

/* option bits for ccp_options.mppe */
#define MPPE_OPT_40             0x01    /* 40 bit */
#define MPPE_OPT_128            0x02    /* 128 bit */
#define MPPE_OPT_STATEFUL       0x04    /* stateful mode */
/* unsupported opts */
#define MPPE_OPT_56             0x08    /* 56 bit */
#define MPPE_OPT_MPPC           0x10    /* MPPC compression */
#define MPPE_OPT_D              0x20    /* Unknown */
#define MPPE_OPT_UNSUPPORTED (MPPE_OPT_56|MPPE_OPT_MPPC|MPPE_OPT_D)
#define MPPE_OPT_UNKNOWN        0x40    /* Bits !defined in RFC 3078 were set */

/*
 * This is not nice ... the alternative is a bitfield struct though.
 * And unfortunately, we cannot share the same bits for the option
 * names above since C and H are the same bit.  We could do a u_int32
 * but then we have to do a htonl() all the time and/or we still need
 * to know which octet is which.
 */
#define MPPE_C_BIT              0x01    /* MPPC */
#define MPPE_D_BIT              0x10    /* Obsolete, usage unknown */
#define MPPE_L_BIT              0x20    /* 40-bit */
#define MPPE_S_BIT              0x40    /* 128-bit */
#define MPPE_M_BIT              0x80    /* 56-bit, not supported */
#define MPPE_H_BIT              0x01    /* Stateless (in a different byte) */

/* Does not include H bit; used for least significant octet only. */
#define MPPE_ALL_BITS (MPPE_D_BIT|MPPE_L_BIT|MPPE_S_BIT|MPPE_M_BIT|MPPE_H_BIT)

/* Build a CI from mppe opts (see RFC 3078) */
#define MPPE_OPTS_TO_CI(opts, ci)               \
    do {                                        \
        u_char *ptr = ci; /* u_char[4] */       \
                                                \
        /* H bit */                             \
        if (opts & MPPE_OPT_STATEFUL)           \
            *ptr++ = 0x0;                       \
        else                                    \
            *ptr++ = MPPE_H_BIT;                \
        *ptr++ = 0;                             \
        *ptr++ = 0;                             \
                                                \
        /* S,L bits */                          \
        *ptr = 0;                               \
        if (opts & MPPE_OPT_128)                \
            *ptr |= MPPE_S_BIT;                 \
        if (opts & MPPE_OPT_40)                 \
            *ptr |= MPPE_L_BIT;                 \
        /* M,D,C bits not supported */          \
    } while (/* CONSTCOND */ 0)

/* The reverse of the above */
#define MPPE_CI_TO_OPTS(ci, opts)               \
    do {                                        \
        u_char *ptr = ci; /* u_char[4] */       \
                                                \
        opts = 0;                               \
                                                \
        /* H bit */                             \
        if (!(ptr[0] & MPPE_H_BIT))             \
            opts |= MPPE_OPT_STATEFUL;          \
                                                \
        /* S,L bits */                          \
        if (ptr[3] & MPPE_S_BIT)                \
            opts |= MPPE_OPT_128;               \
        if (ptr[3] & MPPE_L_BIT)                \
            opts |= MPPE_OPT_40;                \
                                                \
        /* M,D,C bits */                        \
        if (ptr[3] & MPPE_M_BIT)                \
            opts |= MPPE_OPT_56;                \
        if (ptr[3] & MPPE_D_BIT)                \
            opts |= MPPE_OPT_D;                 \
        if (ptr[3] & MPPE_C_BIT)                \
            opts |= MPPE_OPT_MPPC;              \
                                                \
        /* Other bits */                        \
        if (ptr[0] & ~MPPE_H_BIT)               \
            opts |= MPPE_OPT_UNKNOWN;           \
        if (ptr[1] || ptr[2])                   \
            opts |= MPPE_OPT_UNKNOWN;           \
        if (ptr[3] & ~MPPE_ALL_BITS)            \
            opts |= MPPE_OPT_UNKNOWN;           \
    } while (/* CONSTCOND */ 0)

/*
 * Definitions for other, as yet unsupported, compression methods.
 */
#define CI_PREDICTOR_1		1	/* config option for Predictor-1 */
#define CILEN_PREDICTOR_1	2	/* length of its config option */
#define CI_PREDICTOR_2		2	/* config option for Predictor-2 */
#define CILEN_PREDICTOR_2	2	/* length of its config option */


#ifdef KERNEL

/* 
 * FUNCTION :
 * Register the compressor to the ppp family
 * 
 * PARAMETERS :
 * compreg : 	Registration structure containing compressor information
 *          	and callback functions for the compressor. 
 * 
 * RETURN CODE :
 * 0 : 		No error
 *     		*compref will be filled with a compressor reference, 
 * 		to use in subsequent call to the ppp family
 * EINVAL : 	Invalid registration structure
 * ENOMEM : 	Not enough memory available to register the compressor
 * EEXIST : 	compressor already registered
 */

int ppp_comp_register(struct ppp_comp_reg *compreg, ppp_comp_ref *compref);

/*
 * FUNCTION :
 * Deregister the compressor
 * 
 * PARAMETERS :
 * compref : 	Reference to the compressor previously registered
 *
 * RETURN CODE :
 * 0 : 		No error, 
 * 		The ppp family no longer knows about the compressor
 * EINVAL : 	Invalid reference
 */
int ppp_comp_deregister(ppp_comp_ref *compref);

#endif /* KERNEL */

#endif /* _NET_PPP_COMP_H */
