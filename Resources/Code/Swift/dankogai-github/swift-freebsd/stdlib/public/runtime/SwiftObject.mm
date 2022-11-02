//===--- SwiftObject.mm - Native Swift Object root class ------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
//
// This implements runtime support for bridging between Swift and Objective-C
// types in cases where they aren't trivial.
//
//===----------------------------------------------------------------------===//

#include "swift/Runtime/Config.h"
#if SWIFT_OBJC_INTEROP
#include <objc/NSObject.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <objc/objc.h>
#endif
#include "llvm/ADT/StringRef.h"
#include "swift/Basic/Demangle.h"
#include "swift/Basic/Lazy.h"
#include "swift/Runtime/Heap.h"
#include "swift/Runtime/HeapObject.h"
#include "swift/Runtime/Metadata.h"
#include "swift/Runtime/ObjCBridge.h"
#include "swift/Strings.h"
#include "../SwiftShims/RuntimeShims.h"
#include "Private.h"
#include "swift/Runtime/Debug.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <mutex>
#include <unordered_map>
#if SWIFT_OBJC_INTEROP
# import <CoreFoundation/CFBase.h> // for CFTypeID
# include <malloc/malloc.h>
# include <dispatch/dispatch.h>
#endif
#if SWIFT_RUNTIME_ENABLE_DTRACE
# include "SwiftRuntimeDTraceProbes.h"
#else
#define SWIFT_ISUNIQUELYREFERENCED()
#define SWIFT_ISUNIQUELYREFERENCEDORPINNED()
#endif

using namespace swift;

#if SWIFT_HAS_ISA_MASKING
extern "C" __attribute__((weak_import))
const uintptr_t objc_debug_isa_class_mask;

static uintptr_t computeISAMask() {
  // The versions of the Objective-C runtime which use non-pointer
  // ISAs also export this symbol.
  if (auto runtimeSymbol = &objc_debug_isa_class_mask)
    return *runtimeSymbol;
  return ~uintptr_t(0);
}

SWIFT_ALLOWED_RUNTIME_GLOBAL_CTOR_BEGIN
uintptr_t swift::swift_isaMask = computeISAMask();
SWIFT_ALLOWED_RUNTIME_GLOBAL_CTOR_END
#endif

const ClassMetadata *swift::_swift_getClass(const void *object) {
#if SWIFT_OBJC_INTEROP
  if (!isObjCTaggedPointer(object))
    return _swift_getClassOfAllocated(object);
  return reinterpret_cast<const ClassMetadata*>(object_getClass((id) object));
#else
  return _swift_getClassOfAllocated(object);
#endif
}

#if SWIFT_OBJC_INTEROP
struct SwiftObject_s {
  void *isa  __attribute__((unavailable));
  long refCount  __attribute__((unavailable));
};

static_assert(std::is_trivially_constructible<SwiftObject_s>::value,
              "SwiftObject must be trivially constructible");
static_assert(std::is_trivially_destructible<SwiftObject_s>::value,
              "SwiftObject must be trivially destructible");

#if __has_attribute(objc_root_class)
__attribute__((objc_root_class))
#endif
@interface SwiftObject<NSObject> {
  // FIXME: rdar://problem/18950072 Clang emits ObjC++ classes as having
  // non-trivial structors if they contain any struct fields at all, regardless of
  // whether they in fact have nontrivial default constructors. Dupe the body
  // of SwiftObject_s into here as a workaround because we don't want to pay
  // the cost of .cxx_destruct method dispatch at deallocation time.
  void *magic_isa  __attribute__((unavailable));
  long magic_refCount  __attribute__((unavailable));
}

- (BOOL)isEqual:(id)object;
- (NSUInteger)hash;

- (Class)superclass;
- (Class)class;
- (instancetype)self;
- (struct _NSZone *)zone;

- (id)performSelector:(SEL)aSelector;
- (id)performSelector:(SEL)aSelector withObject:(id)object;
- (id)performSelector:(SEL)aSelector withObject:(id)object1 withObject:(id)object2;

- (BOOL)isProxy;

+ (BOOL)isSubclassOfClass:(Class)aClass;
- (BOOL)isKindOfClass:(Class)aClass;
- (BOOL)isMemberOfClass:(Class)aClass;
- (BOOL)conformsToProtocol:(Protocol *)aProtocol;

- (BOOL)respondsToSelector:(SEL)aSelector;

- (instancetype)retain;
- (oneway void)release;
- (instancetype)autorelease;
- (NSUInteger)retainCount;

- (NSString *)description;
- (NSString *)debugDescription;
@end

static SwiftObject *_allocHelper(Class cls) {
  // XXX FIXME
  // When we have layout information, do precise alignment rounding
  // For now, assume someone is using hardware vector types
#if defined(__x86_64__) || defined(__i386__)
  const size_t mask = 32 - 1;
#else
  const size_t mask = 16 - 1;
#endif
  return reinterpret_cast<SwiftObject *>(swift::swift_allocObject(
    reinterpret_cast<HeapMetadata const *>(cls),
    class_getInstanceSize(cls), mask));
}

// Helper from the standard library for stringizing an arbitrary object.
namespace {
  struct String { void *x, *y, *z; };
}

extern "C" void swift_getSummary(String *out, OpaqueValue *value,
                                 const Metadata *T);

static NSString *_getDescription(SwiftObject *obj) {
  // Cached lookup of swift_convertStringToNSString, which is in Foundation.
  static NSString *(*convertStringToNSString)(void *sx, void *sy, void *sz)
    = nullptr;
  
  if (!convertStringToNSString) {
    convertStringToNSString = (decltype(convertStringToNSString))(uintptr_t)
      dlsym(RTLD_DEFAULT, "swift_convertStringToNSString");
    // If Foundation hasn't loaded yet, fall back to returning the static string
    // "SwiftObject". The likelihood of someone invoking -description without
    // ObjC interop is low.
    if (!convertStringToNSString)
      return @"SwiftObject";
  }
  
  String tmp;
  swift_retain((HeapObject*)obj);
  swift_getSummary(&tmp, (OpaqueValue*)&obj, _swift_getClassOfAllocated(obj));
  return [convertStringToNSString(tmp.x, tmp.y, tmp.z) autorelease];
}

