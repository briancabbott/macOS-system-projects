/* APPLE LOCAL file.  */
/* { dg-do run } */
/* { dg-options "-O3 -funroll-loops -ftree-vectorize -fdump-tree-vect-stats -maltivec" { target powerpc*-*-* } } */


#include <stdarg.h>
#include <signal.h>

#define N 16
#define MAX 42

extern void abort(void); 

int A[N] = {36,39,42,45,43,32,21,12,23,34,45,56,67,78,89,11};
int B[N] = {0,0,42,42,42,0,0,0,0,0,42,42,42,42,42,0};
void foo ()  __attribute__((always_inline));
void foo ()
{  
  int i, j;

  for (i = 0; i < 16; i++)
    A[i] = ( A[i] >= MAX ? MAX : 0); 
}

int main ()
{

  int i, j;
  foo ();
  /* check results:  */
  for (i = 0; i < N; i++)
    if (A[i] != B[i])
      abort ();

  return 0;
}



/* { dg-final { scan-tree-dump-times "vectorized 1 loops" 2 "vect" { xfail i?86-*-* x86_64-*-* } } } */
