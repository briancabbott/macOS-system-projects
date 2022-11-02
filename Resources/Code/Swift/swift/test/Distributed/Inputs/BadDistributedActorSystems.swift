//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift open source project
//
// Copyright (c) 2021 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0
//
// See LICENSE.txt for license information
// See CONTRIBUTORS.txt for the list of Swift project authors
//
// SPDX-License-Identifier: Apache-2.0
//
//===----------------------------------------------------------------------===//

import Distributed

// ==== Fake Address -----------------------------------------------------------

public struct ActorAddress: Hashable, Sendable, Codable {
  public let address: String

  public init(parse address: String) {
    self.address = address
  }

  public init(from decoder: Decoder) throws {
    let container = try decoder.singleValueContainer()
    self.address = try container.decode(String.self)
  }

  public func encode(to encoder: Encoder) throws {
    var container = encoder.singleValueContainer()
    try container.encode(self.address)
  }
}

// ==== Fake Systems -----------------------------------------------------------

public struct MissingRemoteCallVoidActorSystem: DistributedActorSystem {
  public typealias ActorID = ActorAddress
  public typealias InvocationDecoder = FakeInvocationDecoder
  public typealias InvocationEncoder = FakeInvocationEncoder
  public typealias SerializationRequirement = Codable

  // just so that the struct does not become "trivial"
  let someValue: String = ""
  let someValue2: String = ""
  let someValue3: String = ""
  let someValue4: String = ""

  init() {
    print("Initialized new FakeActorSystem")
  }

  public func resolve<Act>(id: ActorID, as actorType: Act.Type) throws -> Act?
      where Act: DistributedActor,
      Act.ID == ActorID  {
    nil
  }

  public func assignID<Act>(_ actorType: Act.Type) -> ActorID
      where Act: DistributedActor,
      Act.ID == ActorID {
    ActorAddress(parse: "xxx")
  }

  public func actorReady<Act>(_ actor: Act)
      where Act: DistributedActor,
            Act.ID == ActorID {
  }

  public func resignID(_ id: ActorID) {
  }

  public func makeInvocationEncoder() -> InvocationEncoder {
    .init()
  }

  public func remoteCall<Act, Err, Res>(
      on actor: Act,
      target: RemoteCallTarget,
      invocation invocationEncoder: inout InvocationEncoder,
      throwing: Err.Type,
      returning: Res.Type
  ) async throws -> Res
    where Act: DistributedActor,
          Act.ID == ActorID,
          Err: Error,
          Res: SerializationRequirement {
    throw ExecuteDistributedTargetError(message: "\(#function) not implemented.")
  }

//  func remoteCallVoid<Act, Err>(
//    on actor: Act,
//    target: RemoteCallTarget,
//    invocation invocationEncoder: inout InvocationEncoder,
//    throwing: Err.Type
//  ) async throws
//    where Act: DistributedActor,
//          Act.ID == ActorID,
//          Err: Error {
//    throw ExecuteDistributedTargetError(message: "\(#function) not implemented.")
//  }
}

// ==== Fake Roundtrip Transport -----------------------------------------------

// TODO(distributed): not thread safe...
public final class FakeRoundtripActorSystem: DistributedActorSystem, @unchecked Sendable {
  public typealias ActorID = ActorAddress
  public typealias InvocationEncoder = FakeInvocationEncoder
  public typealias InvocationDecoder = FakeInvocationDecoder
  public typealias SerializationRequirement = Codable

  var activeActors: [ActorID: any DistributedActor] = [:]

  public init() {}

  public func resolve<Act>(id: ActorID, as actorType: Act.Type)
    throws -> Act? where Act: DistributedActor {
    print("| resolve \(id) as remote // this system always resolves as remote")
    return nil
  }

  public func assignID<Act>(_ actorType: Act.Type) -> ActorID
    where Act: DistributedActor {
    let id = ActorAddress(parse: "<unique-id>")
    print("| assign id: \(id) for \(actorType)")
    return id
  }