static NSString *_getClassDescription(Class cls) {
  // Cached lookup of NSStringFromClass, which is in Foundation.
  static NSString *(*NSStringFromClass_fn)(Class cls) = nullptr;
  
  if (!NSStringFromClass_fn) {
    NSStringFromClass_fn = (decltype(NSStringFromClass_fn))(uintptr_t)
      dlsym(RTLD_DEFAULT, "NSStringFromClass");
    // If Foundation hasn't loaded yet, fall back to returning the static string
    // "SwiftObject". The likelihood of someone invoking +description without
    // ObjC interop is low.
    if (!NSStringFromClass_fn)
      return @"SwiftObject";
  }
  
  return NSStringFromClass_fn(cls);
}


@implementation SwiftObject
+ (void)initialize {}

+ (instancetype)allocWithZone:(struct _NSZone *)zone {
  assert(zone == nullptr);
  return _allocHelper(self);
}

+ (instancetype)alloc {
  // we do not support "placement new" or zones,
  // so there is no need to call allocWithZone
  return _allocHelper(self);
}

+ (Class)class {
  return self;
}
- (Class)class {
  return (Class) _swift_getClassOfAllocated(self);
}
+ (Class)superclass {
  return (Class) _swift_getSuperclass((const ClassMetadata*) self);
}
- (Class)superclass {
  return (Class) _swift_getSuperclass(_swift_getClassOfAllocated(self));
}

+ (BOOL)isMemberOfClass:(Class)cls {
  return cls == (Class) _swift_getClassOfAllocated(self);
}

- (BOOL)isMemberOfClass:(Class)cls {
  return cls == (Class) _swift_getClassOfAllocated(self);
}

- (instancetype)self {
  return self;
}
- (BOOL)isProxy {
  return NO;
}

- (struct _NSZone *)zone {
  return (struct _NSZone *)
    (malloc_zone_from_ptr(self) ?: malloc_default_zone());
}

- (void)doesNotRecognizeSelector: (SEL) sel {
  Class cls = (Class) _swift_getClassOfAllocated(self);
  fatalError("Unrecognized selector %c[%s %s]\n", 
             class_isMetaClass(cls) ? '+' : '-', 
             class_getName(cls), sel_getName(sel));
}

- (id)retain {
  auto SELF = reinterpret_cast<HeapObject *>(self);
  swift_retain(SELF);
  return self;
}
- (void)release {
  auto SELF = reinterpret_cast<HeapObject *>(self);
  swift_release(SELF);
}
- (id)autorelease {
  return _objc_rootAutorelease(self);
}
- (NSUInteger)retainCount {
  return swift::swift_retainCount(reinterpret_cast<HeapObject *>(self));
}
- (BOOL)_isDeallocating {
  return swift_isDeallocating(reinterpret_cast<HeapObject *>(self));
}
- (BOOL)_tryRetain {
  return swift_tryRetain(reinterpret_cast<HeapObject*>(self)) != nullptr;
}
- (BOOL)allowsWeakReference {
  return !swift_isDeallocating(reinterpret_cast<HeapObject *>(self));
}
- (BOOL)retainWeakReference {
  return swift_tryRetain(reinterpret_cast<HeapObject*>(self)) != nullptr;
}

// Retaining the class object itself is a no-op.
+ (id)retain {
  return self;
}
+ (void)release {
  /* empty */
}
+ (id)autorelease {
  return self;
}
+ (NSUInteger)retainCount {
  return ULONG_MAX;
}
+ (BOOL)_isDeallocating {
  return NO;
}
+ (BOOL)_tryRetain {
  return YES;
}
+ (BOOL)allowsWeakReference {
  return YES;
}
+ (BOOL)retainWeakReference {
  return YES;
}

- (void)dealloc {
  swift_rootObjCDealloc(reinterpret_cast<HeapObject *>(self));
}

- (BOOL)isKindOfClass:(Class)someClass {
  for (auto isa = _swift_getClassOfAllocated(self); isa != nullptr;
       isa = _swift_getSuperclass(isa))
    if (isa == (const ClassMetadata*) someClass)
      return YES;

  return NO;
}

+ (BOOL)isSubclassOfClass:(Class)someClass {
  for (auto isa = (const ClassMetadata*) self; isa != nullptr;
       isa = _swift_getSuperclass(isa))
    if (isa == (const ClassMetadata*) someClass)
      return YES;

  return NO;
}

+ (BOOL)respondsToSelector:(SEL)sel {
  if (!sel) return NO;
  return class_respondsToSelector((Class) _swift_getClassOfAllocated(self), sel);
}

- (BOOL)respondsToSelector:(SEL)sel {
  if (!sel) return NO;
  return class_respondsToSelector((Class) _swift_getClassOfAllocated(self), sel);
}

+ (BOOL)instancesRespondToSelector:(SEL)sel {
  if (!sel) return NO;
  return class_respondsToSelector(self, sel);
}

- (BOOL)conformsToProtocol:(Protocol*)proto {
  if (!proto) return NO;
  auto selfClass = (Class) _swift_getClassOfAllocated(self);
  
  // Walk the superclass chain.
  while (selfClass) {
    if (class_conformsToProtocol(selfClass, proto))
      return YES;
    selfClass = class_getSuperclass(selfClass);
  }

  return NO;
}

+ (BOOL)conformsToProtocol:(Protocol*)proto {
  if (!proto) return NO;

  // Walk the superclass chain.
  Class selfClass = self;
  while (selfClass) {
    if (class_conformsToProtocol(selfClass, proto))
      return YES;
    selfClass = class_getSuperclass(selfClass);
  }
  
  return NO;
}

- (NSUInteger)hash {
  return (NSUInteger)self;
}

- (BOOL)isEqual:(id)object {
  return self == object;
}

- (id)performSelector:(SEL)aSelector {
  return ((id(*)(id, SEL))objc_msgSend)(self, aSelector);
}

- (id)performSelector:(SEL)aSelector withObject:(id)object {
  return ((id(*)(id, SEL, id))objc_msgSend)(self, aSelector, object);
}

- (id)performSelector:(SEL)aSelector withObject:(id)object1
                                     withObject:(id)object2 {
  return ((id(*)(id, SEL, id, id))objc_msgSend)(self, aSelector, object1,
                                                                 object2);
}

- (NSString *)description {
  return _getDescription(self);
}
- (NSString *)debugDescription {
  return _getDescription(self);
}

+ (NSString *)description {
  return _getClassDescription(self);
}
+ (NSString *)debugDescription {
  return _getClassDescription(self);
}

- (NSString *)_copyDescription {
  // The NSObject version of this pushes an autoreleasepool in case -description
  // autoreleases, but we're OK with leaking things if we're at the top level
  // of the main thread with no autorelease pool.
  return [[self description] retain];
}

