//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift open source project
//
// Copyright (c) 2020-2022 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import ArgumentParser
import Basics
import Foundation
import PackageCollections
import PackageModel
import TSCBasic

private enum CollectionsError: Swift.Error {
    case invalidArgument(String)
    case invalidVersionString(String)
    case unsigned
    case cannotVerifySignature
    case invalidSignature
    case missingSignature
}

// FIXME: add links to docs in error messages
extension CollectionsError: CustomStringConvertible {
    var description: String {
        switch self {
        case .invalidArgument(let argumentName):
            return "Invalid argument '\(argumentName)'"
        case .invalidVersionString(let versionString):
            return "Invalid version string '\(versionString)'"
        case .unsigned:
            return "The collection is not signed. If you would still like to add it please rerun 'add' with '--trust-unsigned'."
        case .cannotVerifySignature:
            return "The collection's signature cannot be verified due to missing configuration. Please refer to documentations on how to set up trusted root certificates or rerun command with '--skip-signature-check'."
        case .invalidSignature:
            return "The collection's signature is invalid. If you would like to continue please rerun command with '--skip-signature-check'."
        case .missingSignature:
            return "The collection is missing required signature, which means it might have been compromised. Please contact the collection's authors and alert them of the issue."
        }
    }
}

struct JSONOptions: ParsableArguments {
    @Flag(name: .long, help: "Output as JSON")
    var json: Bool = false
}

public struct SwiftPackageCollectionsTool: ParsableCommand {
    public static var configuration = CommandConfiguration(
        commandName: "package-collection",
        _superCommandName: "swift",
        abstract: "Interact with package collections",
        discussion: "SEE ALSO: swift build, swift package, swift run, swift test",
        version: SwiftVersion.current.completeDisplayString,
        subcommands: [
            Add.self,
            Describe.self,
            List.self,
            Refresh.self,
            Remove.self,
            Search.self,
        ],
        helpNames: [.short, .long, .customLong("help", withSingleDash: true)]
    )

    public init() {}

    // MARK: Collections

    struct List: SwiftCommand {
        static let configuration = CommandConfiguration(abstract: "List configured collections")

        @OptionGroup
        var jsonOptions: JSONOptions

        @OptionGroup(_hiddenFromHelp: true)
        var globalOptions: GlobalOptions

        func run(_ swiftTool: SwiftTool) throws {
            let collections = try with(swiftTool) { collections in
                try tsc_await { collections.listCollections(identifiers: nil, callback: $0) }
            }

            if self.jsonOptions.json {
                try JSONEncoder.makeWithDefaults().print(collections)
            } else {
                collections.forEach {
                    print("\($0.name) - \($0.source.url)")
                }
            }
        }
    }

    struct Refresh: SwiftCommand {
        static let configuration = CommandConfiguration(abstract: "Refresh configured collections")

        @OptionGroup(_hiddenFromHelp: true)
        var globalOptions: GlobalOptions

        func run(_ swiftTool: SwiftTool) throws {
            let collections = try with(swiftTool) { collections in
                try tsc_await { collections.refreshCollections(callback: $0) }
            }
            print("Refreshed \(collections.count) configured package collection\(collections.count == 1 ? "" : "s").")
        }
    }

    struct Add: SwiftCommand {
        static let configuration = CommandConfiguration(abstract: "Add a new collection")

        @Argument(help: "URL of the collection to add")
        var collectionURL: String

        @Option(name: .long, help: "Sort order for the added collection")
        var order: Int?

        @Flag(name: .long, help: "Trust the collection even if it is unsigned")
        var trustUnsigned: Bool = false

        @Flag(name: .long, help: "Skip signature check if the collection is signed")
        var skipSignatureCheck: Bool = false

        @OptionGroup(_hiddenFromHelp: true)
        var globalOptions: GlobalOptions

        func run(_ swiftTool: SwiftTool) throws {
            let collectionURL = try url(self.collectionURL)

            let source = PackageCollectionsModel.CollectionSource(type: .json, url: collectionURL, skipSignatureCheck: self.skipSignatureCheck)
            let collection: PackageCollectionsModel.Collection = try with(swiftTool) { collections in
                do {
                    let userTrusted = self.trustUnsigned
                    return try tsc_await {
                        collections.addCollection(
                            source,
                            order: order,
                            trustConfirmationProvider: { _, callback in callback(userTrusted) },
                            callback: $0
                        )
                    }
                } catch PackageCollectionError.trustConfirmationRequired, PackageCollectionError.untrusted {
                    throw CollectionsError.unsigned
                } catch PackageCollectionError.cannotVerifySignature {
                    throw CollectionsError.cannotVerifySignature
                } catch PackageCollectionError.invalidSignature {
                    throw CollectionsError.invalidSignature
                } catch PackageCollectionError.missingSignature {
                    throw CollectionsError.missingSignature
                }
            }

            print("Added \"\(collection.name)\" to your package collections.")
        }
    }

