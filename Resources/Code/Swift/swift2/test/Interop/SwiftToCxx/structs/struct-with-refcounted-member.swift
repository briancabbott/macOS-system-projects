// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend %s -typecheck -module-name Structs -clang-header-expose-decls=all-public -emit-clang-header-path %t/structs.h
// RUN: %FileCheck %s < %t/structs.h

// RUN: %check-interop-cxx-header-in-clang(%t/structs.h -Wno-unused-function)

class RefCountedClass {
    init() {
        print("create RefCountedClass")
    }
    deinit {
        print("destroy RefCountedClass")
    }
}

public struct StructWithRefcountedMember {
    let x: RefCountedClass
}

public func returnNewStructWithRefcountedMember() -> StructWithRefcountedMember {
    return StructWithRefcountedMember(x: RefCountedClass())
}

public func printBreak(_ x: Int) {
    print("breakpoint \(x)")
}

// CHECK:      class StructWithRefcountedMember final {
// CHECK-NEXT: public:
// CHECK-NEXT:   inline ~StructWithRefcountedMember() {
// CHECK-NEXT:     auto metadata = _impl::$s7Structs26StructWithRefcountedMemberVMa(0);
// CHECK-NEXT:     auto *vwTableAddr = reinterpret_cast<swift::_impl::ValueWitnessTable **>(metadata._0) - 1;
// CHECK-NEXT: #ifdef __arm64e__
// CHECK-NEXT:   auto *vwTable = reinterpret_cast<swift::_impl::ValueWitnessTable *>(ptrauth_auth_data(reinterpret_cast<void *>(*vwTableAddr), ptrauth_key_process_independent_data, ptrauth_blend_discriminator(vwTableAddr, 11839)));
// CHECK-NEXT: #else
// CHECK-NEXT:   auto *vwTable = *vwTableAddr;
// CHECK-NEXT: #endif
// CHECK-NEXT:     vwTable->destroy(_getOpaquePointer(), metadata._0);
// CHECK-NEXT:   }
// CHECK-NEXT:   inline StructWithRefcountedMember(const StructWithRefcountedMember &other) {
// CHECK-NEXT:     auto metadata = _impl::$s7Structs26StructWithRefcountedMemberVMa(0);
// CHECK-NEXT:     auto *vwTableAddr = reinterpret_cast<swift::_impl::ValueWitnessTable **>(metadata._0) - 1;
// CHECK-NEXT: #ifdef __arm64e__
// CHECK-NEXT:   auto *vwTable = reinterpret_cast<swift::_impl::ValueWitnessTable *>(ptrauth_auth_data(reinterpret_cast<void *>(*vwTableAddr), ptrauth_key_process_independent_data, ptrauth_blend_discriminator(vwTableAddr, 11839)));
// CHECK-NEXT: #else
// CHECK-NEXT:   auto *vwTable = *vwTableAddr;
// CHECK-NEXT: #endif
// CHECK-NEXT:     vwTable->initializeWithCopy(_getOpaquePointer(), const_cast<char *>(other._getOpaquePointer()), metadata._0);
// CHECK-NEXT:   }
// CHECK-NEXT:   inline StructWithRefcountedMember(StructWithRefcountedMember &&) { abort(); }
// CHECK-NEXT: private:
