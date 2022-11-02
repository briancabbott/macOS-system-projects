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

import class Foundation.ProcessInfo

import TSCBasic
import PackageModel
import PackageGraph

import struct TSCUtility.BuildFlags
import struct TSCUtility.Triple

public struct BuildParameters: Encodable {
    /// Mode for the indexing-while-building feature.
    public enum IndexStoreMode: String, Encodable {
        /// Index store should be enabled.
        case on
        /// Index store should be disabled.
        case off
        /// Index store should be enabled in debug configuration.
        case auto
    }

    /// Represents the debugging strategy.
    ///
    /// Swift binaries requires the swiftmodule files in order for lldb to work.
    /// On Darwin, linker can directly take the swiftmodule file path using the
    /// -add_ast_path flag. On other platforms, we convert the swiftmodule into
    /// an object file using Swift's modulewrap tool.
    public enum DebuggingStrategy {
        case swiftAST
        case modulewrap
    }

    /// Represents the test product style.
    public enum TestProductStyle: Encodable {
        /// Test product is a loadable bundle. This style is used on Darwin platforms and, for XCTest tests, relies on the Objective-C
        /// runtime to automatically discover all tests.
        case loadableBundle

        /// Test product is an executable which serves as the testing entry point. This style is used on non-Darwin platforms and,
        /// for XCTests, relies on the testing entry point file to indicate which tests to run. By default, the test entry point file is
        /// synthesized automatically, and uses indexer data to locate all tests and run them. But the entry point may be customized
        /// in one of two ways: if a path to a test entry point file was explicitly passed via the
        /// `--experimental-test-entry-point-path <file>` option, that file is used, otherwise if an `XCTMain.swift`
        /// (formerly `LinuxMain.swift`) file is located in the package, it is used.
        ///
        /// - Parameter explicitlyEnabledDiscovery: Whether test discovery generation was forced by passing
        ///   `--enable-test-discovery`, overriding any custom test entry point file specified via other CLI options or located in
        ///   the package.
        /// - Parameter explicitlySpecifiedPath: The path to the test entry point file, if one was specified explicitly via
        ///   `--experimental-test-entry-point-path <file>`.
        case entryPointExecutable(
            explicitlyEnabledDiscovery: Bool,
            explicitlySpecifiedPath: AbsolutePath?
        )

        /// Whether this test product style requires additional, derived test targets, i.e. there must be additional test targets, beyond those
        /// listed explicitly in the package manifest, created in order to add additional behavior (such as entry point logic).
        public var requiresAdditionalDerivedTestTargets: Bool {
            switch self {
            case .loadableBundle:
                return false
            case .entryPointExecutable:
                return true
            }
        }

        /// The explicitly-specified entry point file path, if this style of test product supports it and a path was specified.
        public var explicitlySpecifiedEntryPointPath: AbsolutePath? {
            switch self {
            case .loadableBundle:
                return nil
            case .entryPointExecutable(explicitlyEnabledDiscovery: _, explicitlySpecifiedPath: let entryPointPath):
                return entryPointPath
            }
        }

        public enum DiscriminatorKeys: String, Codable {
            case loadableBundle
            case entryPointExecutable
        }

        public enum CodingKeys: CodingKey {
            case _case
            case explicitlyEnabledDiscovery
            case explicitlySpecifiedPath
        }

        public func encode(to encoder: Encoder) throws {
            var container = encoder.container(keyedBy: CodingKeys.self)
            switch self {
            case .loadableBundle:
                try container.encode(DiscriminatorKeys.loadableBundle, forKey: ._case)
            case .entryPointExecutable(let explicitlyEnabledDiscovery, let explicitlySpecifiedPath):
                try container.encode(DiscriminatorKeys.entryPointExecutable, forKey: ._case)
                try container.encode(explicitlyEnabledDiscovery, forKey: .explicitlyEnabledDiscovery)
                try container.encode(explicitlySpecifiedPath, forKey: .explicitlySpecifiedPath)
            }
        }
    }

    /// A mode for explicit import checking
    public enum TargetDependencyImportCheckingMode : Codable {
        case none
        case warn
        case error
    }

    /// The path to the data directory.
    public var dataPath: AbsolutePath

    /// The build configuration.
    public var configuration: BuildConfiguration

    /// The toolchain.
    public var toolchain: Toolchain { _toolchain.toolchain }
    private let _toolchain: _Toolchain

    /// Host triple.
    public var hostTriple: Triple

    /// Destination triple.
    public var triple: Triple

    /// The architectures to build for.
    public var archs: [String]

    /// Extra build flags.
    public var flags: BuildFlags

    /// How many jobs should llbuild and the Swift compiler spawn
    public var jobs: UInt32

    /// If should link the Swift stdlib statically.
    public var shouldLinkStaticSwiftStdlib: Bool

    /// Which compiler sanitizers should be enabled
    public var sanitizers: EnabledSanitizers

