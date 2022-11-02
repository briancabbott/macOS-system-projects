//
//  Use this file to import your target's public headers that you would like to expose to Swift.
//

#include <gmp.h>
void gmpint_seti(mpz_t *op, long i);
void gmpint_sets(mpz_t *op, const char *str, int base);
void gmpint_unset(mpz_t *op);
size_t gmpint_strlen(mpz_t *op, int base);
char *gmpint2str(mpz_t *op, int base);
int gmpint_fits_int(mpz_t *op);
long gmpint2int(mpz_t *op);
int gmpint_cmp(mpz_t *op, mpz_t *op2);
void gmpint_negz(mpz_t *rop, mpz_t *op);
void gmpint_absz(mpz_t *rop, mpz_t *op);
void gmpint_lshift(mpz_t *rop, mpz_t *op, mp_bitcnt_t bits);
void gmpint_rshift(mpz_t *rop, mpz_t *op, mp_bitcnt_t bits);
void gmpint_addz(mpz_t *rop, mpz_t *op, mpz_t *op2);
void gmpint_subz(mpz_t *rop, mpz_t *op, mpz_t *op2);
void gmpint_mulz(mpz_t *rop, mpz_t *op, mpz_t *op2);
void gmpint_divmodz(mpz_t *r, mpz_t *q, mpz_t *op, mpz_t *op2);
void gmpint_powui(mpz_t *rop, mpz_t *op, unsigned long exp);
void gmpint_powmodz(mpz_t *rop, mpz_t *op, mpz_t *exp, mpz_t *mod);
