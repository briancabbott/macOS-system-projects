// RUN: %empty-directory(%t)
// RUN: %target-swift-frontend -emit-module -emit-module-path %t/StrictModule.swiftmodule -module-name StrictModule -swift-version 6 %S/Inputs/StrictModule.swift
// RUN: %target-swift-frontend -emit-module -emit-module-path %t/NonStrictModule.swiftmodule -module-name NonStrictModule %S/Inputs/NonStrictModule.swift
// RUN: %target-swift-frontend -emit-module -emit-module-path %t/OtherActors.swiftmodule -module-name OtherActors %S/Inputs/OtherActors.swift -disable-availability-checking

// RUN: %target-swift-frontend -emit-module -I %t -verify -primary-file %s -emit-module-path %t/predates_concurrency_import_swiftmodule.swiftmodule -experimental-skip-all-function-bodies

// REQUIRES: concurrency
// REQUIRES: asserts

@preconcurrency import NonStrictModule
@preconcurrency import StrictModule
@preconcurrency import OtherActors

// expected-no-warning

func acceptSendable<T: Sendable>(_: T) { }

@available(SwiftStdlib 5.1, *)
func test(
  ss: StrictStruct, ns: NonStrictClass, oma: OtherModuleActor,
  ssc: SomeSendableClass
) async {
  acceptSendable(ss)
  acceptSendable(ns)
  acceptSendable(oma)
  acceptSendable(ssc)
}
