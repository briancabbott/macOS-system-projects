// RUN: %target-swift-frontend -Xcc -Xclang -Xcc -opaque-pointers -primary-file %s -emit-ir   | %FileCheck %s --check-prefix=CHECK
// RUN: %target-swift-frontend -Xcc -Xclang -Xcc -no-opaque-pointers -primary-file %s -emit-ir   | %FileCheck %s --check-prefix=CHECK-NO

// CHECK: define{{.*}} @main({{.*}} %0, ptr %1)
// CHECK-NO: define{{.*}} @main({{.*}} %0, i8** %1)
public func test() {}
