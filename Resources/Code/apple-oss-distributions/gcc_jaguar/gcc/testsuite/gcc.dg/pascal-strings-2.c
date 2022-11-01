/* APPLE LOCAL file Negative C test cases 2001-07-05 zll */
/* Origin: Ziemowit Laski <zlaski@apple.com> */
/* { dg-do compile } */
/* { dg-options "-std=iso9899:1999 -Wwrite-strings -fpascal-strings" } */

typedef __WCHAR_TYPE__ wchar_t;

const wchar_t *pascalStr1 = L"\pHi!"; /* { dg-error "not allowed in wide" } */
const wchar_t *pascalStr2 = L"Bye\p!"; /* { dg-error "not allowed in wide" } */

const wchar_t *initErr0 = "\pHi";   /* { dg-warning "incompatible pointer type" } */
const wchar_t initErr0a[] = "\pHi";  /* { dg-error "initialized from non-wide string" } */
const wchar_t *initErr1 = "Bye";   /* { dg-warning "incompatible pointer type" } */
const wchar_t initErr1a[] = "Bye";   /* { dg-error "initialized from non-wide string" } */

const char *initErr2 = L"Hi";   /* { dg-warning "incompatible pointer type" } */
const char initErr2a[] = L"Hi";  /* { dg-error "initialized from wide string" } */
const signed char *initErr3 = L"Hi";  /* { dg-warning "incompatible pointer type" } */
const signed char initErr3a[] = L"Hi";  /* { dg-error "initialized from wide string" } */
const unsigned char *initErr4 = L"Hi";  /* { dg-warning "incompatible pointer type" } */
const unsigned char initErr4a[] = L"Hi"; /* { dg-error "initialized from wide string" } */

const char *pascalStr3 = "Hello\p, World!"; /* { dg-error "must be at beginning" } */

const char *concat2 = "Hi" "\pthere"; /* { dg-error "not allowed in concatenation" } */
const char *concat3 = "Hi" "there\p"; /* { dg-error "must be at beginning" } */

const char *s2 = "\pGoodbye!";   /* ok */
unsigned char *s3 = "\pHi!";     /* { dg-warning "initialization discards qualifiers" } */
char *s4 = "\pHi";               /* { dg-warning "initialization discards qualifiers" } */
signed char *s5 = "\pHi";        /* { dg-warning "initialization discards qualifiers" } */
const signed char *s6 = "\pHi";  /* ok */

/* the maximum length of a Pascal literal is 255. */
const unsigned char *almostTooLong =
  "\p12345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "12345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "12345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "123456789012345";  /* ok */
const unsigned char *definitelyTooLong =
  "\p12345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "12345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "12345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456";  /* { dg-error "too long" } */
