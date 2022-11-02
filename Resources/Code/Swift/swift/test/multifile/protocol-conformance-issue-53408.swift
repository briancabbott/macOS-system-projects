// RUN: %target-swift-frontend -emit-sil -module-name main -primary-file %s %S/Inputs/protocol-conformance-issue-53408-other.swift -verify
// RUN: %target-swift-frontend -emit-sil -module-name main %s -primary-file %S/Inputs/protocol-conformance-issue-53408-other.swift

// https://github.com/apple/swift/issues/53408

func reproducer() -> Float { return Struct().func1(1.0) }
// expected-error@-1 {{cannot convert value of type 'Double' to expected argument type 'Struct.Input'}}
