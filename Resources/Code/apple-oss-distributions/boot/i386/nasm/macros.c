/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* This file auto-generated from standard.mac by macros.pl - don't edit it */

static char *stdmac[] = {
    "%define __NASM_MAJOR__ 0",
    "%define __NASM_MINOR__ 97",
    "%define __FILE__",
    "%define __LINE__",
    "%define __SECT__",
    "%imacro section 1+.nolist",
    "%define __SECT__ [section %1]",
    "__SECT__",
    "%endmacro",
    "%imacro segment 1+.nolist",
    "%define __SECT__ [segment %1]",
    "__SECT__",
    "%endmacro",
    "%imacro absolute 1+.nolist",
    "%define __SECT__ [absolute %1]",
    "__SECT__",
    "%endmacro",
    "%imacro struc 1.nolist",
    "%push struc",
    "%define %$strucname %1",
    "[absolute 0]",
    "%$strucname:",
    "%endmacro",
    "%imacro endstruc 0.nolist",
    "%{$strucname}_size:",
    "%pop",
    "__SECT__",
    "%endmacro",
    "%imacro istruc 1.nolist",
    "%push istruc",
    "%define %$strucname %1",
    "%$strucstart:",
    "%endmacro",
    "%imacro at 1-2+.nolist",
    "times %1-($-%$strucstart) db 0",
    "%2",
    "%endmacro",
    "%imacro iend 0.nolist",
    "times %{$strucname}_size-($-%$strucstart) db 0",
    "%pop",
    "%endmacro",
    "%imacro align 1-2+.nolist nop",
    "times ($$-$) & ((%1)-1) %2",
    "%endmacro",
    "%imacro alignb 1-2+.nolist resb 1",
    "times ($$-$) & ((%1)-1) %2",
    "%endmacro",
    "%imacro extern 1-*.nolist",
    "%rep %0",
    "[extern %1]",
    "%rotate 1",
    "%endrep",
    "%endmacro",
    "%imacro bits 1+.nolist",
    "[bits %1]",
    "%endmacro",
    "%imacro global 1-*.nolist",
    "%rep %0",
    "[global %1]",
    "%rotate 1",
    "%endrep",
    "%endmacro",
    "%imacro common 1-*.nolist",
    "%rep %0",
    "[common %1]",
    "%rotate 1",
    "%endrep",
    "%endmacro",
    NULL
};
