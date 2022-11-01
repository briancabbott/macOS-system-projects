/* { dg-require-effective-target vect_int } */

#include <stdarg.h>
#include "tree-vect.h"

#define N 8
#define OFF 8

typedef int aint __attribute__ ((__aligned__(16)));

aint ib[N+OFF] = {0, 1, 3, 5, 7, 11, 13, 17, 0, 2, 6, 10, 14, 22, 26, 34};
int off = 8;

int main1 (aint *ib)
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

  main1 (ib);
  return 0;
}

/* { dg-final { scan-tree-dump-times "vectorized 1 loops" 1 "vect" { xfail { lp64 || vect_no_align } } } } */
/* { dg-final { scan-tree-dump-times "Vectorizing an unaligned access" 1 "vect" { xfail { lp64 || vect_no_align } } } } */
/* { dg-final { scan-tree-dump-times "Alignment of access forced using peeling" 0 "vect" } } */