    struct Remove: SwiftCommand {
        static let configuration = CommandConfiguration(abstract: "Remove a configured collection")

        @Argument(help: "URL of the collection to remove")
        var collectionURL: String

        @OptionGroup(_hiddenFromHelp: true)
        var globalOptions: GlobalOptions

        func run(_ swiftTool: SwiftTool) throws {
            let collectionURL = try url(self.collectionURL)

            let source = PackageCollectionsModel.CollectionSource(type: .json, url: collectionURL)
            try with(swiftTool) { collections in
                let collection = try tsc_await { collections.getCollection(source, callback: $0) }
                _ = try tsc_await { collections.removeCollection(source, callback: $0) }
                print("Removed \"\(collection.name)\" from your package collections.")
            }
        }
    }

    // MARK: Search

    enum SearchMethod: String, EnumerableFlag {
        case keywords
        case module
    }

    struct Search: SwiftCommand {
        static var configuration = CommandConfiguration(abstract: "Search for packages by keywords or module names")

        @OptionGroup
        var jsonOptions: JSONOptions

        @Flag(help: "Pick the method for searching")
        var searchMethod: SearchMethod

        @Argument(help: "Search query")
        var searchQuery: String

        @OptionGroup(_hiddenFromHelp: true)
        var globalOptions: GlobalOptions

        func run(_ swiftTool: SwiftTool) throws {
            try with(swiftTool) { collections in
                switch searchMethod {
                case .keywords:
                    let results = try tsc_await { collections.findPackages(searchQuery, collections: nil, callback: $0) }

                    if jsonOptions.json {
                        try JSONEncoder.makeWithDefaults().print(results.items)
                    } else {
                        results.items.forEach {
                            print("\($0.package.identity): \($0.package.summary ?? "")")
                        }
                    }

                case .module:
                    let results = try tsc_await { collections.findTargets(searchQuery, searchType: .exactMatch, collections: nil, callback: $0) }

                    let packages = Set(results.items.flatMap { $0.packages })
                    if jsonOptions.json {
                        try JSONEncoder.makeWithDefaults().print(packages)
                    } else {
                        packages.forEach {
                            print("\($0.identity): \($0.summary ?? "")")
                        }
                    }
                }
            }
        }
    }

    // MARK: Packages

    struct Describe: SwiftCommand {
        static var configuration = CommandConfiguration(abstract: "Get metadata for a collection or a package included in an imported collection")

        @OptionGroup
        var jsonOptions: JSONOptions

        @Argument(help: "URL of the package or collection to get information for")
        var packageURL: String

        @Option(name: .long, help: "Version of the package to get information for")
        var version: String?

        @Flag(name: .long, help: "Skip signature check if the collection is signed")
        var skipSignatureCheck: Bool = false

        @OptionGroup(_hiddenFromHelp: true)
        var globalOptions: GlobalOptions

        private func printVersion(_ version: PackageCollectionsModel.Package.Version?) -> String? {
            guard let version = version else {
                return nil
            }
            guard let defaultManifest = version.defaultManifest else {
                return nil
            }

            let manifests = version.manifests.values.filter { $0.toolsVersion != version.defaultToolsVersion }.map { printManifest($0) }.joined(separator: "\n")
            let compatibility = optionalRow(
                "Verified Compatibility (Platform, Swift Version)",
                version.verifiedCompatibility?.map { "(\($0.platform.name), \($0.swiftVersion.rawValue))" }.joined(separator: ", ")
            )
            let license = optionalRow("License", version.license?.type.description)

            return """
            \(version.version)
            \(self.printManifest(defaultManifest))\(manifests)\(compatibility)\(license)
            """
        }

        private func printManifest(_ manifest: PackageCollectionsModel.Package.Version.Manifest) -> String {
            let modules = manifest.targets.compactMap { $0.moduleName }.joined(separator: ", ")
            let products = optionalRow("Products", manifest.products.isEmpty ? nil : manifest.products.compactMap { $0.name }.joined(separator: ", "), indentationLevel: 3)

            return """
                    Tools Version: \(manifest.toolsVersion.description)
                        Package Name: \(manifest.packageName)
                        Modules: \(modules)\(products)
            """
        }

