// RUN: %empty-directory(%t)
// RUN: split-file --leading-lines %s %t
// RUN: %swift-target-frontend -parse-as-library -static -O -module-name M -c -primary-file %t/A.swift %t/B.swift -S -emit-ir -o - | %FileCheck %t/A.swift -check-prefix CHECK
// RUN: %swift-target-frontend -parse-as-library -static -O -module-name M -c %t/A.swift -primary-file %t/B.swift -S -emit-ir -o - | %FileCheck %t/B.swift -check-prefix CHECK

//--- A.swift
open class C {
    private var i: [ObjectIdentifier:Any] = [:]
}

// CHECK: @"$s1M1CC1i33_807E3D81CC6CDD898084F3279464DDF9LLSDySOypGvg" = hidden alias void (), void ()* @_swift_dead_method_stub
// CHECK: @"$s1M1CC1i33_807E3D81CC6CDD898084F3279464DDF9LLSDySOypGvs" = hidden alias void (), void ()* @_swift_dead_method_stub
// CHECK: @"$s1M1CC1i33_807E3D81CC6CDD898084F3279464DDF9LLSDySOypGvM" = hidden alias void (), void ()* @_swift_dead_method_stub

//--- B.swift
final class D: C {
}

// CHECK: declare swiftcc %swift.bridge* @"$s1M1CC1i33_807E3D81CC6CDD898084F3279464DDF9LLSDySOypGvg"(%T1M1CC* swiftself) #0
// CHECK: declare swiftcc void @"$s1M1CC1i33_807E3D81CC6CDD898084F3279464DDF9LLSDySOypGvs"(%swift.bridge*, %T1M1CC* swiftself) #0
// CHECK: declare swiftcc { i8*, %TSD* } @"$s1M1CC1i33_807E3D81CC6CDD898084F3279464DDF9LLSDySOypGvM"(i8* noalias dereferenceable(32), %T1M1CC* swiftself) #0
