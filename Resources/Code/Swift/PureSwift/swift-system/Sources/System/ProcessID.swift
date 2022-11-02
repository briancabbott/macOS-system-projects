/*
 This source file is part of the Swift System open source project

 Copyright (c) 2021 Apple Inc. and the Swift System project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
*/

/// POSIX Process ID
@frozen
public struct ProcessID: RawRepresentable, Equatable, Hashable, Codable {
    
    /// The raw C process ID.
    @_alwaysEmitIntoClient
    public let rawValue: CInterop.ProcessID

    /// Creates a strongly typed process ID from a raw C process ID.
    @_alwaysEmitIntoClient
    public init(rawValue: CInterop.ProcessID) { self.rawValue = rawValue }
}

public extension ProcessID {
    
    @_alwaysEmitIntoClient
    static var current: ProcessID { ProcessID(rawValue: system_getpid()) }
    
    @_alwaysEmitIntoClient
    static var parent: ProcessID { ProcessID(rawValue: system_getppid()) }
}
