// RUN: %target-swift-frontend -primary-file %s -emit-ir -g -o - | FileCheck %s
// Capture the pointer size from type Int
// CHECK: %Si = type <{ i[[PTRSIZE:[0-9]+]] }>

struct A {
  var fn : () -> ()
}

func test(x : A) {
  var vx = x
}
// CHECK:    define hidden void @_TF7structs4test
// CHECK: [[VX:%.*]] = alloca
// CHECK: [[X_DBG:%.*]] = alloca
// CHECK: call void @llvm.dbg.declare(metadata {{.*}}* [[X_DBG]], metadata [[X_MD:!.*]], metadata
// CHECK: !DICompositeType(tag: DW_TAG_structure_type, name: "A"
// CHECK-SAME:             identifier: [[A_DI:"[^"]+"]]


class C {
  var lots_of_extra_storage: (Int, Int, Int) = (1, 2, 3)
  var member: C = C()
}

// A class is represented by a pointer, so B's total size should be PTRSIZE.
// CHECK: !DICompositeType(tag: DW_TAG_structure_type, name: "B",
// CHECK-SAME:             {{.*}}size: [[PTRSIZE]]
struct B {
  var c : C
}

// CHECK: [[X_MD]] = !DILocalVariable(name: "x", arg: 1
// CHECK-SAME:                         type: ![[A_DI]]