    /// If should enable llbuild manifest caching.
    public var shouldEnableManifestCaching: Bool

    /// The mode to use for indexing-while-building feature.
    public var indexStoreMode: IndexStoreMode

    /// Whether to enable code coverage.
    public var enableCodeCoverage: Bool

    /// Whether to enable generation of `.swiftinterface` files alongside
    /// `.swiftmodule`s.
    public var enableParseableModuleInterfaces: Bool

    /// Emit Swift module separately from object files. This can enable more parallelism
    /// since downstream targets can begin compiling without waiting for the entire
    /// module to finish building.
    public var emitSwiftModuleSeparately: Bool

    /// Whether to use the integrated Swift driver rather than shelling out
    /// to a separate process.
    public var useIntegratedSwiftDriver: Bool

    /// Whether to use the explicit module build flow (with the integrated driver)
    public var useExplicitModuleBuild: Bool

    /// A flag that inidcates this build should check whether targets only import
    /// their explicitly-declared dependencies
    public var explicitTargetDependencyImportCheckingMode: TargetDependencyImportCheckingMode

    /// Whether to create dylibs for dynamic library products.
    public var shouldCreateDylibForDynamicProducts: Bool

    /// Whether to enable the entry-point-function-name feature.
    public var canRenameEntrypointFunctionName: Bool

    /// The current build environment.
    public var buildEnvironment: BuildEnvironment {
        BuildEnvironment(platform: currentPlatform, configuration: configuration)
    }

    /// The current platform we're building for.
    var currentPlatform: PackageModel.Platform {
        if self.triple.isDarwin() {
            return .macOS
        } else if self.triple.isAndroid() {
            return .android
        } else if self.triple.isWASI() {
            return .wasi
        } else if self.triple.isWindows() {
            return .windows
        } else if self.triple.isOpenBSD() {
            return .openbsd
        } else {
            return .linux
        }
    }

    /// Whether the Xcode build system is used.
    public var isXcodeBuildSystemEnabled: Bool

    /// Extra arguments to pass when using xcbuild.
    public var xcbuildFlags: [String]

    // Whether building for testability is enabled.
    public var enableTestability: Bool

    /// The style of test product to produce.
    public var testProductStyle: TestProductStyle

    /// Whether to disable dead code stripping by the linker
    public var linkerDeadStrip: Bool

    public var colorizedOutput: Bool

    public var verboseOutput: Bool

    public init(
        dataPath: AbsolutePath,
        configuration: BuildConfiguration,
        toolchain: Toolchain,
        hostTriple: Triple? = nil,
        destinationTriple: Triple? = nil,
        archs: [String] = [],
        flags: BuildFlags,
        xcbuildFlags: [String] = [],
        jobs: UInt32 = UInt32(ProcessInfo.processInfo.activeProcessorCount),
        shouldLinkStaticSwiftStdlib: Bool = false,
        shouldEnableManifestCaching: Bool = false,
        canRenameEntrypointFunctionName: Bool = false,
        shouldCreateDylibForDynamicProducts: Bool = true,
        sanitizers: EnabledSanitizers = EnabledSanitizers(),
        enableCodeCoverage: Bool = false,
        indexStoreMode: IndexStoreMode = .auto,
        enableParseableModuleInterfaces: Bool = false,
        emitSwiftModuleSeparately: Bool = false,
        useIntegratedSwiftDriver: Bool = false,
        useExplicitModuleBuild: Bool = false,
        isXcodeBuildSystemEnabled: Bool = false,
        enableTestability: Bool? = nil,
        forceTestDiscovery: Bool = false,
        testEntryPointPath: AbsolutePath? = nil,
        explicitTargetDependencyImportCheckingMode: TargetDependencyImportCheckingMode = .none,
        linkerDeadStrip: Bool = true,
        colorizedOutput: Bool = false,
        verboseOutput: Bool = false
    ) {
        let triple = destinationTriple ?? .getHostTriple(usingSwiftCompiler: toolchain.swiftCompilerPath)

        self.dataPath = dataPath
        self.configuration = configuration
        self._toolchain = _Toolchain(toolchain: toolchain)
        self.hostTriple = hostTriple ?? .getHostTriple(usingSwiftCompiler: toolchain.swiftCompilerPath)
        self.triple = triple
        self.archs = archs
        self.flags = flags
        self.xcbuildFlags = xcbuildFlags
        self.jobs = jobs
        self.shouldLinkStaticSwiftStdlib = shouldLinkStaticSwiftStdlib
        self.shouldEnableManifestCaching = shouldEnableManifestCaching
        self.shouldCreateDylibForDynamicProducts = shouldCreateDylibForDynamicProducts
        self.canRenameEntrypointFunctionName = canRenameEntrypointFunctionName
        self.sanitizers = sanitizers
        self.enableCodeCoverage = enableCodeCoverage
        self.indexStoreMode = indexStoreMode
        self.enableParseableModuleInterfaces = enableParseableModuleInterfaces
        self.emitSwiftModuleSeparately = emitSwiftModuleSeparately
        self.useIntegratedSwiftDriver = useIntegratedSwiftDriver
        self.useExplicitModuleBuild = useExplicitModuleBuild
        self.isXcodeBuildSystemEnabled = isXcodeBuildSystemEnabled
        // decide on testability based on debug/release config
        // the goals of this being based on the build configuration is
        // that `swift build` followed by a `swift test` will need to do minimal rebuilding
        // given that the default configuration for `swift build` is debug
        // and that `swift test` normally requires building with testable enabled.
        // when building and testing in release mode, one can use the '--disable-testable-imports' flag
        // to disable testability in `swift test`, but that requires that the tests do not use the testable imports feature
        self.enableTestability = enableTestability ?? (.debug == configuration)
        self.testProductStyle = triple.isDarwin() ? .loadableBundle : .entryPointExecutable(
            explicitlyEnabledDiscovery: forceTestDiscovery,
            explicitlySpecifiedPath: testEntryPointPath
        )
        self.explicitTargetDependencyImportCheckingMode = explicitTargetDependencyImportCheckingMode
        self.linkerDeadStrip = linkerDeadStrip
        self.colorizedOutput = colorizedOutput
        self.verboseOutput = verboseOutput
    }

