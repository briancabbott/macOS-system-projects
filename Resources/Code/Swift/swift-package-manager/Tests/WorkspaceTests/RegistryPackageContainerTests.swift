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
import Foundation
import PackageGraph
import PackageLoading
import PackageModel
import PackageRegistry
import SPMTestSupport
import TSCBasic
@testable import Workspace
import XCTest

class RegistryPackageContainerTests: XCTestCase {

    func testToolsVersionCompatibleVersions() throws {
        let fs = InMemoryFileSystem()

        let packageIdentity = PackageIdentity.plain("org.foo")
        let packageVersion = Version("1.0.0")
        let packagePath = AbsolutePath.root

        func createProvider(_ toolsVersion: ToolsVersion) throws -> PackageContainerProvider {
            let registryClient = try makeRegistryClient(
                packageIdentity: packageIdentity,
                packageVersion: packageVersion,
                packagePath: packagePath,
                fileSystem: fs,
                releasesRequestHandler: { request, _ , completion in
                    let metadata = RegistryClient.Serialization.PackageMetadata(
                        releases: [
                            "1.0.0":  .init(url: .none, problem: .none),
                            "1.0.1":  .init(url: .none, problem: .none),
                            "1.0.2":  .init(url: .none, problem: .none),
                            "1.0.3":  .init(url: .none, problem: .none)
                        ]
                    )
                    completion(.success(
                        HTTPClientResponse(
                            statusCode: 200,
                            headers: [
                                "Content-Version": "1",
                                "Content-Type": "application/json"
                            ],
                            body: try! JSONEncoder.makeWithDefaults().encode(metadata)
                        )
                    ))
                },
                manifestRequestHandler: { request, _ , completion in
                    let toolsVersion: ToolsVersion
                    switch request.url.deletingLastPathComponent().lastPathComponent {
                    case "1.0.0":
                        toolsVersion = .v3
                    case "1.0.1":
                        toolsVersion = .v4
                    case "1.0.2":
                        toolsVersion = .v4_2
                    case "1.0.3":
                        toolsVersion = .v5_4
                    default:
                        toolsVersion = .current
                    }
                    completion(.success(
                        HTTPClientResponse(
                            statusCode: 200,
                            headers: [
                                "Content-Version": "1",
                                "Content-Type": "text/x-swift"
                            ],
                            body: "// swift-tools-version:\(toolsVersion)".data(using: .utf8)
                        )
                    ))
                }
            )

            return try Workspace._init(
                fileSystem: fs,
                location: .init(forRootPackage: packagePath, fileSystem: fs),
                customToolsVersion: toolsVersion,
                customManifestLoader: MockManifestLoader(manifests: [:]),
                customRegistryClient: registryClient
            )
        }

        do {
            let provider = try createProvider(.v4)
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false)
            let versions = try container.toolsVersionsAppropriateVersionsDescending()
            XCTAssertEqual(versions, ["1.0.1"])
        }

