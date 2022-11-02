/*
 This source file is part of the Swift System open source project

 Copyright (c) 2021 Apple Inc. and the Swift System project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
*/

/// POSIX User ID
@frozen
public struct UserID: RawRepresentable, Equatable, Hashable, Codable {
    
    /// The raw C user ID.
    @_alwaysEmitIntoClient
    public let rawValue: CInterop.UserID

    /// Creates a strongly typed process ID from a raw C user ID.
    @_alwaysEmitIntoClient
    public init(rawValue: CInterop.UserID) { self.rawValue = rawValue }
}
