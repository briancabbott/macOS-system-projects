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

import Basics
import Foundation
import TSCBasic

import struct TSCUtility.Triple
#if os(Windows)
private let hostExecutableSuffix = ".exe"
#else
private let hostExecutableSuffix = ""
#endif

// FIXME: This is messy and needs a redesign.
public final class UserToolchain: Toolchain {
    public typealias SwiftCompilers = (compile: AbsolutePath, manifest: AbsolutePath)

    /// The toolchain configuration.
    private let configuration: ToolchainConfiguration

    /// Path of the librarian.
    public let librarianPath: AbsolutePath

    /// Path of the `swiftc` compiler.
    public let swiftCompilerPath: AbsolutePath

    public var extraCCFlags: [String]

    public let extraSwiftCFlags: [String]

    public var extraCPPFlags: [String]

    /// Path of the `swift` interpreter.
    public var swiftInterpreterPath: AbsolutePath {
        return self.swiftCompilerPath.parentDirectory.appending(component: "swift" + hostExecutableSuffix)
    }

    /// The compilation destination object.
    public let destination: Destination

    /// The target triple that should be used for compilation.
    public let triple: Triple

    /// The list of archs to build for.
    public let archs: [String]

    /// Search paths from the PATH environment variable.
    let envSearchPaths: [AbsolutePath]

    /// Only use search paths, do not fall back to `xcrun`.
    let useXcrun: Bool

    private var _clangCompiler: AbsolutePath?

    private let environment: EnvironmentVariables

    /// Returns the runtime library for the given sanitizer.
    public func runtimeLibrary(for sanitizer: Sanitizer) throws -> AbsolutePath {
        // FIXME: This is only for SwiftPM development time support. It is OK
        // for now but we shouldn't need to resolve the symlink.  We need to lay
        // down symlinks to runtimes in our fake toolchain as part of the
        // bootstrap script.
        let swiftCompiler = resolveSymlinks(self.swiftCompilerPath)

        let runtime = swiftCompiler.appending(
            RelativePath("../../lib/swift/clang/lib/darwin/libclang_rt.\(sanitizer.shortName)_osx_dynamic.dylib"))

        // Ensure that the runtime is present.
        guard localFileSystem.exists(runtime) else {
            throw InvalidToolchainDiagnostic("Missing runtime for \(sanitizer) sanitizer")
        }

        return runtime
    }

    // MARK: - private utilities

    private static func lookup(variable: String, searchPaths: [AbsolutePath], environment: EnvironmentVariables) -> AbsolutePath? {
        return lookupExecutablePath(filename: environment[variable], searchPaths: searchPaths)
    }

    private static func getTool(_ name: String, binDir: AbsolutePath) throws -> AbsolutePath {
        let executableName = "\(name)\(hostExecutableSuffix)"
        let toolPath = binDir.appending(component: executableName)
        guard localFileSystem.isExecutableFile(toolPath) else {
            throw InvalidToolchainDiagnostic("could not find \(name) at expected path \(toolPath)")
        }
        return toolPath
    }

    private static func findTool(_ name: String, envSearchPaths: [AbsolutePath], useXcrun: Bool) throws -> AbsolutePath {
        if useXcrun {
#if os(macOS)
            let foundPath = try TSCBasic.Process.checkNonZeroExit(arguments: ["/usr/bin/xcrun", "--find", name]).spm_chomp()
            return try AbsolutePath(validating: foundPath)
#endif
        }

        for folder in envSearchPaths {
            if let toolPath = try? getTool(name, binDir: folder) {
                return toolPath
            }
        }
        throw InvalidToolchainDiagnostic("could not find \(name)")
    }

    // MARK: - public API

