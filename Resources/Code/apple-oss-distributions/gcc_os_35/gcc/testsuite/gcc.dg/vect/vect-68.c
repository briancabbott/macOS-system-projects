/* { dg-do run { target powerpc*-*-* } } */
/* { dg-do run { target i?86-*-* x86_64-*-* } } */
/* { dg-options "-O2 -ftree-vectorize -fdump-tree-vect-stats -maltivec" { target powerpc*-*-* } } */
/* { dg-options "-O2 -ftree-vectorize -fdump-tree-vect-stats -msse2" { target i?86-*-* x86_64-*-* } } */

#include <stdarg.h>
#include "tree-vect.h"

#define N 32

struct s{
  int m;
  int n[N][N][N];
};

struct test1{
  struct s a; /* array a.n is unaligned */
  int b;
  int c;
  struct s e; /* array e.n is aligned */
};

int main1 ()
{  
  int i,j;
  struct test1 tmp1;

  /* 1. unaligned */
  for (i = 0; i < N; i++)
    {
      tmp1.a.n[1][2][i] = 5;
    }

  /* check results:  */
  for (i = 0; i <N; i++)
    {
      if (tmp1.a.n[1][2][i] != 5)
        abort ();
    }

  /* 2. aligned */
  for (i = 3; i < N-1; i++)
    {
      tmp1.a.n[1][2][i] = 6;
    }

  /* check results:  */
  for (i = 3; i < N-1; i++)
    {
      if (tmp1.a.n[1][2][i] != 6)
        abort ();
    }

  /* 3. aligned */
  for (i = 0; i < N; i++)
    {
      tmp1.e.n[1][2][i] = 7;
    }

  /* check results:  */
  for (i = 0; i < N; i++)
    {
      if (tmp1.e.n[1][2][i] != 7)
        abort ();
    }

  /* 4. unaligned */
  for (i = 3; i < N-3; i++)
    {
      tmp1.e.n[1][2][i] = 8;
    }
 
  /* check results:  */
  for (i = 3; i <N-3; i++)
    {
      if (tmp1.e.n[1][2][i] != 8)
        abort ();
    }

  return 0;
}

int main (void)
{ 
  check_vect ();
  
  return main1 ();
} 

/* { dg-final { scan-tree-dump-times "vectorized 2 loops" 1 "vect" } } */