- (CFTypeID)_cfTypeID {
  // Adopt the same CFTypeID as NSObject.
  static CFTypeID result;
  static dispatch_once_t predicate;
  dispatch_once_f(&predicate, &result, [](void *resultAddr) {
    id obj = [[NSObject alloc] init];
    *(CFTypeID*)resultAddr = [obj _cfTypeID];
    [obj release];
  });
  return result;
}

// Foundation collections expect these to be implemented.
- (BOOL)isNSArray__      { return NO; }
- (BOOL)isNSDictionary__ { return NO; }
- (BOOL)isNSSet__        { return NO; }
- (BOOL)isNSOrderedSet__ { return NO; }
- (BOOL)isNSNumber__     { return NO; }
- (BOOL)isNSData__       { return NO; }
- (BOOL)isNSDate__       { return NO; }
- (BOOL)isNSString__     { return NO; }
- (BOOL)isNSValue__      { return NO; }

@end

/*****************************************************************************/
/****************************** WEAK REFERENCES ******************************/
/*****************************************************************************/

/// A side-table of shared weak references for use by the unowned entry.
///
/// FIXME: this needs to be integrated with the ObjC runtime so that
/// entries will actually get collected.  Also, that would make this just
/// a simple manipulation of the internal structures there.
///
/// FIXME: this is not actually safe; if the ObjC runtime deallocates
/// the pointer, the keys in UnownedRefs will become dangling
/// references.  rdar://16968733
namespace {
  struct UnownedRefEntry {
    id Value;
    size_t Count;
  };
}

// The ObjC runtime will hold a point into the UnownedRefEntry,
// so we require pointers to objects to be stable across rehashes.
// DenseMap doesn't guarantee that, but std::unordered_map does.

namespace {
struct UnownedTable {
  std::unordered_map<const void*, UnownedRefEntry> Refs;
  std::mutex Mutex;
};
}

static Lazy<UnownedTable> UnownedRefs;

static void objc_rootRetainUnowned(id object) {
  auto &Unowned = UnownedRefs.get();

  std::lock_guard<std::mutex> lock(Unowned.Mutex);
  auto it = Unowned.Refs.find((const void*) object);
  assert(it != Unowned.Refs.end());
  assert(it->second.Count > 0);

  // Do an unbalanced retain.
  id result = objc_loadWeakRetained(&it->second.Value);

  // If that yielded null, abort.
  if (!result) _swift_abortRetainUnowned((const void*) object);
}

static void objc_rootWeakRetain(id object) {
  auto &Unowned = UnownedRefs.get();

  std::lock_guard<std::mutex> lock(Unowned.Mutex);
  auto ins = Unowned.Refs.insert({ (const void*) object, UnownedRefEntry() });
  if (!ins.second) {
    ins.first->second.Count++;
  } else {
    objc_initWeak(&ins.first->second.Value, object);
    ins.first->second.Count = 1;
  }
}

static void objc_rootWeakRelease(id object) {
  auto &Unowned = UnownedRefs.get();

  std::lock_guard<std::mutex> lock(Unowned.Mutex);
  auto it = Unowned.Refs.find((const void*) object);
  assert(it != Unowned.Refs.end());
  assert(it->second.Count > 0);
  if (--it->second.Count == 0) {
    objc_destroyWeak(&it->second.Value);
    Unowned.Refs.erase(it);
  }
}
#endif

/// Decide dynamically whether the given object uses native Swift
/// reference-counting.
bool swift::usesNativeSwiftReferenceCounting(const ClassMetadata *theClass) {
#if SWIFT_OBJC_INTEROP
  if (!theClass->isTypeMetadata()) return false;
  return (theClass->getFlags() & ClassFlags::UsesSwift1Refcounting);
#else
  return true;
#endif
}

// version for SwiftShims
bool
swift::_swift_usesNativeSwiftReferenceCounting_class(const void *theClass) {
#if SWIFT_OBJC_INTEROP
  return usesNativeSwiftReferenceCounting((const ClassMetadata *)theClass);
#else
  return true;
#endif
}

// The non-pointer bits, excluding the ObjC tag bits.
static auto const unTaggedNonNativeBridgeObjectBits
  = heap_object_abi::SwiftSpareBitsMask
  & ~heap_object_abi::ObjCReservedBitsMask;

#if SWIFT_OBJC_INTEROP

#if defined(__x86_64__)
static uintptr_t const objectPointerIsObjCBit = 0x4000000000000000ULL;
#elif defined(__arm64__)
static uintptr_t const objectPointerIsObjCBit = 0x4000000000000000ULL;
#else
static uintptr_t const objectPointerIsObjCBit = 0x00000002U;
#endif

static bool usesNativeSwiftReferenceCounting_allocated(const void *object) {
  assert(!isObjCTaggedPointerOrNull(object));
  return usesNativeSwiftReferenceCounting(_swift_getClassOfAllocated(object));
}

static bool usesNativeSwiftReferenceCounting_unowned(const void *object) {
  auto &Unowned = UnownedRefs.get();

  // If an unknown object is unowned-referenced, it may in fact be implemented
  // using an ObjC weak reference, which will eagerly deallocate the object
  // when strongly released. We have to check first whether the object is in
  // the side table before dereferencing the pointer.
  if (Unowned.Refs.count(object))
    return false;
  // For a natively unowned reference, even after all strong references have
  // been released, there's enough of a husk left behind to determine its
  // species.
  return usesNativeSwiftReferenceCounting_allocated(object);
}

void swift::swift_unknownRetain_n(void *object, int n) {
  if (isObjCTaggedPointerOrNull(object)) return;
  if (usesNativeSwiftReferenceCounting_allocated(object)) {
    swift_retain_n(static_cast<HeapObject *>(object), n);
    return;
  }
  for (int i = 0; i < n; ++i)
    objc_retain(static_cast<id>(object));
}

void swift::swift_unknownRelease_n(void *object, int n) {
  if (isObjCTaggedPointerOrNull(object)) return;
  if (usesNativeSwiftReferenceCounting_allocated(object))
    return swift_release_n(static_cast<HeapObject *>(object), n);
  for (int i = 0; i < n; ++i)
    objc_release(static_cast<id>(object));
}

void swift::swift_unknownRetain(void *object) {
  if (isObjCTaggedPointerOrNull(object)) return;
  if (usesNativeSwiftReferenceCounting_allocated(object)) {
    swift_retain(static_cast<HeapObject *>(object));
    return;
  }
  objc_retain(static_cast<id>(object));
}

