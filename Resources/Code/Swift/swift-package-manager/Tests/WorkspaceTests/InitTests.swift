//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift open source project
//
// Copyright (c) 2014-2017 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import XCTest
import SPMTestSupport
import TSCBasic
import PackageModel
import Workspace

class InitTests: XCTestCase {

    // MARK: TSCBasic package creation for each package type.
    
    func testInitPackageEmpty() throws {
        try testWithTemporaryDirectory { tmpPath in
            let fs = localFileSystem
            let path = tmpPath.appending(component: "Foo")
            let name = path.basename
            try fs.createDirectory(path)
            
            // Create the package
            let initPackage = try InitPackage(
                name: name,
                packageType: .empty,
                destinationPath: path,
                fileSystem: localFileSystem
            )
            var progressMessages = [String]()
            initPackage.progressReporter = { message in
                progressMessages.append(message)
            }
            try initPackage.writePackageStructure()

            // Not picky about the specific progress messages, just checking that we got some.
            XCTAssertGreaterThan(progressMessages.count, 0)

            // Verify basic file system content that we expect in the package
            let manifest = path.appending(component: "Package.swift")
            XCTAssertFileExists(manifest)
            let manifestContents: String = try localFileSystem.readFileContents(manifest)
            let version = InitPackage.newPackageToolsVersion
            let versionSpecifier = "\(version.major).\(version.minor)"
            XCTAssertMatch(manifestContents, .prefix("// swift-tools-version:\(version < .v5_4 ? "" : " ")\(versionSpecifier)\n"))
            XCTAssertMatch(manifestContents, .contains(packageWithNameAndDependencies(with: name)))
            XCTAssertFileExists(path.appending(component: "README.md"))
            XCTAssertEqual(try fs.getDirectoryContents(path.appending(component: "Sources")), [])
            XCTAssertEqual(try fs.getDirectoryContents(path.appending(component: "Tests")), [])
        }
    }
    
    func testInitPackageExecutable() throws {
        try testWithTemporaryDirectory { tmpPath in
            let fs = localFileSystem
            let path = tmpPath.appending(component: "Foo")
            let name = path.basename
            try fs.createDirectory(path)

            // Create the package
            let initPackage = try InitPackage(
                name: name,
                packageType: .executable,
                destinationPath: path,
                fileSystem: localFileSystem
            )
            var progressMessages = [String]()
            initPackage.progressReporter = { message in
                progressMessages.append(message)
            }
            try initPackage.writePackageStructure()

            // Not picky about the specific progress messages, just checking that we got some.
            XCTAssertGreaterThan(progressMessages.count, 0)
            
            // Verify basic file system content that we expect in the package
            let manifest = path.appending(component: "Package.swift")
            XCTAssertFileExists(manifest)
            let manifestContents: String = try localFileSystem.readFileContents(manifest)
            let version = InitPackage.newPackageToolsVersion
            let versionSpecifier = "\(version.major).\(version.minor)"
            XCTAssertMatch(manifestContents, .prefix("// swift-tools-version:\(version < .v5_4 ? "" : " ")\(versionSpecifier)\n"))
            
            let readme = path.appending(component: "README.md")
            XCTAssertFileExists(readme)
            let readmeContents: String = try localFileSystem.readFileContents(readme)
            XCTAssertMatch(readmeContents, .prefix("# Foo\n"))

            XCTAssertEqual(try fs.getDirectoryContents(path.appending(component: "Sources").appending(component: "Foo")), ["Foo.swift"])
            XCTAssertEqual(try fs.getDirectoryContents(path.appending(component: "Tests")).sorted(), ["FooTests"])
            
            // If we have a compiler that supports `-entry-point-function-name`, we try building it (we need that flag now).
            #if swift(>=5.5)
            XCTAssertBuilds(path)
            let triple = UserToolchain.default.triple
            let binPath = path.appending(components: ".build", triple.platformBuildPathComponent(), "debug")
#if os(Windows)
            XCTAssertFileExists(binPath.appending(component: "Foo.exe"))
#else
            XCTAssertFileExists(binPath.appending(component: "Foo"))
#endif
            XCTAssertFileExists(binPath.appending(components: "Foo.swiftmodule"))
            #endif
        }
    }