    /// The path to the build directory (inside the data directory).
    public var buildPath: AbsolutePath {
        if isXcodeBuildSystemEnabled {
            return dataPath.appending(components: "Products", configuration.dirname.capitalized)
        } else {
            return dataPath.appending(component: configuration.dirname)
        }
    }

    /// The path to the index store directory.
    public var indexStore: AbsolutePath {
        assert(indexStoreMode != .off, "index store is disabled")
        return buildPath.appending(components: "index", "store")
    }

    /// The path to the code coverage directory.
    public var codeCovPath: AbsolutePath {
        return buildPath.appending(component: "codecov")
    }

    /// The path to the code coverage profdata file.
    public var codeCovDataFile: AbsolutePath {
        return codeCovPath.appending(component: "default.profdata")
    }

    public var llbuildManifest: AbsolutePath {
        return dataPath.appending(components: "..", configuration.dirname + ".yaml")
    }

    public var pifManifest: AbsolutePath {
        return dataPath.appending(components: "..", "manifest.pif")
    }

    public var buildDescriptionPath: AbsolutePath {
        return buildPath.appending(components: "description.json")
    }

    /// The debugging strategy according to the current build parameters.
    public var debuggingStrategy: DebuggingStrategy? {
        guard configuration == .debug else {
            return nil
        }

        if triple.isDarwin() {
            return .swiftAST
        }
        return .modulewrap
    }

    /// Returns the path to the binary of a product for the current build parameters.
    public func binaryPath(for product: ResolvedProduct) -> AbsolutePath {
        return buildPath.appending(binaryRelativePath(for: product))
    }

    /// Returns the path to the binary of a product for the current build parameters, relative to the build directory.
    public func binaryRelativePath(for product: ResolvedProduct) -> RelativePath {
        switch product.type {
        case .executable, .snippet:
            return RelativePath("\(product.name)\(triple.executableExtension)")
        case .library(.static):
            return RelativePath("lib\(product.name)\(triple.staticLibraryExtension)")
        case .library(.dynamic):
            return RelativePath("\(triple.dynamicLibraryPrefix)\(product.name)\(triple.dynamicLibraryExtension)")
        case .library(.automatic), .plugin:
            fatalError()
        case .test:
            guard !triple.isWASI() else {
                return RelativePath("\(product.name).wasm")
            }

            let base = "\(product.name).xctest"
            if triple.isDarwin() {
                return RelativePath("\(base)/Contents/MacOS/\(product.name)")
            } else {
                return RelativePath(base)
            }
        }
    }
}

/// A shim struct for toolchain so we can encode it without having to write encode(to:) for
/// entire BuildParameters by hand.
private struct _Toolchain: Encodable {
    let toolchain: Toolchain

    enum CodingKeys: String, CodingKey {
        case swiftCompiler
        case clangCompiler
        case extraCCFlags
        case extraSwiftCFlags
        case extraCPPFlags
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(toolchain.swiftCompilerPath, forKey: .swiftCompiler)
        try container.encode(toolchain.getClangCompiler(), forKey: .clangCompiler)

        try container.encode(toolchain.extraCCFlags, forKey: .extraCCFlags)
        try container.encode(toolchain.extraCPPFlags, forKey: .extraCPPFlags)
        try container.encode(toolchain.extraSwiftCFlags, forKey: .extraSwiftCFlags)
        try container.encode(toolchain.swiftCompilerPath, forKey: .swiftCompiler)
    }
}

extension BuildParameters {
    /// Whether to build Swift code with whole module optimization (WMO)
    /// enabled.
    public var useWholeModuleOptimization: Bool {
        switch configuration {
        case .debug:
            return false

        case .release:
            return true
        }
    }
}