void swift::swift_unknownRelease(void *object) {
  if (isObjCTaggedPointerOrNull(object)) return;
  if (usesNativeSwiftReferenceCounting_allocated(object))
    return swift_release(static_cast<HeapObject *>(object));
  return objc_release(static_cast<id>(object));
}

/// Return true iff the given BridgeObject is not known to use native
/// reference-counting.
///
/// Requires: object does not encode a tagged pointer
static bool isNonNative_unTagged_bridgeObject(void *object) {
  static_assert((heap_object_abi::SwiftSpareBitsMask & objectPointerIsObjCBit) ==
                objectPointerIsObjCBit,
                "isObjC bit not within spare bits");
  return (uintptr_t(object) & objectPointerIsObjCBit) != 0;
}
#endif

// Mask out the spare bits in a bridgeObject, returning the object it
// encodes.
///
/// Requires: object does not encode a tagged pointer
static void* toPlainObject_unTagged_bridgeObject(void *object) {
  return (void*)(uintptr_t(object) & ~unTaggedNonNativeBridgeObjectBits);
}

void *swift::swift_bridgeObjectRetain(void *object) {
#if SWIFT_OBJC_INTEROP
  if (isObjCTaggedPointer(object))
    return object;
#endif

  auto const objectRef = toPlainObject_unTagged_bridgeObject(object);

#if SWIFT_OBJC_INTEROP
  if (!isNonNative_unTagged_bridgeObject(object)) {
    swift_retain(static_cast<HeapObject *>(objectRef));
    return static_cast<HeapObject *>(objectRef);
  }
  return objc_retain(static_cast<id>(objectRef));
#else
  swift_retain(static_cast<HeapObject *>(objectRef));
  return static_cast<HeapObject *>(objectRef);
#endif
}

void swift::swift_bridgeObjectRelease(void *object) {
#if SWIFT_OBJC_INTEROP
  if (isObjCTaggedPointer(object))
    return;
#endif

  auto const objectRef = toPlainObject_unTagged_bridgeObject(object);

#if SWIFT_OBJC_INTEROP
  if (!isNonNative_unTagged_bridgeObject(object))
    return swift_release(static_cast<HeapObject *>(objectRef));
  return objc_release(static_cast<id>(objectRef));
#else
  swift_release(static_cast<HeapObject *>(objectRef));
#endif
}

void *swift::swift_bridgeObjectRetain_n(void *object, int n) {
#if SWIFT_OBJC_INTEROP
  if (isObjCTaggedPointer(object))
    return object;
#endif

  auto const objectRef = toPlainObject_unTagged_bridgeObject(object);

#if SWIFT_OBJC_INTEROP
  void *objc_ret = nullptr;
  if (!isNonNative_unTagged_bridgeObject(object)) {
    swift_retain_n(static_cast<HeapObject *>(objectRef), n);
    return static_cast<HeapObject *>(objectRef);
  }
  for (int i = 0;i < n; ++i)
    objc_ret = objc_retain(static_cast<id>(objectRef));
  return objc_ret;
#else
  swift_retain_n(static_cast<HeapObject *>(objectRef), n);
  return static_cast<HeapObject *>(objectRef);
#endif
}

void swift::swift_bridgeObjectRelease_n(void *object, int n) {
#if SWIFT_OBJC_INTEROP
  if (isObjCTaggedPointer(object))
    return;
#endif

  auto const objectRef = toPlainObject_unTagged_bridgeObject(object);

#if SWIFT_OBJC_INTEROP
  if (!isNonNative_unTagged_bridgeObject(object))
    return swift_release_n(static_cast<HeapObject *>(objectRef), n);
  for (int i = 0; i < n; ++i)
    objc_release(static_cast<id>(objectRef));
#else
  swift_release_n(static_cast<HeapObject *>(objectRef), n);
#endif
}


#if SWIFT_OBJC_INTEROP
void swift::swift_unknownRetainUnowned(void *object) {
  if (isObjCTaggedPointerOrNull(object)) return;
  if (usesNativeSwiftReferenceCounting_unowned(object))
    return swift_retainUnowned((HeapObject*) object);
  objc_rootRetainUnowned((id) object);
}

void swift::swift_unknownWeakRetain(void *object) {
  if (isObjCTaggedPointerOrNull(object)) return;
  if (usesNativeSwiftReferenceCounting_unowned(object))
    return swift_weakRetain((HeapObject*) object);
  objc_rootWeakRetain((id) object);
}
void swift::swift_unknownWeakRelease(void *object) {
  if (isObjCTaggedPointerOrNull(object)) return;
  if (usesNativeSwiftReferenceCounting_unowned(object))
    return swift_weakRelease((HeapObject*) object);
  objc_rootWeakRelease((id) object);
}

// FIXME: these are not really valid implementations; they assume too
// much about the implementation of ObjC weak references, and the
// loads from ->Value can race with clears by the runtime.

static void doWeakInit(WeakReference *addr, void *value, bool valueIsNative) {
  assert(value != nullptr);
  if (valueIsNative) {
    swift_weakInit(addr, (HeapObject*) value);
  } else {
    objc_initWeak((id*) &addr->Value, (id) value);
  }
}

static void doWeakDestroy(WeakReference *addr, bool valueIsNative) {
  if (valueIsNative) {
    swift_weakDestroy(addr);
  } else {
    objc_destroyWeak((id*) &addr->Value);
  }
}

void swift::swift_unknownWeakInit(WeakReference *addr, void *value) {
  if (isObjCTaggedPointerOrNull(value)) {
    addr->Value = (HeapObject*) value;
    return;
  }
  doWeakInit(addr, value, usesNativeSwiftReferenceCounting_allocated(value));
}

void swift::swift_unknownWeakAssign(WeakReference *addr, void *newValue) {
  // If the incoming value is not allocated, this is just a destroy
  // and re-initialize.
  if (isObjCTaggedPointerOrNull(newValue)) {
    swift_unknownWeakDestroy(addr);
    addr->Value = (HeapObject*) newValue;
    return;
  }

  bool newIsNative = usesNativeSwiftReferenceCounting_allocated(newValue);

  // If the existing value is not allocated, this is just an initialize.
  void *oldValue = addr->Value;
  if (isObjCTaggedPointerOrNull(oldValue))
    return doWeakInit(addr, newValue, newIsNative);

  bool oldIsNative = usesNativeSwiftReferenceCounting_allocated(oldValue);

  // If they're both native, we can use the native function.
  if (oldIsNative && newIsNative)
    return swift_weakAssign(addr, (HeapObject*) newValue);

  // If neither is native, we can use the ObjC function.
  if (!oldIsNative && !newIsNative)
    return (void) objc_storeWeak((id*) &addr->Value, (id) newValue);

  // Otherwise, destroy according to one set of semantics and
  // re-initialize with the other.
  doWeakDestroy(addr, oldIsNative);
  doWeakInit(addr, newValue, newIsNative);
}

