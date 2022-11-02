// RUN: %target-swift-frontend -emit-sil -module-name main -primary-file %s %S/Inputs/protocol-conformance-issue-49388-other.swift | %FileCheck -check-prefix CHECK-FIRST %s
// RUN: %target-swift-frontend -emit-sil -module-name main %s -primary-file %S/Inputs/protocol-conformance-issue-49388-other.swift | %FileCheck -check-prefix CHECK-SECOND %s

// https://github.com/apple/swift/issues/49388

// We need to consistently pick where the witness table for _PositionRelation :
// RawRepresentable goes.
// CHECK-FIRST: sil_witness_table hidden _PositionRelation: RawRepresentable
// CHECK-SECOND-NOT: sil_witness_table hidden _PositionRelation: RawRepresentable
enum _PositionRelation: Int {
  case before = 0
}