    public static func determineLibrarian(triple: Triple, binDir: AbsolutePath,
                                          useXcrun: Bool,
                                          environment: EnvironmentVariables,
                                          searchPaths: [AbsolutePath]) throws
            -> AbsolutePath {
        let variable: String = triple.isDarwin() ? "LIBTOOL" : "AR"
        let tool: String = {
            if triple.isDarwin() { return "libtool" }
            if triple.isWindows() {
                if let librarian: AbsolutePath =
                        UserToolchain.lookup(variable: "AR",
                                             searchPaths: searchPaths,
                                             environment: environment) {
                    return librarian.basename
                }
                // TODO(5719) use `lld-link` if the build requests lld.
                return "link"
            }
            // TODO(compnerd) consider defaulting to `llvm-ar` universally with
            // a fallback to `ar`.
            return triple.isAndroid() ? "llvm-ar" : "ar"
        }()

        if let librarian: AbsolutePath = UserToolchain.lookup(variable: variable,
                                                              searchPaths: searchPaths,
                                                              environment: environment) {
            if localFileSystem.isExecutableFile(librarian) {
                return librarian
            }
        }

        if let librarian = try? UserToolchain.getTool(tool, binDir: binDir) {
            return librarian
        }
        return try UserToolchain.findTool(tool, envSearchPaths: searchPaths, useXcrun: useXcrun)
    }

    /// Determines the Swift compiler paths for compilation and manifest parsing.
    public static func determineSwiftCompilers(binDir: AbsolutePath, useXcrun: Bool, environment: EnvironmentVariables, searchPaths: [AbsolutePath]) throws -> SwiftCompilers {
        func validateCompiler(at path: AbsolutePath?) throws {
            guard let path = path else { return }
            guard localFileSystem.isExecutableFile(path) else {
                throw InvalidToolchainDiagnostic("could not find the `swiftc\(hostExecutableSuffix)` at expected path \(path)")
            }
        }

        let lookup = { UserToolchain.lookup(variable: $0, searchPaths: searchPaths, environment: environment) }
        // Get overrides.
        let SWIFT_EXEC_MANIFEST = lookup("SWIFT_EXEC_MANIFEST")
        let SWIFT_EXEC = lookup("SWIFT_EXEC")

        // Validate the overrides.
        try validateCompiler(at: SWIFT_EXEC)
        try validateCompiler(at: SWIFT_EXEC_MANIFEST)

        // We require there is at least one valid swift compiler, either in the
        // bin dir or SWIFT_EXEC.
        let resolvedBinDirCompiler: AbsolutePath
        if let SWIFT_EXEC = SWIFT_EXEC {
            resolvedBinDirCompiler = SWIFT_EXEC
        } else if let binDirCompiler = try? UserToolchain.getTool("swiftc", binDir: binDir) {
            resolvedBinDirCompiler = binDirCompiler
        } else {
            // Try to lookup swift compiler on the system which is possible when
            // we're built outside of the Swift toolchain.
            resolvedBinDirCompiler = try UserToolchain.findTool("swiftc", envSearchPaths: searchPaths, useXcrun: useXcrun)
        }

        // The compiler for compilation tasks is SWIFT_EXEC or the bin dir compiler.
        // The compiler for manifest is either SWIFT_EXEC_MANIFEST or the bin dir compiler.
        return (SWIFT_EXEC ?? resolvedBinDirCompiler, SWIFT_EXEC_MANIFEST ?? resolvedBinDirCompiler)
    }

    /// Returns the path to clang compiler tool.
    public func getClangCompiler() throws -> AbsolutePath {
        // Check if we already computed.
        if let clang = self._clangCompiler {
            return clang
        }

        // Check in the environment variable first.
        if let toolPath = UserToolchain.lookup(variable: "CC", searchPaths: self.envSearchPaths, environment: environment) {
            self._clangCompiler = toolPath
            return toolPath
        }

        // Then, check the toolchain.
        do {
            if let toolPath = try? UserToolchain.getTool("clang", binDir: self.destination.binDir) {
                self._clangCompiler = toolPath
                return toolPath
            }
        }

        // Otherwise, lookup it up on the system.
        let toolPath = try UserToolchain.findTool("clang", envSearchPaths: self.envSearchPaths, useXcrun: useXcrun)
        self._clangCompiler = toolPath
        return toolPath
    }

    public func _isClangCompilerVendorApple() throws -> Bool? {
        // Assume the vendor is Apple on macOS.
        // FIXME: This might not be the best way to determine this.
#if os(macOS)
        return true
#else
        return false
#endif
    }

