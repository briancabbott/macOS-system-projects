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
import Dispatch
import PackageGraph
import PackageModel
import SourceControl
import TSCBasic
import XCTest

import struct TSCUtility.Version

public class MockPackageContainer: CustomPackageContainer {
    public typealias Constraint = PackageContainerConstraint

    public typealias Dependency = (container: PackageReference, requirement: PackageRequirement)

    public let package: PackageReference

    let dependencies: [String: [Dependency]]
    let filteredMode: Bool
    let filteredDependencies: [ProductFilter: [Dependency]]
    let fileSystem: FileSystem?
    let customRetrievalPath: AbsolutePath?

    public var unversionedDeps: [MockPackageContainer.Constraint] = []

    /// Contains the versions for which the dependencies were requested by resolver using getDependencies().
    public var requestedVersions: Set<Version> = []

    public let _versions: [Version]
    public func toolsVersionsAppropriateVersionsDescending() throws -> [Version] {
        return try self.versionsDescending()
    }

    public func versionsAscending() throws -> [Version] {
        return _versions
    }

    public func getDependencies(at version: Version, productFilter: ProductFilter) -> [MockPackageContainer.Constraint] {
        requestedVersions.insert(version)
        return getDependencies(at: version.description, productFilter: productFilter)
    }

    public func getDependencies(at revision: String, productFilter: ProductFilter) -> [MockPackageContainer.Constraint] {
        let dependencies: [Dependency]
        if filteredMode {
            dependencies = filteredDependencies[productFilter]!
        } else {
            dependencies = self.dependencies[revision]!
        }
        return dependencies.map { value in
            let (package, requirement) = value
            return MockPackageContainer.Constraint(package: package, requirement: requirement, products: productFilter)
        }
    }

    public func getUnversionedDependencies(productFilter: ProductFilter) -> [MockPackageContainer.Constraint] {
        return unversionedDeps
    }

    public func loadPackageReference(at boundVersion: BoundVersion) throws -> PackageReference {
        return self.package
    }

    public func isToolsVersionCompatible(at version: Version) -> Bool {
        return true
    }

    public func toolsVersion(for version: Version) throws -> ToolsVersion {
        return ToolsVersion.current
    }

    public var isRemoteContainer: Bool? {
        return true
    }

    public func retrieve(at version: Version, progressHandler: ((Int64, Int64?) -> Void)?, observabilityScope: ObservabilityScope) throws -> AbsolutePath {
        if let path = customRetrievalPath {
            return path
        } else {
            throw StringError("no path configured for mock package container")
        }
    }

    public func getFileSystem() throws -> FileSystem? {
        return fileSystem
    }

    public convenience init(
        name: String,
        dependenciesByVersion: [Version: [(container: String, versionRequirement: VersionSetSpecifier)]]
    ) {
        var dependencies: [String: [Dependency]] = [:]
        for (version, deps) in dependenciesByVersion {
            dependencies[version.description] = deps.map {
                let path = AbsolutePath("/\($0.container)")
                let ref = PackageReference.localSourceControl(identity: .init(path: path), path: path)
                return (ref, .versionSet($0.versionRequirement))
            }
        }
        let path = AbsolutePath("/\(name)")
        let ref = PackageReference.localSourceControl(identity: .init(path: path), path: path)
        self.init(package: ref, dependencies: dependencies)
    }

    public init(
        package: PackageReference,
        dependencies: [String: [Dependency]] = [:],
        fileSystem: FileSystem? = nil,
        customRetrievalPath: AbsolutePath? = nil
    ) {
        self.package = package
        self._versions = dependencies.keys.compactMap(Version.init(_:)).sorted()
        self.dependencies = dependencies
        self.filteredMode = false
        self.filteredDependencies = [:]

        self.fileSystem = fileSystem
        self.customRetrievalPath = customRetrievalPath
    }

    public init(
        name: String,
        dependenciesByProductFilter: [ProductFilter: [(container: String, versionRequirement: VersionSetSpecifier)]]
    ) {
        var dependencies: [ProductFilter: [Dependency]] = [:]
        for (filter, deps) in dependenciesByProductFilter {
            dependencies[filter] = deps.map {
                let path = AbsolutePath("/\($0.container)")
                let ref = PackageReference.localSourceControl(identity: .init(path: path), path: path)
                return (ref, .versionSet($0.versionRequirement))
            }
        }
        let path = AbsolutePath("/\(name)")
        let ref = PackageReference.localSourceControl(identity: .init(path: path), path: path)
        self.package = ref
        self._versions = [Version(1, 0, 0)]
        self.dependencies = [:]
        self.filteredMode = true
        self.filteredDependencies = dependencies

        self.fileSystem = nil
        self.customRetrievalPath = nil
    }
}

public struct MockPackageContainerProvider: PackageContainerProvider {
    public let containers: [MockPackageContainer]
    public let containersByIdentifier: [PackageReference: MockPackageContainer]

    public init(containers: [MockPackageContainer]) {
        self.containers = containers
        self.containersByIdentifier = Dictionary(uniqueKeysWithValues: containers.map { ($0.package, $0) })
    }

    public func getContainer(
        for package: PackageReference,
        skipUpdate: Bool,
        observabilityScope: ObservabilityScope,
        on queue: DispatchQueue,
        completion: @escaping (Result<PackageContainer, Swift.Error>
        ) -> Void
    ) {
        queue.async {
            completion(self.containersByIdentifier[package].map { .success($0) } ??
                .failure(StringError("unknown module \(package)")))
        }
    }
}
