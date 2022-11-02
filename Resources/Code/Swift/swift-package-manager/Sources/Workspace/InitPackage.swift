//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift open source project
//
// Copyright (c) 2014-2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import Basics
import TSCBasic
import PackageModel

/// Create an initial template package.
public final class InitPackage {
    /// The tool version to be used for new packages.
    public static let newPackageToolsVersion = ToolsVersion.current

    /// Options for the template package.
    public struct InitPackageOptions {
        /// The type of package to create.
        public var packageType: PackageType

        /// The list of platforms in the manifest.
        ///
        /// Note: This should only contain Apple platforms right now.
        public var platforms: [SupportedPlatform]

        public init(
            packageType: PackageType,
            platforms: [SupportedPlatform] = []
        ) {
            self.packageType = packageType
            self.platforms = platforms
        }
    }

    /// Represents a package type for the purposes of initialization.
    public enum PackageType: String, CustomStringConvertible {
        case empty = "empty"
        case library = "library"
        case executable = "executable"
        case systemModule = "system-module"
        case manifest = "manifest"
        case `extension` = "extension"

        public var description: String {
            return rawValue
        }
    }

    /// A block that will be called to report progress during package creation
    public var progressReporter: ((String) -> Void)?

    /// The file system to use
    let fileSystem: FileSystem

    /// Where to create the new package
    let destinationPath: AbsolutePath

    /// The type of package to create.
    var packageType: PackageType { options.packageType }

    /// The options for package to create.
    let options: InitPackageOptions

    /// The name of the package to create.
    let pkgname: String

    /// The name of the target to create.
    var moduleName: String

    /// The name of the type to create (within the package).
    var typeName: String {
        return moduleName
    }

    /// Create an instance that can create a package with given arguments.
    public convenience init(
        name: String,
        packageType: PackageType,
        destinationPath: AbsolutePath,
        fileSystem: FileSystem
    ) throws {
        try self.init(
            name: name,
            options: InitPackageOptions(packageType: packageType),
            destinationPath: destinationPath,
            fileSystem: fileSystem
        )
    }

    /// Create an instance that can create a package with given arguments.
    public init(
        name: String,
        options: InitPackageOptions,
        destinationPath: AbsolutePath,
        fileSystem: FileSystem
    ) throws {
        self.options = options
        self.pkgname = name
        self.moduleName = name.spm_mangledToC99ExtendedIdentifier()
        self.destinationPath = destinationPath
        self.fileSystem = fileSystem
    }

    /// Actually creates the new package at the destinationPath
    public func writePackageStructure() throws {
        progressReporter?("Creating \(packageType) package: \(pkgname)")

        // FIXME: We should form everything we want to write, then validate that
        // none of it exists, and then act.
        try writeManifestFile()

        if packageType == .manifest {
            return
        }

        try writeREADMEFile()
        try writeGitIgnore()
        try writeSources()
        try writeModuleMap()
        try writeTests()
    }

    private func writePackageFile(_ path: AbsolutePath, body: (OutputByteStream) -> Void) throws {
        progressReporter?("Creating \(path.relative(to: destinationPath))")
        try self.fileSystem.writeFileContents(path, body: body)
    }