    /// Returns the path to lldb.
    public func getLLDB() throws -> AbsolutePath {
        // Look for LLDB next to the compiler first.
        if let lldbPath = try? UserToolchain.getTool("lldb", binDir: self.swiftCompilerPath.parentDirectory) {
            return lldbPath
        }
        // If that fails, fall back to xcrun, PATH, etc.
        return try UserToolchain.findTool("lldb", envSearchPaths: self.envSearchPaths, useXcrun: useXcrun)
    }

    /// Returns the path to llvm-cov tool.
    public func getLLVMCov() throws -> AbsolutePath {
        return try UserToolchain.getTool("llvm-cov", binDir: self.swiftCompilerPath.parentDirectory)
    }

    /// Returns the path to llvm-prof tool.
    public func getLLVMProf() throws -> AbsolutePath {
        return try UserToolchain.getTool("llvm-profdata", binDir: self.swiftCompilerPath.parentDirectory)
    }

    public func getSwiftAPIDigester() throws -> AbsolutePath {
        if let envValue = UserToolchain.lookup(variable: "SWIFT_API_DIGESTER", searchPaths: self.envSearchPaths, environment: environment) {
            return envValue
        }
        return try UserToolchain.getTool("swift-api-digester", binDir: self.swiftCompilerPath.parentDirectory)
    }

    public func getSymbolGraphExtract() throws -> AbsolutePath {
        if let envValue = UserToolchain.lookup(variable: "SWIFT_SYMBOLGRAPH_EXTRACT", searchPaths: self.envSearchPaths, environment: environment) {
            return envValue
        }
        return try UserToolchain.getTool("swift-symbolgraph-extract", binDir: self.swiftCompilerPath.parentDirectory)
    }

    internal static func deriveSwiftCFlags(triple: Triple, destination: Destination, environment: EnvironmentVariables) -> [String] {
        guard let sdk = destination.sdk else {
            if triple.isWindows() {
                // Windows uses a variable named SDKROOT to determine the root of
                // the SDK.  This is not the same value as the SDKROOT parameter
                // in Xcode, however, the value represents a similar concept.
                if let SDKROOT = environment["SDKROOT"], let sdkroot = try? AbsolutePath(validating: SDKROOT) {
                    var runtime: [String] = []
                    var xctest: [String] = []
                    var extraSwiftCFlags: [String] = []

                    if let settings = WindowsSDKSettings(reading: sdkroot.appending(component: "SDKSettings.plist"),
                                                         diagnostics: nil, filesystem: localFileSystem) {
                        switch settings.defaults.runtime {
                        case .multithreadedDebugDLL:
                            runtime = [ "-libc", "MDd" ]
                        case .multithreadedDLL:
                            runtime = [ "-libc", "MD" ]
                        case .multithreadedDebug:
                            runtime = [ "-libc", "MTd" ]
                        case .multithreaded:
                            runtime = [ "-libc", "MT" ]
                        }
                    }

                    // The layout of the SDK is as follows:
                    //
                    // Library/Developer/Platforms/[PLATFORM].platform/Developer/Library/XCTest-[VERSION]/...
                    // Library/Developer/Platforms/[PLATFORM].platform/Developer/SDKs/[PLATFORM].sdk/...
                    //
                    // SDKROOT points to [PLATFORM].sdk
                    let platform = sdkroot.parentDirectory.parentDirectory.parentDirectory

                    if let info = WindowsPlatformInfo(reading: platform.appending(component: "Info.plist"),
                                                      diagnostics: nil, filesystem: localFileSystem) {
                        let installation: AbsolutePath =
                                platform.appending(component: "Developer")
                                        .appending(component: "Library")
                                        .appending(component: "XCTest-\(info.defaults.xctestVersion)")

                        xctest = [
                            "-I", AbsolutePath("usr/lib/swift/windows", relativeTo: installation).pathString,
                            // Migration Path
                            //
                            // Older Swift (<=5.7) installations placed the
                            // XCTest Swift module into the architecture
                            // specified directory.  This was in order to match
                            // the SDK setup.  However, the toolchain finally
                            // gained the ability to consult the architecture
                            // indepndent directory for Swift modules, allowing
                            // the merged swiftmodules.  XCTest followed suit.
                            "-I", AbsolutePath("usr/lib/swift/windows/\(triple.arch)", relativeTo: installation).pathString,
                            "-L", AbsolutePath("usr/lib/swift/windows/\(triple.arch)", relativeTo: installation).pathString,
                        ]

                        // Migration Path
                        //
                        // In order to support multiple parallel installations
                        // of an SDK, we need to ensure that we can have all the
                        // architecture variant libraries available.  Prior to
                        // this getting enabled (~5.7), we always had a singular
                        // installed SDK.  Prefer the new variant which has an
                        // architecture subdirectory in `bin` if available.
                        let implib: AbsolutePath =
                            AbsolutePath("usr/lib/swift/windows/XCTest.lib", relativeTo: installation)
                        if localFileSystem.exists(implib) {
                            xctest.append(contentsOf: ["-L", implib.parentDirectory.pathString])
                        }

                        extraSwiftCFlags = info.defaults.extraSwiftCFlags ??  []
                    }

                    return [ "-sdk", sdkroot.pathString, ] + runtime + xctest + extraSwiftCFlags
                }
            }

            return destination.extraSwiftCFlags
        }

        return (triple.isDarwin() || triple.isAndroid() || triple.isWASI() || triple.isWindows()
                ? ["-sdk", sdk.pathString]
                : [])
        + destination.extraSwiftCFlags
    }