void *swift::swift_unknownWeakLoadStrong(WeakReference *addr) {
  void *value = addr->Value;
  if (isObjCTaggedPointerOrNull(value)) return value;

  if (usesNativeSwiftReferenceCounting_allocated(value)) {
    return swift_weakLoadStrong(addr);
  } else {
    return (void*) objc_loadWeakRetained((id*) &addr->Value);
  }
}

void *swift::swift_unknownWeakTakeStrong(WeakReference *addr) {
  void *value = addr->Value;
  if (isObjCTaggedPointerOrNull(value)) return value;

  if (usesNativeSwiftReferenceCounting_allocated(value)) {
    return swift_weakTakeStrong(addr);
  } else {
    void *result = (void*) objc_loadWeakRetained((id*) &addr->Value);
    objc_destroyWeak((id*) &addr->Value);
    return result;
  }
}

void swift::swift_unknownWeakDestroy(WeakReference *addr) {
  id object = (id) addr->Value;
  if (isObjCTaggedPointerOrNull(object)) return;
  doWeakDestroy(addr, usesNativeSwiftReferenceCounting_allocated(object));
}
void swift::swift_unknownWeakCopyInit(WeakReference *dest, WeakReference *src) {
  id object = (id) src->Value;
  if (isObjCTaggedPointerOrNull(object)) {
    dest->Value = (HeapObject*) object;
    return;
  }
  if (usesNativeSwiftReferenceCounting_allocated(object))
    return swift_weakCopyInit(dest, src);
  objc_copyWeak((id*) &dest->Value, (id*) src);
}
void swift::swift_unknownWeakTakeInit(WeakReference *dest, WeakReference *src) {
  id object = (id) src->Value;
  if (isObjCTaggedPointerOrNull(object)) {
    dest->Value = (HeapObject*) object;
    return;
  }
  if (usesNativeSwiftReferenceCounting_allocated(object))
    return swift_weakTakeInit(dest, src);
  objc_moveWeak((id*) &dest->Value, (id*) &src->Value);
}
void swift::swift_unknownWeakCopyAssign(WeakReference *dest, WeakReference *src) {
  if (dest == src) return;
  swift_unknownWeakDestroy(dest);
  swift_unknownWeakCopyInit(dest, src);
}
void swift::swift_unknownWeakTakeAssign(WeakReference *dest, WeakReference *src) {
  if (dest == src) return;
  swift_unknownWeakDestroy(dest);
  swift_unknownWeakTakeInit(dest, src);
}
#endif

/*****************************************************************************/
/******************************* DYNAMIC CASTS *******************************/
/*****************************************************************************/
#if SWIFT_OBJC_INTEROP
const void *
swift::swift_dynamicCastObjCClass(const void *object,
                                  const ClassMetadata *targetType) {
  // FIXME: We need to decide if this is really how we want to treat 'nil'.
  if (object == nullptr)
    return nullptr;

  if ([(id)object isKindOfClass:(Class)targetType]) {
    return object;
  }

  return nullptr;
}

const void *
swift::swift_dynamicCastObjCClassUnconditional(const void *object,
                                             const ClassMetadata *targetType) {
  // FIXME: We need to decide if this is really how we want to treat 'nil'.
  if (object == nullptr)
    return nullptr;

  if ([(id)object isKindOfClass:(Class)targetType]) {
    return object;
  }

  Class sourceType = object_getClass((id)object);
  swift_dynamicCastFailure(reinterpret_cast<const Metadata *>(sourceType), 
                           targetType);
}

const void *
swift::swift_dynamicCastForeignClass(const void *object,
                                     const ForeignClassMetadata *targetType) {
  // FIXME: Actually compare CFTypeIDs, once they are available in the metadata.
  return object;
}

const void *
swift::swift_dynamicCastForeignClassUnconditional(
         const void *object,
         const ForeignClassMetadata *targetType) {
  // FIXME: Actual compare CFTypeIDs, once they are available in the metadata.
  return object;
}

extern "C" bool swift_objcRespondsToSelector(id object, SEL selector) {
  return [object respondsToSelector:selector];
}

extern "C" bool swift::_swift_objectConformsToObjCProtocol(const void *theObject,
                                           const ProtocolDescriptor *protocol) {
  return [((id) theObject) conformsToProtocol: (Protocol*) protocol];
}


extern "C" bool swift::_swift_classConformsToObjCProtocol(const void *theClass,
                                           const ProtocolDescriptor *protocol) {
  return [((Class) theClass) conformsToProtocol: (Protocol*) protocol];
}

extern "C" const Metadata *swift_dynamicCastTypeToObjCProtocolUnconditional(
                                                 const Metadata *type,
                                                 size_t numProtocols,
                                                 Protocol * const *protocols) {
  Class classObject;
  
  switch (type->getKind()) {
  case MetadataKind::Class:
    // Native class metadata is also the class object.
    classObject = (Class)type;
    break;
  case MetadataKind::ObjCClassWrapper:
    // Unwrap to get the class object.
    classObject = (Class)static_cast<const ObjCClassWrapperMetadata *>(type)
      ->Class;
    break;
  
  // Other kinds of type can never conform to ObjC protocols.
  case MetadataKind::Struct:
  case MetadataKind::Enum:
  case MetadataKind::Opaque:
  case MetadataKind::Tuple:
  case MetadataKind::Function:
  case MetadataKind::Existential:
  case MetadataKind::Metatype:
  case MetadataKind::ExistentialMetatype:
  case MetadataKind::ForeignClass:
    swift_dynamicCastFailure(type, nameForMetadata(type).c_str(),
                             protocols[0], protocol_getName(protocols[0]));
      
  case MetadataKind::HeapLocalVariable:
  case MetadataKind::HeapGenericLocalVariable:
  case MetadataKind::ErrorObject:
    assert(false && "not type metadata");
    break;
  }
  
  for (size_t i = 0; i < numProtocols; ++i) {
    if (![classObject conformsToProtocol:protocols[i]]) {
      swift_dynamicCastFailure(type, nameForMetadata(type).c_str(),
                               protocols[i], protocol_getName(protocols[i]));
    }
  }
  
  return type;
}

