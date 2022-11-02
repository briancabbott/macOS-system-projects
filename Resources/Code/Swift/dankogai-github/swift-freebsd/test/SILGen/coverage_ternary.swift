// RUN: %target-swift-frontend -profile-generate -profile-coverage-mapping -emit-sorted-sil -emit-sil -module-name coverage_ternary %s | FileCheck %s

// CHECK-LABEL: sil_coverage_map {{.*}}// coverage_ternary.foo
func foo(x : Int32) -> Int32 {
  return x == 3
             ? 9000 // CHECK: [[@LINE]]:16 -> [[@LINE]]:20 : 1
             : 1234 // CHECK: [[@LINE]]:16 -> [[@LINE]]:20 : (0 - 1)
}

foo(1)
foo(2)
foo(3)
