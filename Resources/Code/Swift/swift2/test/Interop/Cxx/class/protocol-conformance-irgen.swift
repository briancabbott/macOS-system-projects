// RUN: %target-swift-emit-ir -I %S/Inputs -enable-experimental-cxx-interop %s | %FileCheck %s

import ProtocolConformance

protocol HasReturn42 {
  mutating func return42() -> CInt
}


// CHECK: define {{.*}}i32 @"$sSo18ConformsToProtocolV4main11HasReturn42A2cDP8return42s5Int32VyFTW"(%TSo18ConformsToProtocolV* nocapture swiftself dereferenceable(1) %{{.*}}, %swift.type* %{{.*}}, i8** %{{.*}})
// CHECK: [[OUT:%.*]] = call i32 @{{_ZN18ConformsToProtocol8return42Ev|"\?return42@ConformsToProtocol@@QEAAHXZ"}}(%struct.ConformsToProtocol*
// CHECK: ret i32 [[OUT]]

// CHECK: define {{.*}}%swift.metadata_response @"$sSo18ConformsToProtocolVMa"({{i64|i32}} [[ARG:%.*]])
// CHECK: load %swift.type*, %swift.type** @"$sSo18ConformsToProtocolVML"
// CHECK: call swiftcc %swift.metadata_response @swift_getForeignTypeMetadata({{i64|i32}} [[ARG]]
// CHECK: ret %swift.metadata_response

extension ConformsToProtocol : HasReturn42 {}