    func testInitPackageLibrary() throws {
        try testWithTemporaryDirectory { tmpPath in
            let fs = localFileSystem
            let path = tmpPath.appending(component: "Foo")
            let name = path.basename
            try fs.createDirectory(path)

            // Create the package
            let initPackage = try InitPackage(
                name: name,
                packageType: .library,
                destinationPath: path,
                fileSystem: localFileSystem
            )
            var progressMessages = [String]()
            initPackage.progressReporter = { message in
                progressMessages.append(message)
            }
            try initPackage.writePackageStructure()

            // Not picky about the specific progress messages, just checking that we got some.
            XCTAssertGreaterThan(progressMessages.count, 0)

            // Verify basic file system content that we expect in the package
            let manifest = path.appending(component: "Package.swift")
            XCTAssertFileExists(manifest)
            let manifestContents: String = try localFileSystem.readFileContents(manifest)
            let version = InitPackage.newPackageToolsVersion
            let versionSpecifier = "\(version.major).\(version.minor)"
            XCTAssertMatch(manifestContents, .prefix("// swift-tools-version:\(version < .v5_4 ? "" : " ")\(versionSpecifier)\n"))

            let readme = path.appending(component: "README.md")
            XCTAssertFileExists(readme)
            let readmeContents: String = try localFileSystem.readFileContents(readme)
            XCTAssertMatch(readmeContents, .prefix("# Foo\n"))

            XCTAssertEqual(try fs.getDirectoryContents(path.appending(component: "Sources").appending(component: "Foo")), ["Foo.swift"])

            let tests = path.appending(component: "Tests")
            XCTAssertEqual(try fs.getDirectoryContents(tests).sorted(), ["FooTests"])

            let testFile = tests.appending(component: "FooTests").appending(component: "FooTests.swift")
            let testFileContents: String = try localFileSystem.readFileContents(testFile)
            XCTAssertTrue(testFileContents.hasPrefix("import XCTest"), """
                          Validates formatting of XCTest source file, in particular that it does not contain leading whitespace:
                          \(testFileContents)
                          """)
            XCTAssertMatch(testFileContents, .contains("func testExample() throws"))

            // Try building it
            XCTAssertBuilds(path)
            let triple = UserToolchain.default.triple
            XCTAssertFileExists(path.appending(components: ".build", triple.platformBuildPathComponent(), "debug", "Foo.swiftmodule"))
        }
    }
    
    func testInitPackageSystemModule() throws {
        try testWithTemporaryDirectory { tmpPath in
            let fs = localFileSystem
            let path = tmpPath.appending(component: "Foo")
            let name = path.basename
            try fs.createDirectory(path)
            
            // Create the package
            let initPackage = try InitPackage(
                name: name,
                packageType: .systemModule,
                destinationPath: path,
                fileSystem: localFileSystem
            )
            var progressMessages = [String]()
            initPackage.progressReporter = { message in
                progressMessages.append(message)
            }
            try initPackage.writePackageStructure()
            
            // Not picky about the specific progress messages, just checking that we got some.
            XCTAssertGreaterThan(progressMessages.count, 0)

            // Verify basic file system content that we expect in the package
            let manifest = path.appending(component: "Package.swift")
            XCTAssertFileExists(manifest)
            let manifestContents: String = try localFileSystem.readFileContents(manifest)
            let version = InitPackage.newPackageToolsVersion
            let versionSpecifier = "\(version.major).\(version.minor)"
            XCTAssertMatch(manifestContents, .prefix("// swift-tools-version:\(version < .v5_4 ? "" : " ")\(versionSpecifier)\n"))
            XCTAssertMatch(manifestContents, .contains(packageWithNameAndDependencies(with: name)))
            XCTAssert(fs.exists(path.appending(component: "README.md")))
            XCTAssert(fs.exists(path.appending(component: "module.modulemap")))
        }
    }

