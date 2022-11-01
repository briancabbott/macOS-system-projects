/* APPLE LOCAL file Panther ObjC enhancements */
/* Check that the compiler does not incorrectly complain about
   exceptions being caught by previous @catch blocks.  */
/* Author: Ziemowit Laski <zlaski@apple.com> */

/* { dg-do compile { target *-*-darwin* } } */
/* { dg-options "-Wall -fobjc-exceptions" } */

@interface Exception
@end

@interface FooException : Exception
@end

extern void foo();

void test()
{
    @try {
        foo();
    }
    @catch (FooException* fe) {
    }
    @catch (Exception* e) {
    }
}

