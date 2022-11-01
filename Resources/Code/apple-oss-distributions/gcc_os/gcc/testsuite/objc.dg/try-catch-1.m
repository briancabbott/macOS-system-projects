/* APPLE LOCAL file Panther ObjC enhancements */
/* Test if the compiler accepts @throw / @try..@catch..@finally 
   syntax.  This will only be usable on MacOS X 10.3 and later.  */
/* Developed by Ziemowit Laski <zlaski@apple.com>.  */
/* { dg-options "-fobjc-exceptions" } */
/* { dg-do compile { target *-*-darwin* } } */

#import <objc/objc.h>
#import <objc/objc-runtime.h>
#import <objc/Object.h>
#include <stdio.h>
#include <setjmp.h>

@interface Frob: Object
@end

@implementation Frob: Object
@end

static int exc_control = 0;

int proc() {
  if(exc_control) {
    printf ("Throwing (%d)... ", exc_control);
    @throw [Frob new];
  }
  return 1;
}

int foo()
{
        @try {
                return proc();
        }
        @catch (Frob* ex) {
		if(exc_control > 1) {
		  printf("Rethrowing (%d)... ", exc_control);
		  @throw;
		}
		return 0;
        }
	@finally {
		printf("In @finally block (%d)... ", exc_control);
	}
}
