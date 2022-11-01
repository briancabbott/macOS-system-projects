/* APPLE LOCAL file Panther ObjC enhancements */
/* Test if caught exception objects are accessible inside the
   @catch block.  (Yes, I managed to break this.)  */
/* Author: Ziemowit Laski <zlaski@apple.com> */

/* { dg-do compile { target *-*-darwin* } } */
/* { dg-options "-fobjc-exceptions" } */

#include <objc/Object.h>

const char *foo(void)
{
    @try {
        return "foo";
    }
    @catch (Object* theException) {
          return [theException name];
    }
}

