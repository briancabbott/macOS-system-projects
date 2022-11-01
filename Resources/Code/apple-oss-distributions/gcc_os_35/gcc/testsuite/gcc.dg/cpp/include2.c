/* Copyright (C) 2000 Free Software Foundation, Inc.  */

/* { dg-do preprocess } */
/* APPLE LOCAL BEGIN -Wextra-tokens */
/* Tests that #include does not allow the terminating '>' or '"' to be
   escaped, as per the standard.  */
/* { dg-options "-Wextra-tokens"  } */
/* Source: Neil Booth, 4 Nov 2000.  */

#include <silly\>>  /* { dg-warning "extra tokens" "" } */
#include "silly\""  /* { dg-warning "extra tokens" "" } */

/* These error is No such file or directory, just once.  However, this
   message is locale-dependent, so don't test for it.  */
/* { dg-error "silly" "" { target *-*-* } 10 } */
/* APPLE LOCAL END  */
