/* APPLE LOCAL file call super */
/* Test calling super from within a category method.  */
/* { dg-do compile } */
#import <objc/objc.h>

@interface NSObject
@end
@interface NSMenuItem: NSObject
@end

@interface NSObject (Test)
+ (int) test_func;
@end

@implementation NSObject (Test)
+ (int) test_func
{}
@end

@interface NSMenuItem (Test)
+ (int) test_func;
@end

@implementation NSMenuItem (Test)
+ (int) test_func
{
   return [super test_func];  /* { dg-bogus "dereferencing pointer to incomplete type" } */
}
@end

