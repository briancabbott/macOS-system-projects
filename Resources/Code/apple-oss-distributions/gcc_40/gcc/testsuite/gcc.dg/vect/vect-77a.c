/* { dg-do run { target powerpc*-*-* } } */
/* { dg-do run { target i?86-*-* x86_64-*-* } } */
/* { dg-options "-O2 -ftree-vectorize -fdump-tree-vect-stats -maltivec" { target powerpc*-*-* } } */
/* { dg-options "-O2 -ftree-vectorize -fdump-tree-vect-stats -mmmx" { target i?86-*-* x86_64-*-* } } */

#include <stdarg.h>
#include "tree-vect.h"

#define N 8
#define OFF 8

typedef int aint __attribute__ ((__aligned__(16)));

aint ib[N+OFF] = {0, 1, 3, 5, 7, 11, 13, 17, 0, 2, 6, 10, 14, 22, 26, 34};

int main1 (aint *ib, int off)
{
  int i;
  int ia[N];

  for (i = 0; i < N; i++)
    {
      ia[i] = ib[i+off];
    }


  /* check results:  */
  for (i = 0; i < N; i++)
    {
     if (ia[i] != ib[i+off])
        abort ();
    }

  return 0;
}

int main (void)
{
  check_vect ();

  main1 (ib, 8);
  return 0;
}


/* { dg-final { scan-tree-dump-times "vectorized 1 loops" 1 "vect" } } */

