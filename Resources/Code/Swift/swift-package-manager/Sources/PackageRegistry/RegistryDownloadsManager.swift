//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift open source project
//
// Copyright (c) 2022 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import Basics
import Dispatch
import Foundation
import PackageModel
import TSCBasic
import PackageLoading

public class RegistryDownloadsManager: Cancellable {
    public typealias Delegate = RegistryDownloadsManagerDelegate

    private let fileSystem: FileSystem
    private let path: AbsolutePath
    private let cachePath: AbsolutePath?
    private let registryClient: RegistryClient
    private let checksumAlgorithm: HashAlgorithm
    private let delegate: Delegate?

    private var pendingLookups = [PackageIdentity: DispatchGroup]()
    private var pendingLookupsLock = NSLock()

    public init(
        fileSystem: FileSystem,
        path: AbsolutePath,
        cachePath: AbsolutePath?,
        registryClient: RegistryClient,
        checksumAlgorithm: HashAlgorithm,
        delegate: Delegate?
    ) {
        self.fileSystem = fileSystem
        self.path = path
        self.cachePath = cachePath
        self.registryClient = registryClient
        self.checksumAlgorithm = checksumAlgorithm
        self.delegate = delegate
    }

    public func lookup(
        package: PackageIdentity,
        version: Version,
        observabilityScope: ObservabilityScope,
        delegateQueue: DispatchQueue,
        callbackQueue: DispatchQueue,
        completion: @escaping  (Result<AbsolutePath, Error>) -> Void
    ) {
        // wrap the callback in the requested queue
        let completion = { result in callbackQueue.async { completion(result) } }
        
        let packageRelativePath: RelativePath
        let packagePath: AbsolutePath

        do {
            packageRelativePath = try package.downloadPath(version: version)
            packagePath = self.path.appending(packageRelativePath)

            // TODO: we can do some finger-print checking to improve the validation
            // already exists and valid, we can exit early
            if try self.fileSystem.validPackageDirectory(packagePath) {
                return completion(.success(packagePath))
            }
        } catch {
            return completion(.failure(error))
        }

        // next we check if there is a pending lookup
        self.pendingLookupsLock.lock()
        if let pendingLookup = self.pendingLookups[package] {
            self.pendingLookupsLock.unlock()
            // chain onto the pending lookup
            pendingLookup.notify(queue: callbackQueue) {
                // at this point the previous lookup should be complete and we can re-lookup
                self.lookup(
                    package: package,
                    version: version,
                    observabilityScope: observabilityScope,
                    delegateQueue: delegateQueue,
                    callbackQueue: callbackQueue,
                    completion: completion
                )
            }
        } else {
            // record the pending lookup
            assert(self.pendingLookups[package] == nil)
            let group = DispatchGroup()
            group.enter()
            self.pendingLookups[package] = group
            self.pendingLookupsLock.unlock()

            // inform delegate that we are starting to fetch
            // calculate if cached (for delegate call) outside queue as it may change while queue is processing
            let isCached = self.cachePath.map{ self.fileSystem.exists($0.appending(packageRelativePath)) } ?? false
            delegateQueue.async {
                let details = FetchDetails(fromCache: isCached, updatedCache: false)
                self.delegate?.willFetch(package: package, version: version, fetchDetails: details)
            }

            // make sure destination is free.
            try? self.fileSystem.removeFileTree(packagePath)

            let start = DispatchTime.now()
            self.downloadAndPopulateCache(
                package: package,
                version: version,
                packagePath: packagePath,
                observabilityScope: observabilityScope,
                delegateQueue: delegateQueue,
                callbackQueue: callbackQueue
            ) { result in
                // inform delegate that we finished to fetch
                let duration = start.distance(to: .now())
                delegateQueue.async {
                    self.delegate?.didFetch(package: package, version: version, result: result, duration: duration)
                }
                // remove the pending lookup
                self.pendingLookupsLock.lock()
                self.pendingLookups[package]?.leave()
                self.pendingLookups[package] = nil
                self.pendingLookupsLock.unlock()
                // and done
                completion(result.map{ _ in packagePath })
            }
        }
    }

    /// Cancel any outstanding requests
    public func cancel(deadline: DispatchTime) throws {
        try self.registryClient.cancel(deadline: deadline)
    }

