#ifndef OBJC_OBJC_H_
#define OBJC_OBJC_H_

#define OBJC_ARC_UNAVAILABLE __attribute__((unavailable("not available in automatic reference counting mode")))
#define NS_AUTOMATED_REFCOUNT_UNAVAILABLE OBJC_ARC_UNAVAILABLE

typedef unsigned long NSUInteger;
typedef long NSInteger;
typedef __typeof__(__objc_yes) BOOL;

typedef struct objc_selector    *SEL;
SEL sel_registerName(const char *str);

void NSDeallocateObject(id object) NS_AUTOMATED_REFCOUNT_UNAVAILABLE;

#undef NS_AUTOMATED_REFCOUNT_UNAVAILABLE

#define OBJC_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
#define OBJC_OPTIONS(_type, _name) enum _name : _type _name; enum _name : _type

typedef OBJC_ENUM(int, objc_abi) {
  objc_v1 = 0,
  objc_v2 = 2
};

typedef OBJC_OPTIONS(int, objc_flags) {
  objc_taggedPointer = 1 << 0,
  objc_swiftRefcount = 1 << 1
};

#endif
