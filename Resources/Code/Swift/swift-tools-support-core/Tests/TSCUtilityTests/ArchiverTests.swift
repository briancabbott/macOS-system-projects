/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See http://swift.org/LICENSE.txt for license information
 See http://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

import XCTest
import TSCBasic
import TSCUtility
import TSCTestSupport

// FIXME: deprecate 2/2022, remove once clients transitioned
@available(*, deprecated, message: "moved to SwiftPM")
class ArchiverTests: XCTestCase {
    // MARK: - ZipArchiver Tests

    func testZipArchiverSuccess() throws {
        try testWithTemporaryDirectory { tmpdir in
            let expectation = XCTestExpectation(description: "success")

            let archiver = ZipArchiver()
            let inputArchivePath = AbsolutePath(#file).parentDirectory.appending(components: "Inputs", "archive.zip")
            archiver.extract(from: inputArchivePath, to: tmpdir, completion: { result in
                XCTAssertResultSuccess(result) { _ in
                    let content = tmpdir.appending(component: "file")
                    XCTAssert(localFileSystem.exists(content))
                    XCTAssertEqual((try? localFileSystem.readFileContents(content))?.cString, "Hello World!")
                }
                expectation.fulfill()
            })

            wait(for: [expectation], timeout: 1.0)
        }
    }

    func testZipArchiverArchiveDoesntExist() {
        let expectation = XCTestExpectation(description: "failure")

        let fileSystem = InMemoryFileSystem()
        let archiver = ZipArchiver(fileSystem: fileSystem)
        let archive = AbsolutePath("/archive.zip")
        archiver.extract(from: archive, to: AbsolutePath("/"), completion: { result in
            XCTAssertResultFailure(result, equals: FileSystemError(.noEntry, archive))
            expectation.fulfill()
        })

        wait(for: [expectation], timeout: 1.0)
    }

    func testZipArchiverDestinationDoesntExist() {
        let expectation = XCTestExpectation(description: "failure")

        let fileSystem = InMemoryFileSystem(emptyFiles: "/archive.zip")
        let archiver = ZipArchiver(fileSystem: fileSystem)
        let destination = AbsolutePath("/destination")
        archiver.extract(from: AbsolutePath("/archive.zip"), to: destination, completion: { result in
            XCTAssertResultFailure(result, equals: FileSystemError(.notDirectory, destination))
            expectation.fulfill()
        })

        wait(for: [expectation], timeout: 1.0)
    }

    func testZipArchiverDestinationIsFile() {
        let expectation = XCTestExpectation(description: "failure")

        let fileSystem = InMemoryFileSystem(emptyFiles: "/archive.zip", "/destination")
        let archiver = ZipArchiver(fileSystem: fileSystem)
        let destination = AbsolutePath("/destination")
        archiver.extract(from: AbsolutePath("/archive.zip"), to: destination, completion: { result in
            XCTAssertResultFailure(result, equals: FileSystemError(.notDirectory, destination))
            expectation.fulfill()
        })

        wait(for: [expectation], timeout: 1.0)
    }

    func testZipArchiverInvalidArchive() throws {
        try testWithTemporaryDirectory { tmpdir in
            let expectation = XCTestExpectation(description: "failure")

            let archiver = ZipArchiver()
            let inputArchivePath = AbsolutePath(#file).parentDirectory
                .appending(components: "Inputs", "invalid_archive.zip")
            archiver.extract(from: inputArchivePath, to: tmpdir, completion: { result in
                XCTAssertResultFailure(result) { error in
                    guard let stringError = error as? StringError else {
                        XCTFail("unexpected error: \(error)")
                        return
                    }
                    XCTAssertMatch(stringError.description, .contains("End-of-central-directory signature not found"))
                }
                expectation.fulfill()
            })

            wait(for: [expectation], timeout: 1.0)
        }
    }
}

private struct DummyError: Error, Equatable {
}

private typealias Extraction = (AbsolutePath, AbsolutePath, (Result<Void, Error>) -> Void) -> Void

@available(*, deprecated)
private struct MockArchiver: Archiver {
    let supportedExtensions: Set<String>
    private let extract: Extraction

    init(supportedExtensions: Set<String>, extract: @escaping Extraction) {
        self.supportedExtensions = supportedExtensions
        self.extract = extract
    }

    func extract(
        from archivePath: AbsolutePath,
        to destinationPath: AbsolutePath,
        completion: @escaping (Result<Void, Error>) -> Void
    ) {
        self.extract(archivePath, destinationPath, completion)
    }
}
