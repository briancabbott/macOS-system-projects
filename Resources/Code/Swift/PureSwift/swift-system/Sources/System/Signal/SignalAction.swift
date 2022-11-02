/*
 This source file is part of the Swift System open source project

 Copyright (c) 2021 Apple Inc. and the Swift System project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
*/

public extension Signal {
    
    /// POSIX Signal Handler
    @frozen
    // @available(macOS 10.16, iOS 14.0, watchOS 7.0, tvOS 14.0, *)
    struct Action {
        
        @usableFromInline
        internal fileprivate(set) var bytes: CInterop.SignalAction
        
        public init(_ bytes: CInterop.SignalAction) {
            self.bytes = bytes
        }
        
        @_alwaysEmitIntoClient
        public init(
            mask: Signal.Set = Signal.Set(),
            flags: Flags = [],
            handler: Signal.Handler
        ) {
            assert(flags.contains(.sigInfo) == false)
            #if os(macOS) || os(iOS) || os(watchOS) || os(tvOS)
            self.init(CInterop.SignalAction(
                __sigaction_u: .init(__sa_handler: handler.rawValue),
                sa_mask: mask.bytes,
                sa_flags: flags.rawValue)
            )
            #elseif os(Linux)
            self.init(CInterop.SignalAction(
                __sigaction_handler: .init(sa_handler: handler.rawValue),
                sa_mask: mask.bytes,
                sa_flags: flags.rawValue,
                sa_restorer: { })
            )
            #endif
        }
        
        @_alwaysEmitIntoClient
        public init(
            mask: Signal.Set = Signal.Set(),
            flags: Flags = [],
            _ body: @escaping CInterop.SignalActionHandler
        ) {
            var flags = flags
            flags.insert(.sigInfo)
            #if os(macOS) || os(iOS) || os(watchOS) || os(tvOS)
            self.init(CInterop.SignalAction(
                __sigaction_u: .init(__sa_sigaction: body),
                sa_mask: mask.bytes,
                sa_flags: flags.rawValue)
            )
            #elseif os(Linux)
            self.init(CInterop.SignalAction(
                __sigaction_handler: .init(sa_sigaction: body),
                sa_mask: mask.bytes,
                sa_flags: flags.rawValue,
                sa_restorer: { })
            )
            #endif
        }
        
        @_alwaysEmitIntoClient
        internal init() {
            self.init(CInterop.SignalAction())
        }
    }
}

public extension Signal.Action {
    
    @_alwaysEmitIntoClient
    var flags: Flags {
        return .init(rawValue: bytes.sa_flags)
    }
    
    @_alwaysEmitIntoClient
    var mask: Signal.Set {
        return Signal.Set(bytes.sa_mask)
    }
}

public extension Signal {
    
    /// Sets a signal handler for the specified signal and returns the old signal handler.
    @discardableResult
    @_alwaysEmitIntoClient
    func handle(
        _ handler: Handler,
        retryOnInterrupt: Bool = true
    ) throws -> Signal.Action {
        return try _handle(handler, retryOnInterrupt: retryOnInterrupt).get()
    }
    
    /// Sets a signal action for the specified signal and returns the old signal handler.
    @discardableResult
    @_alwaysEmitIntoClient
    func action(
        _ action: Action,
        retryOnInterrupt: Bool = true
    ) throws -> Signal.Action {
        return try _action(action, retryOnInterrupt: retryOnInterrupt).get()
    }
    
    @usableFromInline
    internal func _handle(
        _ handler: Handler,
        retryOnInterrupt: Bool
    ) -> Result<Signal.Action, Errno> {
        _action(Action(handler: handler), retryOnInterrupt: retryOnInterrupt)
    }
    
    @usableFromInline
    internal func _action(
        _ action: Signal.Action,
        retryOnInterrupt: Bool
    ) -> Result<Signal.Action, Errno> {
        var oldAction = Signal.Action()
        return nothingOrErrno(retryOnInterrupt: retryOnInterrupt) {
            withUnsafePointer(to: action.bytes) { actionPointer in
                system_sigaction(self.rawValue, actionPointer, &oldAction.bytes)
            }
        }.map { oldAction }
    }
}

public extension Signal.Action {
    
    @frozen
    // @available(macOS 10.16, iOS 14.0, watchOS 7.0, tvOS 14.0, *)
    struct Flags: OptionSet, Hashable, Codable {
        
        /// The raw C file events.
        @_alwaysEmitIntoClient
        public let rawValue: CInt
        
        /// Create a strongly-typed file events from a raw C value.
        @_alwaysEmitIntoClient
        public init(rawValue: CInt) { self.rawValue = rawValue }

        @_alwaysEmitIntoClient
        private init(_ raw: CInt) { self.init(rawValue: raw) }
    }
}

public extension Signal.Action.Flags {
    
    /// Do not receive notification when child processes stop.
    @_alwaysEmitIntoClient
    static var noChildStop: Signal.Action.Flags { .init(_SA_NOCLDSTOP) }
    
    /// A `SIGCHLD` signal is generated when a child process terminates.
    @_alwaysEmitIntoClient
    static var noChildWait: Signal.Action.Flags { .init(_SA_NOCLDWAIT) }
    
    /// Do not prevent the signal from being received from within its own signal handler.
    @_alwaysEmitIntoClient
    static var noDefer: Signal.Action.Flags { .init(_SA_NODEFER) }
    
    /// Call the signal handler on an alternate signal stack.. If an alternate stack is not available, the default stack will be used.
    @_alwaysEmitIntoClient
    static var onStack: Signal.Action.Flags { .init(_SA_ONSTACK) }
    
    /// Restore the signal action to the default upon entry to the signal handler. This flag is only meaningful when establishing a signal handler.
    @_alwaysEmitIntoClient
    static var resetHandler: Signal.Action.Flags { .init(_SA_RESETHAND) }
    
    /// Provide behavior compatible with BSD signal semantics by making certain system calls restartable across signals. This flag is only meaningful when establishing a signal handler.
    @_alwaysEmitIntoClient
    static var restart: Signal.Action.Flags { .init(_SA_RESTART) }
    
    /// The signal handler takes three arguments, not one.
    @_alwaysEmitIntoClient
    internal static var sigInfo: Signal.Action.Flags { .init(_SA_SIGINFO) }
}

extension Signal.Action.Flags: CustomStringConvertible, CustomDebugStringConvertible
{
  /// A textual representation of the file permissions.
  @inline(never)
  public var description: String {
    let descriptions: [(Element, StaticString)] = [
      (.noChildStop, ".noChildStop"),
      (.noChildWait, ".noChildWait"),
      (.noDefer, ".noDefer"),
      (.onStack, ".onStack"),
      (.resetHandler, ".resetHandler"),
      (.restart, ".restart")
    ]

    return _buildDescription(descriptions)
  }

  /// A textual representation of the file permissions, suitable for debugging.
  public var debugDescription: String { self.description }
}