    private func writeManifestFile() throws {
        let manifest = destinationPath.appending(component: Manifest.filename)
        guard self.fileSystem.exists(manifest) == false else {
            throw InitError.manifestAlreadyExists
        }

        try writePackageFile(manifest) { stream in
            stream <<< """
                // The swift-tools-version declares the minimum version of Swift required to build this package.

                import PackageDescription

                let package = Package(

                """

            var pkgParams = [String]()
            pkgParams.append("""
                    name: "\(pkgname)"
                """)

            var platformsParams = [String]()
            for supportedPlatform in options.platforms {
                let version = supportedPlatform.version
                let platform = supportedPlatform.platform

                var param = ".\(platform.manifestName)("
                if supportedPlatform.isManifestAPIAvailable {
                    if version.minor > 0 {
                        param += ".v\(version.major)_\(version.minor)"
                    } else {
                        param += ".v\(version.major)"
                    }
                } else {
                    param += "\"\(version.versionString)\""
                }
                param += ")"

                platformsParams.append(param)
            }
            if !options.platforms.isEmpty {
                pkgParams.append("""
                        platforms: [\(platformsParams.joined(separator: ", "))]
                    """)
            }

            if packageType == .library || packageType == .manifest {
                pkgParams.append("""
                    products: [
                        // Products define the executables and libraries a package produces, and make them visible to other packages.
                        .library(
                            name: "\(pkgname)",
                            targets: ["\(pkgname)"]),
                    ]
                """)
            }

            pkgParams.append("""
                    dependencies: [
                        // Dependencies declare other packages that this package depends on.
                        // .package(url: /* package url */, from: "1.0.0"),
                    ]
                """)

            if packageType == .library || packageType == .executable || packageType == .manifest {
                var param = ""

                param += """
                    targets: [
                        // Targets are the basic building blocks of a package. A target can define a module or a test suite.
                        // Targets can depend on other targets in this package, and on products in packages this package depends on.

                """
                if packageType == .executable {
                    param += """
                            .executableTarget(
                    """
                } else {
                    param += """
                            .target(
                    """
                }
                param += """

                            name: "\(pkgname)",
                            dependencies: []),
                        .testTarget(
                            name: "\(pkgname)Tests",
                            dependencies: ["\(pkgname)"]),
                    ]
                """

                pkgParams.append(param)
            }

            stream <<< pkgParams.joined(separator: ",\n") <<< "\n)\n"
        }

        // Create a tools version with current version but with patch set to zero.
        // We do this to avoid adding unnecessary constraints to patch versions, if
        // the package really needs it, they should add it manually.
        let version = InitPackage.newPackageToolsVersion.zeroedPatch

        // Write the current tools version.
        try ToolsVersionSpecificationWriter.rewriteSpecification(
            manifestDirectory: manifest.parentDirectory,
            toolsVersion: version,
            fileSystem: self.fileSystem
        )
    }

    private func writeREADMEFile() throws {
        let readme = destinationPath.appending(component: "README.md")
        guard self.fileSystem.exists(readme) == false else {
            return
        }

        try writePackageFile(readme) { stream in
            stream <<< """
                # \(pkgname)

                A description of this package.

                """
        }
    }

    private func writeGitIgnore() throws {
        let gitignore = destinationPath.appending(component: ".gitignore")
        guard self.fileSystem.exists(gitignore) == false else {
            return
        }

        try writePackageFile(gitignore) { stream in
            stream <<< """
                .DS_Store
                /.build
                /Packages
                /*.xcodeproj
                xcuserdata/
                DerivedData/
                .swiftpm/config/registries.json
                .swiftpm/xcode/package.xcworkspace/contents.xcworkspacedata
                .netrc

                """
        }
    }

    private func writeSources() throws {
        if packageType == .systemModule || packageType == .manifest {
            return
        }
        let sources = destinationPath.appending(component: "Sources")
        guard self.fileSystem.exists(sources) == false else {
            return
        }
        progressReporter?("Creating \(sources.relative(to: destinationPath))/")
        try makeDirectories(sources)

        if packageType == .empty {
            return
        }

        let moduleDir = sources.appending(component: "\(pkgname)")
        try makeDirectories(moduleDir)

        let sourceFileName = "\(typeName).swift"
        let sourceFile = AbsolutePath(sourceFileName, relativeTo: moduleDir)

        let content: String
        switch packageType {
        case .library:
            content = """
                public struct \(typeName) {
                    public private(set) var text = "Hello, World!"

                    public init() {
                    }
                }

                """
        case .executable:
            content = """
                @main
                public struct \(typeName) {
                    public private(set) var text = "Hello, World!"

                    public static func main() {
                        print(\(typeName)().text)
                    }
                }

                """
        case .systemModule, .empty, .manifest, .`extension`:
            throw InternalError("invalid packageType \(packageType)")
        }

        try writePackageFile(sourceFile) { stream in
            stream.write(content)
        }
    }

