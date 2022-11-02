//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2021 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import Foundation

import TSCBasic
import TSCUtility

struct TestConfigurationGenerator {
    let fileSystem: FileSystem

    init(fileSystem: FileSystem) {
        self.fileSystem = fileSystem
    }

    func run(configuration: Configuration, tmpDir: AbsolutePath) throws -> PackageRegistryCompatibilityTestSuite.Configuration {
        guard !configuration.packages.isEmpty else {
            throw TestError("\"packages\" must not be empty")
        }
        guard configuration.packages.first(where: { $0.releases.isEmpty }) == nil else {
            throw TestError("\"package.releases\" must not be empty")
        }

        let resourceBaseDirectoryPath = try configuration.resourceBaseDirectory.map { try AbsolutePath(validating: $0) }

        // Generate test packages
        let packages: [PackageDescriptor] = try configuration.packages.enumerated().map { offset, element in
            let package: PackageIdentity
            switch element.id {
            case .some(let id):
                package = id
            case .none:
                let randomString = randomAlphaNumericString(length: 6)
                package = PackageIdentity(scope: "test-\(randomString)", name: "package-\(randomString)")
            }

            let explicitVersions = element.releases.enumerated().reduce(into: [Int: Version]()) { result, item in
                guard let v = item.element.version, let version = Version(v) else { return }
                result[item.offset] = version
            }
            var randomVersions: [Version] = explicitVersions.count == element.releases.count ? [] :
                // If we need to generate any versions, start from the "next" major version to avoid conflicts
                randomVersions(major: (explicitVersions.values.sorted(by: >).first?.major ?? 0) + 1, count: element.releases.count - explicitVersions.count)

            let releases: [PackageDescriptor.Release] = try element.releases.enumerated().map { offset, element in
                // Version is either specified in the configuration or generated randomly
                let version = explicitVersions[offset] ?? randomVersions.removeFirst()

                let sourceArchivePath = try makeAbsolutePath(element.sourceArchivePath, relativeTo: resourceBaseDirectoryPath)
                // Compute checksum of the source archive
                let checksum = try Process.checkNonZeroExit(arguments: ["swift", "package", "compute-checksum", sourceArchivePath.pathString])
                    .trimmingCharacters(in: .whitespacesAndNewlines)

                let metadataTemplatePath = try element.metadataPath.map { try makeAbsolutePath($0, relativeTo: resourceBaseDirectoryPath) }
                // Replace values in metadata template if any
                var metadataPath: AbsolutePath?
                if let metadataTemplatePath = metadataTemplatePath {
                    let metadataData = try Data(contentsOf: metadataTemplatePath.asURL)
                    guard let metadataJSON = String(data: metadataData, encoding: .utf8) else {
                        throw TestError("\(metadataTemplatePath) contains invalid JSON")
                    }

                    let modifiedMetadata = metadataJSON.replacingOccurrences(of: "{TEST_SCOPE}", with: package.scope)
                        .replacingOccurrences(of: "{TEST_NAME}", with: package.name)
                        .replacingOccurrences(of: "{TEST_VERSION}", with: version.description)
                    metadataPath = tmpDir.appending(component: "\(package)-\(version)-metadata.json")

                    // Write the modified metadata file to tmp dir
                    try self.fileSystem.writeFileContents(metadataPath!, bytes: ByteString(encodingAsUTF8: modifiedMetadata)) // !-safe since we assign value above
                }

                return PackageDescriptor.Release(
                    version: version,
                    sourceArchivePath: sourceArchivePath,
                    checksum: checksum,
                    metadataPath: metadataPath,
                    versionManifests: element.versionManifests
                )
            }

            return PackageDescriptor(id: package, repositoryURL: element.repositoryURL, releases: releases)
        }

        let unknownPackages = self.randomPackageIdentities(count: 1)

        return PackageRegistryCompatibilityTestSuite.Configuration(
            createPackageRelease: self.buildCreatePackageRelease(packages: packages, configuration: configuration.createPackageRelease),
            listPackageReleases: configuration.listPackageReleases.map {
                self.buildListPackageReleases(packages: packages, unknownPackages: unknownPackages, configuration: $0)
            },
            fetchPackageReleaseInfo: try configuration.fetchPackageReleaseInfo.map {
                try self.buildFetchPackageReleaseInfo(packages: packages, unknownPackages: unknownPackages, configuration: $0)
            },
            fetchPackageReleaseManifest: configuration.fetchPackageReleaseManifest.map {
                self.buildFetchPackageReleaseManifest(packages: packages, unknownPackages: unknownPackages, configuration: $0)
            },
            downloadSourceArchive: configuration.downloadSourceArchive.map {
                self.buildDownloadSourceArchive(packages: packages, unknownPackages: unknownPackages, configuration: $0)
            },
            lookupPackageIdentifiers: try self.buildLookupPackageIdentifiers(packages: packages, unknownPackages: unknownPackages,
                                                                             configuration: configuration.lookupPackageIdentifiers)
        )
    }