    private func downloadAndPopulateCache(
        package: PackageIdentity,
        version: Version,
        packagePath: AbsolutePath,
        observabilityScope: ObservabilityScope,
        delegateQueue: DispatchQueue,
        callbackQueue: DispatchQueue,
        completion: @escaping  (Result<FetchDetails, Error>) -> Void
    ) {
        if let cachePath = self.cachePath {
            do {
                let relativePath = try package.downloadPath(version: version)
                let cachedPackagePath = cachePath.appending(relativePath)

                try self.initializeCacheIfNeeded(cachePath: cachePath)
                try self.fileSystem.withLock(on: cachedPackagePath, type: .exclusive) {
                    // download the package into the cache unless already exists
                    if try self.fileSystem.validPackageDirectory(cachedPackagePath) {
                        // extra validation to defend from racy edge cases
                        if self.fileSystem.exists(packagePath) {
                            throw StringError("\(packagePath) already exists unexpectedly")
                        }
                        // copy the package from the cache into the package path.
                        try self.fileSystem.createDirectory(packagePath.parentDirectory, recursive: true)
                        try self.fileSystem.copy(from: cachedPackagePath, to: packagePath)
                        completion(.success(.init(fromCache: true, updatedCache: false)))
                    } else {
                        // it is possible that we already created the directory before from failed attempts, so clear leftover data if present.
                        try? self.fileSystem.removeFileTree(cachedPackagePath)
                        // download the package from the registry
                        self.registryClient.downloadSourceArchive(
                            package: package,
                            version: version,
                            fileSystem: self.fileSystem,
                            destinationPath: cachedPackagePath,
                            checksumAlgorithm: self.checksumAlgorithm,
                            progressHandler: updateDownloadProgress,
                            observabilityScope: observabilityScope,
                            callbackQueue: callbackQueue
                        ) { result in
                            completion(result.tryMap {
                                // extra validation to defend from racy edge cases
                                if self.fileSystem.exists(packagePath) {
                                    throw StringError("\(packagePath) already exists unexpectedly")
                                }
                                // copy the package from the cache into the package path.
                                try self.fileSystem.createDirectory(packagePath.parentDirectory, recursive: true)
                                try self.fileSystem.copy(from: cachedPackagePath, to: packagePath)
                                return FetchDetails(fromCache: true, updatedCache: true)
                            })
                        }
                    }
                }
            } catch {
                // download without populating the cache in the case of an error.
                observabilityScope.emit(warning: "skipping cache due to an error: \(error)")
                // it is possible that we already created the directory from failed attempts, so clear leftover data if present.
                try? self.fileSystem.removeFileTree(packagePath)
                self.registryClient.downloadSourceArchive(
                    package: package,
                    version: version,
                    fileSystem: self.fileSystem,
                    destinationPath: packagePath,
                    checksumAlgorithm: self.checksumAlgorithm,
                    progressHandler: updateDownloadProgress,
                    observabilityScope: observabilityScope,
                    callbackQueue: callbackQueue
                ) { result in
                    completion(result.map{ FetchDetails(fromCache: false, updatedCache: false) })
                }
            }
        } else {
            // it is possible that we already created the directory from failed attempts, so clear leftover data if present.
            try? self.fileSystem.removeFileTree(packagePath)
            // download without populating the cache when no `cachePath` is set.
            self.registryClient.downloadSourceArchive(
                package: package,
                version: version,
                fileSystem: self.fileSystem,
                destinationPath: packagePath,
                checksumAlgorithm: self.checksumAlgorithm,
                progressHandler: updateDownloadProgress,
                observabilityScope: observabilityScope,
                callbackQueue: callbackQueue
            ) { result in
                completion(result.map{ FetchDetails(fromCache: false, updatedCache: false) })
            }
        }

        // utility to update progress

        func updateDownloadProgress(downloaded: Int64, total: Int64?) -> Void {
            delegateQueue.async {
                self.delegate?.fetching(
                    package: package,
                    version: version,
                    bytesDownloaded: downloaded,
                    totalBytesToDownload: total
                )
            }
        }
    }

    public func remove(package: PackageIdentity) throws {
        let relativePath = try package.downloadPath()
        let packagesPath = self.path.appending(relativePath)
        try self.fileSystem.removeFileTree(packagesPath)
    }

    public func reset() throws {
        try self.fileSystem.removeFileTree(self.path)
    }

    public func purgeCache() throws {
        guard let cachePath = self.cachePath else {
            return
        }
        try self.fileSystem.withLock(on: cachePath, type: .exclusive) {
            let cachedPackages = try self.fileSystem.getDirectoryContents(cachePath)
            for packagePath in cachedPackages {
                try self.fileSystem.removeFileTree(cachePath.appending(component: packagePath))
            }
        }
    }

    private func initializeCacheIfNeeded(cachePath: AbsolutePath) throws {
        if !self.fileSystem.exists(cachePath) {
            try self.fileSystem.createDirectory(cachePath, recursive: true)
        }
    }
}

/// Delegate to notify clients about actions being performed by RegistryManager.
public protocol RegistryDownloadsManagerDelegate {
    /// Called when a package is about to be fetched.
    func willFetch(package: PackageIdentity, version: Version, fetchDetails: RegistryDownloadsManager.FetchDetails)

    /// Called when a package has finished fetching.
    func didFetch(package: PackageIdentity, version: Version, result: Result<RegistryDownloadsManager.FetchDetails, Error>, duration: DispatchTimeInterval)

    /// Called every time the progress of a repository fetch operation updates.
    func fetching(package: PackageIdentity, version: Version, bytesDownloaded: Int64, totalBytesToDownload: Int64?)
}

extension RegistryDownloadsManager {
    /// Additional information about a fetch
    public struct FetchDetails: Equatable {
        /// Indicates if the repository was fetched from the cache or from the remote.
        public let fromCache: Bool
        /// Indicates wether the wether the repository was already present in the cache and updated or if a clean fetch was performed.
        public let updatedCache: Bool
    }
}


extension FileSystem {
    func validPackageDirectory(_ path: AbsolutePath) throws -> Bool {
        if !self.exists(path) {
            return false
        }
        return try self.getDirectoryContents(path).contains(Manifest.filename)
    }
}


extension PackageIdentity {
    internal func downloadPath() throws -> RelativePath {
        guard let (scope, name) = self.scopeAndName else {
            throw StringError("invalid package identity, expected registry scope and name")
        }
        return RelativePath(scope.description).appending(component: name.description)
    }

    internal func downloadPath(version: Version) throws -> RelativePath  {
        try self.downloadPath().appending(component: version.description)
    }
}
