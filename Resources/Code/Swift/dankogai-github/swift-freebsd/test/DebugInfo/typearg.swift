// RUN: %target-swift-frontend %s -emit-ir -g -o - | FileCheck %s

protocol AProtocol {
  func f() -> String;
}
class AClass : AProtocol {
  func f() -> String { return "A" }
}

// CHECK: define hidden void @{{.*}}aFunction
// CHECK:  call void @llvm.dbg.declare(metadata %swift.type** %{{.*}}, metadata ![[TYPEARG:.*]], metadata !{{[0-9]+}}),
// CHECK: ![[VOIDPTR:[0-9]+]] = !DIDerivedType(tag: DW_TAG_pointer_type, name: "_TtBp", baseType: null
// CHECK: ![[TYPEARG]] = !DILocalVariable(name: "$swift.type.T"
// CHECK-SAME:                            type: ![[SWIFTMETATYPE:[^,)]+]]
// CHECK-SAME:                            flags: DIFlagArtificial
// CHECK: ![[SWIFTMETATYPE]] = !DIDerivedType(tag: DW_TAG_typedef, name: "$swift.type",
// CHECK-SAME:                                baseType: ![[VOIDPTR]]
func aFunction<T : AProtocol>(x: T) {
    print("I am in aFunction: \(x.f())")
}

aFunction(AClass())

// Verify that we also emit a swift.type for a generic self.
class Foo<Bar> {
      func one() {
      }

      func two<Baz>(x: Baz) {
    // CHECK: !DILocalVariable(name: "$swift.type.Bar"
    // CHECK: !DILocalVariable(name: "$swift.type.Baz"
      }
}

// Verify that the backend doesn't elide the debug intrinsics.
// RUN: %target-swift-frontend %s -c -g -o %t.o
// RUN: llvm-dwarfdump %t.o | FileCheck %s --check-prefix=CHECK-LLVM
// CHECK-LLVM-DAG:  .debug_str[{{.*}}] = "x"
// CHECK-LLVM-DAG:  .debug_str[{{.*}}] = "$swift.type.T"
// CHECK-LLVM-DAG:  .debug_str[{{.*}}] = "$swift.type.Bar"
// CHECK-LLVM-DAG:  .debug_str[{{.*}}] = "$swift.type.Baz"
