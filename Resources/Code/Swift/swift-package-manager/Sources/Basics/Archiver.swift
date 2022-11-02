//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift open source project
//
// Copyright (c) 2014-2022 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import TSCBasic

/// The `Archiver` protocol abstracts away the different operations surrounding archives.
public protocol Archiver {
    /// A set of extensions the current archiver supports.
    var supportedExtensions: Set<String> { get }

    /// Asynchronously extracts the contents of an archive to a destination folder.
    ///
    /// - Parameters:
    ///   - archivePath: The `AbsolutePath` to the archive to extract.
    ///   - destinationPath: The `AbsolutePath` to the directory to extract to.
    ///   - completion: The completion handler that will be called when the operation finishes to notify of its success.
    func extract(
        from archivePath: AbsolutePath,
        to destinationPath: AbsolutePath,
        completion: @escaping (Result<Void, Error>) -> Void
    )

    /// Asynchronously validates if a file is an archive.
    ///
    /// - Parameters:
    ///   - path: The `AbsolutePath` to the archive to validate.
    ///   - completion: The completion handler that will be called when the operation finishes to notify of its success.
    func validate(
        path: AbsolutePath,
        completion: @escaping (Result<Bool, Error>) -> Void
    )
}