        func run(_ swiftTool: SwiftTool) throws {
            try with(swiftTool) { collections in
                let identity = PackageIdentity(urlString: self.packageURL)

                do { // assume URL is for a package in an imported collection
                    let result = try tsc_await { collections.getPackageMetadata(identity: identity, location: self.packageURL, callback: $0) }

                    if let versionString = version {
                        guard let version = TSCUtility.Version(versionString), let result = result.package.versions.first(where: { $0.version == version }), let printedResult = printVersion(result) else {
                            throw CollectionsError.invalidVersionString(versionString)
                        }

                        if jsonOptions.json {
                            try JSONEncoder.makeWithDefaults().print(result)
                        } else {
                            print("\(indent())Version: \(printedResult)")
                        }
                    } else {
                        let description = optionalRow("Description", result.package.summary)
                        let versions = result.package.versions.map { "\($0.version)" }.joined(separator: ", ")
                        let stars = optionalRow("Stars", result.package.watchersCount?.description)
                        let readme = optionalRow("Readme", result.package.readmeURL?.absoluteString)
                        let authors = optionalRow("Authors", result.package.authors?.map { $0.username }.joined(separator: ", "))
                        let license =  optionalRow("License", result.package.license.map { "\($0.type) (\($0.url))" })
                        let languages = optionalRow("Languages", result.package.languages?.joined(separator: ", "))
                        let latestVersion = optionalRow("\(String(repeating: "-", count: 60))\n\(indent())Latest Version", printVersion(result.package.latestVersion))

                        if jsonOptions.json {
                            try JSONEncoder.makeWithDefaults().print(result.package)
                        } else {
                            print("""
                                \(description)
                                Available Versions: \(versions)\(readme)\(license)\(authors)\(stars)\(languages)\(latestVersion)
                            """)
                        }
                    }
                } catch { // assume URL is for a collection
                    // If a version argument was given, we do not perform the fallback.
                    if version != nil {
                        throw error
                    }

                    let collectionURL = try url(self.packageURL)

                    do {
                        let source = PackageCollectionsModel.CollectionSource(type: .json, url: collectionURL, skipSignatureCheck: self.skipSignatureCheck)
                        let collection = try tsc_await { collections.getCollection(source, callback: $0) }

                        let description = optionalRow("Description", collection.overview)
                        let keywords = optionalRow("Keywords", collection.keywords?.joined(separator: ", "))
                        let createdAt = optionalRow("Created At", DateFormatter().string(from: collection.createdAt))
                        let packages = collection.packages.map { "\($0.identity)" }.joined(separator: "\n\(indent(levels: 2))")

                        if jsonOptions.json {
                            try JSONEncoder.makeWithDefaults().print(collection)
                        } else {
                            let signature = optionalRow("Signed By", collection.signature.map { "\($0.certificate.subject.commonName ?? "Unspecified") (\($0.isVerified ? "" : "not ")verified)" })

                            print("""
                                Name: \(collection.name)
                                Source: \(collection.source.url)\(description)\(keywords)\(createdAt)
                                Packages:
                                    \(packages)\(signature)
                            """)
                        }
                    } catch PackageCollectionError.cannotVerifySignature {
                        throw CollectionsError.cannotVerifySignature
                    } catch PackageCollectionError.invalidSignature {
                        throw CollectionsError.invalidSignature
                    } catch {
                        print("Failed to get metadata. The given URL either belongs to a collection that is invalid or unavailable, or a package that is not found in any of the imported collections.")
                    }
                }
            }
        }
    }
}

private func indent(levels: Int = 1) -> String {
    return String(repeating: "    ", count: levels)
}

private func optionalRow(_ title: String, _ contents: String?, indentationLevel: Int = 1) -> String {
    if let contents = contents, !contents.isEmpty {
        return "\n\(indent(levels: indentationLevel))\(title): \(contents)"
    } else {
        return ""
    }
}

private extension JSONEncoder {
    func print<T>(_ value: T) throws where T: Encodable {
        let jsonData = try self.encode(value)
        let jsonString = String(data: jsonData, encoding: .utf8)!
        Swift.print(jsonString)
    }
}

private extension ParsableCommand {
    func with<T>(_ swiftTool: SwiftTool, handler: (_ collections: PackageCollectionsProtocol) throws -> T) throws -> T {
        _ = try? swiftTool.getActiveWorkspace(emitDeprecatedConfigurationWarning: true)
        let collections = PackageCollections(
            configuration: .init(
                configurationDirectory: swiftTool.sharedConfigurationDirectory,
                cacheDirectory: swiftTool.sharedCacheDirectory
            ),
            fileSystem: swiftTool.fileSystem,
            observabilityScope: swiftTool.observabilityScope
        )
        defer {
            do {
                try collections.shutdown()
            } catch {
                Self.exit(withError: error)
            }
        }

        return try handler(collections)
    }

    func url(_ urlString: String) throws -> URL {
        guard let url = URL(string: urlString) else {
            let filePrefix = "file://"
            guard urlString.hasPrefix(filePrefix) else {
                throw CollectionsError.invalidArgument("collectionURL")
            }
            // URL(fileURLWithPath:) can handle whitespaces in path
            return URL(fileURLWithPath: String(urlString.dropFirst(filePrefix.count)))
        }
        return url
    }
}
