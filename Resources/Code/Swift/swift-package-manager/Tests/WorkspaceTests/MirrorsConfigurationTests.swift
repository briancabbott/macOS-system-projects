//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift open source project
//
// Copyright (c) 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import SPMTestSupport
import TSCBasic
import Workspace
import XCTest

final class MirrorsConfigurationTests: XCTestCase {
    func testLoadingSchema1() throws {
        let fs = InMemoryFileSystem()
        let configFile = AbsolutePath("/config/mirrors.json")

        let originalURL = "https://github.com/apple/swift-argument-parser.git"
        let mirrorURL = "https://github.com/mona/swift-argument-parser.git"

        try fs.writeFileContents(configFile) {
            $0 <<< """
                {
                  "object": [
                    {
                      "mirror": "\(mirrorURL)",
                      "original": "\(originalURL)"
                    }
                  ],
                  "version": 1
                }
                """
        }

        let config = Workspace.Configuration.MirrorsStorage(path: configFile, fileSystem: fs, deleteWhenEmpty: true)
        let mirrors = try config.get()

        XCTAssertEqual(mirrors.mirrorURL(for: originalURL),mirrorURL)
        XCTAssertEqual(mirrors.originalURL(for: mirrorURL), originalURL)
    }

    func testThrowsWhenNotFound() throws {
        let fs = InMemoryFileSystem()
        let configFile = AbsolutePath("/config/mirrors.json")

        let config = Workspace.Configuration.MirrorsStorage(path: configFile, fileSystem: fs, deleteWhenEmpty: true)
        let mirrors = try config.get()

        XCTAssertThrows(StringError("Mirror not found for 'https://github.com/apple/swift-argument-parser.git'")) {
            try mirrors.unset(originalOrMirrorURL: "https://github.com/apple/swift-argument-parser.git")
        }
    }

    func testDeleteWhenEmpty() throws {
        let fs = InMemoryFileSystem()
        let configFile = AbsolutePath("/config/mirrors.json")

        let config = Workspace.Configuration.MirrorsStorage(path: configFile, fileSystem: fs, deleteWhenEmpty: true)

        try config.apply{ _ in }
        XCTAssertFalse(fs.exists(configFile))

        let originalURL = "https://github.com/apple/swift-argument-parser.git"
        let mirrorURL = "https://github.com/mona/swift-argument-parser.git"

        try config.apply{ mirrors in
            mirrors.set(mirrorURL: mirrorURL, forURL: originalURL)
        }
        XCTAssertTrue(fs.exists(configFile))

        try config.apply{ mirrors in
            try mirrors.unset(originalOrMirrorURL: originalURL)
        }
        XCTAssertFalse(fs.exists(configFile))
    }

    func testDontDeleteWhenEmpty() throws {
        let fs = InMemoryFileSystem()
        let configFile = AbsolutePath("/config/mirrors.json")

        let config = Workspace.Configuration.MirrorsStorage(path: configFile, fileSystem: fs, deleteWhenEmpty: false)

        try config.apply{ _ in }
        XCTAssertFalse(fs.exists(configFile))

        let originalURL = "https://github.com/apple/swift-argument-parser.git"
        let mirrorURL = "https://github.com/mona/swift-argument-parser.git"

        try config.apply{ mirrors in
            mirrors.set(mirrorURL: mirrorURL, forURL: originalURL)
        }
        XCTAssertTrue(fs.exists(configFile))

        try config.apply{ mirrors in
            try mirrors.unset(originalOrMirrorURL: originalURL)
        }
        XCTAssertTrue(fs.exists(configFile))
        XCTAssertTrue(try config.get().isEmpty)
    }

    func testLocalAndShared() throws {
        let fs = InMemoryFileSystem()
        let localConfigFile = AbsolutePath("/config/local-mirrors.json")
        let sharedConfigFile = AbsolutePath("/config/shared-mirrors.json")

        let config = try Workspace.Configuration.Mirrors(
            fileSystem: fs,
            localMirrorsFile: localConfigFile,
            sharedMirrorsFile: sharedConfigFile
        )

        // first write to shared location

        let original1URL = "https://github.com/apple/swift-argument-parser.git"
        let mirror1URL = "https://github.com/mona/swift-argument-parser.git"

        try config.applyShared { mirrors in
            mirrors.set(mirrorURL: mirror1URL, forURL: original1URL)
        }

        XCTAssertEqual(config.mirrors.count, 1)
        XCTAssertEqual(config.mirrors.mirrorURL(for: original1URL), mirror1URL)
        XCTAssertEqual(config.mirrors.originalURL(for: mirror1URL), original1URL)

        // now write to local location

        let original2URL = "https://github.com/apple/swift-nio.git"
        let mirror2URL = "https://github.com/mona/swift-nio.git"

        try config.applyLocal { mirrors in
            mirrors.set(mirrorURL: mirror2URL, forURL: original2URL)
        }

        XCTAssertEqual(config.mirrors.count, 1)
        XCTAssertEqual(config.mirrors.mirrorURL(for: original2URL), mirror2URL)
        XCTAssertEqual(config.mirrors.originalURL(for: mirror2URL), original2URL)

        // should not see the shared any longer
        XCTAssertEqual(config.mirrors.mirrorURL(for: original1URL), nil)
        XCTAssertEqual(config.mirrors.originalURL(for: mirror1URL), nil)
    }
}
