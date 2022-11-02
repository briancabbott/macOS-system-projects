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

import Basics
import TSCBasic
import TSCTestSupport
import XCTest
import SPMTestSupport

class ZipArchiverTests: XCTestCase {
    func testZipArchiverSuccess() throws {
        try testWithTemporaryDirectory { tmpdir in
            let archiver = ZipArchiver(fileSystem: localFileSystem)
            let inputArchivePath = AbsolutePath(#file).parentDirectory.appending(components: "Inputs", "archive.zip")
            try archiver.extract(from: inputArchivePath, to: tmpdir)
            let content = tmpdir.appending(component: "file")
            XCTAssert(localFileSystem.exists(content))
            XCTAssertEqual((try? localFileSystem.readFileContents(content))?.cString, "Hello World!")
        }
    }

    func testZipArchiverArchiveDoesntExist() {
        let fileSystem = InMemoryFileSystem()
        let archiver = ZipArchiver(fileSystem: fileSystem)
        let archive = AbsolutePath("/archive.zip")
        XCTAssertThrowsError(try archiver.extract(from: archive, to: AbsolutePath("/"))) { error in
            XCTAssertEqual(error as? FileSystemError, FileSystemError(.noEntry, archive))
        }
    }

    func testZipArchiverDestinationDoesntExist() throws {
        let fileSystem = InMemoryFileSystem(emptyFiles: "/archive.zip")
        let archiver = ZipArchiver(fileSystem: fileSystem)
        let destination = AbsolutePath("/destination")
        XCTAssertThrowsError(try archiver.extract(from: AbsolutePath("/archive.zip"), to: destination)) { error in
            XCTAssertEqual(error as? FileSystemError, FileSystemError(.notDirectory, destination))
        }
    }

    func testZipArchiverDestinationIsFile() throws {
        let fileSystem = InMemoryFileSystem(emptyFiles: "/archive.zip", "/destination")
        let archiver = ZipArchiver(fileSystem: fileSystem)
        let destination = AbsolutePath("/destination")
        XCTAssertThrowsError(try archiver.extract(from: AbsolutePath("/archive.zip"), to: destination)) { error in
            XCTAssertEqual(error as? FileSystemError, FileSystemError(.notDirectory, destination))
        }
    }

    func testZipArchiverInvalidArchive() throws {
        try testWithTemporaryDirectory { tmpdir in
            let archiver = ZipArchiver(fileSystem: localFileSystem)
            let inputArchivePath = AbsolutePath(#file).parentDirectory
                .appending(components: "Inputs", "invalid_archive.zip")
            XCTAssertThrowsError(try archiver.extract(from: inputArchivePath, to: tmpdir)) { error in
#if os(Windows)
                XCTAssertMatch((error as? StringError)?.description, .contains("Unrecognized archive format"))
#else
                XCTAssertMatch((error as? StringError)?.description, .contains("End-of-central-directory signature not found"))
#endif
            }
        }
    }

    func testValidation() throws {
        // valid
        try testWithTemporaryDirectory { tmpdir in
            let archiver = ZipArchiver(fileSystem: localFileSystem)
            let path = AbsolutePath(#file).parentDirectory
                .appending(components: "Inputs", "archive.zip")
            XCTAssertTrue(try archiver.validate(path: path))
        }
        // invalid
        try testWithTemporaryDirectory { tmpdir in
            let archiver = ZipArchiver(fileSystem: localFileSystem)
            let path = AbsolutePath(#file).parentDirectory
                .appending(components: "Inputs", "invalid_archive.zip")
            XCTAssertFalse(try archiver.validate(path: path))
        }
        // error
        try testWithTemporaryDirectory { tmpdir in
            let archiver = ZipArchiver(fileSystem: localFileSystem)
            let path = AbsolutePath.root.appending(component: "does_not_exist.zip")
            XCTAssertThrowsError(try archiver.validate(path: path)) { error in
                XCTAssertEqual(error as? FileSystemError, FileSystemError(.noEntry, path))
            }
        }
    }
}

class ArchiverTests: XCTestCase {
    func testCancel() throws {
        struct MockArchiver: Archiver, Cancellable {
            var supportedExtensions: Set<String> = []

            let cancelSemaphores = ThreadSafeArrayStore<DispatchSemaphore>()
            let startGroup = DispatchGroup()
            let finishGroup = DispatchGroup()

            func extract(from archivePath: AbsolutePath, to destinationPath: AbsolutePath, completion: @escaping (Result<Void, Error>) -> Void) {
                let cancelSemaphore = DispatchSemaphore(value: 0)
                self.cancelSemaphores.append(cancelSemaphore)

                self.startGroup.enter()
                DispatchQueue.sharedConcurrent.async {
                    self.startGroup.leave()
                    self.finishGroup.enter()
                    defer { self.finishGroup.leave() }
                    switch cancelSemaphore.wait(timeout: .now() + .seconds(5)) {
                    case .success:
                        completion(.success(()))
                    case .timedOut:
                        completion(.failure(StringError("should be cancelled")))
                    }
                }
            }

            func validate(path: AbsolutePath, completion: @escaping (Result<Bool, Error>) -> Void) {
                let cancelSemaphore = DispatchSemaphore(value: 0)
                self.cancelSemaphores.append(cancelSemaphore)

                self.startGroup.enter()
                DispatchQueue.sharedConcurrent.async {
                    self.startGroup.leave()
                    self.finishGroup.enter()
                    defer { self.finishGroup.leave() }
                    switch cancelSemaphore.wait(timeout: .now() + .seconds(5)) {
                    case .success:
                        completion(.success(true))
                    case .timedOut:
                        completion(.failure(StringError("should be cancelled")))
                    }
                }
            }

            func cancel(deadline: DispatchTime) throws {
                for semaphore in self.cancelSemaphores.get() {
                    semaphore.signal()
                }
            }
        }

        let observability = ObservabilitySystem.makeForTesting()
        let cancellator = Cancellator(observabilityScope: observability.topScope)

        let archiver = MockArchiver()
        cancellator.register(name: "archiver", handler: archiver)

        archiver.extract(from: .root, to: .root) { result in
            XCTAssertResultSuccess(result)
        }

        archiver.validate(path: .root) { result in
            XCTAssertResultSuccess(result)
        }

        XCTAssertEqual(.success, archiver.startGroup.wait(timeout: .now() + .seconds(5)), "timeout waiting for tasks to start")

        try cancellator.cancel(deadline: .now() + .seconds(5))

        XCTAssertEqual(.success, archiver.finishGroup.wait(timeout: .now() + .seconds(5)), "timeout waiting for tasks to finish")
    }
}

extension Archiver {
    fileprivate func extract(from: AbsolutePath, to: AbsolutePath) throws {
        try tsc_await {
            self.extract(from: from, to: to, completion: $0)
        }
    }
    fileprivate func validate(path: AbsolutePath) throws -> Bool {
        try tsc_await {
            self.validate(path: path, completion: $0)
        }
    }
}