    private func buildCreatePackageRelease(packages: [PackageDescriptor],
                                           configuration: Configuration.CreatePackageRelease) -> CreatePackageReleaseTests.Configuration {
        CreatePackageReleaseTests.Configuration(
            packageReleases: packages.flatMap { package in
                package.releases.map {
                    .init(package: package.id, version: $0.version.description,
                          sourceArchivePath: $0.sourceArchivePath.pathString, metadataPath: $0.metadataPath?.pathString)
                }
            },
            maxProcessingTimeInSeconds: configuration.maxProcessingTimeInSeconds
        )
    }

    private func buildListPackageReleases(packages: [PackageDescriptor],
                                          unknownPackages: [PackageIdentity],
                                          configuration: Configuration.ListPackageReleases) -> ListPackageReleasesTests.Configuration {
        ListPackageReleasesTests.Configuration(
            packages: packages.map { package in
                .init(
                    package: package.id,
                    numberOfReleases: package.releases.count,
                    versions: Set(package.releases.map(\.version.description)),
                    unavailableVersions: nil, // TODO: add support for this when DELETE package release API is defined
                    linkRelations: configuration.linkHeaderIsSet ? ["latest-version"] : nil
                )
            },
            unknownPackages: Set(unknownPackages),
            packageURLProvided: configuration.packageURLProvided,
            problemProvided: configuration.problemProvided,
            paginationSupported: configuration.paginationSupported
        )
    }

    private func buildFetchPackageReleaseInfo(packages: [PackageDescriptor],
                                              unknownPackages: [PackageIdentity],
                                              configuration: Configuration.FetchPackageReleaseInfo) throws -> FetchPackageReleaseInfoTests.Configuration {
        func validatableMetadataKeyValue(metadataPath: AbsolutePath?) throws -> [String: String]? {
            guard let metadataPath = metadataPath else { return nil }

            guard let metadata = try JSONSerialization.jsonObject(with: Data(contentsOf: metadataPath.asURL), options: []) as? [String: Any] else {
                throw TestError("Metadata file at \(metadataPath) contains invalid JSON")
            }

            var result = [String: String]()
            metadata.forEach { key, value in
                guard let value = value as? String else {
                    return
                }
                result[key] = value
            }
            return result.isEmpty ? nil : result
        }

        func linkRelations(version: Version, versions: [Version]) -> Set<String> {
            var relations: Set<String> = ["latest-version"]

            guard let index = versions.firstIndex(of: version) else {
                return relations
            }

            if index > 0 {
                relations.insert("predecessor-version")
            }
            if index < versions.count - 1 {
                relations.insert("successor-version")
            }

            return relations
        }

        return FetchPackageReleaseInfoTests.Configuration(
            packageReleases: try packages.flatMap { package -> [FetchPackageReleaseInfoTests.Configuration.PackageReleaseExpectation] in
                let versions = package.releases.map(\.version).sorted()
                return try package.releases.map {
                    .init(
                        packageRelease: .init(package: package.id, version: $0.version.description),
                        resources: [.init(name: "source-archive", type: "application/zip", checksum: $0.checksum)],
                        keyValues: try validatableMetadataKeyValue(metadataPath: $0.metadataPath),
                        linkRelations: configuration.linkHeaderIsSet ? linkRelations(version: $0.version, versions: versions) : nil
                    )
                }
            },
            unknownPackageReleases: Set(unknownPackages.map {
                .init(package: $0, version: "1.0.0")
            })
        )
    }