    func testInitManifest() throws {
        try testWithTemporaryDirectory { tmpPath in
            let fs = localFileSystem
            let path = tmpPath.appending(component: "Foo")
            let name = path.basename
            try fs.createDirectory(path)

            // Create the package
            let initPackage = try InitPackage(
                name: name,
                packageType: InitPackage.PackageType.manifest,
                destinationPath: path,
                fileSystem: localFileSystem
            )
            var progressMessages = [String]()
            initPackage.progressReporter = { message in
                progressMessages.append(message)
            }
            try initPackage.writePackageStructure()

            // Not picky about the specific progress messages, just checking that we got some.
            XCTAssertGreaterThan(progressMessages.count, 0)

            // Verify basic file system content that we expect in the package
            let manifest = path.appending(component: "Package.swift")
            XCTAssertFileExists(manifest)
            let manifestContents: String = try localFileSystem.readFileContents(manifest)
            let version = InitPackage.newPackageToolsVersion
            let versionSpecifier = "\(version.major).\(version.minor)"
            XCTAssertMatch(manifestContents, .prefix("// swift-tools-version:\(version < .v5_4 ? "" : " ")\(versionSpecifier)\n"))
        }
    }
    
    // MARK: Special case testing
    
    func testInitPackageNonc99Directory() throws {
        try withTemporaryDirectory(removeTreeOnDeinit: true) { tempDirPath in
            XCTAssertDirectoryExists(tempDirPath)
            
            // Create a directory with non c99name.
            let packageRoot = tempDirPath.appending(component: "some-package")
            let packageName = packageRoot.basename
            try localFileSystem.createDirectory(packageRoot)
            XCTAssertDirectoryExists(packageRoot)
            
            // Create the package
            let initPackage = try InitPackage(
                name: packageName,
                packageType: .library,
                destinationPath: packageRoot,
                fileSystem: localFileSystem
            )
            initPackage.progressReporter = { message in }
            try initPackage.writePackageStructure()

            // Try building it.
            XCTAssertBuilds(packageRoot)
            let triple = UserToolchain.default.triple
            XCTAssertFileExists(packageRoot.appending(components: ".build", triple.platformBuildPathComponent(), "debug", "some_package.swiftmodule"))
        }
    }
    
    func testNonC99NameExecutablePackage() throws {
        try withTemporaryDirectory(removeTreeOnDeinit: true) { tempDirPath in
            XCTAssertDirectoryExists(tempDirPath)
            
            let packageRoot = tempDirPath.appending(component: "Foo")
            try localFileSystem.createDirectory(packageRoot)
            XCTAssertDirectoryExists(packageRoot)
            
            // Create package with non c99name.
            let initPackage = try InitPackage(
                name: "package-name",
                packageType: .executable,
                destinationPath: packageRoot,
                fileSystem: localFileSystem
            )
            try initPackage.writePackageStructure()
            
            #if os(macOS)
              XCTAssertSwiftTest(packageRoot)
            #else
              XCTAssertBuilds(packageRoot)
            #endif
        }
    }

    func testPlatforms() throws {
        try withTemporaryDirectory(removeTreeOnDeinit: true) { tempDirPath in
            var options = InitPackage.InitPackageOptions(packageType: .library)
            options.platforms = [
                .init(platform: .macOS, version: PlatformVersion("10.15")),
                .init(platform: .iOS, version: PlatformVersion("12")),
                .init(platform: .watchOS, version: PlatformVersion("2.1")),
                .init(platform: .tvOS, version: PlatformVersion("999")),
            ]

            let packageRoot = tempDirPath.appending(component: "Foo")
            try localFileSystem.removeFileTree(packageRoot)
            try localFileSystem.createDirectory(packageRoot)

            let initPackage = try InitPackage(
                name: "Foo",
                options: options,
                destinationPath: packageRoot,
                fileSystem: localFileSystem
            )
            try initPackage.writePackageStructure()

            let contents: String = try localFileSystem.readFileContents(packageRoot.appending(component: "Package.swift"))
            XCTAssertMatch(contents, .contains(#"platforms: [.macOS(.v10_15), .iOS(.v12), .watchOS("2.1"), .tvOS("999.0")],"#))
        }
    }

    private func packageWithNameAndDependencies(with name: String) -> String {
        return """
let package = Package(
    name: "\(name)",
    dependencies: [
        // Dependencies declare other packages that this package depends on.
        // .package(url: /* package url */, from: "1.0.0"),
    ]
)
"""
    }
}