extern "C" const Metadata *swift_dynamicCastTypeToObjCProtocolConditional(
                                                const Metadata *type,
                                                size_t numProtocols,
                                                Protocol * const *protocols) {
  Class classObject;
  
  switch (type->getKind()) {
  case MetadataKind::Class:
    // Native class metadata is also the class object.
    classObject = (Class)type;
    break;
  case MetadataKind::ObjCClassWrapper:
    // Unwrap to get the class object.
    classObject = (Class)static_cast<const ObjCClassWrapperMetadata *>(type)
      ->Class;
    break;
  
  // Other kinds of type can never conform to ObjC protocols.
  case MetadataKind::Struct:
  case MetadataKind::Enum:
  case MetadataKind::Opaque:
  case MetadataKind::Tuple:
  case MetadataKind::Function:
  case MetadataKind::Existential:
  case MetadataKind::Metatype:
  case MetadataKind::ExistentialMetatype:
  case MetadataKind::ForeignClass:
    return nullptr;
      
  case MetadataKind::HeapLocalVariable:
  case MetadataKind::HeapGenericLocalVariable:
  case MetadataKind::ErrorObject:
    assert(false && "not type metadata");
    break;
  }
  
  for (size_t i = 0; i < numProtocols; ++i) {
    if (![classObject conformsToProtocol:protocols[i]]) {
      return nullptr;
    }
  }
  
  return type;
}

extern "C" id swift_dynamicCastObjCProtocolUnconditional(id object,
                                                 size_t numProtocols,
                                                 Protocol * const *protocols) {
  for (size_t i = 0; i < numProtocols; ++i) {
    if (![object conformsToProtocol:protocols[i]]) {
      Class sourceType = object_getClass(object);
      swift_dynamicCastFailure(sourceType, class_getName(sourceType), 
                               protocols[i], protocol_getName(protocols[i]));
    }
  }
  
  return object;
}

extern "C" id swift_dynamicCastObjCProtocolConditional(id object,
                                                 size_t numProtocols,
                                                 Protocol * const *protocols) {
  for (size_t i = 0; i < numProtocols; ++i) {
    if (![object conformsToProtocol:protocols[i]]) {
      return nil;
    }
  }
  
  return object;
}

extern "C" void swift::swift_instantiateObjCClass(const ClassMetadata *_c) {
  static const objc_image_info ImageInfo = {0, 0};

  // Ensure the superclass is realized.
  Class c = (Class) _c;
  [class_getSuperclass(c) class];

  // Register the class.
  Class registered = objc_readClassPair(c, &ImageInfo);
  assert(registered == c
         && "objc_readClassPair failed to instantiate the class in-place");
  (void)registered;
}

extern "C" Class swift_getInitializedObjCClass(Class c) {
  // Used when we have class metadata and we want to ensure a class has been
  // initialized by the Objective C runtime. We need to do this because the
  // class "c" might be valid metadata, but it hasn't been initialized yet.
  return [c class];
}

const ClassMetadata *
swift::swift_dynamicCastObjCClassMetatype(const ClassMetadata *source,
                                          const ClassMetadata *dest) {
  if ([(Class)source isSubclassOfClass:(Class)dest])
    return source;
  return nil;
}

const ClassMetadata *
swift::swift_dynamicCastObjCClassMetatypeUnconditional(
                                                   const ClassMetadata *source,
                                                   const ClassMetadata *dest) {
  if ([(Class)source isSubclassOfClass:(Class)dest])
    return source;

  swift_dynamicCastFailure(source, dest);
}

static Demangle::NodePointer _buildDemanglingForMetadata(const Metadata *type);

// Build a demangled type tree for a nominal type.
static Demangle::NodePointer
_buildDemanglingForNominalType(Demangle::Node::Kind boundGenericKind,
                               const Metadata *type,
                               const NominalTypeDescriptor *description) {
  using namespace Demangle;
  
  // Demangle the base name.
  auto node = demangleTypeAsNode(description->Name,
                                     strlen(description->Name));
  // If generic, demangle the type parameters.
  if (description->GenericParams.NumPrimaryParams > 0) {
    auto typeParams = NodeFactory::create(Node::Kind::TypeList);
    auto typeBytes = reinterpret_cast<const char *>(type);
    auto genericParam = reinterpret_cast<const Metadata * const *>(
                 typeBytes + sizeof(void*) * description->GenericParams.Offset);
    for (unsigned i = 0, e = description->GenericParams.NumPrimaryParams;
         i < e; ++i, ++genericParam) {
      typeParams->addChild(_buildDemanglingForMetadata(*genericParam));
    }

    auto genericNode = NodeFactory::create(boundGenericKind);
    genericNode->addChild(node);
    genericNode->addChild(typeParams);
    return genericNode;
  }
  return node;
}