    private func buildFetchPackageReleaseManifest(packages: [PackageDescriptor],
                                                  unknownPackages: [PackageIdentity],
                                                  configuration: Configuration.FetchPackageReleaseManifest) -> FetchPackageReleaseManifestTests.Configuration {
        FetchPackageReleaseManifestTests.Configuration(
            packageReleases: packages.flatMap { package in
                package.releases.map {
                    .init(
                        packageRelease: .init(package: package.id, version: $0.version.description),
                        swiftVersions: $0.versionManifests,
                        noSwiftVersions: $0.versionManifests != nil ? ["4.30"] : nil // non-existent Swift version
                    )
                }
            },
            unknownPackageReleases: Set(unknownPackages.map {
                .init(package: $0, version: "1.0.0")
            }),
            contentLengthHeaderIsSet: configuration.contentLengthHeaderIsSet,
            contentDispositionHeaderIsSet: configuration.contentDispositionHeaderIsSet
        )
    }

    private func buildDownloadSourceArchive(packages: [PackageDescriptor],
                                            unknownPackages: [PackageIdentity],
                                            configuration: Configuration.DownloadSourceArchive) -> DownloadSourceArchiveTests.Configuration {
        DownloadSourceArchiveTests.Configuration(
            sourceArchives: packages.flatMap { package in
                package.releases.map {
                    .init(
                        packageRelease: .init(package: package.id, version: $0.version.description),
                        hasDuplicateLinks: configuration.linkHeaderHasDuplicateRelations
                    )
                }
            },
            unknownSourceArchives: Set(unknownPackages.map {
                .init(package: $0, version: "1.0.0")
            }),
            contentDispositionHeaderIsSet: configuration.contentDispositionHeaderIsSet,
            digestHeaderIsSet: configuration.digestHeaderIsSet
        )
    }

    private func buildLookupPackageIdentifiers(packages: [PackageDescriptor],
                                               unknownPackages: [PackageIdentity],
                                               configuration: Configuration.LookupPackageIdentifiers?) throws -> LookupPackageIdentifiersTests.Configuration? {
        guard let configuration = configuration else { return nil }

        func repositoryURL(package: PackageDescriptor) throws -> String? {
            if package.repositoryURL != nil { return package.repositoryURL }

            guard let metadataKey = configuration.repositoryURLMetadataKey else { return nil }

            for release in package.releases {
                guard let metadataPath = release.metadataPath else { continue }

                guard let metadata = try JSONSerialization.jsonObject(with: Data(contentsOf: metadataPath.asURL), options: []) as? [String: Any] else {
                    throw TestError("Metadata file at \(metadataPath) contains invalid JSON")
                }

                if let repositoryURL = metadata[metadataKey] as? String {
                    return repositoryURL
                }
            }

            return nil
        }

        var urlPackageIdentifiers = [String: Set<String>]()
        try packages.forEach {
            guard let repositoryURL = try repositoryURL(package: $0) else { return }
            var packageIdentifiers = urlPackageIdentifiers.removeValue(forKey: repositoryURL) ?? []
            packageIdentifiers.insert($0.id.description)
            urlPackageIdentifiers[repositoryURL] = packageIdentifiers
        }
        guard !urlPackageIdentifiers.isEmpty else { return nil }

        return LookupPackageIdentifiersTests.Configuration(
            urls: urlPackageIdentifiers.map { url, identifiers in
                .init(url: url, packageIdentifiers: identifiers)
            },
            unknownURLs: Set(unknownPackages.map { "https://repos.test/\($0.scope)/\($0.name)" })
        )
    }

    private func randomPackageIdentities(count: Int) -> [PackageIdentity] {
        guard count > 0 else { return [] }

        return (0 ..< count).map { _ in
            let randomString = randomAlphaNumericString(length: 6)
            return PackageIdentity(scope: "test-\(randomString)", name: "package-\(randomString)")
        }
    }

    private func randomVersions(major: Int, count: Int) -> [Version] {
        guard count > 0 else { return [] }

        var versions = [Version]()
        var majorVersion: Int = major
        var minorVersion: Int = 0
        var minorCount = Int.random(in: 1 ... 3)

        (0 ..< count).forEach { _ in
            versions.append(Version(majorVersion, minorVersion, 0))

            minorVersion = minorVersion + 1

            if minorVersion >= minorCount {
                majorVersion = majorVersion + 1
                minorVersion = 0
                minorCount = Int.random(in: 1 ... 5)
            }
        }

        return versions
    }
}

private struct PackageDescriptor {
    let id: PackageIdentity

    let repositoryURL: String?