        do {
            let provider = try createProvider(.v4_2)
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false)
            let versions = try container.toolsVersionsAppropriateVersionsDescending()
            XCTAssertEqual(versions, ["1.0.2", "1.0.1"])
        }

        do {
            let provider = try createProvider(.v5_4)
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false)
            let versions = try container.toolsVersionsAppropriateVersionsDescending()
            XCTAssertEqual(versions, ["1.0.3", "1.0.2", "1.0.1"])
        }
    }

    func testAlternateManifests() throws {
        let fs = InMemoryFileSystem()

        let packageIdentity = PackageIdentity.plain("org.foo")
        let packageVersion = Version("1.0.0")
        let packagePath = AbsolutePath.root

        func createProvider(_ toolsVersion: ToolsVersion) throws -> PackageContainerProvider {
            let registryClient = try makeRegistryClient(
                packageIdentity: packageIdentity,
                packageVersion: packageVersion,
                packagePath: packagePath,
                fileSystem: fs,
                manifestRequestHandler: { request, _ , completion in
                    completion(.success(
                        HTTPClientResponse(
                            statusCode: 200,
                            headers: [
                                "Content-Version": "1",
                                "Content-Type": "text/x-swift",
                                "Link": """
                                \(self.manifestLink(packageIdentity, .v5_4)),
                                \(self.manifestLink(packageIdentity, .v5_5)),
                                """
                            ],
                            body: "// swift-tools-version:\(ToolsVersion.v5_3)".data(using: .utf8)
                        )
                    ))
                }
            )

            return try Workspace._init(
                fileSystem: fs,
                location: .init(forRootPackage: packagePath, fileSystem: fs),
                customToolsVersion: toolsVersion,
                customManifestLoader: MockManifestLoader(manifests: [:]),
                customRegistryClient: registryClient
            )
        }

        do {
            let provider = try createProvider(.v5_2) // the version of the alternate
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false)
            XCTAssertEqual(try container.toolsVersion(for: packageVersion), .v5_3)
            let versions = try container.toolsVersionsAppropriateVersionsDescending()
            XCTAssertEqual(versions, [])
        }

        do {
            let provider = try createProvider(.v5_3) // the version of the alternate
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false)
            XCTAssertEqual(try container.toolsVersion(for: packageVersion), .v5_3)
            let versions = try container.toolsVersionsAppropriateVersionsDescending()
            XCTAssertEqual(versions, [packageVersion])
        }

        do {
            let provider = try createProvider(.v5_4) // the version of the alternate
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false)
            XCTAssertEqual(try container.toolsVersion(for: packageVersion), .v5_4)
            let versions = try container.toolsVersionsAppropriateVersionsDescending()
            XCTAssertEqual(versions, [packageVersion])
        }

        do {
            let provider = try createProvider(.v5_5) // the version of the alternate
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false)
            XCTAssertEqual(try container.toolsVersion(for: packageVersion), .v5_5)
            let versions = try container.toolsVersionsAppropriateVersionsDescending()
            XCTAssertEqual(versions, [packageVersion])
        }

        do {
            let provider = try createProvider(.v5_6) // the version of the alternate
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false)
            XCTAssertEqual(try container.toolsVersion(for: packageVersion), .v5_5)
            let versions = try container.toolsVersionsAppropriateVersionsDescending()
            XCTAssertEqual(versions, [packageVersion])
        }
    }

    func testLoadManifest() throws {
        let fs = InMemoryFileSystem()

        let packageIdentity = PackageIdentity.plain("org.foo")
        let packageVersion = Version("1.0.0")
        let packagePath = AbsolutePath.root

        let v5_3_3 = ToolsVersion(string: "5.3.3")!

        func createProvider(_ toolsVersion: ToolsVersion) throws -> PackageContainerProvider {
            let supportedVersions = Set<ToolsVersion>([ToolsVersion.v5_3, v5_3_3, .v5_4, .v5_5])
            let registryClient = try makeRegistryClient(
                packageIdentity: packageIdentity,
                packageVersion: packageVersion,
                packagePath: packagePath,
                fileSystem: fs,
                manifestRequestHandler: { request, _ , completion in
                    let requestedVersionString = request.url.query?.spm_dropPrefix("swift-version=")
                    let requestedVersion = (requestedVersionString.flatMap{ ToolsVersion(string: $0) }) ?? .v5_3
                    guard supportedVersions.contains(requestedVersion) else {
                        return completion(.failure(StringError("invalid version \(requestedVersion)")))
                    }
                    completion(.success(
                        HTTPClientResponse(
                            statusCode: 200,
                            headers: [
                                "Content-Version": "1",
                                "Content-Type": "text/x-swift",
                                "Link": (supportedVersions.subtracting([requestedVersion])).map {
                                    self.manifestLink(packageIdentity, $0)
                                }.joined(separator: ",\n")
                            ],
                            body: "// swift-tools-version:\(requestedVersion)".data(using: .utf8)
                        )
                    ))
                }
            )

            return try Workspace._init(
                fileSystem: fs,
                location: .init(forRootPackage: packagePath, fileSystem: fs),
                customToolsVersion: toolsVersion,
                customManifestLoader: MockManifestLoader(),
                customRegistryClient: registryClient
            )

            struct MockManifestLoader: ManifestLoaderProtocol {
                func load(manifestPath: AbsolutePath,
                          manifestToolsVersion: ToolsVersion,
                          packageIdentity: PackageIdentity,
                          packageKind: PackageReference.Kind,
                          packageLocation: String,
                          packageVersion: (version: Version?, revision: String?)?,
                          identityResolver: IdentityResolver,
                          fileSystem: FileSystem,
                          observabilityScope: ObservabilityScope,
                          delegateQueue: DispatchQueue,
                          callbackQueue: DispatchQueue,
                          completion: @escaping (Result<Manifest, Error>) -> Void) {
                    completion(.success(
                        Manifest(
                            displayName: packageIdentity.description,
                            path: manifestPath,
                            packageKind: packageKind,
                            packageLocation: packageLocation,
                            platforms: [],
                            toolsVersion: manifestToolsVersion
                        )
                    ))
                }

                func resetCache() throws {}
                func purgeCache() throws {}
            }
        }

        do {
            let provider = try createProvider(.v5_3) // the version of the alternate
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false) as! RegistryPackageContainer
            let manifest = try container.loadManifest(version: packageVersion)
            XCTAssertEqual(manifest.toolsVersion, .v5_3)
        }

        do {
            let provider = try createProvider(v5_3_3) // the version of the alternate
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false) as! RegistryPackageContainer
            let manifest = try container.loadManifest(version: packageVersion)
            XCTAssertEqual(manifest.toolsVersion, v5_3_3)
        }

        do {
            let provider = try createProvider(.v5_4) // the version of the alternate
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false) as! RegistryPackageContainer
            let manifest = try container.loadManifest(version: packageVersion)
            XCTAssertEqual(manifest.toolsVersion, .v5_4)
        }

        do {
            let provider = try createProvider(.v5_5) // the version of the alternate
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false) as! RegistryPackageContainer
            let manifest = try container.loadManifest(version: packageVersion)
            XCTAssertEqual(manifest.toolsVersion, .v5_5)
        }

        do {
            let provider = try createProvider(.v5_6) // the version of the alternate
            let ref = PackageReference.registry(identity: packageIdentity)
            let container = try provider.getContainer(for: ref, skipUpdate: false) as! RegistryPackageContainer
            let manifest = try container.loadManifest(version: packageVersion)
            XCTAssertEqual(manifest.toolsVersion, .v5_5)
        }
    }

    func makeRegistryClient(
        packageIdentity: PackageIdentity,
        packageVersion: Version,
        packagePath: AbsolutePath,
        fileSystem: FileSystem,
        configuration: PackageRegistry.RegistryConfiguration? = .none,
        releasesRequestHandler: HTTPClient.Handler? = .none,
        versionMetadataRequestHandler: HTTPClient.Handler? = .none,
        manifestRequestHandler: HTTPClient.Handler? = .none,
        downloadArchiveRequestHandler: HTTPClient.Handler? = .none,
        archiver: Archiver? = .none
    ) throws -> RegistryClient {
        let jsonEncoder = JSONEncoder.makeWithDefaults()
        let fingerprintStorage = MockPackageFingerprintStorage()

        guard let (packageScope, packageName) = packageIdentity.scopeAndName else {
            throw StringError("Invalid package identity")
        }

        var configuration = configuration
        if configuration == nil {
            configuration = PackageRegistry.RegistryConfiguration()
            configuration!.defaultRegistry = .init(url: URL(string: "http://localhost")!)
        }

        let releasesRequestHandler = releasesRequestHandler ?? { request, _ , completion in
            let metadata = RegistryClient.Serialization.PackageMetadata(
                releases: [packageVersion.description:  .init(url: .none, problem: .none)]
            )
            completion(.success(
                HTTPClientResponse(
                    statusCode: 200,
                    headers: [
                        "Content-Version": "1",
                        "Content-Type": "application/json"
                    ],
                    body: try! jsonEncoder.encode(metadata)
                )
            ))
        }

        let versionMetadataRequestHandler = versionMetadataRequestHandler ?? { request, _ , completion in
            let metadata = RegistryClient.Serialization.VersionMetadata(
                id: packageIdentity.description,
                version: packageVersion.description,
                resources: [
                    .init(
                        name: "source-archive",
                        type: "application/zip",
                        checksum: ""
                    )
                ],
                metadata: .init(description: "")
            )
            completion(.success(
                HTTPClientResponse(
                    statusCode: 200,
                    headers: [
                        "Content-Version": "1",
                        "Content-Type": "application/json"
                    ],
                    body: try! jsonEncoder.encode(metadata)
                )
            ))
        }

        let manifestRequestHandler = manifestRequestHandler ?? { request, _ , completion in
            completion(.success(
                HTTPClientResponse(
                    statusCode: 200,
                    headers: [
                        "Content-Version": "1",
                        "Content-Type": "text/x-swift"
                    ],
                    body: "// swift-tools-version:\(ToolsVersion.current)".data(using: .utf8)
                )
            ))
        }

        let downloadArchiveRequestHandler = downloadArchiveRequestHandler ?? { request, _ , completion in
            // meh
            let path = packagePath
                .appending(components: ".build", "registry", "downloads", packageScope.description, packageName.description)
                .appending(component: "\(packageVersion).zip")
            try! fileSystem.createDirectory(path.parentDirectory, recursive: true)
            try! fileSystem.writeFileContents(path, string: "")

            completion(.success(
                HTTPClientResponse(
                    statusCode: 200,
                    headers: [
                        "Content-Version": "1",
                        "Content-Type": "application/zip"
                    ],
                    body: "".data(using: .utf8)
                )
            ))
        }

        let archiver = archiver ?? MockArchiver(handler: { archiver, from, to, completion in
            do {
                try fileSystem.createDirectory(to.appending(component: "top"), recursive: true)
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        return RegistryClient(
            configuration: configuration!,
            fingerprintStorage: fingerprintStorage,
            fingerprintCheckingMode: .strict,
            authorizationProvider: .none,
            customHTTPClient: HTTPClient(configuration: .init(), handler: { request, progress , completion in
                var pathComponents = request.url.pathComponents
                if pathComponents.first == "/" {
                    pathComponents = Array(pathComponents.dropFirst())
                }
                guard pathComponents.count >= 2 else {
                    return completion(.failure(StringError("invalid url \(request.url)")))
                }
                guard pathComponents[0] == packageScope.description else {
                    return completion(.failure(StringError("invalid url \(request.url)")))
                }
                guard pathComponents[1] == packageName.description else {
                    return completion(.failure(StringError("invalid url \(request.url)")))
                }

                switch pathComponents.count {
                case 2:
                    releasesRequestHandler(request, progress, completion)
                case 3 where pathComponents[2].hasSuffix(".zip"):
                    downloadArchiveRequestHandler(request, progress, completion)
                case 3:
                    versionMetadataRequestHandler(request, progress, completion)
                case 4 where pathComponents[3].hasSuffix(".swift"):
                    manifestRequestHandler(request, progress, completion)
                default:
                    completion(.failure(StringError("unexpected url \(request.url)")))
                }
            }),
            customArchiverProvider: { _ in archiver }
        )
    }

    private func manifestLink(_ identity: PackageIdentity, _ version: ToolsVersion) -> String {
        guard let (scope, name) = identity.scopeAndName else {
            preconditionFailure("invalid identity")
        }
        let versionString = version.patch == 0 ? "\(version.major).\(version.minor)" : version.description
        return "<http://localhost/\(scope)/\(name)/\(version)/\(Manifest.filename)?swift-version=\(version)>; rel=\"alternate\"; filename=\"\(Manifest.basename)@swift-\(versionString).swift\"; swift-tools-version=\"\(version)\""
    }
}

extension PackageContainerProvider {
    fileprivate func getContainer(for package: PackageReference, skipUpdate: Bool) throws -> PackageContainer {
        try tsc_await { self.getContainer(for: package, skipUpdate: skipUpdate, observabilityScope: ObservabilitySystem.NOOP, on: .global(), completion: $0)  }
    }
}