// Build a demangled type tree for a type.
static Demangle::NodePointer _buildDemanglingForMetadata(const Metadata *type) {
  using namespace Demangle;

  switch (type->getKind()) {
  case MetadataKind::Class: {
    auto classType = static_cast<const ClassMetadata *>(type);
    return _buildDemanglingForNominalType(Node::Kind::BoundGenericClass,
                                          type, classType->getDescription());
  }
  case MetadataKind::Enum: {
    auto structType = static_cast<const EnumMetadata *>(type);
    return _buildDemanglingForNominalType(Node::Kind::BoundGenericEnum,
                                          type, structType->Description);
  }
  case MetadataKind::Struct: {
    auto structType = static_cast<const StructMetadata *>(type);
    return _buildDemanglingForNominalType(Node::Kind::BoundGenericStructure,
                                          type, structType->Description);
  }
  case MetadataKind::ObjCClassWrapper: {
#if SWIFT_OBJC_INTEROP
    auto objcWrapper = static_cast<const ObjCClassWrapperMetadata *>(type);
    const char *className = class_getName((Class)objcWrapper->Class);
    
    // ObjC classes mangle as being in the magic "__ObjC" module.
    auto module = NodeFactory::create(Node::Kind::Module, "__ObjC");
    
    auto node = NodeFactory::create(Node::Kind::Class);
    node->addChild(module);
    node->addChild(NodeFactory::create(Node::Kind::Identifier,
                                       llvm::StringRef(className)));
    
    return node;
#else
    assert(false && "no ObjC interop");
    return nullptr;
#endif
  }
  case MetadataKind::ForeignClass: {
    auto foreign = static_cast<const ForeignClassMetadata *>(type);
    return Demangle::demangleTypeAsNode(foreign->getName(),
                                        strlen(foreign->getName()));
  }
  case MetadataKind::Existential: {
    auto exis = static_cast<const ExistentialTypeMetadata *>(type);
    NodePointer proto_list = NodeFactory::create(Node::Kind::ProtocolList);
    NodePointer type_list = NodeFactory::create(Node::Kind::TypeList);

    proto_list->addChild(type_list);
    
    std::vector<const ProtocolDescriptor *> protocols;
    protocols.reserve(exis->Protocols.NumProtocols);
    for (unsigned i = 0, e = exis->Protocols.NumProtocols; i < e; ++i)
      protocols.push_back(exis->Protocols[i]);
    
    // Sort the protocols by their mangled names.
    // The ordering in the existential type metadata is by metadata pointer,
    // which isn't necessarily stable across invocations.
    std::sort(protocols.begin(), protocols.end(),
          [](const ProtocolDescriptor *a, const ProtocolDescriptor *b) -> bool {
            return strcmp(a->Name, b->Name) < 0;
          });
    
    for (auto *protocol : protocols) {
      // The protocol name is mangled as a type symbol, with the _Tt prefix.
      auto protocolNode = demangleSymbolAsNode(protocol->Name,
                                               strlen(protocol->Name));
      
      // ObjC protocol names aren't mangled.
      if (!protocolNode) {
        auto module = NodeFactory::create(Node::Kind::Module,
                                          MANGLING_MODULE_OBJC);
        auto node = NodeFactory::create(Node::Kind::Protocol);
        node->addChild(module);
        node->addChild(NodeFactory::create(Node::Kind::Identifier,
                                           llvm::StringRef(protocol->Name)));
        auto typeNode = NodeFactory::create(Node::Kind::Type);
        typeNode->addChild(node);
        type_list->addChild(typeNode);
        continue;
      }

      // FIXME: We have to dig through a ridiculous number of nodes to get
      // to the Protocol node here.
      protocolNode = protocolNode->getChild(0); // Global -> TypeMangling
      protocolNode = protocolNode->getChild(0); // TypeMangling -> Type
      protocolNode = protocolNode->getChild(0); // Type -> ProtocolList
      protocolNode = protocolNode->getChild(0); // ProtocolList -> TypeList
      protocolNode = protocolNode->getChild(0); // TypeList -> Type
      
      assert(protocolNode->getKind() == Node::Kind::Type);
      assert(protocolNode->getChild(0)->getKind() == Node::Kind::Protocol);
      type_list->addChild(protocolNode);
    }
    
    return proto_list;
  }
  case MetadataKind::ExistentialMetatype: {
    auto metatype = static_cast<const ExistentialMetatypeMetadata *>(type);
    auto instance = _buildDemanglingForMetadata(metatype->InstanceType);
    auto node = NodeFactory::create(Node::Kind::ExistentialMetatype);
    node->addChild(instance);
    return node;
  }
  case MetadataKind::Function: {
    auto func = static_cast<const FunctionTypeMetadata *>(type);

    Node::Kind kind;
    switch (func->getConvention()) {
    case FunctionMetadataConvention::Swift:
      kind = Node::Kind::FunctionType;
      break;
    case FunctionMetadataConvention::Block:
      kind = Node::Kind::ObjCBlock;
      break;
    case FunctionMetadataConvention::CFunctionPointer:
      kind = Node::Kind::CFunctionPointer;
      break;
    case FunctionMetadataConvention::Thin:
      kind = Node::Kind::ThinFunctionType;
      break;
    }
    
    std::vector<NodePointer> inputs;
    for (unsigned i = 0, e = func->getNumArguments(); i < e; ++i) {
      auto arg = func->getArguments()[i];
      auto input = _buildDemanglingForMetadata(arg.getPointer());
      if (arg.getFlag()) {
        NodePointer inout = NodeFactory::create(Node::Kind::InOut);
        inout->addChild(input);
        input = inout;
      }
      inputs.push_back(input);
    }

    NodePointer totalInput;
    if (inputs.size() > 1) {
      auto tuple = NodeFactory::create(Node::Kind::NonVariadicTuple);
      for (auto &input : inputs)
        tuple->addChild(input);
      totalInput = tuple;
    } else {
      totalInput = inputs.front();
    }
    
    NodePointer args = NodeFactory::create(Node::Kind::ArgumentTuple);
    args->addChild(totalInput);
    
    NodePointer resultTy = _buildDemanglingForMetadata(func->ResultType);
    NodePointer result = NodeFactory::create(Node::Kind::ReturnType);
    result->addChild(resultTy);
    
    auto funcNode = NodeFactory::create(kind);
    if (func->throws())
      funcNode->addChild(NodeFactory::create(Node::Kind::ThrowsAnnotation));
    funcNode->addChild(args);
    funcNode->addChild(result);
    return funcNode;
  }
  case MetadataKind::Metatype: {
    auto metatype = static_cast<const MetatypeMetadata *>(type);
    auto instance = _buildDemanglingForMetadata(metatype->InstanceType);
    auto node = NodeFactory::create(Node::Kind::Metatype);
    node->addChild(instance);
    return node;
  }
  case MetadataKind::Tuple: {
    auto tuple = static_cast<const TupleTypeMetadata *>(type);
    auto tupleNode = NodeFactory::create(Node::Kind::NonVariadicTuple);
    for (unsigned i = 0, e = tuple->NumElements; i < e; ++i) {
      auto elt = _buildDemanglingForMetadata(tuple->getElement(i).Type);
      tupleNode->addChild(elt);
    }
    return tupleNode;
  }
  case MetadataKind::Opaque:
    // FIXME: Some opaque types do have manglings, but we don't have enough info
    // to figure them out.
  case MetadataKind::HeapLocalVariable:
  case MetadataKind::HeapGenericLocalVariable:
  case MetadataKind::ErrorObject:
    break;
  }
  // Not a type.
  return nullptr;
}

extern "C" const char *
swift_getGenericClassObjCName(const ClassMetadata *clas) {
  // Use the remangler to generate a mangled name from the type metadata.
  auto demangling = _buildDemanglingForMetadata(clas);

  // Remangle that into a new type mangling string.
  auto typeNode
    = Demangle::NodeFactory::create(Demangle::Node::Kind::TypeMangling);
  typeNode->addChild(demangling);
  auto globalNode
    = Demangle::NodeFactory::create(Demangle::Node::Kind::Global);
  globalNode->addChild(typeNode);
  
  auto string = Demangle::mangleNode(globalNode);
  
  auto fullNameBuf = (char*)swift_slowAlloc(string.size() + 1, 0);
  memcpy(fullNameBuf, string.c_str(), string.size() + 1);
  return fullNameBuf;
}
#endif

