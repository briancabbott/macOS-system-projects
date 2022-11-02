// This source file is part of the Swift.org open source project
//
// Copyright (c) 2022 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for Swift project authors

import Foundation
@testable import SwiftDocCPluginUtilities
import XCTest

final class SnippetBuilderTests: XCTestCase {
    lazy var workingDirectory = URL(
        fileURLWithPath: "/test-working-directory",
        isDirectory: true
    )
    
    lazy var snippetBuilder = SnippetBuilder(
        snippetTool: URL(fileURLWithPath: "/snippet-tool", isDirectory: false),
        workingDirectory: workingDirectory
    )
    
    func testSnippetGeneration() throws {
        let expectedFilePaths: Set<String> = [
            "/my/package/Snippets",
            "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyPackage-package-id",
        ]
        let existingFilePaths: Set<String> = [
            "/my/package/Snippets",
            "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyPackage-package-id",
        ]
        snippetBuilder._fileExists = { path in
            XCTAssertTrue(
                expectedFilePaths.contains(path),
                "Unexpected file path: '\(path)'"
            )
            
            return existingFilePaths.contains(path)
        }
        
        var snippetBuilderRunProcessCount = 0
        snippetBuilder._runProcess = { process in
            if snippetBuilderRunProcessCount == 0 {
                XCTAssertEqual(
                    process.arguments,
                    [
                        "/my/package/Snippets",
                        "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyPackage-package-id",
                        "MyPackage",
                    ]
                )
            } else {
                XCTFail("Snippet build process ran unexpectedly.")
            }
            
            snippetBuilderRunProcessCount += 1
        }
        
        // Assert that generating snippets for the same package multiple times
        // only invokes the snippet-build process once
        for _ in 1...10 {
            let snippetDirectory = try snippetBuilder.generateSnippets(
                for: "package-id",
                packageDisplayName: "MyPackage",
                packageDirectory: URL(fileURLWithPath: "/my/package")
            )
            
            XCTAssertEqual(snippetBuilderRunProcessCount, 1)
            XCTAssertEqual(
                snippetDirectory?.path,
                "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyPackage-package-id"
            )
        }
    }
    
    func testSnippetGenerationForNonSnippetPackage() throws {
        let expectedFilePaths: Set<String> = [
            "/my/package/Snippets",
        ]
        
        snippetBuilder._fileExists = { path in
            XCTAssertTrue(
                expectedFilePaths.contains(path),
                "Unexpected file path: '\(path)'"
            )
            
            // Return false when the snippet builder checks for the existence
            // of a snippets directory
            return false
        }
        
        snippetBuilder._runProcess = { process in
            XCTFail("Snippet builder process ran for package that does not contain snippets.")
        }
        
        let snippetDirectory = try snippetBuilder.generateSnippets(
            for: "package-id",
            packageDisplayName: "MyPackage",
            packageDirectory: URL(fileURLWithPath: "/my/package")
        )
        
        XCTAssertNil(snippetDirectory)
    }
    
    func testSnippetGenerationForMultiplePackages() throws {
        let expectedFilePaths: Set<String> = [
            "/my/package/Snippets",
            "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyPackage-package-id",
            "/my/other/package/Snippets",
            "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyOtherPackage-other-package-id",
        ]
        let existingFilePaths: Set<String> = [
            "/my/package/Snippets",
            "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyPackage-package-id",
            "/my/other/package/Snippets",
            "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyOtherPackage-other-package-id",
        ]
        snippetBuilder._fileExists = { path in
            XCTAssertTrue(
                expectedFilePaths.contains(path),
                "Unexpected file path: '\(path)'"
            )
            
            return existingFilePaths.contains(path)
        }
        
        var snippetBuilderRunProcessCount = 0
        snippetBuilder._runProcess = { process in
            if snippetBuilderRunProcessCount == 0 {
                XCTAssertEqual(
                    process.arguments,
                    [
                        "/my/package/Snippets",
                        "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyPackage-package-id",
                        "MyPackage",
                    ]
                )
            } else if snippetBuilderRunProcessCount == 1 {
                XCTAssertEqual(
                    process.arguments,
                    [
                        "/my/other/package/Snippets",
                        "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyOtherPackage-other-package-id",
                        "MyOtherPackage",
                    ]
                )
            } else {
                XCTFail("Snippet build process ran unexpectedly.")
            }
            snippetBuilderRunProcessCount += 1
        }
        
        for _ in 1...10 {
            let snippetDirectory = try snippetBuilder.generateSnippets(
                for: "package-id",
                packageDisplayName: "MyPackage",
                packageDirectory: URL(fileURLWithPath: "/my/package")
            )
            
            XCTAssertEqual(snippetBuilderRunProcessCount, 1)
            XCTAssertEqual(
                snippetDirectory?.path,
                "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyPackage-package-id"
            )
        }
        
        for _ in 1...10 {
            let snippetDirectory = try snippetBuilder.generateSnippets(
                for: "other-package-id",
                packageDisplayName: "MyOtherPackage",
                packageDirectory: URL(fileURLWithPath: "/my/other/package")
            )
            
            XCTAssertEqual(snippetBuilderRunProcessCount, 2)
            XCTAssertEqual(
                snippetDirectory?.path,
                "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyOtherPackage-other-package-id"
            )
        }
    }
    
    func testSnippetGenerationForPackageWithSnippetsDirectoryButNoSnippets() throws {
        let expectedFilePaths: Set<String> = [
            "/my/package/Snippets",
            "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyPackage-package-id",
        ]
        let existingFilePaths: Set<String> = [
            "/my/package/Snippets",
            // Don't include the .build directory here to simulate the situation
            // where we have a `Snippets` directory but running the snippet-build tool
            // on them doesn't produce snippets.
        ]
        snippetBuilder._fileExists = { path in
            XCTAssertTrue(
                expectedFilePaths.contains(path),
                "Unexpected file path: '\(path)'"
            )
            
            return existingFilePaths.contains(path)
        }
        
        var snippetBuilderRunProcessCount = 0
        snippetBuilder._runProcess = { process in
            if snippetBuilderRunProcessCount == 0 {
                XCTAssertEqual(
                    process.arguments,
                    [
                        "/my/package/Snippets",
                        "/test-working-directory/.build/symbol-graphs/snippet-symbol-graphs/MyPackage-package-id",
                        "MyPackage",
                    ]
                )
            } else {
                XCTFail("Snippet build process ran unexpectedly.")
            }
            
            snippetBuilderRunProcessCount += 1
        }
        
        // Assert that generating snippets for the same package multiple times
        // only invokes the snippet-build process once
        for _ in 1...10 {
            let snippetDirectory = try snippetBuilder.generateSnippets(
                for: "package-id",
                packageDisplayName: "MyPackage",
                packageDirectory: URL(fileURLWithPath: "/my/package")
            )
            
            XCTAssertEqual(snippetBuilderRunProcessCount, 1)
            XCTAssertNil(snippetDirectory)
        }
    }
}