    let releases: [Release]

    struct Release {
        let version: Version

        /// Absolute path of the source archive file
        let sourceArchivePath: AbsolutePath

        /// Source archive checksum computed using `swift package compute-checksum`
        let checksum: String

        /// Absolute path of the metadata JSON file
        let metadataPath: AbsolutePath?

        /// Swift versions with version-specific manifest
        let versionManifests: Set<String>?
    }
}

extension TestConfigurationGenerator {
    struct Configuration: Codable {
        /// Absolute path of the directory containing test resource files (e.g., source archives, metadata JSON files)
        var resourceBaseDirectory: String?

        /// Package releases that will serve as the basis of compatibility test configuration
        let packages: [PackageInfo]

        /// For creating `CreatePackageReleaseTests.Configuration`
        let createPackageRelease: CreatePackageRelease

        /// For creating `ListPackageReleasesTests.Configuration`
        let listPackageReleases: ListPackageReleases?

        /// For creating `FetchPackageReleaseInfoTests.Configuration`
        let fetchPackageReleaseInfo: FetchPackageReleaseInfo?

        /// For creating `FetchPackageReleaseManifestTests.Configuration`
        let fetchPackageReleaseManifest: FetchPackageReleaseManifest?

        /// For creating `DownloadSourceArchiveTests.Configuration`
        let downloadSourceArchive: DownloadSourceArchive?

        /// For creating `LookupPackageIdentifiersTests.Configuration`
        let lookupPackageIdentifiers: LookupPackageIdentifiers?

        struct PackageInfo: Codable {
            /// Identity to use for the test package. A random identity is generated if this is unspecified.
            let id: PackageIdentity?

            /// Repository URL of the package
            let repositoryURL: String?

            /// Package releases
            let releases: [PackageReleaseInfo]
        }

        struct PackageReleaseInfo: Codable {
            /// Package release version. A random version is generated if this is unspecified.
            let version: String?

            /// Absolute or relative path of the source archive file.
            /// If relative path is used, it is assumed to be under `resourceBaseDirectory`.
            let sourceArchivePath: String

            /// Absolute or relative path of the metadata JSON file.
            /// If relative path is used, it is assumed to be under `resourceBaseDirectory`.
            let metadataPath: String?

            /// Swift versions with version-specific manifest
            let versionManifests: Set<String>?
        }

        struct CreatePackageRelease: Codable {
            /// See `CreatePackageReleaseTests.Configuration.maxProcessingTimeInSeconds`
            @DecodableDefault.MaxPublicationTimeInSeconds var maxProcessingTimeInSeconds: Int
        }

        struct ListPackageReleases: Codable {
            /// If `true`, the generator will set `PackageExpectation.linkRelations` accordingly.
            let linkHeaderIsSet: Bool

            /// See `ListPackageReleasesTests.Configuration.packageURLProvided`
            let packageURLProvided: Bool

            /// See `ListPackageReleasesTests.Configuration.problemProvided`
            let problemProvided: Bool

            /// See `ListPackageReleasesTests.Configuration.paginationSupported`
            let paginationSupported: Bool
        }

        struct FetchPackageReleaseInfo: Codable {
            /// If `true`, the generator will set `PackageReleaseExpectation.linkRelations` accordingly.
            let linkHeaderIsSet: Bool
        }

        struct FetchPackageReleaseManifest: Codable {
            /// See `FetchPackageReleaseManifestTests.Configuration.contentLengthHeaderIsSet`
            let contentLengthHeaderIsSet: Bool

            /// See `FetchPackageReleaseManifestTests.Configuration.contentDispositionHeaderIsSet`
            let contentDispositionHeaderIsSet: Bool
        }

        struct DownloadSourceArchive: Codable {
            /// See `DownloadSourceArchiveTests.Configuration.contentDispositionHeaderIsSet`
            let contentDispositionHeaderIsSet: Bool

            /// See `DownloadSourceArchiveTests.Configuration.digestHeaderIsSet`
            let digestHeaderIsSet: Bool

            /// See `DownloadSourceArchiveTests.Configuration.SourceArchiveExpectation.hasDuplicateLinks`
            let linkHeaderHasDuplicateRelations: Bool
        }

        struct LookupPackageIdentifiers: Codable {
            /// Metadata key of a package's repository URL
            let repositoryURLMetadataKey: String?
        }
    }
}
