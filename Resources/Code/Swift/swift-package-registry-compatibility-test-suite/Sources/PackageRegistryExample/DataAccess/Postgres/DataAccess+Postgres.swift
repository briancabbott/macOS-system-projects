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

import NIO
import PostgresKit
import PostgresMigrations

struct PostgresDataAccess: DataAccess {
    typealias Configuration = PostgresDataAccessConfiguration

    private let connectionPool: EventLoopGroupConnectionPool<PostgresConnectionSource>

    private let _packageReleases: PackageReleasesDAO
    private let _packageResources: PackageResourcesDAO
    private let _packageManifests: PackageManifestsDAO

    var packageReleases: PackageReleasesDAO { self._packageReleases }
    var packageResources: PackageResourcesDAO { self._packageResources }
    var packageManifests: PackageManifestsDAO { self._packageManifests }

    init(eventLoopGroup: EventLoopGroup, configuration config: PostgresDataAccessConfiguration) {
        let tls = config.tls ? TLSConfiguration.clientDefault : nil
        let configuration = PostgresConfiguration(hostname: config.host,
                                                  port: config.port,
                                                  username: config.username,
                                                  password: config.password,
                                                  database: config.database,
                                                  tlsConfiguration: tls)
        self.connectionPool = EventLoopGroupConnectionPool(source: PostgresConnectionSource(configuration: configuration), on: eventLoopGroup)

        self._packageResources = PackageResources(self.connectionPool)
        self._packageManifests = PackageManifests(self.connectionPool)
        self._packageReleases = PackageReleases(self.connectionPool, packageResources: self._packageResources, packageManifests: self._packageManifests)
    }

    func migrate() async throws {
        _ = try await DatabaseMigrations.Postgres(self.connectionPool).apply()
    }

    func shutdown() {
        self.connectionPool.shutdown()
    }
}

protocol PostgresDataAccessConfiguration {
    var host: String { get }
    var port: Int { get }
    var tls: Bool { get }
    var database: String { get }
    var username: String { get }
    var password: String { get }
}
