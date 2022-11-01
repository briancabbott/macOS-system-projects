/* { dg-do compile { target "mips*-*-*" } } */
/* { dg-options "-O2 -ffast-math -mips4" } */
/* { dg-final { scan-assembler "nmadd.s" } } */
/* { dg-final { scan-assembler "nmadd.d" } } */
/* { dg-final { scan-assembler "nmsub.s" } } */
/* { dg-final { scan-assembler "nmsub.d" } } */

float
sub1 (float f, float g, float h)
{
  return -((f * g) + h);
}

double
sub2 (double f, double g, double h)
{
  return -((f * g) + h);
}

float
sub3 (float f, float g, float h)
{
  return -((f * g) - h);
}

double
sub4 (double f, double g, double h)
{
  return -((f * g) - h);
}
