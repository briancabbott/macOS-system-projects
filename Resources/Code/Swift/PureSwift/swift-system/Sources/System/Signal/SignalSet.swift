/*
 This source file is part of the Swift System open source project

 Copyright (c) 2021 Apple Inc. and the Swift System project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
*/

public extension Signal {
    
    /// POSIX Signal Set
    @frozen
    struct Set {
        
        @usableFromInline
        internal var bytes: CInterop.SignalSet
        
        public init(_ bytes: CInterop.SignalSet) {
            self.bytes = bytes
        }
        
        /// Initializes the signal set set to exclude all of the defined signals.
        public init() {
            var bytes = CInterop.SignalSet()
            system_sigemptyset(&bytes)
            self.init(bytes)
        }
    }
}

extension Signal { // CaseIterable required Signal.Set be a collection
    
    public static var allCases: Signal.Set {
        var bytes = CInterop.SignalSet()
        system_sigfillset(&bytes)
        return Set(bytes)
    }
}

public extension Signal.Set {
    
    /// Adds the signal signum to the signal set.
    @_alwaysEmitIntoClient
    mutating func insert(
        _ element: Signal,
        retryOnInterrupt: Bool = true
    ) throws {
        try _insert(element, retryOnInterrupt: retryOnInterrupt).get()
    }
    
    @usableFromInline
    internal mutating func _insert(
        _ element: Signal,
        retryOnInterrupt: Bool
    ) -> Result<(), Errno> {
        nothingOrErrno(retryOnInterrupt: retryOnInterrupt) {
            system_sigaddset(&bytes, element.rawValue)
        }
    }
    
    /// Removes the signal from the signal set.
    @_alwaysEmitIntoClient
    mutating func remove(
        _ element: Signal,
        retryOnInterrupt: Bool = true
    ) throws {
        try _remove(element, retryOnInterrupt: retryOnInterrupt).get()
    }
    
    @usableFromInline
    internal mutating func _remove(
        _ element: Signal,
        retryOnInterrupt: Bool
    ) -> Result<(), Errno> {
        nothingOrErrno(retryOnInterrupt: retryOnInterrupt) {
            system_sigdelset(&bytes, element.rawValue)
        }
    }
    
    /// Removes the signal from the signal set.
    @_alwaysEmitIntoClient
    mutating func contains(
        _ element: Signal,
        retryOnInterrupt: Bool = true
    ) throws -> Bool {
        return try _contains(element, retryOnInterrupt: retryOnInterrupt).get()
    }
    
    @usableFromInline
    internal mutating func _contains(
        _ element: Signal,
        retryOnInterrupt: Bool
    ) -> Result<Bool, Errno> {
        valueOrErrno(retryOnInterrupt: retryOnInterrupt) {
            system_sigismember(&bytes, element.rawValue)
        }.map(Bool.init(_:))
    }
}

public extension Signal.Set {
    
    @_alwaysEmitIntoClient
    func withUnsafePointer<T>(
        _ body: (UnsafePointer<CInterop.SignalSet>) throws -> T
    ) rethrows -> T {
        try Swift.withUnsafePointer(to: self.bytes, body)
    }
    
    @_alwaysEmitIntoClient
    mutating func withUnsafeMutablePointer<T>(
        _ body: (UnsafeMutablePointer<CInterop.SignalSet>) throws -> T
    ) rethrows -> T {
        try Swift.withUnsafeMutablePointer(to: &self.bytes, body)
    }
}

extension Signal.Set: ExpressibleByArrayLiteral {
    
    public init(arrayLiteral elements: Signal...) {
        self.init()
        for element in elements {
            let result = _insert(element, retryOnInterrupt: false)
            switch result {
            case let .failure(error):
                preconditionFailure("Unable to append element \(element). \(error)")
            case .success:
                continue
            }
        }
    }
}
