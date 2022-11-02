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
import OrderedCollections
import PackageGraph
import PackageLoading
import PackageModel
import SourceControl
import TSCBasic
import Workspace
import func XCTest.XCTFail

import enum TSCUtility.Git

@_exported import TSCTestSupport

#if os(macOS)
import class Foundation.Bundle
#endif

/// Test-helper function that runs a block of code on a copy of a test fixture
/// package.  The copy is made into a temporary directory, and the block is
/// given a path to that directory.  The block is permitted to modify the copy.
/// The temporary copy is deleted after the block returns.  The fixture name may
/// contain `/` characters, which are treated as path separators, exactly as if
/// the name were a relative path.
public func fixture(
    name: String,
    createGitRepo: Bool = true,
    file: StaticString = #file,
    line: UInt = #line,
    body: (AbsolutePath) throws -> Void
) throws {
    do {
        // Make a suitable test directory name from the fixture subpath.
        let fixtureSubpath = RelativePath(name)
        let copyName = fixtureSubpath.components.joined(separator: "_")

        // Create a temporary directory for the duration of the block.
        try withTemporaryDirectory(prefix: copyName) { tmpDirPath in

            defer {
                // Unblock and remove the tmp dir on deinit.
                try? localFileSystem.chmod(.userWritable, path: tmpDirPath, options: [.recursive])
                try? localFileSystem.removeFileTree(tmpDirPath)
            }

            // Construct the expected path of the fixture.
            // FIXME: This seems quite hacky; we should provide some control over where fixtures are found.
            let fixtureDir = AbsolutePath("../../../Fixtures", relativeTo: AbsolutePath(#file)).appending(fixtureSubpath)

            // Check that the fixture is really there.
            guard localFileSystem.isDirectory(fixtureDir) else {
                XCTFail("No such fixture: \(fixtureDir)", file: file, line: line)
                return
            }

            // The fixture contains either a checkout or just a Git directory.
            if localFileSystem.isFile(fixtureDir.appending(component: "Package.swift")) {
                // It's a single package, so copy the whole directory as-is.
                let dstDir = tmpDirPath.appending(component: copyName)
                try systemQuietly("cp", "-R", "-H", fixtureDir.pathString, dstDir.pathString)

                // Invoke the block, passing it the path of the copied fixture.
                try body(dstDir)
            } else {
                // Copy each of the package directories and construct a git repo in it.
                for fileName in try localFileSystem.getDirectoryContents(fixtureDir).sorted() {
                    let srcDir = fixtureDir.appending(component: fileName)
                    guard localFileSystem.isDirectory(srcDir) else { continue }
                    let dstDir = tmpDirPath.appending(component: fileName)
                    try systemQuietly("cp", "-R", "-H", srcDir.pathString, dstDir.pathString)
                    if createGitRepo {
                        initGitRepo(dstDir, tag: "1.2.3", addFile: false)
                    }
                }

                // Invoke the block, passing it the path of the copied fixture.
                try body(tmpDirPath)
            }
        }
    } catch SwiftPMProductError.executionFailure(let error, let output, let stderr) {
        print("**** FAILURE EXECUTING SUBPROCESS ****")
        print("output:", output)
        print("stderr:", stderr)
        throw error
    }
}

/// Test-helper function that creates a new Git repository in a directory.  The new repository will contain
/// exactly one empty file unless `addFile` is `false`, and if a tag name is provided, a tag with that name will be
/// created.
public func initGitRepo(
    _ dir: AbsolutePath,
    tag: String? = nil,
    addFile: Bool = true,
    file: StaticString = #file,
    line: UInt = #line
) {
    initGitRepo(dir, tags: tag.flatMap { [$0] } ?? [], addFile: addFile, file: file, line: line)
}

public func initGitRepo(
    _ dir: AbsolutePath,
    tags: [String],
    addFile: Bool = true,
    file: StaticString = #file,
    line: UInt = #line
) {
    do {
        if addFile {
            let file = dir.appending(component: "file.swift")
            try localFileSystem.writeFileContents(file, bytes: "")
        }

        try systemQuietly([Git.tool, "-C", dir.pathString, "init"])
        try systemQuietly([Git.tool, "-C", dir.pathString, "config", "user.email", "example@example.com"])
        try systemQuietly([Git.tool, "-C", dir.pathString, "config", "user.name", "Example Example"])
        try systemQuietly([Git.tool, "-C", dir.pathString, "config", "commit.gpgsign", "false"])
        let repo = GitRepository(path: dir)
        try repo.stageEverything()
        try repo.commit(message: "msg")
        for tag in tags {
            try repo.tag(name: tag)
        }
        try systemQuietly([Git.tool, "-C", dir.pathString, "branch", "-m", "main"])
    } catch {
        XCTFail("\(error)", file: file, line: line)
    }
}

@discardableResult
public func executeSwiftBuild(
    _ packagePath: AbsolutePath,
    configuration: Configuration = .Debug,
    extraArgs: [String] = [],
    Xcc: [String] = [],
    Xld: [String] = [],
    Xswiftc: [String] = [],
    env: EnvironmentVariables? = nil
) throws -> (stdout: String, stderr: String) {
    let args = swiftArgs(configuration: configuration, extraArgs: extraArgs, Xcc: Xcc, Xld: Xld, Xswiftc: Xswiftc)
    return try SwiftPMProduct.SwiftBuild.execute(args, packagePath: packagePath, env: env)
}

@discardableResult
public func executeSwiftRun(
    _ packagePath: AbsolutePath,
    _ executable: String,
    configuration: Configuration = .Debug,
    extraArgs: [String] = [],
    Xcc: [String] = [],
    Xld: [String] = [],
    Xswiftc: [String] = [],
    env: EnvironmentVariables? = nil
) throws -> (stdout: String, stderr: String) {
    var args = swiftArgs(configuration: configuration, extraArgs: extraArgs, Xcc: Xcc, Xld: Xld, Xswiftc: Xswiftc)
    args.append(executable)
    return try SwiftPMProduct.SwiftRun.execute(args, packagePath: packagePath, env: env)
}

@discardableResult
public func executeSwiftPackage(
    _ packagePath: AbsolutePath,
    configuration: Configuration = .Debug,
    extraArgs: [String] = [],
    Xcc: [String] = [],
    Xld: [String] = [],
    Xswiftc: [String] = [],
    env: EnvironmentVariables? = nil
) throws -> (stdout: String, stderr: String) {
    let args = swiftArgs(configuration: configuration, extraArgs: extraArgs, Xcc: Xcc, Xld: Xld, Xswiftc: Xswiftc)
    return try SwiftPMProduct.SwiftPackage.execute(args, packagePath: packagePath, env: env)
}

@discardableResult
public func executeSwiftTest(
    _ packagePath: AbsolutePath,
    configuration: Configuration = .Debug,
    extraArgs: [String] = [],
    Xcc: [String] = [],
    Xld: [String] = [],
    Xswiftc: [String] = [],
    env: EnvironmentVariables? = nil
) throws -> (stdout: String, stderr: String) {
    let args = swiftArgs(configuration: configuration, extraArgs: extraArgs, Xcc: Xcc, Xld: Xld, Xswiftc: Xswiftc)
    return try SwiftPMProduct.SwiftTest.execute(args, packagePath: packagePath, env: env)
}

private func swiftArgs(
    configuration: Configuration,
    extraArgs: [String],
    Xcc: [String],
    Xld: [String],
    Xswiftc: [String]
) -> [String] {
    var args = ["--configuration"]
    switch configuration {
    case .Debug:
        args.append("debug")
    case .Release:
        args.append("release")
    }

    args += extraArgs
    args += Xcc.flatMap { ["-Xcc", $0] }
    args += Xld.flatMap { ["-Xlinker", $0] }
    args += Xswiftc.flatMap { ["-Xswiftc", $0] }
    return args
}

public func loadPackageGraph(
    identityResolver: IdentityResolver = DefaultIdentityResolver(),
    fileSystem: FileSystem,
    manifests: [Manifest],
    binaryArtifacts: [PackageIdentity: [String: BinaryArtifact]] = [:],
    explicitProduct: String? = .none,
    shouldCreateMultipleTestProducts: Bool = false,
    createREPLProduct: Bool = false,
    useXCBuildFileRules: Bool = false,
    customXCTestMinimumDeploymentTargets: [PackageModel.Platform: PlatformVersion]? = .none,
    observabilityScope: ObservabilityScope
) throws -> PackageGraph {
    let rootManifests = manifests.filter { $0.packageKind.isRoot }.spm_createDictionary{ ($0.path, $0) }
    let externalManifests = try manifests.filter { !$0.packageKind.isRoot }.reduce(into: OrderedCollections.OrderedDictionary<PackageIdentity, (manifest: Manifest, fs: FileSystem)>()) { partial, item in
        partial[try identityResolver.resolveIdentity(for: item.packageKind)] = (item, fileSystem)
    }

    let packages = Array(rootManifests.keys)
    let input = PackageGraphRootInput(packages: packages)
    let graphRoot = PackageGraphRoot(input: input, manifests: rootManifests, explicitProduct: explicitProduct)

    return try PackageGraph.load(
        root: graphRoot,
        identityResolver: identityResolver,
        additionalFileRules: useXCBuildFileRules ? FileRuleDescription.xcbuildFileTypes : FileRuleDescription.swiftpmFileTypes,
        externalManifests: externalManifests,
        binaryArtifacts: binaryArtifacts,
        shouldCreateMultipleTestProducts: shouldCreateMultipleTestProducts,
        createREPLProduct: createREPLProduct,
        customXCTestMinimumDeploymentTargets: customXCTestMinimumDeploymentTargets,
        fileSystem: fileSystem,
        observabilityScope: observabilityScope
    )
}

public let emptyZipFile = ByteString([0x80, 0x75, 0x05, 0x06] + [UInt8](repeating: 0x00, count: 18))
