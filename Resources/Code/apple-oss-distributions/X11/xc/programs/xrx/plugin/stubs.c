/* $Xorg: stubs.c,v 1.3 2000/08/17 19:54:59 cpqbld Exp $ */
/* -*- Mode: C; tab-width: 4; -*- */
/*******************************************************************************
 * Simple LiveConnect Sample Plugin
 * Copyright (c) 1996 Netscape Communications. All rights reserved.
 ******************************************************************************/

/*
** Ok, so we don't usually include .c files (only .h files) but we're
** doing it here to avoid some fancy make rules. First pull in the common
** glue code:
*/
#ifdef XP_UNIX
#include "common/npunix.c"
#endif