    // MARK: - initializer

    public enum SearchStrategy {
        case `default`
        case custom(searchPaths: [AbsolutePath], useXcrun: Bool = true)
    }

    public init(destination: Destination, environment: EnvironmentVariables = .process(), searchStrategy: SearchStrategy = .default, customLibrariesLocation: ToolchainConfiguration.SwiftPMLibrariesLocation? = nil) throws {
        self.destination = destination
        self.environment = environment

        switch searchStrategy {
        case .default:
            // Get the search paths from PATH.
            self.envSearchPaths = getEnvSearchPaths(
                pathString: environment.path,
                currentWorkingDirectory: localFileSystem.currentWorkingDirectory
            )
            self.useXcrun = true
        case .custom(let searchPaths, let useXcrun):
            self.envSearchPaths = searchPaths
            self.useXcrun = useXcrun
        }

        // Get the binDir from destination.
        let binDir = destination.binDir

        let swiftCompilers = try UserToolchain.determineSwiftCompilers(binDir: binDir, useXcrun: useXcrun, environment: environment, searchPaths: envSearchPaths)
        self.swiftCompilerPath = swiftCompilers.compile
        self.archs = destination.archs

        // Use the triple from destination or compute the host triple using swiftc.
        var triple = destination.target ?? Triple.getHostTriple(usingSwiftCompiler: swiftCompilers.compile)

        self.librarianPath = try UserToolchain.determineLibrarian(triple: triple, binDir: binDir, useXcrun: useXcrun, environment: environment, searchPaths: envSearchPaths)

        // Change the triple to the specified arch if there's exactly one of them.
        // The Triple property is only looked at by the native build system currently.
        if archs.count == 1 {
            let components = triple.tripleString.drop(while: { $0 != "-" })
            triple = try Triple(archs[0] + components)
        }

        self.triple = triple

        self.extraSwiftCFlags = Self.deriveSwiftCFlags(triple: triple, destination: destination, environment: environment)

        if let sdk = destination.sdk {
            self.extraCCFlags = [
                triple.isDarwin() ? "-isysroot" : "--sysroot", sdk.pathString
            ] + destination.extraCCFlags

            self.extraCPPFlags = destination.extraCPPFlags
        } else {
            self.extraCCFlags = destination.extraCCFlags
            self.extraCPPFlags = destination.extraCPPFlags
        }

        if triple.isWindows() {
            if let SDKROOT = environment["SDKROOT"], let root = try? AbsolutePath(validating: SDKROOT) {
                if let settings = WindowsSDKSettings(reading: root.appending(component: "SDKSettings.plist"),
                                                     diagnostics: nil, filesystem: localFileSystem) {
                    switch settings.defaults.runtime {
                    case .multithreadedDebugDLL:
                        // Defines _DEBUG, _MT, and _DLL
                        // Linker uses MSVCRTD.lib
                        self.extraCCFlags += ["-D_DEBUG", "-D_MT", "-D_DLL", "-Xclang", "--dependent-lib=msvcrtd"]

                    case .multithreadedDLL:
                        // Defines _MT, and _DLL
                        // Linker uses MSVCRT.lib
                        self.extraCCFlags += ["-D_MT", "-D_DLL", "-Xclang", "--dependent-lib=msvcrt"]

                    case .multithreadedDebug:
                        // Defines _DEBUG, and _MT
                        // Linker uses LIBCMTD.lib
                        self.extraCCFlags += ["-D_DEBUG", "-D_MT", "-Xclang", "--dependent-lib=libcmtd"]

                    case .multithreaded:
                        // Defines _MT
                        // Linker uses LIBCMT.lib
                        self.extraCCFlags += ["-D_MT", "-Xclang", "--dependent-lib=libcmt"]
                    }
                }
            }
        }

        let swiftPMLibrariesLocation = try customLibrariesLocation ?? Self.deriveSwiftPMLibrariesLocation(swiftCompilerPath: swiftCompilerPath, destination: destination, environment: environment)

        let xctestPath: AbsolutePath?
        if case let .custom(_, useXcrun) = searchStrategy, !useXcrun {
            xctestPath = nil
        } else {
            xctestPath = try Self.deriveXCTestPath(destination: self.destination, triple: triple, environment: environment)
        }

        self.configuration = .init(
            librarianPath: librarianPath,
            swiftCompilerPath: swiftCompilers.manifest,
            swiftCompilerFlags: self.extraSwiftCFlags,
            swiftCompilerEnvironment: environment,
            swiftPMLibrariesLocation: swiftPMLibrariesLocation,
            sdkRootPath: self.destination.sdk,
            xctestPath: xctestPath
        )
    }

