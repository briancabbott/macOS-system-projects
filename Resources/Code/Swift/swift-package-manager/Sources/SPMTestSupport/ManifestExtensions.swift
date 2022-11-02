//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift open source project
//
// Copyright (c) 2014-2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import Foundation
import PackageModel
import TSCBasic

public extension Manifest {

    static func createRootManifest(
        name: String,
        path: AbsolutePath = .root,
        defaultLocalization: String? = nil,
        platforms: [PlatformDescription] = [],
        version: TSCUtility.Version? = nil,
        toolsVersion: ToolsVersion = .v4,
        pkgConfig: String? = nil,
        providers: [SystemPackageProviderDescription]? = nil,
        cLanguageStandard: String? = nil,
        cxxLanguageStandard: String? = nil,
        swiftLanguageVersions: [SwiftLanguageVersion]? = nil,
        dependencies: [PackageDependency] = [],
        products: [ProductDescription] = [],
        targets: [TargetDescription] = []
    ) -> Manifest {
        Self.createManifest(
            name: name,
            path: path,
            packageKind: .root(path),
            packageLocation: path.pathString,
            defaultLocalization: defaultLocalization,
            platforms: platforms,
            version: version,
            toolsVersion: toolsVersion,
            pkgConfig: pkgConfig,
            providers: providers,
            cLanguageStandard: cLanguageStandard,
            cxxLanguageStandard: cxxLanguageStandard,
            swiftLanguageVersions: swiftLanguageVersions,
            dependencies: dependencies,
            products: products,
            targets: targets
        )
    }

    static func createFileSystemManifest(
        name: String,
        path: AbsolutePath,
        defaultLocalization: String? = nil,
        platforms: [PlatformDescription] = [],
        version: TSCUtility.Version? = nil,
        toolsVersion: ToolsVersion = .v4,
        pkgConfig: String? = nil,
        providers: [SystemPackageProviderDescription]? = nil,
        cLanguageStandard: String? = nil,
        cxxLanguageStandard: String? = nil,
        swiftLanguageVersions: [SwiftLanguageVersion]? = nil,
        dependencies: [PackageDependency] = [],
        products: [ProductDescription] = [],
        targets: [TargetDescription] = []
    ) -> Manifest {
        Self.createManifest(
            name: name,
            path: path,
            packageKind: .fileSystem(path),
            packageLocation: path.pathString,
            defaultLocalization: defaultLocalization,
            platforms: platforms,
            version: version,
            toolsVersion: toolsVersion,
            pkgConfig: pkgConfig,
            providers: providers,
            cLanguageStandard: cLanguageStandard,
            cxxLanguageStandard: cxxLanguageStandard,
            swiftLanguageVersions: swiftLanguageVersions,
            dependencies: dependencies,
            products: products,
            targets: targets
        )
    }

    static func createLocalSourceControlManifest(
        name: String,
        path: AbsolutePath,
        defaultLocalization: String? = nil,
        platforms: [PlatformDescription] = [],
        version: TSCUtility.Version? = nil,
        toolsVersion: ToolsVersion = .v4,
        pkgConfig: String? = nil,
        providers: [SystemPackageProviderDescription]? = nil,
        cLanguageStandard: String? = nil,
        cxxLanguageStandard: String? = nil,
        swiftLanguageVersions: [SwiftLanguageVersion]? = nil,
        dependencies: [PackageDependency] = [],
        products: [ProductDescription] = [],
        targets: [TargetDescription] = []
    ) -> Manifest {
        Self.createManifest(
            name: name,
            path: path,
            packageKind: .localSourceControl(path),
            packageLocation: path.pathString,
            defaultLocalization: defaultLocalization,
            platforms: platforms,
            version: version,
            toolsVersion: toolsVersion,
            pkgConfig: pkgConfig,
            providers: providers,
            cLanguageStandard: cLanguageStandard,
            cxxLanguageStandard: cxxLanguageStandard,
            swiftLanguageVersions: swiftLanguageVersions,
            dependencies: dependencies,
            products: products,
            targets: targets
        )
    }

    static func createRemoteSourceControlManifest(
        name: String,
        url: URL,
        path: AbsolutePath,
        defaultLocalization: String? = nil,
        platforms: [PlatformDescription] = [],
        version: TSCUtility.Version? = nil,
        toolsVersion: ToolsVersion = .v4,
        pkgConfig: String? = nil,
        providers: [SystemPackageProviderDescription]? = nil,
        cLanguageStandard: String? = nil,
        cxxLanguageStandard: String? = nil,
        swiftLanguageVersions: [SwiftLanguageVersion]? = nil,
        dependencies: [PackageDependency] = [],
        products: [ProductDescription] = [],
        targets: [TargetDescription] = []
    ) -> Manifest {
        Self.createManifest(
            name: name,
            path: path,
            packageKind: .remoteSourceControl(url),
            packageLocation: url.absoluteString,
            defaultLocalization: defaultLocalization,
            platforms: platforms,
            version: version,
            toolsVersion: toolsVersion,
            pkgConfig: pkgConfig,
            providers: providers,
            cLanguageStandard: cLanguageStandard,
            cxxLanguageStandard: cxxLanguageStandard,
            swiftLanguageVersions: swiftLanguageVersions,
            dependencies: dependencies,
            products: products,
            targets: targets
        )
    }

    static func createManifest(
        name: String,
        path: AbsolutePath = .root,
        packageKind: PackageReference.Kind,
        packageLocation: String? = nil,
        defaultLocalization: String? = nil,
        platforms: [PlatformDescription] = [],
        version: TSCUtility.Version? = nil,
        toolsVersion: ToolsVersion,
        pkgConfig: String? = nil,
        providers: [SystemPackageProviderDescription]? = nil,
        cLanguageStandard: String? = nil,
        cxxLanguageStandard: String? = nil,
        swiftLanguageVersions: [SwiftLanguageVersion]? = nil,
        dependencies: [PackageDependency] = [],
        products: [ProductDescription] = [],
        targets: [TargetDescription] = []
    ) -> Manifest {
        return Manifest(
            displayName: name,
            path: path.appending(component: Manifest.filename),
            packageKind: packageKind,
            packageLocation: packageLocation ?? path.pathString,
            defaultLocalization: defaultLocalization,
            platforms: platforms,
            version: version,
            toolsVersion: toolsVersion,
            pkgConfig: pkgConfig,
            providers: providers,
            cLanguageStandard: cLanguageStandard,
            cxxLanguageStandard: cxxLanguageStandard,
            swiftLanguageVersions: swiftLanguageVersions,
            dependencies: dependencies,
            products: products,
            targets: targets
        )
    }

    func with(location: String) -> Manifest {
        return Manifest(
            displayName: self.displayName,
            path: self.path,
            packageKind: self.packageKind,
            packageLocation: location,
            platforms: self.platforms,
            version: self.version,
            toolsVersion: self.toolsVersion,
            pkgConfig: self.pkgConfig,
            providers: self.providers,
            cLanguageStandard: self.cLanguageStandard,
            cxxLanguageStandard: self.cxxLanguageStandard,
            swiftLanguageVersions: self.swiftLanguageVersions,
            dependencies: self.dependencies,
            products: self.products,
            targets: self.targets
        )
    }
}
