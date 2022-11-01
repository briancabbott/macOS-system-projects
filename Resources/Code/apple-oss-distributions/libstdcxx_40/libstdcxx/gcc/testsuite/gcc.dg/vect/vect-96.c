/* { dg-require-effective-target vect_int } */

#include <stdarg.h>
#include "tree-vect.h"

#define N 16

struct tmp
{
     int x;
     int ia[N];
};

int main1 (int off)
{
  struct tmp sb[N];
  struct tmp *pp = &sb[off];
  int i, ib[N];

  for (i = 0; i < N; i++)
      pp->ia[i] = ib[i];

  /* check results: */  
  for (i = 0; i < N; i++)
    {
       if (pp->ia[i] != ib[i])
         abort();
    }

  return 0;
}

int main (void)
{ 
  check_vect ();

  return main1 (8);
}

/* { dg-final { scan-tree-dump-times "vectorized 1 loops" 1 "vect" { xfail vect_no_align } } } */
/* { dg-final { scan-tree-dump-times "Vectorizing an unaligned access" 1 "vect" { xfail vect_no_align } } } */
/* { dg-final { scan-tree-dump-times "Alignment of access forced using peeling" 1 "vect" } } */
