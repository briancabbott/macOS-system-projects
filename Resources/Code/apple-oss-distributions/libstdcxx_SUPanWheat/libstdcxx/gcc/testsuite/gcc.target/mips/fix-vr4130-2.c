/* { dg-do compile { target mips*-*-* } } */
/* { dg-skip-if "" { mips-sgi-irix* } { "-mabi=32" } { "" } } */
/* { dg-options "-mfix-vr4130 -march=vr4130" } */
#if _MIPS_ARCH_VR4130 && !__mips16
int foo (void) { int r; asm ("# foo" : "=l" (r)); return r; }
#else
asm ("#\tmacc\t");
#endif
/* { dg-final { scan-assembler "\tmacc\t" } } */