  public func actorReady<Act>(_ actor: Act)
    where Act: DistributedActor,
    Act.ID == ActorID {
    print("| actor ready: \(actor)")
    self.activeActors[actor.id] = actor
  }

  public func resignID(_ id: ActorID) {
    print("X resign id: \(id)")
  }

  public func makeInvocationEncoder() -> InvocationEncoder {
    .init()
  }

  private var remoteCallResult: Any? = nil
  private var remoteCallError: Error? = nil

  public func remoteCall<Act, Err, Res>(
    on actor: Act,
    target: RemoteCallTarget,
    invocation: inout InvocationEncoder,
    throwing errorType: Err.Type,
    returning returnType: Res.Type
  ) async throws -> Res
    where Act: DistributedActor,
          Act.ID == ActorID,
          Err: Error,
          Res: SerializationRequirement {
    print("  >> remoteCall: on:\(actor), target:\(target), invocation:\(invocation), throwing:\(String(reflecting: errorType)), returning:\(String(reflecting: returnType))")
    guard let targetActor = activeActors[actor.id] else {
      fatalError("Attempted to call mock 'roundtrip' on: \(actor.id) without active actor")
    }

    func doIt<A: DistributedActor>(active: A) async throws -> Res {
      guard (actor.id) == active.id as! ActorID else {
        fatalError("Attempted to call mock 'roundtrip' on unknown actor: \(actor.id), known: \(active.id)")
      }

      let resultHandler = FakeRoundtripResultHandler { value in
        self.remoteCallResult = value
        self.remoteCallError = nil
      } onError: { error in
        self.remoteCallResult = nil
        self.remoteCallError = error
      }
      try await executeDistributedTarget(
        on: active,
        target: target,
        invocationDecoder: invocation.makeDecoder(),
        handler: resultHandler
      )

      switch (remoteCallResult, remoteCallError) {
      case (.some(let value), nil):
        print("  << remoteCall return: \(value)")
        return remoteCallResult! as! Res
      case (nil, .some(let error)):
        print("  << remoteCall throw: \(error)")
        throw error
      default:
        fatalError("No reply!")
      }
    }
    return try await _openExistential(targetActor, do: doIt)
  }

  public func remoteCallVoid<Act, Err>(
    on actor: Act,
    target: RemoteCallTarget,
    invocation: inout InvocationEncoder,
    throwing errorType: Err.Type
  ) async throws
    where Act: DistributedActor,
          Act.ID == ActorID,
          Err: Error {
    print("  >> remoteCallVoid: on:\(actor), target:\(target), invocation:\(invocation), throwing:\(String(reflecting: errorType))")
    guard let targetActor = activeActors[actor.id] else {
      fatalError("Attempted to call mock 'roundtrip' on: \(actor.id) without active actor")
    }

    func doIt<A: DistributedActor>(active: A) async throws {
      guard (actor.id) == active.id as! ActorID else {
        fatalError("Attempted to call mock 'roundtrip' on unknown actor: \(actor.id), known: \(active.id)")
      }

      let resultHandler = FakeRoundtripResultHandler { value in
        self.remoteCallResult = value
        self.remoteCallError = nil
      } onError: { error in
        self.remoteCallResult = nil
        self.remoteCallError = error
      }
      try await executeDistributedTarget(
        on: active,
        target: target,
        invocationDecoder: invocation.makeDecoder(),
        handler: resultHandler
      )

      switch (remoteCallResult, remoteCallError) {
      case (.some, nil):
        return
      case (nil, .some(let error)):
        print("  << remoteCall throw: \(error)")
        throw error
      default:
        fatalError("No reply!")
      }
    }
    try await _openExistential(targetActor, do: doIt)
  }

}

public struct FakeInvocationEncoder : DistributedTargetInvocationEncoder {
  public typealias SerializationRequirement = Codable