    private static func deriveSwiftPMLibrariesLocation(
        swiftCompilerPath: AbsolutePath,
        destination: Destination,
        environment: EnvironmentVariables
    ) throws -> ToolchainConfiguration.SwiftPMLibrariesLocation? {
        // Look for an override in the env.
        if let pathEnvVariable = environment["SWIFTPM_CUSTOM_LIBS_DIR"] ?? environment["SWIFTPM_PD_LIBS"] {
            if environment["SWIFTPM_PD_LIBS"] != nil {
                print("SWIFTPM_PD_LIBS was deprecated in favor of SWIFTPM_CUSTOM_LIBS_DIR")
            }
            // We pick the first path which exists in an environment variable
            // delimited by the platform specific string separator.
#if os(Windows)
            let separator: Character = ";"
#else
            let separator: Character = ":"
#endif
            let paths = pathEnvVariable.split(separator: separator).map(String.init)
            for pathString in paths {
                if let path = try? AbsolutePath(validating: pathString), localFileSystem.exists(path) {
                    // we found the custom one
                    return .init(root: path)
                }
            }

            // fail if custom one specified but not found
            throw InternalError("Couldn't find the custom libraries location defined by SWIFTPM_CUSTOM_LIBS_DIR / SWIFTPM_PD_LIBS: \(pathEnvVariable)")
        }

        // FIXME: the following logic is pretty fragile, but has always been this way
        // an alternative cloud be to force explicit locations to always be set explicitly when running in XCode/SwiftPM
        // debug and assert if not set but we detect that we are in this mode

        let applicationPath = destination.binDir

        // this is the normal case when using the toolchain
        let librariesPath = applicationPath.parentDirectory.appending(components: "lib", "swift", "pm")
        if localFileSystem.exists(librariesPath) {
            return .init(root: librariesPath)
        }

        // this tests if we are debugging / testing SwiftPM with Xcode
        let manifestFrameworksPath = applicationPath.appending(components: "PackageFrameworks", "PackageDescription.framework")
        let pluginFrameworksPath = applicationPath.appending(components: "PackageFrameworks", "PackagePlugin.framework")
        if localFileSystem.exists(manifestFrameworksPath) && localFileSystem.exists(pluginFrameworksPath) {
            return .init(
                manifestLibraryPath: manifestFrameworksPath,
                pluginLibraryPath: pluginFrameworksPath
            )
        }

        // this tests if we are debugging / testing SwiftPM with SwiftPM
        if localFileSystem.exists(applicationPath.appending(component: "swift-package")) {
            return .init(
                manifestLibraryPath: applicationPath,
                pluginLibraryPath: applicationPath
            )
        }

        // we are using a SwiftPM outside a toolchain, use the compiler path to compute the location
        return .init(swiftCompilerPath: swiftCompilerPath)
    }

