// RUN: %target-swift-ide-test -dump-importer-lookup-table -source-filename %s -import-objc-header %S/Inputs/swift_name.h > %t.log 2>&1
// RUN: FileCheck %s < %t.log

// REQUIRES: objc_interop

// CHECK:      Base -> full name mappings:
// CHECK-NEXT:   Bar --> Bar
// CHECK-NEXT:   Blue --> Blue
// CHECK-NEXT:   Green --> Green
// CHECK-NEXT:   MyInt --> MyInt
// CHECK-NEXT:   Point --> Point
// CHECK-NEXT:   Rouge --> Rouge
// CHECK-NEXT:   SNColorChoice --> SNColorChoice
// CHECK-NEXT:   SomeStruct --> SomeStruct
// CHECK-NEXT:   __SNTransposeInPlace --> __SNTransposeInPlace
// CHECK-NEXT:   makeSomeStruct --> makeSomeStruct(x:y:), makeSomeStruct(x:)
// CHECK-NEXT:   x --> x
// CHECK-NEXT:   y --> y
// CHECK-NEXT:   z --> z

// CHECK:      Full name -> entry mappings:
// CHECK-NEXT:   Bar:
// CHECK-NEXT:     TU: SNFoo
// CHECK-NEXT:   Blue:
// CHECK-NEXT:     SNColorChoice: SNColorBlue
// CHECK-NEXT:   Green:
// CHECK-NEXT:     SNColorChoice: SNColorGreen
// CHECK-NEXT:   MyInt:
// CHECK-NEXT:     TU: SNIntegerType
// CHECK-NEXT:   Point:
// CHECK-NEXT:     TU: SNPoint
// CHECK-NEXT:   Rouge:
// CHECK-NEXT:     SNColorChoice: SNColorRed
// CHECK-NEXT:   SNColorChoice:
// CHECK-NEXT:     TU: SNColorChoice, SNColorChoice
// CHECK-NEXT:   SomeStruct:
// CHECK-NEXT:     TU: SNSomeStruct
// CHECK-NEXT:   __SNTransposeInPlace:
// CHECK-NEXT:     TU: SNTransposeInPlace
// CHECK-NEXT:   makeSomeStruct(x:):
// CHECK-NEXT:     TU: SNMakeSomeStructForX
// CHECK-NEXT:   makeSomeStruct(x:y:):
// CHECK-NEXT:     TU: SNMakeSomeStruct
// CHECK-NEXT:   x:
// CHECK-NEXT:     SNSomeStruct: X
// CHECK-NEXT:     SNPoint: x
// CHECK-NEXT:   y:
// CHECK-NEXT:     SNPoint: y
// CHECK-NEXT:   z:
// CHECK-NEXT:     SNPoint: z