  var genericSubs: [Any.Type] = []
  var arguments: [Any] = []
  var returnType: Any.Type? = nil
  var errorType: Any.Type? = nil

  public mutating func recordGenericSubstitution<T>(_ type: T.Type) throws {
    print(" > encode generic sub: \(String(reflecting: type))")
    genericSubs.append(type)
  }

  public mutating func recordArgument<Value: SerializationRequirement>(_ argument: RemoteCallArgument<Value>) throws {
    print(" > encode argument name:\(argument.effectiveLabel), argument: \(argument.value)")
    arguments.append(argument)
  }
  public mutating func recordErrorType<E: Error>(_ type: E.Type) throws {
    print(" > encode error type: \(String(reflecting: type))")
    self.errorType = type
  }
  public mutating func recordReturnType<R: SerializationRequirement>(_ type: R.Type) throws {
    print(" > encode return type: \(String(reflecting: type))")
    self.returnType = type
  }
  public mutating func doneRecording() throws {
    print(" > done recording")
  }

  public func makeDecoder() -> FakeInvocationDecoder {
    return .init(
      args: arguments,
      substitutions: genericSubs,
      returnType: returnType,
      errorType: errorType
    )
  }
}

// === decoding --------------------------------------------------------------
public class FakeInvocationDecoder : DistributedTargetInvocationDecoder {
    public typealias SerializationRequirement = Codable

  var genericSubs: [Any.Type] = []
  var arguments: [Any] = []
  var returnType: Any.Type? = nil
  var errorType: Any.Type? = nil

  var argumentIndex: Int = 0

  fileprivate init(
    args: [Any],
    substitutions: [Any.Type] = [],
    returnType: Any.Type? = nil,
    errorType: Any.Type? = nil
  ) {
    self.arguments = args
    self.genericSubs = substitutions
    self.returnType = returnType
    self.errorType = errorType
  }

  public func decodeGenericSubstitutions() throws -> [Any.Type] {
    print("  > decode generic subs: \(genericSubs)")
    return genericSubs
  }

  public func decodeNextArgument<Argument: SerializationRequirement>() throws -> Argument {
    guard argumentIndex < arguments.count else {
      fatalError("Attempted to decode more arguments than stored! Index: \(argumentIndex), args: \(arguments)")
    }

    let anyArgument = arguments[argumentIndex]
    guard let argument = anyArgument as? Argument else {
      fatalError("Cannot cast argument\(anyArgument) to expected \(Argument.self)")
    }

    print("  > decode argument: \(argument)")
    argumentIndex += 1
    return argument
  }

  public func decodeErrorType() throws -> Any.Type? {
    print("  > decode return type: \(errorType.map { String(describing: $0) }  ?? "nil")")
    return self.errorType
  }

  public func decodeReturnType() throws -> Any.Type? {
    print("  > decode return type: \(returnType.map { String(describing: $0) }  ?? "nil")")
    return self.returnType
  }
}

@available(SwiftStdlib 5.5, *)
public struct FakeRoundtripResultHandler: DistributedTargetInvocationResultHandler {
  public typealias SerializationRequirement = Codable

  let storeReturn: (any Any) -> Void
  let storeError: (any Error) -> Void
  init(_ storeReturn: @escaping (Any) -> Void, onError storeError: @escaping (Error) -> Void) {
    self.storeReturn = storeReturn
    self.storeError = storeError
  }

  // FIXME(distributed): can we return void here?
  public func onReturn<Success: SerializationRequirement>(value: Success) async throws {
    print(" << onReturn: \(value)")
    storeReturn(value)
  }

  public func onThrow<Err: Error>(error: Err) async throws {
    print(" << onThrow: \(error)")
    storeError(error)
  }
}

// ==== Helpers ----------------------------------------------------------------

@_silgen_name("swift_distributed_actor_is_remote")
func __isRemoteActor(_ actor: AnyObject) -> Bool