    // TODO: We should have some general utility to find tools.
    private static func deriveXCTestPath(destination: Destination, triple: Triple, environment: EnvironmentVariables) throws -> AbsolutePath? {
        if triple.isDarwin() {
            // XCTest is optional on macOS, for example when Xcode is not installed
            let xctestFindArgs = ["/usr/bin/xcrun", "--sdk", "macosx", "--find", "xctest"]
            if let path = try? TSCBasic.Process.checkNonZeroExit(arguments: xctestFindArgs, environment: environment).spm_chomp() {
                return try AbsolutePath(validating: path)
            }
        } else if triple.isWindows() {
            let sdkroot: AbsolutePath

            if let sdk = destination.sdk {
                sdkroot = sdk
            } else if let SDKROOT = environment["SDKROOT"], let sdk = try? AbsolutePath(validating: SDKROOT) {
                sdkroot = sdk
            } else {
                return .none
            }

            // The layout of the SDK is as follows:
            //
            // Library/Developer/Platforms/[PLATFORM].platform/Developer/Library/XCTest-[VERSION]/...
            // Library/Developer/Platforms/[PLATFORM].platform/Developer/SDKs/[PLATFORM].sdk/...
            //
            // SDKROOT points to [PLATFORM].sdk
            let platform = sdkroot.parentDirectory.parentDirectory.parentDirectory

            if let info = WindowsPlatformInfo(reading: platform.appending(component: "Info.plist"),
                                              diagnostics: nil, filesystem: localFileSystem) {
                let xctest: AbsolutePath =
                        platform.appending(component: "Developer")
                                .appending(component: "Library")
                                .appending(component: "XCTest-\(info.defaults.xctestVersion)")

                // Migration Path
                //
                // In order to support multiple parallel installations of an
                // SDK, we need to ensure that we can have all the architecture
                // variant libraries available.  Prior to this getting enabled
                // (~5.7), we always had a singular installed SDK.  Prefer the
                // new variant which has an architecture subdirectory in `bin`
                // if available.
                switch triple.arch {
                case .x86_64, .x86_64h:
                    let path: AbsolutePath =
                        xctest.appending(component: "usr")
                              .appending(component: "bin64")
                    if localFileSystem.exists(path) {
                        return path
                    }

                case .i686:
                    let path: AbsolutePath =
                        xctest.appending(component: "usr")
                              .appending(component: "bin32")
                    if localFileSystem.exists(path) {
                        return path
                    }

                case .armv7:
                    let path: AbsolutePath =
                        xctest.appending(component: "usr")
                              .appending(component: "bin32a")
                    if localFileSystem.exists(path) {
                        return path
                    }

                case .arm64:
                    let path: AbsolutePath =
                        xctest.appending(component: "usr")
                              .appending(component: "bin64a")
                    if localFileSystem.exists(path) {
                        return path
                    }

                default:
                    // Fallback to the old-style layout.  We should really
                    // report an error in this case - this architecture is
                    // unavailable.
                    break
                }

                // Assume that we are in the old-style layout.
                return xctest.appending(component: "usr")
                             .appending(component: "bin")
            }
        }
        return .none
    }

    public var sdkRootPath: AbsolutePath? {
        return configuration.sdkRootPath
    }

    public var swiftCompilerEnvironment: EnvironmentVariables {
        return configuration.swiftCompilerEnvironment
    }

    public var swiftCompilerFlags: [String] {
        return configuration.swiftCompilerFlags
    }

    public var swiftCompilerPathForManifests: AbsolutePath {
        return configuration.swiftCompilerPath
    }

    public var swiftPMLibrariesLocation: ToolchainConfiguration.SwiftPMLibrariesLocation {
        return configuration.swiftPMLibrariesLocation
    }

    public var xctestPath: AbsolutePath? {
        return configuration.xctestPath
    }
}