    private func writeModuleMap() throws {
        if packageType != .systemModule {
            return
        }
        let modulemap = destinationPath.appending(component: "module.modulemap")
        guard self.fileSystem.exists(modulemap) == false else {
            return
        }

        try writePackageFile(modulemap) { stream in
            stream <<< """
                module \(moduleName) [system] {
                  header "/usr/include/\(moduleName).h"
                  link "\(moduleName)"
                  export *
                }

                """
        }
    }

    private func writeTests() throws {
        if packageType == .systemModule {
            return
        }
        let tests = destinationPath.appending(component: "Tests")
        guard self.fileSystem.exists(tests) == false else {
            return
        }
        progressReporter?("Creating \(tests.relative(to: destinationPath))/")
        try makeDirectories(tests)

        switch packageType {
        case .systemModule, .empty, .manifest, .`extension`: break
        case .library, .executable:
            try writeTestFileStubs(testsPath: tests)
        }
    }

    private func writeLibraryTestsFile(_ path: AbsolutePath) throws {
        try writePackageFile(path) { stream in
            stream <<< """
                import XCTest
                @testable import \(moduleName)

                final class \(moduleName)Tests: XCTestCase {
                    func testExample() throws {
                        // This is an example of a functional test case.
                        // Use XCTAssert and related functions to verify your tests produce the correct
                        // results.
                        XCTAssertEqual(\(typeName)().text, "Hello, World!")
                    }
                }

                """
        }
    }

    private func writeExecutableTestsFile(_ path: AbsolutePath) throws {
        try writePackageFile(path) { stream in
            stream <<< """
                import XCTest
                @testable import \(moduleName)

                final class \(moduleName)Tests: XCTestCase {
                    func testExample() throws {
                        // This is an example of a functional test case.
                        // Use XCTAssert and related functions to verify your tests produce the correct
                        // results.
                        XCTAssertEqual(\(typeName)().text, "Hello, World!")
                    }
                }

                """
        }
    }

    private func writeTestFileStubs(testsPath: AbsolutePath) throws {
        let testModule = AbsolutePath(pkgname + Target.testModuleNameSuffix, relativeTo: testsPath)
        progressReporter?("Creating \(testModule.relative(to: destinationPath))/")
        try makeDirectories(testModule)

        let testClassFile = AbsolutePath("\(moduleName)Tests.swift", relativeTo: testModule)
        switch packageType {
        case .systemModule, .empty, .manifest, .`extension`: break
        case .library:
            try writeLibraryTestsFile(testClassFile)
        case .executable:
            try writeExecutableTestsFile(testClassFile)
        }
    }
}

// Private helpers

private enum InitError: Swift.Error {
    case manifestAlreadyExists
}

extension InitError: CustomStringConvertible {
    var description: String {
        switch self {
        case .manifestAlreadyExists:
            return "a manifest file already exists in this directory"
        }
    }
}

extension PackageModel.Platform {
    var manifestName: String {
        switch self {
        case .macOS:
            return "macOS"
        case .macCatalyst:
            return "macCatalyst"
        case .iOS:
            return "iOS"
        case .tvOS:
            return "tvOS"
        case .watchOS:
            return "watchOS"
        case .driverKit:
            return "DriverKit"
        default:
            fatalError("unexpected manifest name call for platform \(self)")
        }
    }
}

extension SupportedPlatform {
    var isManifestAPIAvailable: Bool {
        if platform == .macOS && self.version.major == 10 {
            guard self.version.patch == 0 else {
                return false
            }
        } else if [Platform.macOS, .macCatalyst, .iOS, .watchOS, .tvOS, .driverKit].contains(platform) {
            guard self.version.minor == 0, self.version.patch == 0 else {
                return false
            }
        } else {
            return false
        }

        switch platform {
        case .macOS where version.major == 10:
            return (10...15).contains(version.minor)
        case .macOS:
            return (11...11).contains(version.major)
        case .macCatalyst:
            return (13...14).contains(version.major)
        case .iOS:
            return (8...14).contains(version.major)
        case .tvOS:
            return (9...14).contains(version.major)
        case .watchOS:
            return (2...7).contains(version.major)
        case .driverKit:
            return (19...20).contains(version.major)

        default:
            return false
        }
    }
}