const ClassMetadata *
swift::swift_dynamicCastForeignClassMetatype(const ClassMetadata *sourceType,
                                             const ClassMetadata *targetType) {
  // FIXME: Actually compare CFTypeIDs, once they are available in
  // the metadata.
  return sourceType;
}

const ClassMetadata *
swift::swift_dynamicCastForeignClassMetatypeUnconditional(
  const ClassMetadata *sourceType,
  const ClassMetadata *targetType) 
{
  // FIXME: Actually compare CFTypeIDs, once they arae available in
  // the metadata.
  return sourceType;
}

// Given a non-nil object reference, return true iff the object uses
// native swift reference counting.
bool swift::_swift_usesNativeSwiftReferenceCounting_nonNull(
  const void* object
) {
    assert(object != nullptr);
#if SWIFT_OBJC_INTEROP
    return !isObjCTaggedPointer(object) &&
      usesNativeSwiftReferenceCounting_allocated(object);
#else 
    return true;
#endif 
}

bool swift::swift_isUniquelyReferenced_nonNull_native(
  const HeapObject* object
) {
  assert(object != nullptr);
  assert(!object->refCount.isDeallocating());
  SWIFT_ISUNIQUELYREFERENCED();
  return object->refCount.isUniquelyReferenced();
}

bool swift::swift_isUniquelyReferenced_native(const HeapObject* object) {
  return object != nullptr
    && swift::swift_isUniquelyReferenced_nonNull_native(object);
}

bool swift::swift_isUniquelyReferencedNonObjC_nonNull(const void* object) {
  assert(object != nullptr);
  return
#if SWIFT_OBJC_INTEROP
    _swift_usesNativeSwiftReferenceCounting_nonNull(object) &&
#endif 
    swift_isUniquelyReferenced_nonNull_native((HeapObject*)object);
}

// Given an object reference, return true iff it is non-nil and refers
// to a native swift object with strong reference count of 1.
bool swift::swift_isUniquelyReferencedNonObjC(
  const void* object
) {
  return object != nullptr
    && swift_isUniquelyReferencedNonObjC_nonNull(object);
}

/// Return true if the given bits of a Builtin.BridgeObject refer to a
/// native swift object whose strong reference count is 1.
bool swift::swift_isUniquelyReferencedNonObjC_nonNull_bridgeObject(
  uintptr_t bits
) {
  auto bridgeObject = (void*)bits;
  
  if (isObjCTaggedPointer(bridgeObject))
    return false;

  const auto object = toPlainObject_unTagged_bridgeObject(bridgeObject);

  // Note: we could just return false if all spare bits are set,
  // but in that case the cost of a deeper check for a unique native
  // object is going to be a negligible cost for a possible big win.
#if SWIFT_OBJC_INTEROP
  return !isNonNative_unTagged_bridgeObject(bridgeObject)
    ? swift_isUniquelyReferenced_nonNull_native((const HeapObject *)object)
    : swift_isUniquelyReferencedNonObjC_nonNull(object);
#else
  return swift_isUniquelyReferenced_nonNull_native((const HeapObject *)object);
#endif
}

/// Return true if the given bits of a Builtin.BridgeObject refer to a
/// native swift object whose strong reference count is 1.
bool swift::swift_isUniquelyReferencedOrPinnedNonObjC_nonNull_bridgeObject(
  uintptr_t bits
) {
  auto bridgeObject = (void*)bits;

  if (isObjCTaggedPointer(bridgeObject))
    return false;

  const auto object = toPlainObject_unTagged_bridgeObject(bridgeObject);

  // Note: we could just return false if all spare bits are set,
  // but in that case the cost of a deeper check for a unique native
  // object is going to be a negligible cost for a possible big win.
#if SWIFT_OBJC_INTEROP
  if (isNonNative_unTagged_bridgeObject(bridgeObject))
    return swift_isUniquelyReferencedOrPinnedNonObjC_nonNull(object);
#endif
  return swift_isUniquelyReferencedOrPinned_nonNull_native(
                                                   (const HeapObject *)object);
}


/// Given a non-nil object reference, return true if the object is a
/// native swift object and either its strong reference count is 1 or
/// its pinned flag is set.
bool swift::swift_isUniquelyReferencedOrPinnedNonObjC_nonNull(
                                                          const void *object) {
  assert(object != nullptr);
  return
#if SWIFT_OBJC_INTEROP
    _swift_usesNativeSwiftReferenceCounting_nonNull(object) &&
#endif 
    swift_isUniquelyReferencedOrPinned_nonNull_native(
                                                    (const HeapObject*)object);
}

// Given a non-@objc object reference, return true iff the
// object is non-nil and either has a strong reference count of 1
// or is pinned.
bool swift::swift_isUniquelyReferencedOrPinned_native(
  const HeapObject* object
) {
  return object != nullptr
    && swift_isUniquelyReferencedOrPinned_nonNull_native(object);
}

/// Given a non-nil native swift object reference, return true if
/// either the object has a strong reference count of 1 or its
/// pinned flag is set.
bool swift::swift_isUniquelyReferencedOrPinned_nonNull_native(
                                                    const HeapObject* object) {
  SWIFT_ISUNIQUELYREFERENCEDORPINNED();
  assert(object != nullptr);
  assert(!object->refCount.isDeallocating());
  return object->refCount.isUniquelyReferencedOrPinned();
}

#if SWIFT_OBJC_INTEROP
/// Returns class_getInstanceSize(c)
///
/// That function is otherwise unavailable to the core stdlib.
size_t swift::_swift_class_getInstancePositiveExtentSize(const void* c) {
  return class_getInstanceSize((Class)c);
}
#endif

extern "C" size_t _swift_class_getInstancePositiveExtentSize_native(
    const Metadata *c) {
  assert(c && c->isClassObject());
  auto metaData = c->getClassObject();
  return metaData->getInstanceSize() - metaData->getInstanceAddressPoint();
}

const ClassMetadata *swift::getRootSuperclass() {
#if SWIFT_OBJC_INTEROP
  return (const ClassMetadata *)[SwiftObject class];
#else
  return nullptr;
#endif
}
