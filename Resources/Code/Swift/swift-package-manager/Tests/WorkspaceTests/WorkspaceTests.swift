//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift open source project
//
// Copyright (c) 2014-2021 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See http://swift.org/LICENSE.txt for license information
// See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import Basics
import PackageFingerprint
import PackageGraph
import PackageLoading
import PackageModel
import PackageRegistry
import SourceControl
import SPMBuildCore
import SPMTestSupport
import TSCBasic
@testable import Workspace
import XCTest

import enum TSCUtility.Diagnostics
import struct TSCUtility.Triple

final class WorkspaceTests: XCTestCase {
    func testBasics() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar"]),
                        MockTarget(name: "Bar", dependencies: ["Baz"]),
                        MockTarget(name: "BarTests", dependencies: ["Bar"], type: .test),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo", "Bar"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0", "1.5.0"]
                ),
                MockPackage(
                    name: "Quix",
                    targets: [
                        MockTarget(name: "Quix"),
                    ],
                    products: [
                        MockProduct(name: "Quix", targets: ["Quix"]),
                    ],
                    versions: ["1.0.0", "1.2.0"]
                ),
            ]
        )

        let deps: [MockDependency] = [
            .sourceControl(path: "./Quix", requirement: .upToNextMajor(from: "1.0.0"), products: .specific(["Quix"])),
            .sourceControl(path: "./Baz", requirement: .exact("1.0.0"), products: .specific(["Baz"])),
        ]
        try workspace.checkPackageGraph(roots: ["Foo"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Foo")
                result.check(packages: "Baz", "Foo", "Quix")
                result.check(targets: "Bar", "Baz", "Foo", "Quix")
                result.check(testModules: "BarTests")
                result.checkTarget("Foo") { result in result.check(dependencies: "Bar") }
                result.checkTarget("Bar") { result in result.check(dependencies: "Baz") }
                result.checkTarget("BarTests") { result in result.check(dependencies: "Bar") }
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "baz", at: .checkout(.version("1.0.0")))
            result.check(dependency: "quix", at: .checkout(.version("1.2.0")))
        }

        // Check the load-package callbacks.
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for root package: \(sandbox.appending(components: "roots", "Foo"))"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for root package: \(sandbox.appending(components: "roots", "Foo"))"])

        XCTAssertMatch(workspace.delegate.events, ["will load manifest for localSourceControl package: \(sandbox.appending(components: "pkgs", "Quix"))"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for localSourceControl package: \(sandbox.appending(components: "pkgs", "Quix"))"])
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for localSourceControl package: \(sandbox.appending(components: "pkgs", "Baz"))"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for localSourceControl package: \(sandbox.appending(components: "pkgs", "Baz"))"])

        // Close and reopen workspace.
        try workspace.closeWorkspace(resetState: false)
        workspace.checkManagedDependencies { result in
            result.check(dependency: "baz", at: .checkout(.version("1.0.0")))
            result.check(dependency: "quix", at: .checkout(.version("1.2.0")))
        }

        let stateFile = try workspace.getOrCreateWorkspace().state.storagePath

        // Remove state file and check we can get the state back automatically.
        try fs.removeFileTree(stateFile)

        try workspace.checkPackageGraph(roots: ["Foo"], deps: deps) { _, _ in }
        XCTAssertTrue(fs.exists(stateFile), "workspace state file should exist")

        // Remove state file and check we get back to a clean state.
        try fs.removeFileTree(workspace.getOrCreateWorkspace().state.storagePath)
        try workspace.closeWorkspace()
        workspace.checkManagedDependencies { result in
            result.checkEmpty()
        }
    }

    func testInterpreterFlags() throws {
        let fs = localFileSystem

        try testWithTemporaryDirectory { path in
            let foo = path.appending(component: "foo")

            func createWorkspace(withManifest manifest: (OutputByteStream) -> Void) throws -> Workspace {
                try fs.writeFileContents(foo.appending(component: "Package.swift")) {
                    manifest($0)
                }

                let manifestLoader = ManifestLoader(toolchain: UserToolchain.default)

                let sandbox = path.appending(component: "ws")
                return try Workspace(
                    fileSystem: fs,
                    forRootPackage: sandbox,
                    customManifestLoader: manifestLoader,
                    delegate: MockWorkspaceDelegate()
                )
            }

            do {
                let ws = try createWorkspace {
                    $0 <<<
                          """
                          // swift-tools-version:4.0
                          import PackageDescription
                          let package = Package(
                              name: "foo"
                          )
                          """
                }

                XCTAssertMatch(ws.interpreterFlags(for: foo), [.equal("-swift-version"), .equal("4")])
            }

            do {
                let ws = try createWorkspace {
                    $0 <<<
                          """
                          // swift-tools-version:3.1
                          import PackageDescription
                          let package = Package(
                              name: "foo"
                          )
                          """
                }

                XCTAssertEqual(ws.interpreterFlags(for: foo), [])
            }
        }
    }

    func testManifestParseError() throws {
        let observability = ObservabilitySystem.makeForTesting()

        try testWithTemporaryDirectory { path in
            let pkgDir = path.appending(component: "MyPkg")
            try localFileSystem.writeFileContents(pkgDir.appending(component: "Package.swift")) {
                $0 <<<
                    """
                    // swift-tools-version:4.0
                    import PackageDescription
                    #error("An error in MyPkg")
                    let package = Package(
                        name: "MyPkg"
                    )
                    """
            }
            let workspace = try Workspace(
                fileSystem: localFileSystem,
                forRootPackage: pkgDir,
                customManifestLoader: ManifestLoader(toolchain: UserToolchain.default),
                delegate: MockWorkspaceDelegate()
            )
            let rootInput = PackageGraphRootInput(packages: [pkgDir], dependencies: [])
            let rootManifests = try tsc_await {
                workspace.loadRootManifests(
                    packages: rootInput.packages,
                    observabilityScope: observability.topScope,
                    completion: $0
                )
            }

            XCTAssert(rootManifests.count == 0, "\(rootManifests)")

            testDiagnostics(observability.diagnostics) { result in
                let diagnostic = result.check(diagnostic: .prefix("Invalid manifest\n\(path.appending(components: "MyPkg", "Package.swift")):3:8: error: An error in MyPkg"), severity: .error)
                XCTAssertEqual(diagnostic?.metadata?.packageIdentity, .init(path: pkgDir))
                XCTAssertEqual(diagnostic?.metadata?.packageKind, .root(pkgDir))
            }
        }
    }

    func testMultipleRootPackages() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Baz"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar", dependencies: ["Baz"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Baz", requirement: .exact("1.0.1")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0", "1.0.1", "1.0.3", "1.0.5", "1.0.8"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Foo", "Bar"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Bar", "Foo")
                result.check(packages: "Bar", "Baz", "Foo")
                result.checkTarget("Foo") { result in result.check(dependencies: "Baz") }
                result.checkTarget("Bar") { result in result.check(dependencies: "Baz") }
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "baz", at: .checkout(.version("1.0.1")))
        }
    }

    func testRootPackagesOverride() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Baz"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "bazzz", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: []
                ),
                MockPackage(
                    name: "Baz",
                    path: "Overridden/bazzz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Baz",
                    path: "bazzz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0", "1.0.1", "1.0.3", "1.0.5", "1.0.8"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Foo", "Bar", "Overridden/bazzz"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Bar", "Foo", "Baz")
                result.check(packages: "Bar", "Baz", "Foo")
                result.checkTarget("Foo") { result in result.check(dependencies: "Baz") }
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testDuplicateRootPackages() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: []),
                    ],
                    products: [],
                    dependencies: []
                ),
                MockPackage(
                    name: "Foo",
                    path: "Nested/Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: []),
                    ],
                    products: [],
                    dependencies: []
                ),
            ],
            packages: []
        )

        workspace.checkPackageGraphFailure(roots: ["Foo", "Nested/Foo"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .equal("found multiple top-level packages named 'Foo'"), severity: .error)
            }
        }
    }

    /// Test that the explicit name given to a package is not used as its identity.
    func testExplicitPackageNameIsNotUsedAsPackageIdentity() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "FooPackage",
                    path: "foo-package",
                    targets: [
                        MockTarget(name: "FooTarget", dependencies: [.product(name: "BarProduct", package: "BarPackage")]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControlWithDeprecatedName(name: "BarPackage", path: "bar-package", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
                MockPackage(
                    name: "BarPackage",
                    path: "bar-package",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "1.0.1"]
                ),
            ],
            packages: [
                MockPackage(
                    name: "BarPackage",
                    path: "bar-package",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "1.0.1"]
                ),
            ]
        )

        try workspace.checkPackageGraph(
            roots: ["foo-package", "bar-package"],
            dependencies: [
                .localSourceControl(path: .init("/tmp/ws/pkgs/bar-package"), requirement: .upToNextMajor(from: "1.0.0"))
            ]
        ) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "FooPackage", "BarPackage")
                result.check(packages: "FooPackage", "BarPackage")
                result.checkTarget("FooTarget") { result in result.check(dependencies: "BarProduct") }
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    /// Test that the remote repository is not resolved when a root package with same name is already present.
    func testRootAsDependency1() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["BazAB"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "BazA"),
                        MockTarget(name: "BazB"),
                    ],
                    products: [
                        MockProduct(name: "BazAB", targets: ["BazA", "BazB"]),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Foo", "Baz"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Baz", "Foo")
                result.check(packages: "Baz", "Foo")
                result.check(targets: "BazA", "BazB", "Foo")
                result.checkTarget("Foo") { result in result.check(dependencies: "BazAB") }
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(notPresent: "baz")
        }
        XCTAssertNoMatch(workspace.delegate.events, [.equal("fetching package: /tmp/ws/pkgs/Baz")])
        XCTAssertNoMatch(workspace.delegate.events, [.equal("will resolve dependencies")])
    }

    /// Test that a root package can be used as a dependency when the remote version was resolved previously.
    func testRootAsDependency2() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Baz"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "BazA"),
                        MockTarget(name: "BazB"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["BazA", "BazB"]),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        // Load only Foo right now so Baz is loaded from remote.
        try workspace.checkPackageGraph(roots: ["Foo"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Foo")
                result.check(packages: "Baz", "Foo")
                result.check(targets: "Baz", "Foo")
                result.checkTarget("Foo") { result in result.check(dependencies: "Baz") }
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "baz", at: .checkout(.version("1.0.0")))
        }
        XCTAssertMatch(workspace.delegate.events, [.equal("fetching package: \(sandbox.appending(components: "pkgs", "Baz"))")])
        XCTAssertMatch(workspace.delegate.events, [.equal("will resolve dependencies")])

        // Now load with Baz as a root package.
        workspace.delegate.clear()
        try workspace.checkPackageGraph(roots: ["Foo", "Baz"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Baz", "Foo")
                result.check(packages: "Baz", "Foo")
                result.check(targets: "BazA", "BazB", "Foo")
                result.checkTarget("Foo") { result in result.check(dependencies: "Baz") }
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(notPresent: "baz")
        }
        XCTAssertNoMatch(workspace.delegate.events, [.equal("fetching package: \(sandbox.appending(components: "pkgs", "Baz"))")])
        XCTAssertNoMatch(workspace.delegate.events, [.equal("will resolve dependencies")])
        XCTAssertMatch(workspace.delegate.events, [.equal("removing repo: \(sandbox.appending(components: "pkgs", "Baz"))")])
    }

    func testGraphRootDependencies() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar"]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        let dependencies: [PackageDependency] = [
            .localSourceControl(
                path: workspace.packagesDir.appending(component: "Bar"),
                requirement: .upToNextMajor(from: "1.0.0"),
                productFilter: .specific(["Bar"])
            ),
            .localSourceControl(
                path: workspace.packagesDir.appending(component: "Foo"),
                requirement: .upToNextMajor(from: "1.0.0"),
                productFilter: .specific(["Foo"])
            ),
        ]

        try workspace.checkPackageGraph(dependencies: dependencies) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(packages: "Bar", "Foo")
                result.check(targets: "Bar", "Foo")
                result.checkTarget("Foo") { result in result.check(dependencies: "Bar") }
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }
    }

    func testCanResolveWithIncompatiblePins() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [],
            packages: [
                MockPackage(
                    name: "A",
                    targets: [
                        MockTarget(name: "A", dependencies: ["AA"]),
                    ],
                    products: [
                        MockProduct(name: "A", targets: ["A"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./AA", requirement: .exact("1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "A",
                    targets: [
                        MockTarget(name: "A", dependencies: ["AA"]),
                    ],
                    products: [
                        MockProduct(name: "A", targets: ["A"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./AA", requirement: .exact("2.0.0")),
                    ],
                    versions: ["1.0.1"]
                ),
                MockPackage(
                    name: "AA",
                    targets: [
                        MockTarget(name: "AA"),
                    ],
                    products: [
                        MockProduct(name: "AA", targets: ["AA"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        // Resolve when A = 1.0.0.
        do {
            let deps: [MockDependency] = [
                .sourceControl(path: "./A", requirement: .exact("1.0.0"), products: .specific(["A"])),
            ]
            try workspace.checkPackageGraph(deps: deps) { graph, diagnostics in
                PackageGraphTester(graph) { result in
                    result.check(packages: "A", "AA")
                    result.check(targets: "A", "AA")
                    result.checkTarget("A") { result in result.check(dependencies: "AA") }
                }
                XCTAssertNoDiagnostics(diagnostics)
            }
            workspace.checkManagedDependencies { result in
                result.check(dependency: "a", at: .checkout(.version("1.0.0")))
                result.check(dependency: "aa", at: .checkout(.version("1.0.0")))
            }
            workspace.checkResolved { result in
                result.check(dependency: "a", at: .checkout(.version("1.0.0")))
                result.check(dependency: "aa", at: .checkout(.version("1.0.0")))
            }
        }

        // Resolve when A = 1.0.1.
        do {
            let deps: [MockDependency] = [
                .sourceControl(path: "./A", requirement: .exact("1.0.1"), products: .specific(["A"])),
            ]
            try workspace.checkPackageGraph(deps: deps) { graph, diagnostics in
                PackageGraphTester(graph) { result in
                    result.checkTarget("A") { result in result.check(dependencies: "AA") }
                }
                XCTAssertNoDiagnostics(diagnostics)
            }
            workspace.checkManagedDependencies { result in
                result.check(dependency: "a", at: .checkout(.version("1.0.1")))
                result.check(dependency: "aa", at: .checkout(.version("2.0.0")))
            }
            workspace.checkResolved { result in
                result.check(dependency: "a", at: .checkout(.version("1.0.1")))
                result.check(dependency: "aa", at: .checkout(.version("2.0.0")))
            }
            XCTAssertMatch(workspace.delegate.events, [.equal("updating repo: \(sandbox.appending(components: "pkgs", "A"))")])
            XCTAssertMatch(workspace.delegate.events, [.equal("updating repo: \(sandbox.appending(components: "pkgs", "AA"))")])
            XCTAssertEqual(workspace.delegate.events.filter { $0.hasPrefix("updating repo") }.count, 2)
        }
    }

    func testResolverCanHaveError() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [],
            packages: [
                MockPackage(
                    name: "A",
                    targets: [
                        MockTarget(name: "A", dependencies: ["AA"]),
                    ],
                    products: [
                        MockProduct(name: "A", targets: ["A"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./AA", requirement: .exact("1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "B",
                    targets: [
                        MockTarget(name: "B", dependencies: ["AA"]),
                    ],
                    products: [
                        MockProduct(name: "B", targets: ["B"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./AA", requirement: .exact("2.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "AA",
                    targets: [
                        MockTarget(name: "AA"),
                    ],
                    products: [
                        MockProduct(name: "AA", targets: ["AA"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        let deps: [MockDependency] = [
            .sourceControl(path: "./A", requirement: .exact("1.0.0"), products: .specific(["A"])),
            .sourceControl(path: "./B", requirement: .exact("1.0.0"), products: .specific(["B"])),
        ]
        try workspace.checkPackageGraph(deps: deps) { _, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("Dependencies could not be resolved"), severity: .error)
            }
        }
        // There should be no extra fetches.
        XCTAssertNoMatch(workspace.delegate.events, [.contains("updating repo")])
    }

    func testPrecomputeResolution_empty() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let bPath = RelativePath("B")
        let v1_5 = CheckoutState.version("1.0.5", revision: Revision(identifier: "hello"))
        let v2 = CheckoutState.version("2.0.0", revision: Revision(identifier: "hello"))

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "A",
                    targets: [MockTarget(name: "A")],
                    products: []
                ),
            ],
            packages: []
        )

        let bPackagePath = workspace.pathToPackage(withName: "B")
        let bRef = PackageReference.localSourceControl(identity: PackageIdentity(path: bPackagePath), path: bPackagePath)

        let cPackagePath = workspace.pathToPackage(withName: "C")
        let cRef = PackageReference.localSourceControl(identity: PackageIdentity(path: cPackagePath), path: cPackagePath)

        try workspace.set(
            pins: [bRef: v1_5, cRef: v2],
            managedDependencies: [
                bPackagePath: .sourceControlCheckout(packageRef: bRef, state: v1_5, subpath: bPath)
                    .edited(subpath: bPath, unmanagedPath: .none),
            ]
        )

        try workspace.checkPrecomputeResolution { result in
            XCTAssertNoDiagnostics(result.diagnostics)
            XCTAssertEqual(result.result.isRequired, false)
        }
    }

    func testPrecomputeResolution_newPackages() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let bPath = RelativePath("B")
        let v1Requirement: SourceControlRequirement = .range("1.0.0" ..< "2.0.0")
        let v1 = CheckoutState.version("1.0.0", revision: Revision(identifier: "hello"))

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "A",
                    targets: [MockTarget(name: "A")],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./B", requirement: v1Requirement),
                        .sourceControl(path: "./C", requirement: v1Requirement),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "B",
                    targets: [MockTarget(name: "B")],
                    products: [MockProduct(name: "B", targets: ["B"])],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "C",
                    targets: [MockTarget(name: "C")],
                    products: [MockProduct(name: "C", targets: ["C"])],
                    versions: ["1.0.0"]
                ),
            ]
        )

        let bPackagePath = workspace.pathToPackage(withName: "B")
        let bRef = PackageReference.localSourceControl(identity: PackageIdentity(path: bPackagePath), path: bPackagePath)

        let cPackagePath = workspace.pathToPackage(withName: "C")
        let cRef = PackageReference.localSourceControl(identity: PackageIdentity(path: cPackagePath), path: cPackagePath)

        try workspace.set(
            pins: [bRef: v1],
            managedDependencies: [
                bPackagePath: .sourceControlCheckout(packageRef: bRef, state: v1, subpath: bPath)
            ]
        )

        try workspace.checkPrecomputeResolution { result in
            XCTAssertNoDiagnostics(result.diagnostics)
            XCTAssertEqual(result.result, .required(reason: .newPackages(packages: [cRef])))
        }
    }

    func testPrecomputeResolution_requirementChange_versionToBranch() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let bPath = RelativePath("B")
        let cPath = RelativePath("C")
        let v1Requirement: SourceControlRequirement = .range("1.0.0" ..< "2.0.0")
        let branchRequirement: SourceControlRequirement = .branch("master")
        let v1_5 = CheckoutState.version("1.0.5", revision: Revision(identifier: "hello"))

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "A",
                    targets: [MockTarget(name: "A")],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./B", requirement: v1Requirement),
                        .sourceControl(path: "./C", requirement: branchRequirement),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "B",
                    targets: [MockTarget(name: "B")],
                    products: [MockProduct(name: "B", targets: ["B"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
                MockPackage(
                    name: "C",
                    targets: [MockTarget(name: "C")],
                    products: [MockProduct(name: "C", targets: ["C"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
            ]
        )

        let bPackagePath = workspace.pathToPackage(withName: "B")
        let bRef = PackageReference.localSourceControl(identity: PackageIdentity(path: bPackagePath), path: bPackagePath)

        let cPackagePath = workspace.pathToPackage(withName: "C")
        let cRef = PackageReference.localSourceControl(identity: PackageIdentity(path: cPackagePath), path: cPackagePath)

        try workspace.set(
            pins: [bRef: v1_5, cRef: v1_5],
            managedDependencies: [
                bPackagePath: .sourceControlCheckout(packageRef: bRef, state: v1_5, subpath: bPath),
                cPackagePath: .sourceControlCheckout(packageRef: cRef, state: v1_5, subpath: cPath),
            ]
        )

        try workspace.checkPrecomputeResolution { result in
            XCTAssertNoDiagnostics(result.diagnostics)
            XCTAssertEqual(result.result, .required(reason: .packageRequirementChange(
                package: cRef,
                state: .sourceControlCheckout(v1_5),
                requirement: .revision("master")
            )))
        }
    }

    func testPrecomputeResolution_requirementChange_versionToRevision() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let cPath = RelativePath("C")
        let v1_5 = CheckoutState.version("1.0.5", revision: Revision(identifier: "hello"))

        let testWorkspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "A",
                    targets: [MockTarget(name: "A")],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./C", requirement: .revision("hello")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "C",
                    targets: [MockTarget(name: "C")],
                    products: [MockProduct(name: "C", targets: ["C"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
            ]
        )

        let cPackagePath = testWorkspace.pathToPackage(withName: "C")
        let cRef = PackageReference.localSourceControl(identity: PackageIdentity(path: cPackagePath), path: cPackagePath)

        try testWorkspace.set(
            pins: [cRef: v1_5],
            managedDependencies: [
                cPackagePath: .sourceControlCheckout(packageRef: cRef, state: v1_5, subpath: cPath),
            ]
        )

        try testWorkspace.checkPrecomputeResolution { result in
            XCTAssertNoDiagnostics(result.diagnostics)
            XCTAssertEqual(result.result, .required(reason: .packageRequirementChange(
                package: cRef,
                state: .sourceControlCheckout(v1_5),
                requirement: .revision("hello")
            )))
        }
    }

    func testPrecomputeResolution_requirementChange_localToBranch() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let bPath = RelativePath("B")
        let v1Requirement: SourceControlRequirement = .range("1.0.0" ..< "2.0.0")
        let masterRequirement: SourceControlRequirement = .branch("master")
        let v1_5 = CheckoutState.version("1.0.5", revision: Revision(identifier: "hello"))

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "A",
                    targets: [MockTarget(name: "A")],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./B", requirement: v1Requirement),
                        .sourceControl(path: "./C", requirement: masterRequirement),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "B",
                    targets: [MockTarget(name: "B")],
                    products: [MockProduct(name: "B", targets: ["B"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
                MockPackage(
                    name: "C",
                    targets: [MockTarget(name: "C")],
                    products: [MockProduct(name: "C", targets: ["C"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
            ]
        )

        let bPackagePath = workspace.pathToPackage(withName: "B")
        let bRef = PackageReference.localSourceControl(identity: PackageIdentity(path: bPackagePath), path: bPackagePath)

        let cPackagePath = workspace.pathToPackage(withName: "C")
        let cRef = PackageReference.localSourceControl(identity: PackageIdentity(path: cPackagePath), path: cPackagePath)

        try workspace.set(
            pins: [bRef: v1_5],
            managedDependencies: [
                bPackagePath: .sourceControlCheckout(packageRef: bRef, state: v1_5, subpath: bPath),
                cPackagePath: .fileSystem(packageRef: cRef),
            ]
        )

        try workspace.checkPrecomputeResolution { result in
            XCTAssertNoDiagnostics(result.diagnostics)
            XCTAssertEqual(result.result, .required(reason: .packageRequirementChange(
                package: cRef,
                state: .fileSystem(cPackagePath),
                requirement: .revision("master")
            )))
        }
    }

    func testPrecomputeResolution_requirementChange_versionToLocal() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let bPath = RelativePath("B")
        let cPath = RelativePath("C")
        let v1Requirement: SourceControlRequirement = .range("1.0.0" ..< "2.0.0")
        let v1_5 = CheckoutState.version("1.0.5", revision: Revision(identifier: "hello"))

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "A",
                    targets: [MockTarget(name: "A")],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./B", requirement: v1Requirement),
                        .fileSystem(path: "./C"),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "B",
                    targets: [MockTarget(name: "B")],
                    products: [MockProduct(name: "B", targets: ["B"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
                MockPackage(
                    name: "C",
                    targets: [MockTarget(name: "C")],
                    products: [MockProduct(name: "C", targets: ["C"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
            ]
        )

        let bPackagePath = workspace.pathToPackage(withName: "B")
        let bRef = PackageReference.localSourceControl(identity: PackageIdentity(path: bPackagePath), path: bPackagePath)

        let cPackagePath = workspace.pathToPackage(withName: "C")
        let cRef = PackageReference.localSourceControl(identity: PackageIdentity(path: cPackagePath), path: cPackagePath)

        try workspace.set(
            pins: [bRef: v1_5, cRef: v1_5],
            managedDependencies: [
                bPackagePath: .sourceControlCheckout(packageRef: bRef, state: v1_5, subpath: bPath),
                cPackagePath: .sourceControlCheckout(packageRef: cRef, state: v1_5, subpath: cPath),
            ]
        )

        try workspace.checkPrecomputeResolution { result in
            XCTAssertNoDiagnostics(result.diagnostics)
            XCTAssertEqual(result.result, .required(reason: .packageRequirementChange(
                package: cRef,
                state: .sourceControlCheckout(v1_5),
                requirement: .unversioned
            )))
        }
    }

    func testPrecomputeResolution_requirementChange_branchToLocal() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let bPath = RelativePath("B")
        let cPath = RelativePath("C")
        let v1Requirement: SourceControlRequirement = .range("1.0.0" ..< "2.0.0")
        let v1_5 = CheckoutState.version("1.0.5", revision: Revision(identifier: "hello"))
        let master = CheckoutState.branch(name: "master", revision: Revision(identifier: "master"))

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "A",
                    targets: [MockTarget(name: "A")],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./B", requirement: v1Requirement),
                        .fileSystem(path: "./C"),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "B",
                    targets: [MockTarget(name: "B")],
                    products: [MockProduct(name: "B", targets: ["B"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
                MockPackage(
                    name: "C",
                    targets: [MockTarget(name: "C")],
                    products: [MockProduct(name: "C", targets: ["C"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
            ]
        )

        let bPackagePath = workspace.pathToPackage(withName: "B")
        let bRef = PackageReference.localSourceControl(identity: PackageIdentity(path: bPackagePath), path: bPackagePath)

        let cPackagePath = workspace.pathToPackage(withName: "C")
        let cRef = PackageReference.localSourceControl(identity: PackageIdentity(path: cPackagePath), path: cPackagePath)


        try workspace.set(
            pins: [bRef: v1_5, cRef: master],
            managedDependencies: [
                bPackagePath: .sourceControlCheckout(packageRef: bRef, state: v1_5, subpath: bPath),
                cPackagePath: .sourceControlCheckout(packageRef: cRef, state: master, subpath: cPath),
            ]
        )

        try workspace.checkPrecomputeResolution { result in
            XCTAssertNoDiagnostics(result.diagnostics)
            XCTAssertEqual(result.result, .required(reason: .packageRequirementChange(
                package: cRef,
                state: .sourceControlCheckout(master),
                requirement: .unversioned
            )))
        }
    }

    func testPrecomputeResolution_other() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let bPath = RelativePath("B")
        let cPath = RelativePath("C")
        let v1Requirement: SourceControlRequirement = .range("1.0.0" ..< "2.0.0")
        let v2Requirement: SourceControlRequirement = .range("2.0.0" ..< "3.0.0")
        let v1_5 = CheckoutState.version("1.0.5", revision: Revision(identifier: "hello"))

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "A",
                    targets: [MockTarget(name: "A")],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./B", requirement: v1Requirement),
                        .sourceControl(path: "./C", requirement: v2Requirement),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "B",
                    targets: [MockTarget(name: "B")],
                    products: [MockProduct(name: "B", targets: ["B"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
                MockPackage(
                    name: "C",
                    targets: [MockTarget(name: "C")],
                    products: [MockProduct(name: "C", targets: ["C"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
            ]
        )

        let bPackagePath = workspace.pathToPackage(withName: "B")
        let bRef = PackageReference.localSourceControl(identity: PackageIdentity(path: bPackagePath), path: bPackagePath)

        let cPackagePath = workspace.pathToPackage(withName: "C")
        let cRef = PackageReference.localSourceControl(identity: PackageIdentity(path: cPackagePath), path: cPackagePath)

        try workspace.set(
            pins: [bRef: v1_5, cRef: v1_5],
            managedDependencies: [
                bPackagePath: .sourceControlCheckout(packageRef: bRef, state: v1_5, subpath: bPath),
                cPackagePath: .sourceControlCheckout(packageRef: cRef, state: v1_5, subpath: cPath),
            ]
        )

        try workspace.checkPrecomputeResolution { result in
            XCTAssertNoDiagnostics(result.diagnostics)
            XCTAssertEqual(
                result.result,
                .required(reason: .other("Dependencies could not be resolved because no versions of \'c\' match the requirement 2.0.0..<3.0.0 and root depends on \'c\' 2.0.0..<3.0.0."))
            )
        }
    }

    func testPrecomputeResolution_notRequired() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let bPath = RelativePath("B")
        let cPath = RelativePath("C")
        let v1Requirement: SourceControlRequirement = .range("1.0.0" ..< "2.0.0")
        let v2Requirement: SourceControlRequirement = .range("2.0.0" ..< "3.0.0")
        let v1_5 = CheckoutState.version("1.0.5", revision: Revision(identifier: "hello"))
        let v2 = CheckoutState.version("2.0.0", revision: Revision(identifier: "hello"))

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "A",
                    targets: [MockTarget(name: "A")],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./B", requirement: v1Requirement),
                        .sourceControl(path: "./C", requirement: v2Requirement),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "B",
                    targets: [MockTarget(name: "B")],
                    products: [MockProduct(name: "B", targets: ["B"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
                MockPackage(
                    name: "C",
                    targets: [MockTarget(name: "C")],
                    products: [MockProduct(name: "C", targets: ["C"])],
                    versions: [nil, "1.0.0", "1.0.5", "2.0.0"]
                ),
            ]
        )

        let bPackagePath = workspace.pathToPackage(withName: "B")
        let bRef = PackageReference.localSourceControl(identity: PackageIdentity(path: bPackagePath), path: bPackagePath)

        let cPackagePath = workspace.pathToPackage(withName: "C")
        let cRef = PackageReference.localSourceControl(identity: PackageIdentity(path: cPackagePath), path: cPackagePath)

        try workspace.set(
            pins: [bRef: v1_5, cRef: v2],
            managedDependencies: [
                bPackagePath: .sourceControlCheckout(packageRef: bRef, state: v1_5, subpath: bPath),
                cPackagePath: .sourceControlCheckout(packageRef: cRef, state: v2, subpath: cPath),
            ]
        )

        try workspace.checkPrecomputeResolution { result in
            XCTAssertNoDiagnostics(result.diagnostics)
            XCTAssertEqual(result.result.isRequired, false)
        }
    }

    func testLoadingRootManifests() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                .genericPackage1(named: "A"),
                .genericPackage1(named: "B"),
                .genericPackage1(named: "C"),
            ],
            packages: []
        )

        try workspace.checkPackageGraph(roots: ["A", "B", "C"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(packages: "A", "B", "C")
                result.check(targets: "A", "B", "C")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testUpdate() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [
                        MockProduct(name: "Root", targets: ["Root"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar"]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.5.0"]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        // Do an initial run, capping at Foo at 1.0.0.
        let deps: [MockDependency] = [
            .sourceControl(path: "./Foo", requirement: .exact("1.0.0"), products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Bar", "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }

        // Run update.
        try workspace.checkUpdate(roots: ["Root"]) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.5.0")))
        }
        XCTAssertMatch(workspace.delegate.events, [.equal("removing repo: \(sandbox.appending(components: "pkgs", "Bar"))")])

        // Run update again.
        // Ensure that up-to-date delegate is called when there is nothing to update.
        try workspace.checkUpdate(roots: ["Root"]) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        XCTAssertMatch(workspace.delegate.events, [.equal("Everything is already up-to-date")])
    }

    func testUpdateDryRun() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [
                        MockProduct(name: "Root", targets: ["Root"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.5.0"]
                ),
            ]
        )

        // Do an initial run, capping at Foo at 1.0.0.
        let deps: [MockDependency] = [
            .sourceControl(path: "./Foo", requirement: .exact("1.0.0"), products: .specific(["Foo"])),
        ]

        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }

        // Run update.
        try workspace.checkUpdateDryRun(roots: ["Root"]) { changes, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            #if ENABLE_TARGET_BASED_DEPENDENCY_RESOLUTION
            let stateChange = Workspace.PackageStateChange.updated(.init(requirement: .version(Version("1.5.0")), products: .specific(["Foo"])))
            #else
            let stateChange = Workspace.PackageStateChange.updated(.init(requirement: .version(Version("1.5.0")), products: .everything))
            #endif

            let path = AbsolutePath("/tmp/ws/pkgs/Foo")
            let expectedChange = (
                PackageReference.localSourceControl(identity: PackageIdentity(path: path), path: path),
                stateChange
            )
            guard let change = changes?.first, changes?.count == 1 else {
                XCTFail()
                return
            }
            XCTAssertEqual(expectedChange, change)
        }
        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }
    }

    func testPartialUpdate() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [
                        MockProduct(name: "Root", targets: ["Root"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar"]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.5.0"]
                ),
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar"]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMinor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", "1.2.0"]
                ),
            ]
        )

        // Do an initial run, capping at Foo at 1.0.0.
        let deps: [MockDependency] = [
            .sourceControl(path: "./Foo", requirement: .exact("1.0.0"), products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }

        // Run partial updates.
        //
        // Try to update just Bar. This shouldn't do anything because Bar can't be updated due
        // to Foo's requirements.
        try workspace.checkUpdate(roots: ["Root"], packages: ["Bar"]) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }

        // Try to update just Foo. This should update Foo but not Bar.
        try workspace.checkUpdate(roots: ["Root"], packages: ["Foo"]) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.5.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }

        // Run full update.
        try workspace.checkUpdate(roots: ["Root"]) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.5.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.2.0")))
        }
    }

    func testCleanAndReset() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [
                        MockProduct(name: "Root", targets: ["Root"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        // Load package graph.
        try workspace.checkPackageGraph(roots: ["Root"]) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }

        // Drop a build artifact in data directory.
        let ws = try workspace.getOrCreateWorkspace()
        let buildArtifact = ws.location.scratchDirectory.appending(component: "test.o")
        try fs.writeFileContents(buildArtifact, bytes: "Hi")

        // Double checks.
        XCTAssert(fs.exists(buildArtifact))
        XCTAssert(fs.exists(ws.location.repositoriesCheckoutsDirectory))

        // Check clean.
        workspace.checkClean { diagnostics in
            // Only the build artifact should be removed.
            XCTAssertFalse(fs.exists(buildArtifact))
            XCTAssert(fs.exists(ws.location.repositoriesCheckoutsDirectory))
            XCTAssert(fs.exists(ws.location.scratchDirectory))

            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }

        // Add the build artifact again.
        try fs.writeFileContents(buildArtifact, bytes: "Hi")

        // Check reset.
        workspace.checkReset { diagnostics in
            // Only the build artifact should be removed.
            XCTAssertFalse(fs.exists(buildArtifact))
            XCTAssertFalse(fs.exists(ws.location.repositoriesCheckoutsDirectory))
            XCTAssertFalse(fs.exists(ws.location.scratchDirectory))

            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.checkEmpty()
        }
    }

    func testDependencyManifestLoading() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root1",
                    targets: [
                        MockTarget(name: "Root1", dependencies: ["Foo"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
                MockPackage(
                    name: "Root2",
                    targets: [
                        MockTarget(name: "Root2", dependencies: ["Bar"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                .genericPackage1(named: "Foo"),
                .genericPackage1(named: "Bar"),
            ]
        )

        // Check that we can compute missing dependencies.
        try workspace.loadDependencyManifests(roots: ["Root1", "Root2"]) { manifests, diagnostics in
            XCTAssertEqual(try! manifests.missingPackages().map { $0.locationString }.sorted(), [sandbox.appending(components: "pkgs", "Bar").pathString, sandbox.appending(components: "pkgs", "Foo").pathString])
            XCTAssertNoDiagnostics(diagnostics)
        }

        // Load the graph with one root.
        try workspace.checkPackageGraph(roots: ["Root1"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(packages: "Foo", "Root1")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }

        // Check that we compute the correct missing dependencies.
        try workspace.loadDependencyManifests(roots: ["Root1", "Root2"]) { manifests, diagnostics in
            XCTAssertEqual(try! manifests.missingPackages().map { $0.locationString }.sorted(), [sandbox.appending(components: "pkgs", "Bar").pathString])
            XCTAssertNoDiagnostics(diagnostics)
        }

        // Load the graph with both roots.
        try workspace.checkPackageGraph(roots: ["Root1", "Root2"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(packages: "Bar", "Foo", "Root1", "Root2")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }

        // Check that we compute the correct missing dependencies.
        try workspace.loadDependencyManifests(roots: ["Root1", "Root2"]) { manifests, diagnostics in
            XCTAssertEqual(try! manifests.missingPackages().map { $0.locationString }.sorted(), [])
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testDependencyManifestsOrder() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root1",
                    targets: [
                        MockTarget(name: "Root1", dependencies: ["Foo", "Bar", "Baz", "Bam"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./Baz", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./Bam", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar", "Baz"]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./Baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                .genericPackage1(named: "Bar"),
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz", dependencies: ["Bam"]),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bam", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                .genericPackage1(named: "Bam"),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root1"]) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }

        try workspace.loadDependencyManifests(roots: ["Root1"]) { manifests, diagnostics in
            // Ensure that the order of the manifests is stable.
            XCTAssertEqual(manifests.allDependencyManifests().map { $0.value.manifest.displayName }, ["Foo", "Baz", "Bam", "Bar"])
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testBranchAndRevision() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .branch("develop")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["develop"]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["boo"]
                ),
            ]
        )

        // Get some revision identifier of Bar.
        let bar = RepositorySpecifier(path: .init("/tmp/ws/pkgs/Bar"))
        let barRevision = workspace.repositoryProvider.specifierMap[bar]!.revisions[0]

        // We request Bar via revision.
        let deps: [MockDependency] = [
            .sourceControl(path: "./Bar", requirement: .revision(barRevision), products: .specific(["Bar"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Bar", "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.branch("develop")))
            result.check(dependency: "bar", at: .checkout(.revision(barRevision)))
        }
    }

    func testResolve() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", "1.2.3"]
                ),
            ]
        )

        // Load initial version.
        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.2.3")))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.2.3")))
        }

        // Resolve to an older version.
        workspace.checkResolve(pkg: "Foo", roots: ["Root"], version: "1.0.0") { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }

        // Check failure.
        workspace.checkResolve(pkg: "Foo", roots: ["Root"], version: "1.3.0") { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("'foo' 1.3.0"), severity: .error)
            }
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }
    }

    func testDeletedCheckoutDirectory() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                .genericPackage1(named: "Foo"),
            ]
        )

        // Load the graph.
        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }

        try fs.removeFileTree(workspace.getOrCreateWorkspace().location.repositoriesCheckoutsDirectory)

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("dependency 'foo' is missing; cloning again"), severity: .warning)
            }
        }
    }

    func testMinimumRequiredToolsVersionInDependencyResolution() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"],
                    toolsVersion: .v3
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { _, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("'foo' 1.0.0..<2.0.0"), severity: .error)
            }
        }
    }

    func testToolsVersionRootPackages() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: []
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: []
                ),
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: []
                ),
            ],
            packages: [],
            toolsVersion: .v4
        )

        let roots = workspace.rootPaths(for: ["Foo", "Bar", "Baz"]).map { $0.appending(component: "Package.swift") }

        try fs.writeFileContents(roots[0], bytes: "// swift-tools-version:4.0")
        try fs.writeFileContents(roots[1], bytes: "// swift-tools-version:4.1.0")
        try fs.writeFileContents(roots[2], bytes: "// swift-tools-version:3.1")

        try workspace.checkPackageGraph(roots: ["Foo"]) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }


        workspace.checkPackageGraphFailure(roots: ["Bar"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                let diagnostic = result.check(
                    diagnostic: .equal("package 'bar' is using Swift tools version 4.1.0 but the installed version is 4.0.0"),
                    severity: .error
                )
                XCTAssertEqual(diagnostic?.metadata?.packageIdentity, .plain("bar"))
            }
        }
        workspace.checkPackageGraphFailure(roots: ["Foo", "Bar"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                let diagnostic = result.check(
                    diagnostic: .equal("package 'bar' is using Swift tools version 4.1.0 but the installed version is 4.0.0"),
                    severity: .error
                )
                XCTAssertEqual(diagnostic?.metadata?.packageIdentity, .plain("bar"))
            }
        }
        workspace.checkPackageGraphFailure(roots: ["Baz"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                let diagnostic = result.check(
                    diagnostic: .equal("package 'baz' is using Swift tools version 3.1.0 which is no longer supported; consider using '// swift-tools-version:4.0' to specify the current tools version"),
                    severity: .error
                )
                XCTAssertEqual(diagnostic?.metadata?.packageIdentity, .plain("baz"))
            }
        }
    }

    func testEditDependency() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo", "Bar"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", nil]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", nil]
                ),
            ]
        )

        // Load the graph.
        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Bar", "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }

        // Edit foo.
        let fooPath = try workspace.getOrCreateWorkspace().location.editsDirectory.appending(component: "Foo")
        workspace.checkEdit(packageName: "Foo") { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .edited(nil))
        }
        XCTAssertTrue(fs.exists(fooPath))

        try workspace.loadDependencyManifests(roots: ["Root"]) { manifests, diagnostics in
            let editedPackages = manifests.editedPackagesConstraints()
            XCTAssertEqual(editedPackages.map { $0.package.locationString }, [fooPath.pathString])
            XCTAssertNoDiagnostics(diagnostics)
        }

        // Try re-editing foo.
        workspace.checkEdit(packageName: "Foo") { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .equal("dependency 'foo' already in edit mode"), severity: .error)
            }
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .edited(nil))
        }

        // Try editing bar at bad revision.
        workspace.checkEdit(packageName: "Bar", revision: Revision(identifier: "dev")) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .equal("revision 'dev' does not exist"), severity: .error)
            }
        }

        // Edit bar at a custom path and branch (ToT).
        let barPath = AbsolutePath("/tmp/ws/custom/bar")
        workspace.checkEdit(packageName: "Bar", path: barPath, checkoutBranch: "dev") { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "bar", at: .edited(barPath))
        }
        let barRepo = try workspace.repositoryProvider.openWorkingCopy(at: barPath) as! InMemoryGitRepository
        XCTAssert(barRepo.revisions.contains("dev"))

        // Test unediting.
        workspace.checkUnedit(packageName: "Foo", roots: ["Root"]) { diagnostics in
            XCTAssertFalse(fs.exists(fooPath))
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkUnedit(packageName: "Bar", roots: ["Root"]) { diagnostics in
            XCTAssert(fs.exists(barPath))
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testMissingEditCanRestoreOriginalCheckout() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", nil]
                ),
            ]
        )

        // Load the graph.
        try workspace.checkPackageGraph(roots: ["Root"]) { _, _ in }

        // Edit foo.
        let fooPath = try workspace.getOrCreateWorkspace().location.editsDirectory.appending(component: "Foo")
        workspace.checkEdit(packageName: "Foo") { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .edited(nil))
        }
        XCTAssertTrue(fs.exists(fooPath))

        // Remove the edited package.
        try fs.removeFileTree(fooPath)
        try workspace.checkPackageGraph(roots: ["Root"]) { _, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .equal("dependency 'foo' was being edited but is missing; falling back to original checkout"), severity: .warning)
            }
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }
    }

    func testCanUneditRemovedDependencies() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", nil]
                ),
            ]
        )

        let deps: [MockDependency] = [
            .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0"), products: .specific(["Foo"])),
        ]
        let ws = try workspace.getOrCreateWorkspace()

        // Load the graph and edit foo.
        try workspace.checkPackageGraph(deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(packages: "Foo")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkEdit(packageName: "Foo") { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .edited(nil))
        }

        // Remove foo.
        try workspace.checkUpdate { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        XCTAssertMatch(workspace.delegate.events, [.equal("removing repo: \(sandbox.appending(components: "pkgs", "Foo"))")])
        try workspace.checkPackageGraph(deps: []) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }

        // There should still be an entry for `foo`, which we can unedit.
        let editedDependency = ws.state.dependencies[.plain("foo")]
        if case .edited(let basedOn, _) = editedDependency?.state {
            XCTAssertNil(basedOn)
        } else {
            XCTFail("expected edited depedency")
        }

        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .edited(nil))
        }

        // Unedit foo.
        workspace.checkUnedit(packageName: "Foo", roots: []) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.checkEmpty()
        }
    }

    func testDependencyResolutionWithEdit() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo", "Bar"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", "1.2.0", "1.3.2"]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", nil]
                ),
            ]
        )

        let deps: [MockDependency] = [
            .sourceControl(path: "./Foo", requirement: .exact("1.0.0"), products: .specific(["Foo"])),
        ]
        // Load the graph.
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Bar", "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }

        // Edit bar.
        workspace.checkEdit(packageName: "Bar") { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .edited(nil))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }

        // Add entry for the edited package.
        do {
            let barKey = MockManifestLoader.Key(url: sandbox.appending(components: "pkgs", "Bar").pathString)
            let editedBarKey = MockManifestLoader.Key(url: sandbox.appending(components: "edits", "Bar").pathString)
            let manifest = workspace.manifestLoader.manifests[barKey]!
            workspace.manifestLoader.manifests[editedBarKey] = manifest
        }

        // Now, resolve foo at a different version.
        workspace.checkResolve(pkg: "Foo", roots: ["Root"], version: "1.2.0") { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.2.0")))
            result.check(dependency: "bar", at: .edited(nil))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.2.0")))
            result.check(notPresent: "bar")
        }

        // Try package update.
        try workspace.checkUpdate(roots: ["Root"]) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.3.2")))
            result.check(dependency: "bar", at: .edited(nil))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.3.2")))
            result.check(notPresent: "bar")
        }

        // Unedit should get the Package.resolved entry back.
        workspace.checkUnedit(packageName: "bar", roots: ["Root"]) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.3.2")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.3.2")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }
    }

    func testPrefetchingWithOverridenPackage() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar"]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: [nil]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        // Load the graph.
        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }

        let deps: [MockDependency] = [
            .fileSystem(path: "./Foo", products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Bar", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .local)
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }
    }

    // Test that changing a particular dependency re-resolves the graph.
    func testChangeOneDependency() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar"]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .exact("1.0.0"))
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", "1.5.0"]
                ),
            ]
        )

        // Initial resolution.
        try workspace.checkPackageGraph(roots: ["Foo"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Foo")
                result.check(packages: "Bar", "Foo")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }

        // Check that changing the requirement to 1.5.0 triggers re-resolution.
        let fooKey = MockManifestLoader.Key(url: sandbox.appending(components: "roots", "Foo").pathString)
        let manifest = workspace.manifestLoader.manifests[fooKey]!

        let dependency = manifest.dependencies[0]
        switch dependency {
        case .sourceControl(let settings):
            let updatedDependency: PackageDependency = .sourceControl(
                identity: settings.identity,
                nameForTargetDependencyResolutionOnly: settings.nameForTargetDependencyResolutionOnly,
                location: settings.location,
                requirement: .exact("1.5.0"),
                productFilter: settings.productFilter
            )

            workspace.manifestLoader.manifests[fooKey] = Manifest(
                displayName: manifest.displayName,
                path: manifest.path,
                packageKind: manifest.packageKind,
                packageLocation: manifest.packageLocation,
                platforms: [],
                version: manifest.version,
                toolsVersion: manifest.toolsVersion,
                dependencies: [updatedDependency],
                targets: manifest.targets
            )
        default:
            XCTFail("unexpected dependency type")
        }

        try workspace.checkPackageGraph(roots: ["Foo"]) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "bar", at: .checkout(.version("1.5.0")))
        }
    }

    func testResolutionFailureWithEditedDependency() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", nil]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", nil]
                ),
            ]
        )

        // Load the graph.
        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkEdit(packageName: "Foo") { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .edited(nil))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }

        // Add entry for the edited package.
        do {
            let fooKey = MockManifestLoader.Key(url: sandbox.appending(components: "pkgs", "Foo").pathString)
            let editedFooKey = MockManifestLoader.Key(url: sandbox.appending(components: "edits", "Foo").pathString)
            let manifest = workspace.manifestLoader.manifests[fooKey]!
            workspace.manifestLoader.manifests[editedFooKey] = manifest
        }

        // Try resolving a bad graph.
        let deps: [MockDependency] = [
            .sourceControl(path: "./Bar", requirement: .exact("1.1.0"), products: .specific(["Bar"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { _, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("'bar' 1.1.0"), severity: .error)
            }
        }
    }

    func testStateModified() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: [
                            .product(name: "Foo", package: "foo"),
                            .product(name: "Bar", package: "bar")
                        ]),
                    ],
                    dependencies: [
                        .sourceControl(url: "https://scm.com/org/foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(url: "https://scm.com/org/bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    url: "https://scm.com/org/foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: [nil, "1.0.0", "1.1.0"]
                ),
                MockPackage(
                    name: "Bar",
                    url: "https://scm.com/org/bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "1.2.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"], deps: []) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            PackageGraphTester(graph) { result in
                result.check(packages: "Bar", "Foo", "Root")
            }
        }

        let underlying = try workspace.getOrCreateWorkspace()
        let fooEditPath = sandbox.appending(components: ["edited", "foo"])

        // mimic external process putting a dependency into edit mode
        do {
            try fs.createDirectory(fooEditPath, recursive: true)
            try fs.writeFileContents(fooEditPath.appending(component: "Package.swift"), bytes: "// swift-tools-version: 5.6")

            let fooState = underlying.state.dependencies[.plain("foo")]!
            let externalState = WorkspaceState(fileSystem: fs, storageDirectory: underlying.state.storagePath.parentDirectory, initializationWarningHandler: { _ in })
            externalState.dependencies.remove(fooState.packageRef.identity)
            externalState.dependencies.add(try fooState.edited(subpath: .init("foo"), unmanagedPath: fooEditPath))
            try externalState.save()
        }

        // reload graph after "external" change
        try workspace.checkPackageGraph(roots: ["Root"], deps: []) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(packages: "Bar", "Foo", "Root")
            }
        }

        do {
            let fooState = underlying.state.dependencies[.plain("foo")]!
            guard case .edited(basedOn: _, unmanagedPath: fooEditPath) = fooState.state else {
                XCTFail("'\(fooState.packageRef.identity)' dependency expected to be in edit mode, but was: \(fooState)")
                return
            }
        }
    }

    func testSkipUpdate() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [
                        MockProduct(name: "Root", targets: ["Root"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.5.0"]
                ),
            ],
            skipDependenciesUpdates: true
        )

        // Run update and remove all events.
        try workspace.checkUpdate(roots: ["Root"]) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.delegate.clear()

        // Check we don't have updating Foo event.
        try workspace.checkUpdate(roots: ["Root"]) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            XCTAssertMatch(workspace.delegate.events, ["Everything is already up-to-date"])
        }
    }

    func testLocalDependencyBasics() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar", "Baz"]),
                        MockTarget(name: "FooTests", dependencies: ["Foo"], type: .test),
                    ],
                    products: [],
                    dependencies: [
                        .fileSystem(path: "./Bar"),
                        .sourceControl(path: "./Baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", "1.5.0", nil]
                ),
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz", dependencies: ["Bar"]),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0", "1.5.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Foo"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Foo")
                result.check(packages: "Bar", "Baz", "Foo")
                result.check(targets: "Bar", "Baz", "Foo")
                result.check(testModules: "FooTests")
                result.checkTarget("Baz") { result in result.check(dependencies: "Bar") }
                result.checkTarget("Foo") { result in result.check(dependencies: "Baz", "Bar") }
                result.checkTarget("FooTests") { result in result.check(dependencies: "Foo") }
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "baz", at: .checkout(.version("1.5.0")))
            result.check(dependency: "bar", at: .local)
        }

        // Test that its not possible to edit or resolve this package.
        workspace.checkEdit(packageName: "Bar") { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("local dependency 'bar' can't be edited"), severity: .error)
            }
        }
        workspace.checkResolve(pkg: "Bar", roots: ["Foo"], version: "1.0.0") { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("local dependency 'bar' can't be resolved"), severity: .error)
            }
        }
    }

    func testLocalDependencyTransitive() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar"]),
                        MockTarget(name: "FooTests", dependencies: ["Foo"], type: .test),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar", dependencies: ["Baz"]),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    dependencies: [
                        .fileSystem(path: "./Baz"),
                    ],
                    versions: ["1.0.0", "1.5.0", nil]
                ),
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0", "1.5.0", nil]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Foo"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Foo")
                result.check(packages: "Foo")
                result.check(targets: "Foo")
            }
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("'bar' {1.0.0..<1.5.0, 1.5.1..<2.0.0} cannot be used"), severity: .error)
            }
        }
    }

    func testLocalDependencyWithPackageUpdate() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", "1.5.0", nil]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Foo"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Foo")
                result.check(packages: "Bar", "Foo")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "bar", at: .checkout(.version("1.5.0")))
        }

        // Override with local package and run update.
        let deps: [MockDependency] = [
            .fileSystem(path: "./Bar", products: .specific(["Bar"])),
        ]
        try workspace.checkUpdate(roots: ["Foo"], deps: deps) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "bar", at: .local)
        }

        // Go back to the versioned state.
        try workspace.checkUpdate(roots: ["Foo"]) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "bar", at: .checkout(.version("1.5.0")))
        }
    }

    func testMissingLocalDependencyDiagnostic() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: []),
                    ],
                    products: [],
                    dependencies: [
                        .fileSystem(path: "Bar"),
                    ]
                ),
            ],
            packages: [
            ]
        )

        try workspace.checkPackageGraph(roots: ["Foo"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Foo")
                result.check(packages: "Foo")
                result.check(targets: "Foo")
            }
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("the package at '\(sandbox.appending(components: "pkgs", "Bar"))' cannot be accessed (\(sandbox.appending(components: "pkgs", "Bar")) doesn't exist in file system"), severity: .error)
            }
        }
    }

    func testRevisionVersionSwitch() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: []),
                    ],
                    products: [],
                    dependencies: []
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["develop", "1.0.0"]
                ),
            ]
        )

        // Test that switching between revision and version requirement works
        // without running swift package update.

        var deps: [MockDependency] = [
            .sourceControl(path: "./Foo", requirement: .branch("develop"), products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.branch("develop")))
        }

        deps = [
            .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0"), products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }

        deps = [
            .sourceControl(path: "./Foo", requirement: .branch("develop"), products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.branch("develop")))
        }
    }

    func testLocalVersionSwitch() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: []),
                    ],
                    products: [],
                    dependencies: []
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["develop", "1.0.0", nil]
                ),
            ]
        )

        // Test that switching between local and version requirement works
        // without running swift package update.

        var deps: [MockDependency] = [
            .fileSystem(path: "./Foo", products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .local)
        }

        deps = [
            .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0"), products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }

        deps = [
            .fileSystem(path: "./Foo", products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .local)
        }
    }

    func testLocalLocalSwitch() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: []),
                    ],
                    products: [],
                    dependencies: []
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: [nil]
                ),
                MockPackage(
                    name: "Foo",
                    path: "Foo2",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: [nil]
                ),
            ]
        )

        // Test that switching between two same local packages placed at
        // different locations works correctly.

        var deps: [MockDependency] = [
            .fileSystem(path: "./Foo", products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .local)
        }

        deps = [
            .fileSystem(path: "./Foo2", products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo2", at: .local)
        }
    }


    // Test that switching between two same local packages placed at
    // different locations works correctly.
    func testDependencySwitchLocalWithSameIdentity() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: []),
                    ],
                    products: [],
                    dependencies: []
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: [nil]
                ),
                MockPackage(
                    name: "Foo",
                    path: "Nested/Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: [nil]
                ),
            ]
        )

        var deps: [MockDependency] = [
            .fileSystem(path: "./Foo", products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .local)
            XCTAssertEqual(result.managedDependencies[.plain("foo")]?.packageRef.locationString, sandbox.appending(components: "pkgs", "Foo").pathString)
        }

        deps = [
            .fileSystem(path: "./Nested/Foo", products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .local)
            XCTAssertEqual(result.managedDependencies[.plain("foo")]?.packageRef.locationString, sandbox.appending(components: "pkgs", "Nested", "Foo").pathString)
        }
    }

    // Test that switching between two remote packages at
    // different locations works correctly.
    func testDependencySwitchRemoteWithSameIdentity() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: []),
                    ],
                    products: [],
                    dependencies: []
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    url: "https://scm.com/org/foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "Foo",
                    url: "https://scm.com/other/foo",
                    targets: [
                        MockTarget(name: "OtherFoo"),
                    ],
                    products: [
                        MockProduct(name: "OtherFoo", targets: ["OtherFoo"]),
                    ],
                    versions: ["1.1.0"]
                ),
            ]
        )

        var deps: [MockDependency] = [
            .sourceControl(url: "https://scm.com/org/foo", requirement: .exact("1.0.0")),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Foo", "Root")
            }
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }
        do {
            let ws = try workspace.getOrCreateWorkspace()
            XCTAssertEqual(ws.state.dependencies[.plain("foo")]?.packageRef.locationString, "https://scm.com/org/foo")
        }

        deps = [
            .sourceControl(url: "https://scm.com/other/foo", requirement: .exact("1.1.0"))
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.1.0")))
        }
        do {
            let ws = try workspace.getOrCreateWorkspace()
            XCTAssertEqual(ws.state.dependencies[.plain("foo")]?.packageRef.locationString, "https://scm.com/other/foo")
        }
    }

    func testResolvedFileUpdate() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: []),
                    ],
                    products: [],
                    dependencies: []
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        let deps: [MockDependency] = [
            .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0"), products: .specific(["Foo"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }

        try workspace.checkPackageGraph(roots: ["Root"], deps: []) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
        }
        workspace.checkResolved { result in
            result.check(notPresent: "foo")
        }
    }

    func testResolvedFileSchemeToolsVersion() throws {
        let fs = InMemoryFileSystem()

        for pair in [(ToolsVersion.v5_2, ToolsVersion.v5_2), (ToolsVersion.v5_6, ToolsVersion.v5_6), (ToolsVersion.v5_2, ToolsVersion.v5_6)] {
            let sandbox = AbsolutePath("/tmp/ws/")
            let workspace = try MockWorkspace(
                sandbox: sandbox,
                fileSystem: fs,
                roots: [
                    MockPackage(
                        name: "Root1",
                        targets: [
                            MockTarget(name: "Root1", dependencies: ["Foo"]),
                        ],
                        products: [],
                        dependencies: [
                            .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0"), products: .specific(["Foo"])),
                        ],
                        toolsVersion: pair.0
                    ),
                    MockPackage(
                        name: "Root2",
                        targets: [
                            MockTarget(name: "Root2", dependencies: []),
                        ],
                        products: [],
                        dependencies: [],
                        toolsVersion: pair.1
                    ),
                ],
                packages: [
                    MockPackage(
                        name: "Foo",
                        targets: [
                            MockTarget(name: "Foo"),
                        ],
                        products: [
                            MockProduct(name: "Foo", targets: ["Foo"]),
                        ],
                        versions: ["1.0.0"]
                    ),
                ]
            )

            try workspace.checkPackageGraph(roots: ["Root1", "Root2"]) { _, diagnostics in
                XCTAssertNoDiagnostics(diagnostics)
            }
            workspace.checkManagedDependencies { result in
                result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            }
            workspace.checkResolved { result in
                result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            }

            let minToolsVersion = [pair.0, pair.1].min()!
            let expectedSchemeVersion = minToolsVersion >= .v5_6 ? 2 : 1
            XCTAssertEqual(try workspace.getOrCreateWorkspace().pinsStore.load().schemeVersion(), expectedSchemeVersion)
        }
    }

    func testResolvedFileStableCanonicalLocation() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: []),
                    ],
                    products: [],
                    dependencies: []
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    url: "https://localhost/org/foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"],
                    revisionProvider: { version in version } // stable revisions
                ),
                MockPackage(
                    name: "Bar",
                    url: "https://localhost/org/bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0"],
                    revisionProvider: { version in version } // stable revisions
                ),
                MockPackage(
                    name: "Foo",
                    url: "https://localhost/ORG/FOO", // diff: case
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", "1.1.0"],
                    revisionProvider: { version in version } // stable revisions
                ),
                MockPackage(
                    name: "Foo",
                    url: "https://localhost/org/foo.git", // diff: .git extension
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", "1.1.0"],
                    revisionProvider: { version in version } // stable revisions
                ),
                MockPackage(
                    name: "Bar",
                    url: "https://localhost/org/bar.git", // diff: .git extension
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", "1.1.0"],
                    revisionProvider: { version in version } // stable revisions
                ),
            ]
        )

        // case 1: initial loading

        var deps: [MockDependency] = [
            .sourceControl(url: "https://localhost/org/foo", requirement: .exact("1.0.0")),
            .sourceControl(url: "https://localhost/org/bar", requirement: .exact("1.0.0")),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
            XCTAssertEqual(result.managedDependencies[.plain("foo")]?.packageRef.locationString, "https://localhost/org/foo")
            XCTAssertEqual(result.managedDependencies[.plain("bar")]?.packageRef.locationString, "https://localhost/org/bar")
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
            XCTAssertEqual(result.store.pinsMap[.plain("foo")]?.packageRef.locationString, "https://localhost/org/foo")
            XCTAssertEqual(result.store.pinsMap[.plain("bar")]?.packageRef.locationString, "https://localhost/org/bar")
        }

        // case 2: set state with slightly different URLs that are canonically the same

        deps = [
            .sourceControl(url: "https://localhost/ORG/FOO", requirement: .exact("1.0.0")),
            .sourceControl(url: "https://localhost/org/bar.git", requirement: .exact("1.0.0")),
        ]

        // reset state, excluding the resolved file
        try workspace.closeWorkspace()
        XCTAssertTrue(fs.exists(sandbox.appending(component: "Package.resolved")))
        // run update
        try workspace.checkUpdate(roots: ["Root"], deps: deps) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }

        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
            // URLs should reflect the actual dependencies
            XCTAssertEqual(result.managedDependencies[.plain("foo")]?.packageRef.locationString, "https://localhost/ORG/FOO")
            XCTAssertEqual(result.managedDependencies[.plain("bar")]?.packageRef.locationString, "https://localhost/org/bar.git")
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
            // URLs should be stable since URLs are canonically the same and we kept the resolved file between the two iterations
            XCTAssertEqual(result.store.pinsMap[.plain("foo")]?.packageRef.locationString, "https://localhost/org/foo")
            XCTAssertEqual(result.store.pinsMap[.plain("bar")]?.packageRef.locationString, "https://localhost/org/bar")
        }

        // case 2: set state with slightly different URLs that are canonically the same but request different versions

        deps = [
            .sourceControl(url: "https://localhost/ORG/FOO", requirement: .exact("1.1.0")),
            .sourceControl(url: "https://localhost/org/bar.git", requirement: .exact("1.1.0")),
        ]
        // reset state, excluding the resolved file
        try workspace.closeWorkspace()
        XCTAssertTrue(fs.exists(sandbox.appending(component: "Package.resolved")))
        // run update
        try workspace.checkUpdate(roots: ["Root"], deps: deps) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }

        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.1.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.1.0")))
            // URLs should reflect the actual dependencies
            XCTAssertEqual(result.managedDependencies[.plain("foo")]?.packageRef.locationString, "https://localhost/ORG/FOO")
            XCTAssertEqual(result.managedDependencies[.plain("bar")]?.packageRef.locationString, "https://localhost/org/bar.git")
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.1.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.1.0")))
            // URLs should reflect the actual dependencies since the new version forces rewrite of the resolved file
            XCTAssertEqual(result.store.pinsMap[.plain("foo")]?.packageRef.locationString, "https://localhost/ORG/FOO")
            XCTAssertEqual(result.store.pinsMap[.plain("bar")]?.packageRef.locationString, "https://localhost/org/bar.git")
        }

        // case 3: set state with slightly different URLs that are canonically the same but remove resolved file

        deps = [
            .sourceControl(url: "https://localhost/org/foo.git", requirement: .exact("1.0.0")),
            .sourceControl(url: "https://localhost/org/bar.git", requirement: .exact("1.0.0")),
        ]
        // reset state, including the resolved file
        workspace.checkReset { XCTAssertNoDiagnostics($0) }
        try fs.removeFileTree(sandbox.appending(component: "Package.resolved"))
        XCTAssertFalse(fs.exists(sandbox.appending(component: "Package.resolved")))
        // run update
        try workspace.checkUpdate(roots: ["Root"], deps: deps) { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }

        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
            // URLs should reflect the actual dependencies
            XCTAssertEqual(result.managedDependencies[.plain("foo")]?.packageRef.locationString, "https://localhost/org/foo.git")
            XCTAssertEqual(result.managedDependencies[.plain("bar")]?.packageRef.locationString, "https://localhost/org/bar.git")
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
            // URLs should reflect the actual dependencies since we deleted the resolved file
            XCTAssertEqual(result.store.pinsMap[.plain("foo")]?.packageRef.locationString, "https://localhost/org/foo.git")
            XCTAssertEqual(result.store.pinsMap[.plain("bar")]?.packageRef.locationString, "https://localhost/org/bar.git")
        }
    }

    func testPackageMirror() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let mirrors = DependencyMirrors()
        mirrors.set(mirrorURL: sandbox.appending(components: "pkgs", "Baz").pathString, forURL: sandbox.appending(components: "pkgs", "Bar").pathString)
        mirrors.set(mirrorURL: sandbox.appending(components: "pkgs", "Baz").pathString, forURL: sandbox.appending(components: "pkgs", "Bam").pathString)

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Dep"]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Dep", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "Dep",
                    targets: [
                        MockTarget(name: "Dep", dependencies: ["Bar"]),
                    ],
                    products: [
                        MockProduct(name: "Dep", targets: ["Dep"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0", "1.5.0"],
                    toolsVersion: .v5
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", "1.5.0"]
                ),
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0", "1.4.0"]
                ),
                MockPackage(
                    name: "Bam",
                    targets: [
                        MockTarget(name: "Bam"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bam"]),
                    ],
                    versions: ["1.0.0", "1.5.0"]
                ),
            ],
            mirrors: mirrors
        )

        let deps: [MockDependency] = [
            .sourceControl(path: "./Bam", requirement: .upToNextMajor(from: "1.0.0"), products: .specific(["Bar"])),
        ]

        try workspace.checkPackageGraph(roots: ["Foo"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Foo")
                result.check(packages: "Foo", "Dep", "Baz")
                result.check(targets: "Foo", "Dep", "Baz")
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "dep", at: .checkout(.version("1.5.0")))
            result.check(dependency: "baz", at: .checkout(.version("1.4.0")))
            result.check(notPresent: "bar")
            result.check(notPresent: "bam")
        }
    }

    // In this test, we get into a state where an entry in the resolved
    // file for a transitive dependency whose URL is later changed to
    // something else, while keeping the same package identity.
    func testTransitiveDependencySwitchWithSameIdentity() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        // Use the same revision (hash) for "foo" to indicate they are the same
        // package despite having different URLs.
        let fooRevision = String((UUID().uuidString + UUID().uuidString).prefix(40))

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "Root",
                            dependencies: [
                                .product(name: "Bar", package: "bar")
                            ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://scm.com/org/bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Bar",
                    url: "https://scm.com/org/bar",
                    targets: [
                        MockTarget(
                            name: "Bar",
                            dependencies: [
                                .product(name: "Foo", package: "foo")
                            ]),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "https://scm.com/org/foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "Bar",
                    url: "https://scm.com/org/bar",
                    targets: [
                        MockTarget(
                            name: "Bar",
                            dependencies: [
                                .product(name: "OtherFoo", package: "foo")
                            ]),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "https://scm.com/other/foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.1.0"],
                    toolsVersion: .v5
                ),
                MockPackage(
                    name: "Foo",
                    url: "https://scm.com/org/foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"],
                    revisionProvider: { _ in fooRevision }
                ),
                MockPackage(
                    name: "Foo",
                    url: "https://scm.com/other/foo",
                    targets: [
                        MockTarget(name: "OtherFoo"),
                    ],
                    products: [
                        MockProduct(name: "OtherFoo", targets: ["OtherFoo"]),
                    ],
                    versions: ["1.0.0"],
                    revisionProvider: { _ in fooRevision }
                ),
            ]
        )

        var deps: [MockDependency] = [
            .sourceControl(url: "https://scm.com/org/bar", requirement: .exact("1.0.0")),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Bar", "Foo", "Root")
            }
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
            XCTAssertEqual(result.managedDependencies[.plain("foo")]?.packageRef.locationString, "https://scm.com/org/foo")
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
            XCTAssertEqual(result.store.pinsMap[.plain("foo")]?.packageRef.locationString, "https://scm.com/org/foo")
        }

        // reset state
        workspace.checkReset { XCTAssertNoDiagnostics($0) }

        deps = [
            .sourceControl(url: "https://scm.com/org/bar", requirement: .exact("1.1.0")),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "Bar", "Foo", "Root")
            }
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.1.0")))
            XCTAssertEqual(result.managedDependencies[.plain("foo")]?.packageRef.locationString, "https://scm.com/other/foo")
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.1.0")))
            XCTAssertEqual(result.store.pinsMap[.plain("foo")]?.packageRef.locationString, "https://scm.com/other/foo")
        }
    }

    func testForceResolveToResolvedVersions() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo", "Bar"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", "1.2.0", "1.3.2"]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", "develop"]
                ),
            ]
        )

        // Load the initial graph.
        let deps: [MockDependency] = [
            .sourceControl(path: "./Bar", requirement: .revision("develop"), products: .specific(["Bar"])),
        ]
        try workspace.checkPackageGraph(roots: ["Root"], deps: deps) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.3.2")))
            result.check(dependency: "bar", at: .checkout(.branch("develop")))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.3.2")))
            result.check(dependency: "bar", at: .checkout(.branch("develop")))
        }

        // Change pin of foo to something else.
        do {
            let ws = try workspace.getOrCreateWorkspace()
            let pinsStore = try ws.pinsStore.load()
            let fooPin = pinsStore.pins.first(where: { $0.packageRef.identity.description == "foo" })!

            let fooRepo = workspace.repositoryProvider.specifierMap[RepositorySpecifier(path: AbsolutePath(fooPin.packageRef.locationString))]!
            let revision = try fooRepo.resolveRevision(tag: "1.0.0")
            let newState = PinsStore.PinState.version("1.0.0", revision: revision.identifier)

            pinsStore.pin(packageRef: fooPin.packageRef, state: newState)
            try pinsStore.saveState(toolsVersion: ToolsVersion.current)
        }

        // Check force resolve. This should produce an error because the resolved file is out-of-date.
        workspace.checkPackageGraphFailure(roots: ["Root"], forceResolvedVersions: true) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: "an out-of-date resolved file was detected at \(sandbox.appending(components: "Package.resolved")), which is not allowed when automatic dependency resolution is disabled; please make sure to update the file to reflect the changes in dependencies. Running resolver because  requirements have changed.", severity: .error)
            }
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.branch("develop")))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.branch("develop")))
        }

        // A normal resolution.
        try workspace.checkPackageGraph(roots: ["Root"]) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }

        // This force resolution should succeed.
        try workspace.checkPackageGraph(roots: ["Root"], forceResolvedVersions: true) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }
        workspace.checkResolved { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }
    }

    func testForceResolveToResolvedVersionsDuplicateLocalDependency() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo", "Bar"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .fileSystem(path: "./Bar"),
                    ]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", "1.2.0", "1.3.2"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root", "Bar"]) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        try workspace.checkPackageGraph(roots: ["Root", "Bar"], forceResolvedVersions: true) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testForceResolveWithNoResolvedFile() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo", "Bar"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", "1.2.0", "1.3.2"]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", "develop"]
                ),
            ]
        )

        workspace.checkPackageGraphFailure(roots: ["Root"], forceResolvedVersions: true) { diagnostics in
            guard let diagnostic = diagnostics.first else { return XCTFail("unexpectedly got no diagnostics") }
            // rdar://82544922 (`WorkspaceResolveReason` is non-deterministic)
            XCTAssertTrue(diagnostic.message.hasPrefix("a resolved file is required when automatic dependency resolution is disabled and should be placed at \(sandbox.appending(components: "Package.resolved")). Running resolver because the following dependencies were added:"), "unexpected diagnostic message")
        }
    }

    func testForceResolveToResolvedVersionsLocalPackage() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [],
                    dependencies: [
                        .fileSystem(path: "./Foo"),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: [nil]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"], forceResolvedVersions: true) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .local)
        }
    }

    // This verifies that the simplest possible loading APIs are available for package clients.
    func testSimpleAPI() throws {
        try testWithTemporaryDirectory { path in
            // Create a temporary package as a test case.
            let packagePath = path.appending(component: "MyPkg")
            let initPackage = try InitPackage(
                name: packagePath.basename,
                packageType: .executable,
                destinationPath: packagePath,
                fileSystem: localFileSystem
            )
            try initPackage.writePackageStructure()

            // Load the workspace.
            let observability = ObservabilitySystem.makeForTesting()
            let workspace = try Workspace(forRootPackage: packagePath)

            // From here the API should be simple and straightforward:
            let manifest = try tsc_await {
                workspace.loadRootManifest(
                    at: packagePath,
                    observabilityScope: observability.topScope,
                    completion: $0
                )
            }
            XCTAssertFalse(observability.hasWarningDiagnostics, observability.diagnostics.description)
            XCTAssertFalse(observability.hasErrorDiagnostics, observability.diagnostics.description)

            let package = try tsc_await {
                workspace.loadRootPackage(
                    at: packagePath,
                    observabilityScope: observability.topScope,
                    completion: $0
                )
            }
            XCTAssertFalse(observability.hasWarningDiagnostics, observability.diagnostics.description)
            XCTAssertFalse(observability.hasErrorDiagnostics, observability.diagnostics.description)

            let graph = try workspace.loadPackageGraph(
                rootPath: packagePath,
                observabilityScope: observability.topScope
            )
            XCTAssertFalse(observability.hasWarningDiagnostics, observability.diagnostics.description)
            XCTAssertFalse(observability.hasErrorDiagnostics, observability.diagnostics.description)

            XCTAssertEqual(manifest.displayName, "MyPkg")
            XCTAssertEqual(package.identity, .plain(manifest.displayName))
            XCTAssert(graph.reachableProducts.contains(where: { $0.name == "MyPkg" }))
        }
    }

    func testRevisionDepOnLocal() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .branch("develop")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Local"]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .fileSystem(path: "./Local"),
                    ],
                    versions: ["develop"]
                ),
                MockPackage(
                    name: "Local",
                    targets: [
                        MockTarget(name: "Local"),
                    ],
                    products: [
                        MockProduct(name: "Local", targets: ["Local"]),
                    ],
                    versions: [nil]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { _, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .equal("package 'foo' is required using a revision-based requirement and it depends on local package 'local', which is not supported"), severity: .error)
            }
        }
    }

    func testRootPackagesOverrideBasenameMismatch() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Baz",
                    path: "Overridden/bazzz-master",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Baz",
                    path: "bazzz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        let deps: [MockDependency] = [
            .sourceControl(path: "./bazzz", requirement: .exact("1.0.0"), products: .specific(["Baz"])),
        ]

        try workspace.checkPackageGraphFailure(roots: ["Overridden/bazzz-master"], deps: deps) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .equal("unable to override package 'Baz' because its identity 'bazzz' doesn't match override's identity (directory name) 'bazzz-master'"), severity: .error)
            }
        }
    }

    func testManagedDependenciesNotCaseSensitive() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: [
                            .product(name: "Bar", package: "bar"),
                            .product(name: "Baz", package: "baz")
                        ])
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://localhost/org/bar", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(url: "https://localhost/org/baz", requirement: .upToNextMajor(from: "1.0.0"))
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Bar",
                    url: "https://localhost/org/bar",
                    targets: [
                        MockTarget(name: "Bar", dependencies: [
                            .product(name: "Baz", package: "Baz")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"])
                    ],
                    dependencies: [
                        .sourceControl(url: "https://localhost/org/Baz", requirement: .upToNextMajor(from: "1.0.0"))
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "Baz",
                    url: "https://localhost/org/baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"])
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "Baz",
                    url: "https://localhost/org/Baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"])
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Foo"]) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Foo")
                result.check(packages: "Bar", "Baz", "Foo")
                result.checkTarget("Foo") { result in result.check(dependencies: "Bar", "Baz") }
                result.checkTarget("Bar") { result in result.check(dependencies: "Baz") }
            }
            testDiagnostics(diagnostics, minSeverity: .info) { result in
                result.checkUnordered(
                    diagnostic: "dependency on 'baz' is represented by similar locations ('https://localhost/org/baz' and 'https://localhost/org/Baz') which are treated as the same canonical location 'localhost/org/baz'.",
                    severity: .info
                )
            }
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
            result.check(dependency: "baz", at: .checkout(.version("1.0.0")))
            XCTAssertEqual(result.managedDependencies[.plain("bar")]?.packageRef.locationString, "https://localhost/org/bar")
            // root casing should win, so testing for lower case
            XCTAssertEqual(result.managedDependencies[.plain("baz")]?.packageRef.locationString, "https://localhost/org/baz")
        }
    }

    func testUnsafeFlags() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar", settings: [.init(tool: .swift, kind: .unsafeFlags(["-F", "/tmp"]))]),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", nil]
                ),
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar", "Baz"]),
                    ],
                    products: [],
                    dependencies: [
                        .fileSystem(path: "./Bar"),
                        .sourceControl(path: "./Baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar", settings: [.init(tool: .swift, kind: .unsafeFlags(["-F", "/tmp"]))]),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["1.0.0", nil]
                ),
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz", dependencies: ["Bar"], settings: [.init(tool: .swift, kind: .unsafeFlags(["-F", "/tmp"]))]),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0", "1.5.0"]
                ),
            ]
        )

        // We should only see errors about use of unsafe flag in the version-based dependency.
        try workspace.checkPackageGraph(roots: ["Foo", "Bar"]) { _, diagnostics in
            testDiagnostics(diagnostics) { result in
                let diagnostic1 = result.checkUnordered(
                    diagnostic: .equal("the target 'Baz' in product 'Baz' contains unsafe build flags"),
                    severity: .error
                )
                XCTAssertEqual(diagnostic1?.metadata?.targetName, "Foo")
                let diagnostic2 = result.checkUnordered(
                    diagnostic: .equal("the target 'Bar' in product 'Baz' contains unsafe build flags"),
                    severity: .error
                )
                XCTAssertEqual(diagnostic2?.metadata?.targetName, "Foo")
            }
        }
    }

    func testEditDependencyHadOverridableConstraints() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo", "Baz"]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .branch("master")),
                        .sourceControl(path: "./Baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar"]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .branch("master")),
                    ],
                    versions: ["master", nil]
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["master", "1.0.0", nil]
                ),
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz", dependencies: ["Bar"]),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0", nil]
                ),
            ]
        )

        // Load the graph.
        try workspace.checkPackageGraph(roots: ["Root"]) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.branch("master")))
            result.check(dependency: "bar", at: .checkout(.branch("master")))
            result.check(dependency: "baz", at: .checkout(.version("1.0.0")))
        }

        // Edit foo.
        let fooPath = try workspace.getOrCreateWorkspace().location.editsDirectory.appending(component: "Foo")
        workspace.checkEdit(packageName: "Foo") { diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .edited(nil))
        }
        XCTAssertTrue(fs.exists(fooPath))

        // Add entry for the edited package.
        do {
            let fooKey = MockManifestLoader.Key(url: sandbox.appending(components: "pkgs", "Foo").pathString)
            let editedFooKey = MockManifestLoader.Key(url: sandbox.appending(components: "edits", "Foo").pathString)
            let manifest = workspace.manifestLoader.manifests[fooKey]!
            workspace.manifestLoader.manifests[editedFooKey] = manifest
        }
        XCTAssertMatch(workspace.delegate.events, [.equal("will resolve dependencies")])
        workspace.delegate.clear()

        try workspace.checkPackageGraph(roots: ["Root"]) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .edited(nil))
            result.check(dependency: "bar", at: .checkout(.branch("master")))
            result.check(dependency: "baz", at: .checkout(.version("1.0.0")))
        }
        XCTAssertNoMatch(workspace.delegate.events, [.equal("will resolve dependencies")])
    }

    func testTargetBasedDependency() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let barProducts: [MockProduct]
        #if ENABLE_TARGET_BASED_DEPENDENCY_RESOLUTION
        barProducts = [MockProduct(name: "Bar", targets: ["Bar"]), MockProduct(name: "BarUnused", targets: ["BarUnused"])]
        #else
        // Whether a product is being used does not affect dependency resolution in this case, so we omit the unused product.
        barProducts = [MockProduct(name: "Bar", targets: ["Bar"])]
        #endif

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Root", dependencies: ["Foo", "Bar"]),
                        MockTarget(name: "RootTests", dependencies: ["TestHelper1"], type: .test),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./Foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./Bar", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./TestHelper1", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_2
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo1", dependencies: ["Foo2"]),
                        MockTarget(name: "Foo2", dependencies: ["Baz"]),
                        MockTarget(name: "FooTests", dependencies: ["TestHelper2"], type: .test),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo1"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./TestHelper2", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./Baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"],
                    toolsVersion: .v5_2
                ),
                MockPackage(
                    name: "Bar",
                    targets: [
                        MockTarget(name: "Bar"),
                        MockTarget(name: "BarUnused", dependencies: ["Biz"]),
                        MockTarget(name: "BarTests", dependencies: ["TestHelper2"], type: .test),
                    ],
                    products: barProducts,
                    dependencies: [
                        .sourceControl(path: "./TestHelper2", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "./Biz", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"],
                    toolsVersion: .v5_2
                ),
                MockPackage(
                    name: "Baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0"],
                    toolsVersion: .v5_2
                ),
                MockPackage(
                    name: "TestHelper1",
                    targets: [
                        MockTarget(name: "TestHelper1"),
                    ],
                    products: [
                        MockProduct(name: "TestHelper1", targets: ["TestHelper1"]),
                    ],
                    versions: ["1.0.0"],
                    toolsVersion: .v5_2
                ),
            ],
            toolsVersion: .v5_2
        )

        // Load the graph.
        try workspace.checkPackageGraph(roots: ["Root"]) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.0.0")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
            result.check(dependency: "baz", at: .checkout(.version("1.0.0")))
            result.check(dependency: "testhelper1", at: .checkout(.version("1.0.0")))
            result.check(notPresent: "biz")
            result.check(notPresent: "testhelper2")
        }
    }

    func testLocalArchivedArtifactExtractionHappyPath() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        // create dummy xcframework and artifactbundle directories from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "A1.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A1")
                case "A2.zip":
                    try createDummyArtifactBundle(fileSystem: fs, path: destinationPath, name: "A2")
                case "B.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "B")
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Foo", dependencies: [
                            .product(name: "A1", package: "A"),
                            .product(name: "A2", package: "A"),
                            .product(name: "B", package: "B"),
                        ])
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./A", requirement: .exact("1.0.0")),
                        .sourceControl(path: "./B", requirement: .exact("1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "A",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            path: "XCFrameworks/A1.zip"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            path: "ArtifactBundles/A2.zip"
                        ),
                    ],
                    products: [
                        MockProduct(name: "A1", targets: ["A1"]),
                        MockProduct(name: "A2", targets: ["A2"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "B",
                    targets: [
                        MockTarget(
                            name: "B",
                            type: .binary,
                            path: "XCFrameworks/B.zip"
                        )
                    ],
                    products: [
                        MockProduct(name: "B", targets: ["B"])
                    ],
                    versions: ["1.0.0"]
                )
            ],
            binaryArtifactsManager: .init(archiver: archiver)
        )

        // Create dummy xcframework/artifactbundle zip files
        let aPath = workspace.packagesDir.appending(components: "A")

        let aFrameworksPath = aPath.appending(component: "XCFrameworks")
        let a1FrameworkArchivePath = aFrameworksPath.appending(component: "A1.zip")
        try fs.createDirectory(aFrameworksPath, recursive: true)
        try fs.writeFileContents(a1FrameworkArchivePath, bytes: ByteString([0xA1]))

        let aArtifactBundlesPath = aPath.appending(component: "ArtifactBundles")
        let a2ArtifactBundleArchivePath = aArtifactBundlesPath.appending(component: "A2.zip")
        try fs.createDirectory(aArtifactBundlesPath, recursive: true)
        try fs.writeFileContents(a2ArtifactBundleArchivePath, bytes: ByteString([0xA2]))

        let bPath = workspace.packagesDir.appending(components: "B")

        let bFrameworksPath = bPath.appending(component: "XCFrameworks")
        let bFrameworkArchivePath = bFrameworksPath.appending(component: "B.zip")
        try fs.createDirectory(bFrameworksPath, recursive: true)
        try fs.writeFileContents(bFrameworkArchivePath, bytes: ByteString([0xB0]))

        // Ensure that the artifacts do not exist yet
        XCTAssertFalse(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/A/A1.xcframework")))
        XCTAssertFalse(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/A/A2.artifactbundle")))
        XCTAssertFalse(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/B/B.xcframework")))

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)

            // Ensure that the artifacts have been properly extracted
            XCTAssertTrue(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/a/A1/A1.xcframework")))
            XCTAssertTrue(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/a/A2/A2.artifactbundle")))
            XCTAssertTrue(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/b/B/B.xcframework")))

            // Ensure that the original archives have been untouched
            XCTAssertTrue(fs.exists(a1FrameworkArchivePath))
            XCTAssertTrue(fs.exists(a2ArtifactBundleArchivePath))
            XCTAssertTrue(fs.exists(bFrameworkArchivePath))

            // Ensure that the temporary folders have been properly created
            XCTAssertEqual(archiver.extractions.map { $0.destinationPath.parentDirectory }.sorted(), [
                AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A1"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A2"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/b/B")
            ])

            // Ensure that the temporary directories have been removed
            XCTAssertTrue(try! fs.getDirectoryContents(AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A1")).isEmpty)
            XCTAssertTrue(try! fs.getDirectoryContents(AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A2")).isEmpty)
            XCTAssertTrue(try! fs.getDirectoryContents(AbsolutePath("/tmp/ws/.build/artifacts/extract/b/B")).isEmpty)
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("a"),
                         targetName: "A1",
                         source: .local(checksum: "a1"),
                         path: workspace.artifactsDir.appending(components: "a", "A1", "A1.xcframework")
            )
            result.check(packageIdentity: .plain("a"),
                         targetName: "A2",
                         source: .local(checksum: "a2"),
                         path: workspace.artifactsDir.appending(components: "a", "A2", "A2.artifactbundle")
            )
            result.check(packageIdentity: .plain("b"),
                         targetName: "B",
                         source: .local(checksum: "b0"),
                         path: workspace.artifactsDir.appending(components: "b", "B", "B.xcframework")
            )
        }
    }

    // There are 6 possible transition permutations of the artifact source set
    // {local, local-archived, and remote}, namely:
    //
    // (remote         -> local)
    // (local          -> remote)
    // (local          -> local-archived)
    // (local-archived -> local)
    // (remote         -> local-archived)
    // (local-archived -> remote)
    //
    // This test covers the last 4 permutations where the `local-archived` source is involved.
    // It ensures that all the appropriate clean-up operations are executed, and the workspace
    // contains the correct set of managed artifacts after the transition.
    func testLocalArchivedArtifactSourceTransitionPermutations() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let a1FrameworkName = "A1.xcframework"
        let a2FrameworkName = "A2.xcframework"
        let a3FrameworkName = "A3.xcframework"
        let a4FrameworkName = "A4.xcframework"
        let a5FrameworkName = "A5.xcframework"

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                let contents: [UInt8]
                switch request.url.lastPathComponent {
                case "a4.zip":
                    contents = [0xA4]
                default:
                    throw StringError("unexpected url \(request.url)")
                }

                try fileSystem.writeFileContents(
                    destination,
                    bytes: ByteString(contents),
                    atomically: true
                )

                completion(.success(.okay()))
            } catch {
                completion(.failure(error))
            }
        })

        // create a dummy xcframework directory (with a marker subdirectory) from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                //var subdirectoryName: String?
                switch archivePath.basename {
                case "A1.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A1")
                case "A2.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A2")
                case "A3.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A3")
                    try fs.createDirectory(destinationPath.appending(components: [a3FrameworkName, "local-archived"]), recursive: false)
                case "a4.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A4")
                    try fs.createDirectory(destinationPath.appending(components: [a4FrameworkName, "remote"]), recursive: false)
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }

                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Foo", dependencies: [
                            .product(name: "A1", package: "A"),
                            .product(name: "A2", package: "A"),
                            .product(name: "A3", package: "A"),
                            .product(name: "A4", package: "A")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./A", requirement: .exact("1.0.0"))
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "A",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            path: "XCFrameworks/A1.zip"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            path: "XCFrameworks/\(a2FrameworkName)"
                        ),
                        MockTarget(
                            name: "A3",
                            type: .binary,
                            path: "XCFrameworks/A3.zip"
                        ),
                        MockTarget(
                            name: "A4",
                            type: .binary,
                            url: "https://a.com/a4.zip",
                            checksum: "a4"
                        )
                    ],
                    products: [
                        MockProduct(name: "A1", targets: ["A1"]),
                        MockProduct(name: "A2", targets: ["A2"]),
                        MockProduct(name: "A3", targets: ["A3"]),
                        MockProduct(name: "A4", targets: ["A4"])
                    ],
                    versions: ["1.0.0"]
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        // Create dummy xcframework directories and zip files
        let aFrameworksPath = workspace.packagesDir.appending(components: "A", "XCFrameworks")
        try fs.createDirectory(aFrameworksPath, recursive: true)

        let a1FrameworkPath = aFrameworksPath.appending(component: a1FrameworkName)
        let a1FrameworkArchivePath = aFrameworksPath.appending(component: "A1.zip")
        try fs.createDirectory(a1FrameworkPath, recursive: true)
        try fs.writeFileContents(a1FrameworkArchivePath, bytes: ByteString([0xA1]))

        let a2FrameworkPath = aFrameworksPath.appending(component: a2FrameworkName)
        let a2FrameworkArchivePath = aFrameworksPath.appending(component: "A2.zip")
        try createDummyXCFramework(fileSystem: fs, path: a2FrameworkPath.parentDirectory, name: "A2")
        try fs.writeFileContents(a2FrameworkArchivePath, bytes: ByteString([0xA2]))

        let a3FrameworkArchivePath = aFrameworksPath.appending(component: "A3.zip")
        try fs.writeFileContents(a3FrameworkArchivePath, bytes: ByteString([0xA3]))

        let a4FrameworkArchivePath = aFrameworksPath.appending(component: "A4.zip")
        try fs.writeFileContents(a4FrameworkArchivePath, bytes: ByteString([0xA4]))

        // Pin A to 1.0.0, Checkout B to 1.0.0
        let aPath = workspace.pathToPackage(withName: "A")
        let aRef = PackageReference.localSourceControl(identity: PackageIdentity(path: aPath), path: aPath)
        let aRepo = workspace.repositoryProvider.specifierMap[RepositorySpecifier(path: aPath)]!
        let aRevision = try aRepo.resolveRevision(tag: "1.0.0")
        let aState = CheckoutState.version("1.0.0", revision: aRevision)

        // Set an initial workspace state
        try workspace.set(
            pins: [aRef: aState],
            managedArtifacts: [
                .init(
                    packageRef: aRef,
                    targetName: "A1",
                    source: .local(),
                    path: a1FrameworkPath,
                    kind: .xcframework
                ),
                .init(
                    packageRef: aRef,
                    targetName: "A2",
                    source: .local(checksum: "a2"),
                    path: workspace.artifactsDir.appending(components: "A", a2FrameworkName),
                    kind: .xcframework
                ),
                .init(
                    packageRef: aRef,
                    targetName: "A3",
                    source: .remote(url: "https://a.com/a3.zip", checksum: "a3"),
                    path: workspace.artifactsDir.appending(components: "A", a3FrameworkName),
                    kind: .xcframework
                ),
                .init(
                    packageRef: aRef,
                    targetName: "A4",
                    source: .local(checksum: "a4"),
                    path: workspace.artifactsDir.appending(components: "A", a4FrameworkName),
                    kind: .xcframework
                ),
                .init(
                    packageRef: aRef,
                    targetName: "A5",
                    source: .local(checksum: "a5"),
                    path: workspace.artifactsDir.appending(components: "A", a5FrameworkName),
                    kind: .xcframework
                )
            ]
        )

        // Create marker folders to later check that the frameworks' content is properly overwritten
        try fs.createDirectory(workspace.artifactsDir.appending(components: "A", "A3", a3FrameworkName, "remote"), recursive: true)
        try fs.createDirectory(workspace.artifactsDir.appending(components: "A", "A4", a4FrameworkName, "local-archived"), recursive: true)

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)

            // Ensure that the original archives have been untouched
            XCTAssertTrue(fs.exists(a1FrameworkArchivePath))
            XCTAssertTrue(fs.exists(a2FrameworkArchivePath))
            XCTAssertTrue(fs.exists(a3FrameworkArchivePath))
            XCTAssertTrue(fs.exists(a4FrameworkArchivePath))

            // Ensure that the new artifacts have been properly extracted
            XCTAssertTrue(fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A1/\(a1FrameworkName)")))
            XCTAssertTrue(fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A3/\(a3FrameworkName)/local-archived")))
            XCTAssertTrue(fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A4/\(a4FrameworkName)/remote")))

            // Ensure that the old artifacts have been removed
            XCTAssertFalse(fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A2/\(a2FrameworkName)")))
            XCTAssertFalse(fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A3/\(a3FrameworkName)/remote")))
            XCTAssertFalse(fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A4/\(a4FrameworkName)/local-archived")))
            XCTAssertFalse(fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A5/\(a5FrameworkName)")))
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("a"),
                         targetName: "A1",
                         source: .local(checksum: "a1"),
                         path: workspace.artifactsDir.appending(components: "a", "A1", a1FrameworkName)
            )
            result.check(packageIdentity: .plain("a"),
                         targetName: "A2",
                         source: .local(),
                         path: a2FrameworkPath
            )
            result.check(packageIdentity: .plain("a"),
                         targetName: "A3",
                         source: .local(checksum: "a3"),
                         path: workspace.artifactsDir.appending(components: "a", "A3", a3FrameworkName)
            )
            result.check(packageIdentity: .plain("a"),
                         targetName: "A4",
                         source: .remote(url: "https://a.com/a4.zip", checksum: "a4"),
                         path: workspace.artifactsDir.appending(components: "a", "A4", a4FrameworkName)
            )
        }
    }

    func testLocalArchivedArtifactNameDoesNotMatchTargetName() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "archived-does-not-match-target-name.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "artifact-does-not-match-target-name")
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            path: "XCFrameworks/archived-does-not-match-target-name.zip"
                        )
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                archiver: archiver
            )
        )

        // Create dummy zip files
        let rootPath = workspace.pathToRoot(withName: "Root")
        let frameworksPath = rootPath.appending(component: "XCFrameworks")
        try fs.createDirectory(frameworksPath, recursive: true)
        try fs.writeFileContents(frameworksPath.appending(component: "archived-does-not-match-target-name.zip"), bytes: ByteString([0xA1]))

        try workspace.checkPackageGraph(roots: ["Root"]) { result, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testLocalArchivedArtifactExtractionError() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let archiver = MockArchiver(handler: { _, _, destinationPath, completion in
            completion(.failure(DummyError()))
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            path: "XCFrameworks/A1.zip"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            path: "ArtifactBundles/A2.zip"
                        )
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                archiver: archiver
            )
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.checkUnordered(diagnostic: .contains("failed extracting '\(sandbox.appending(components: "roots", "Root", "XCFrameworks", "A1.zip"))' which is required by binary target 'A1': dummy error"), severity: .error)
                result.checkUnordered(diagnostic: .contains("failed extracting '\(sandbox.appending(components: "roots", "Root", "ArtifactBundles", "A2.zip"))' which is required by binary target 'A2': dummy error"), severity: .error)
            }
        }
    }

    func testLocalArchiveDoesNotMatchTargetName() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        // create dummy xcframework and artifactbundle directories from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "A1.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "foo")
                case "A2.zip":
                    try createDummyArtifactBundle(fileSystem: fs, path: destinationPath, name: "bar")
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })
        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            path: "XCFrameworks/A1.zip"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            path: "ArtifactBundles/A2.zip"
                        )
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                archiver: archiver
            )
        )

        // Create dummy zip files
        let rootPath = workspace.pathToRoot(withName: "Root")
        let frameworksPath = rootPath.appending(component: "XCFrameworks")
        try fs.createDirectory(frameworksPath, recursive: true)
        try fs.writeFileContents(frameworksPath.appending(component: "A1.zip"), bytes: ByteString([0xA1]))

        let aArtifactBundlesPath = rootPath.appending(component: "ArtifactBundles")
        try fs.createDirectory(aArtifactBundlesPath, recursive: true)
        try fs.writeFileContents(aArtifactBundlesPath.appending(component: "A2.zip"), bytes: ByteString([0xA2]))

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("root"),
                         targetName: "A1",
                         source: .local(checksum: "a1"),
                         path: workspace.artifactsDir.appending(components: "root", "A1", "foo.xcframework")
            )
            result.check(packageIdentity: .plain("root"),
                         targetName: "A2",
                         source: .local(checksum: "a2"),
                         path: workspace.artifactsDir.appending(components: "root", "A2", "bar.artifactbundle")
            )
        }
    }

    func testLocalArchivedArtifactChecksumChange() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        // create dummy xcframework directories from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "A1.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A1")
                case "A2.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A2")
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            path: "XCFrameworks/A1.zip"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            path: "XCFrameworks/A2.zip"
                        )
                    ]
                ),
            ],
            binaryArtifactsManager: .init(
                archiver: archiver
            )
        )

        let rootPath = workspace.pathToRoot(withName: "Root")
        let rootRef = PackageReference.root(identity: PackageIdentity(path: rootPath), path: rootPath)

        // Set an initial workspace state
        try workspace.set(
            managedArtifacts: [
                .init(
                    packageRef: rootRef,
                    targetName: "A1",
                    source: .local(checksum: "old-checksum"),
                    path: workspace.artifactsDir.appending(components: "root", "A1", "A1.xcframework"),
                    kind: .xcframework
                ),
                .init(
                    packageRef: rootRef,
                    targetName: "A2",
                    source: .local(checksum: "a2"),
                    path: workspace.artifactsDir.appending(components: "root", "A2", "A2.xcframework"),
                    kind: .xcframework
                )
            ]
        )

        // Create dummy zip files
        let frameworksPath = rootPath.appending(components: "XCFrameworks")
        try fs.createDirectory(frameworksPath, recursive: true)

        let a1FrameworkArchivePath = frameworksPath.appending(component: "A1.zip")
        try fs.writeFileContents(a1FrameworkArchivePath, bytes: ByteString([0xA1]))

        let a2FrameworkArchivePath = frameworksPath.appending(component: "A2.zip")
        try fs.writeFileContents(a2FrameworkArchivePath, bytes: ByteString([0xA2]))

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            // Ensure that only the artifact archive with the changed checksum has been extracted
            XCTAssertEqual(archiver.extractions.map { $0.destinationPath.parentDirectory }.sorted(), [
                AbsolutePath("/tmp/ws/.build/artifacts/extract/root/A1")
            ])
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("root"),
                         targetName: "A1",
                         source: .local(checksum: "a1"),
                         path: workspace.artifactsDir.appending(components: "root", "A1", "A1.xcframework")
            )
            result.check(packageIdentity: .plain("root"),
                         targetName: "A2",
                         source: .local(checksum: "a2"),
                         path: workspace.artifactsDir.appending(components: "root", "A2", "A2.xcframework")
            )
        }
    }

    func testLocalArchivedArtifactStripFirstComponent() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()


        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "flat.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "flat")
                case "nested.zip":
                    let nestedPath = destinationPath.appending(component: "root")
                    try fs.createDirectory(nestedPath, recursive: true)
                    try createDummyXCFramework(fileSystem: fs, path: nestedPath, name: "nested")
                case "nested2.zip":
                    let nestedPath = destinationPath.appending(component: "root")
                    try fs.createDirectory(nestedPath, recursive: true)
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "nested2")
                    try fs.writeFileContents(nestedPath.appending(component: ".DS_Store"), bytes: []) // add a file next to the directory
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "flat",
                            type: .binary,
                            path: "frameworks/flat.zip"
                        ),
                        MockTarget(
                            name: "nested",
                            type: .binary,
                            path: "frameworks/nested.zip"
                        )
                        ,
                        MockTarget(
                            name: "nested2",
                            type: .binary,
                            path: "frameworks/nested2.zip"
                        )
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                archiver: archiver
            )
        )

        // create the mock archives
        let rootPath = workspace.pathToRoot(withName: "Root")
        let archivesPath = rootPath.appending(components: "frameworks")
        try fs.createDirectory(archivesPath, recursive: true)
        try fs.writeFileContents(archivesPath.appending(component: "flat.zip"), bytes: ByteString([0x1]))
        try fs.writeFileContents(archivesPath.appending(component: "nested.zip"), bytes: ByteString([0x2]))
        try fs.writeFileContents(archivesPath.appending(component: "nested2.zip"), bytes: ByteString([0x3]))

        // ensure that the artifacts do not exist yet
        XCTAssertFalse(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/root/flat/flat.xcframework")))
        XCTAssertFalse(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/root/nested/nested.artifactbundle")))
        XCTAssertFalse(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/root/nested2/nested2.xcframework")))

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            XCTAssert(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/root")))
            XCTAssertEqual(archiver.extractions.map { $0.destinationPath.parentDirectory }.sorted(), [
                AbsolutePath("/tmp/ws/.build/artifacts/extract/root/flat"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/root/nested"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/root/nested2"),
            ])
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("root"),
                         targetName: "flat",
                         source: .local(checksum: "01"),
                         path: workspace.artifactsDir.appending(components: "root", "flat", "flat.xcframework")
            )
            result.check(packageIdentity: .plain("root"),
                         targetName: "nested",
                         source: .local(checksum: "02"),
                         path: workspace.artifactsDir.appending(components: "root", "nested", "nested.xcframework")
            )
            result.check(packageIdentity: .plain("root"),
                         targetName: "nested2",
                         source: .local(checksum: "03"),
                         path: workspace.artifactsDir.appending(components: "root", "nested2", "nested2.xcframework")
            )
        }
    }

    func testLocalArtifactHappyPath() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            path: "XCFrameworks/A1.xcframework"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            path: "ArtifactBundles/A2.artifactbundle"
                        )
                    ]
                )
            ]
        )

        let rootPath = workspace.pathToRoot(withName: "Root")

        // make sure the directory exist in their destined location
        try createDummyXCFramework(fileSystem: fs, path: rootPath.appending(component: "XCFrameworks"), name: "A1")
        try createDummyArtifactBundle(fileSystem: fs, path: rootPath.appending(component: "ArtifactBundles"), name: "A2")

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("root"),
                         targetName: "A1",
                         source: .local(checksum: .none),
                         path: rootPath.appending(components: "XCFrameworks", "A1.xcframework")
            )
            result.check(packageIdentity: .plain("root"),
                         targetName: "A2",
                         source: .local(checksum: .none),
                         path: rootPath.appending(components: "ArtifactBundles", "A2.artifactbundle")
            )
        }
    }

    func testLocalArtifactDoesNotExist() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            path: "XCFrameworks/incorrect.xcframework"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            path: "ArtifactBundles/incorrect.artifactbundle"
                        )
                    ]
                )
            ]
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.checkUnordered(diagnostic: .contains("local binary target 'A1' at '/tmp/ws/roots/Root/XCFrameworks/incorrect.xcframework' does not contain a binary artifact.") , severity: .error)
                result.checkUnordered(diagnostic: .contains("local binary target 'A2' at '/tmp/ws/roots/Root/ArtifactBundles/incorrect.artifactbundle' does not contain a binary artifact.") , severity: .error)
            }
        }
    }

    func testArtifactDownloadHappyPath() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let downloads = ThreadSafeKeyValueStore<URL, AbsolutePath>()

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                let contents: [UInt8]
                switch request.url.lastPathComponent {
                case "a1.zip":
                    contents = [0xA1]
                case "a2.zip":
                    contents = [0xA2]
                case "b.zip":
                    contents = [0xB0]
                default:
                    throw StringError("unexpected url \(request.url)")
                }

                try fileSystem.writeFileContents(
                    destination,
                    bytes: ByteString(contents),
                    atomically: true
                )

                downloads[request.url] = destination
                completion(.success(.okay()))
            } catch {
                completion(.failure(error))
            }
        })

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "a1.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A1")
                case "a2.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A2")
                case "b.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "B")
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "A1", package: "A"),
                            .product(name: "A2", package: "A"),
                            "B"
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./A", requirement: .exact("1.0.0")),
                        .sourceControl(path: "./B", requirement: .exact("1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "A",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/a1.zip",
                            checksum: "a1"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            url: "https://a.com/a2.zip",
                            checksum: "a2"
                        )
                    ],
                    products: [
                        MockProduct(name: "A1", targets: ["A1"]),
                        MockProduct(name: "A2", targets: ["A2"])
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "B",
                    targets: [
                        MockTarget(
                            name: "B",
                            type: .binary,
                            url: "https://b.com/b.zip",
                            checksum: "b0"
                        ),
                    ],
                    products: [
                        MockProduct(name: "B", targets: ["B"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            XCTAssert(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/a")))
            XCTAssert(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/b")))
            XCTAssertEqual(downloads.map { $0.key.absoluteString }.sorted(), [
                "https://a.com/a1.zip",
                "https://a.com/a2.zip",
                "https://b.com/b.zip",
            ])
            XCTAssertEqual(workspace.checksumAlgorithm.hashes.map{ $0.hexadecimalRepresentation }.sorted(), [
                ByteString([0xA1]).hexadecimalRepresentation,
                ByteString([0xA2]).hexadecimalRepresentation,
                ByteString([0xB0]).hexadecimalRepresentation,
            ])
            XCTAssertEqual(archiver.extractions.map { $0.destinationPath.parentDirectory }.sorted(), [
                AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A1"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A2"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/b/B")
            ])
            XCTAssertEqual(
                downloads.map { $0.value }.sorted(),
                archiver.extractions.map { $0.archivePath }.sorted()
            )
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("a"),
                         targetName: "A1",
                         source: .remote(
                            url: "https://a.com/a1.zip",
                            checksum: "a1"
                         ),
                         path: workspace.artifactsDir.appending(components: "a", "A1", "A1.xcframework")
            )
            result.check(packageIdentity: .plain("a"),
                         targetName: "A2",
                         source: .remote(
                            url: "https://a.com/a2.zip",
                            checksum: "a2"
                         ),
                         path: workspace.artifactsDir.appending(components: "a", "A2", "A2.xcframework")
            )
            result.check(packageIdentity: .plain("b"),
                         targetName: "B",
                         source: .remote(
                            url: "https://b.com/b.zip",
                            checksum: "b0"
                         ),
                         path: workspace.artifactsDir.appending(components: "b", "B", "B.xcframework")
            )
        }

        XCTAssertMatch(workspace.delegate.events, ["downloading binary artifact package: https://a.com/a1.zip"])
        XCTAssertMatch(workspace.delegate.events, ["downloading binary artifact package: https://a.com/a2.zip"])
        XCTAssertMatch(workspace.delegate.events, ["downloading binary artifact package: https://b.com/b.zip"])
        XCTAssertMatch(workspace.delegate.events, ["finished downloading binary artifact package: https://a.com/a1.zip"])
        XCTAssertMatch(workspace.delegate.events, ["finished downloading binary artifact package: https://a.com/a2.zip"])
        XCTAssertMatch(workspace.delegate.events, ["finished downloading binary artifact package: https://b.com/b.zip"])
    }

    func testArtifactDownloadWithPreviousState() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let downloads = ThreadSafeKeyValueStore<URL, AbsolutePath>()

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                let contents: [UInt8]
                switch request.url.lastPathComponent {
                case "a1.zip":
                    contents = [0xA1]
                case "a2.zip":
                    contents = [0xA2]
                case "a3.zip":
                    contents = [0xA3]
                case "a7.zip":
                    contents = [0xA7]
                case "b.zip":
                    contents = [0xB0]
                default:
                    throw StringError("unexpected url \(request.url)")
                }

                try fileSystem.writeFileContents(
                    destination,
                    bytes: ByteString(contents),
                    atomically: true
                )

                downloads[request.url] = destination
                completion(.success(.okay()))
            } catch {
                completion(.failure(error))
            }
        })

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "a1.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A1")
                case "a2.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A2")
                case "a3.zip":
                    fs.createEmptyFiles(at: destinationPath, files: ".DS_Store") // invalid binary artifact
                case "a7.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A7")
                case "b.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "B")
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            "B",
                            .product(name: "A1", package: "A"),
                            .product(name: "A2", package: "A"),
                            .product(name: "A3", package: "A"),
                            .product(name: "A4", package: "A"),
                            .product(name: "A7", package: "A")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./A", requirement: .exact("1.0.0")),
                        .sourceControl(path: "./B", requirement: .exact("1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "A",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/a1.zip",
                            checksum: "a1"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            url: "https://a.com/a2.zip",
                            checksum: "a2"
                        ),
                        MockTarget(
                            name: "A3",
                            type: .binary,
                            url: "https://a.com/a3.zip",
                            checksum: "a3"
                        ),
                        MockTarget(
                            name: "A4",
                            type: .binary,
                            path: "XCFrameworks/A4.xcframework"
                        ),
                        MockTarget(
                            name: "A7",
                            type: .binary,
                            url: "https://a.com/a7.zip",
                            checksum: "a7"
                        )
                    ],
                    products: [
                        MockProduct(name: "A1", targets: ["A1"]),
                        MockProduct(name: "A2", targets: ["A2"]),
                        MockProduct(name: "A3", targets: ["A3"]),
                        MockProduct(name: "A4", targets: ["A4"]),
                        MockProduct(name: "A7", targets: ["A7"])
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "B",
                    targets: [
                        MockTarget(
                            name: "B",
                            type: .binary,
                            url: "https://b.com/b.zip",
                            checksum: "b0"
                        ),
                    ],
                    products: [
                        MockProduct(name: "B", targets: ["B"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        let a4FrameworkPath = workspace.packagesDir.appending(components: "A", "XCFrameworks", "A4.xcframework")
        try createDummyXCFramework(fileSystem: fs, path: a4FrameworkPath.parentDirectory, name: "A4")

        // Pin A to 1.0.0, Checkout B to 1.0.0
        let aPath = workspace.pathToPackage(withName: "A")
        let aRef = PackageReference.localSourceControl(identity: PackageIdentity(path: aPath), path: aPath)
        let aRepo = workspace.repositoryProvider.specifierMap[RepositorySpecifier(path: aPath)]!
        let aRevision = try aRepo.resolveRevision(tag: "1.0.0")
        let aState = CheckoutState.version("1.0.0", revision: aRevision)

        try workspace.set(
            pins: [aRef: aState],
            managedArtifacts: [
                .init(
                    packageRef: aRef,
                    targetName: "A1",
                    source: .remote(
                        url: "https://a.com/a1.zip",
                        checksum: "a1"
                    ),
                    path: workspace.artifactsDir.appending(components: "a", "A1", "A1.xcframework"),
                    kind: .xcframework
                ),
                .init(
                    packageRef: aRef,
                    targetName: "A3",
                    source: .remote(
                        url: "https://a.com/old/a3.zip",
                        checksum: "a3-old-checksum"
                    ),
                    path: workspace.artifactsDir.appending(components: "a", "A3", "A3.xcframework"),
                    kind: .xcframework
                ),
                .init(
                    packageRef: aRef,
                    targetName: "A4",
                    source: .remote(
                        url: "https://a.com/a4.zip",
                        checksum: "a4"
                    ),
                    path: workspace.artifactsDir.appending(components: "a", "A4", "A4.xcframework"),
                    kind: .xcframework
                ),
                .init(
                    packageRef: aRef,
                    targetName: "A5",
                    source: .remote(
                        url: "https://a.com/a5.zip",
                        checksum: "a5"
                    ),
                    path: workspace.artifactsDir.appending(components: "a", "A5","A5.xcframework"),
                    kind: .xcframework
                ),
                .init(
                    packageRef: aRef,
                    targetName: "A6",
                    source: .local(),
                    path: workspace.artifactsDir.appending(components: "a", "A6", "A6.xcframework"),
                    kind: .xcframework
                ),
                .init(
                    packageRef: aRef,
                    targetName: "A7",
                    source: .local(),
                    path: workspace.packagesDir.appending(components: "a", "XCFrameworks", "A7.xcframework"),
                    kind: .xcframework
                )
            ]
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: "downloaded archive of binary target 'A3' from 'https://a.com/a3.zip' does not contain a binary artifact.", severity: .error)
            }
            XCTAssert(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/b")))
            XCTAssert(fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A1/A1.xcframework")))
            XCTAssert(fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A2/A2.xcframework")))
            XCTAssert(!fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A3/A3.xcframework")))
            XCTAssert(!fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A4/A4.xcframework")))
            XCTAssert(!fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/a/A5/A5.xcframework")))
            XCTAssert(fs.exists(AbsolutePath("/tmp/ws/pkgs/a/XCFrameworks/A7.xcframework")))
            XCTAssert(!fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/Foo")))
            XCTAssertEqual(downloads.map { $0.key.absoluteString }.sorted(), [
                "https://a.com/a2.zip",
                "https://a.com/a3.zip",
                "https://a.com/a7.zip",
                "https://b.com/b.zip",
            ])
            XCTAssertEqual(workspace.checksumAlgorithm.hashes.map{ $0.hexadecimalRepresentation }.sorted(), [
                ByteString([0xA2]).hexadecimalRepresentation,
                ByteString([0xA3]).hexadecimalRepresentation,
                ByteString([0xA7]).hexadecimalRepresentation,
                ByteString([0xB0]).hexadecimalRepresentation,
            ])
            XCTAssertEqual(archiver.extractions.map { $0.destinationPath.parentDirectory }.sorted(), [
                AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A2"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A3"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A7"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/b/B"),
            ])
            XCTAssertEqual(
                downloads.map { $0.value }.sorted(),
                archiver.extractions.map { $0.archivePath }.sorted()
            )
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("a"),
                         targetName: "A1",
                         source: .remote(
                            url: "https://a.com/a1.zip",
                            checksum: "a1"
                         ),
                         path: workspace.artifactsDir.appending(components: "a", "A1", "A1.xcframework")
            )
            result.check(packageIdentity: .plain("a"),
                         targetName: "A2",
                         source: .remote(
                            url: "https://a.com/a2.zip",
                            checksum: "a2"
                         ),
                         path: workspace.artifactsDir.appending(components: "a", "A2", "A2.xcframework")
            )
            result.checkNotPresent(packageName: "A", targetName: "A3")
            result.check(packageIdentity: .plain("a"),
                         targetName: "A4",
                         source: .local(),
                         path: a4FrameworkPath
            )
            result.check(packageIdentity: .plain("a"),
                         targetName: "A7",
                         source: .remote(
                            url: "https://a.com/a7.zip",
                            checksum: "a7"
                         ),
                         path: workspace.artifactsDir.appending(components: "a", "A7", "A7.xcframework")
            )
            result.checkNotPresent(packageName: "A", targetName: "A5")
            result.check(packageIdentity: .plain("b"),
                         targetName: "B",
                         source: .remote(
                            url: "https://b.com/b.zip",
                            checksum: "b0"
                         ),
                         path: workspace.artifactsDir.appending(components: "b", "B", "B.xcframework")
            )
        }
    }

    func testArtifactDownloadTwice() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let downloads = ThreadSafeArrayStore<(URL, AbsolutePath)>()

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                let contents: [UInt8]
                switch request.url.lastPathComponent {
                case "a1.zip":
                    contents = [0xA1]
                default:
                    throw StringError("unexpected url \(request.url)")
                }

                try fileSystem.writeFileContents(
                    destination,
                    bytes: ByteString(contents),
                    atomically: true
                )

                downloads.append((request.url, destination))
                completion(.success(.okay()))
            } catch {
                completion(.failure(error))
            }
        })

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                let name: String
                switch archivePath.basename {
                case "a1.zip":
                    name = "A1.xcframework"
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                let path = destinationPath.appending(component: name)
                if fs.exists(path) {
                    throw StringError("\(path) already exists")
                }
                try createDummyXCFramework(fileSystem: fs, path: path.parentDirectory, name: "A1")
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/a1.zip",
                            checksum: "a1"
                        ),
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            XCTAssert(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/root")))
            XCTAssertEqual(workspace.checksumAlgorithm.hashes.map{ $0.hexadecimalRepresentation }.sorted(), [
                ByteString([0xA1]).hexadecimalRepresentation,
            ])
        }

        XCTAssertEqual(downloads.map { $0.0.absoluteString }.sorted(), [
            "https://a.com/a1.zip",
        ])
        XCTAssertEqual(archiver.extractions.map { $0.destinationPath.parentDirectory }.sorted(), [
            AbsolutePath("/tmp/ws/.build/artifacts/extract/root/A1"),
        ])
        XCTAssertEqual(
            downloads.map { $0.1 }.sorted(),
            archiver.extractions.map { $0.archivePath }.sorted()
        )

        // reset

        try workspace.resetState()

        // do it again

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            XCTAssert(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/root")))

            XCTAssertEqual(workspace.checksumAlgorithm.hashes.map{ $0.hexadecimalRepresentation }.sorted(), [
                ByteString([0xA1]).hexadecimalRepresentation, ByteString([0xA1]).hexadecimalRepresentation,
            ])
        }

        XCTAssertEqual(downloads.map { $0.0.absoluteString }.sorted(), [
            "https://a.com/a1.zip", "https://a.com/a1.zip",
        ])
        XCTAssertEqual(archiver.extractions.map { $0.destinationPath.parentDirectory }.sorted(), [
            AbsolutePath("/tmp/ws/.build/artifacts/extract/root/A1"),
            AbsolutePath("/tmp/ws/.build/artifacts/extract/root/A1"),
        ])
        XCTAssertEqual(
            downloads.map { $0.1 }.sorted(),
            archiver.extractions.map { $0.archivePath }.sorted()
        )
    }

    func testArtifactDownloadServerError() throws {
        let fs = InMemoryFileSystem()
        let sandbox = AbsolutePath("/tmp/ws/")
        try fs.createDirectory(sandbox, recursive: true)

        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                // mimics URLSession behavior which write the file even if sends an error message
                try fileSystem.writeFileContents(
                    destination,
                    bytes: "not found",
                    atomically: true
                )

                completion(.success(.notFound()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/a.zip",
                            checksum: "a1"
                        ),
                    ]
                ),
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient
            )
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("failed downloading 'https://a.com/a.zip' which is required by binary target 'A1': badResponseStatusCode(404)"), severity: .error)
            }
        }

        // make sure artifact downloaded is deleted
        XCTAssertTrue(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/root")))
        XCTAssertFalse(fs.exists(AbsolutePath("/tmp/ws/.build/artifacts/root/a.zip")))
    }

    func testArtifactDownloaderOrArchiverError() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                switch request.url {
                case URL(string: "https://a.com/a1.zip")!:
                    completion(.success(.serverError()))
                case URL(string: "https://a.com/a2.zip")!:
                    try fileSystem.writeFileContents(destination, bytes: ByteString([0xA2]))
                    completion(.success(.okay()))
                case URL(string: "https://a.com/a3.zip")!:
                    try fileSystem.writeFileContents(destination, bytes: "different contents = different checksum")
                    completion(.success(.okay()))
                default:
                    throw StringError("unexpected url")
                }
            } catch {
                completion(.failure(error))
            }
        })

        let archiver = MockArchiver(handler: { _, _, destinationPath, completion in
            XCTAssertEqual(destinationPath.parentDirectory, AbsolutePath("/tmp/ws/.build/artifacts/extract/root/A2"))
            completion(.failure(DummyError()))
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/a1.zip",
                            checksum: "a1"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            url: "https://a.com/a2.zip",
                            checksum: "a2"
                        ),
                        MockTarget(
                            name: "A3",
                            type: .binary,
                            url: "https://a.com/a3.zip",
                            checksum: "a3"
                        ),
                    ]
                ),
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.checkUnordered(diagnostic: .contains("failed downloading 'https://a.com/a1.zip' which is required by binary target 'A1': badResponseStatusCode(500)"), severity: .error)
                result.checkUnordered(diagnostic: .contains("failed extracting 'https://a.com/a2.zip' which is required by binary target 'A2': dummy error"), severity: .error)
                result.checkUnordered(diagnostic: .contains("checksum of downloaded artifact of binary target 'A3' (6d75736b6365686320746e65726566666964203d2073746e65746e6f6320746e65726566666964) does not match checksum specified by the manifest (a3)"), severity: .error)
            }
        }
    }

    func testDownloadedArtifactNotAnArchiveError() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                switch request.url {
                case URL(string: "https://a.com/a1.zip")!:
                    try fileSystem.writeFileContents(destination, bytes: ByteString([0xA1]))
                    completion(.success(.okay()))
                case URL(string: "https://a.com/a2.zip")!:
                    try fileSystem.writeFileContents(destination, bytes: ByteString([0xA2]))
                    completion(.success(.okay()))
                case URL(string: "https://a.com/a3.zip")!:
                    try fileSystem.writeFileContents(destination, bytes: ByteString([0xA3]))
                    completion(.success(.okay()))
                default:
                    throw StringError("unexpected url")
                }
            } catch {
                completion(.failure(error))
            }
        })

        let archiver = MockArchiver(
            extractionHandler: { archiver, archivePath, destinationPath, completion in
                do {
                    if archivePath.basenameWithoutExt == "a1" {
                        try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A1")
                        archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                        completion(.success(()))
                    } else {
                        throw StringError("unexpected path")
                    }
                } catch {
                    completion(.failure(error))
                }
            },
            validationHandler: { _, path, completion in
                if path.basenameWithoutExt == "a1" {
                    completion(.success(true))
                } else if path.basenameWithoutExt == "a2" {
                    completion(.success(false))
                } else if path.basenameWithoutExt == "a3" {
                    completion(.failure(DummyError()))
                } else {
                    XCTFail("unexpected path")
                    completion(.success(false))
                }
            })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/a1.zip",
                            checksum: "a1"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            url: "https://a.com/a2.zip",
                            checksum: "a2"
                        ),
                        MockTarget(
                            name: "A3",
                            type: .binary,
                            url: "https://a.com/a3.zip",
                            checksum: "a3"
                        ),
                    ],
                    products: [],
                    dependencies: []
                ),
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.checkUnordered(diagnostic: .contains("invalid archive returned from 'https://a.com/a2.zip' which is required by binary target 'A2'"), severity: .error)
                result.checkUnordered(diagnostic: .contains("failed validating archive from 'https://a.com/a3.zip' which is required by binary target 'A3': dummy error"), severity: .error)
            }
        }
    }

    func testDownloadedArtifactInvalid() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                switch request.url {
                case URL(string: "https://a.com/a1.zip")!:
                    try fileSystem.writeFileContents(destination, bytes: ByteString([0xA1]))
                    completion(.success(.okay()))
                default:
                    throw StringError("unexpected url")
                }
            } catch {
                completion(.failure(error))
            }
        })

        let archiver = MockArchiver(
            extractionHandler: { archiver, archivePath, destinationPath, completion in
                do {
                    if archivePath.basenameWithoutExt == "a1" {
                        // create file instead of directory
                        fs.createEmptyFiles(at: destinationPath, files: "A1.Framework")
                        completion(.success(()))
                    } else {
                        throw StringError("unexpected path")
                    }
                } catch {
                    completion(.failure(error))
                }
            })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/a1.zip",
                            checksum: "a1"
                        )
                    ],
                    products: [],
                    dependencies: []
                ),
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.checkUnordered(diagnostic: .contains("downloaded archive of binary target 'A1' from 'https://a.com/a1.zip' does not contain a binary artifact."), severity: .error)
            }
        }
    }

    func testDownloadedArtifactDoesNotMatchTargetName() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                switch request.url {
                case URL(string: "https://a.com/foo.zip")!:
                    try fileSystem.writeFileContents(destination, bytes: ByteString([0xA1]))
                    completion(.success(.okay()))
                default:
                    throw StringError("unexpected url")
                }
            } catch {
                completion(.failure(error))
            }
        })

        let archiver = MockArchiver(
            extractionHandler: { archiver, archivePath, destinationPath, completion in
                do {
                    if archivePath.basename == "foo.zip" {
                        try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "bar")
                        completion(.success(()))
                    } else {
                        throw StringError("unexpected path")
                    }
                } catch {
                    completion(.failure(error))
                }
            })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/foo.zip",
                            checksum: "a1"
                        )
                    ],
                    products: [],
                    dependencies: []
                ),
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("root"),
                         targetName: "A1",
                         source: .remote(
                            url: "https://a.com/foo.zip",
                            checksum: "a1"
                         ),
                         path: workspace.artifactsDir.appending(components: "root", "A1", "bar.xcframework")
            )
        }
    }

    func testArtifactChecksum() throws {
        let fs = InMemoryFileSystem()
        let sandbox = AbsolutePath("/tmp/ws/")
        try fs.createDirectory(sandbox, recursive: true)

        let checksumAlgorithm = MockHashAlgorithm()
        let binaryArtifactsManager = try Workspace.BinaryArtifactsManager(
            fileSystem: fs,
            authorizationProvider: .none,
            hostToolchain: UserToolchain(destination: .hostDestination()),
            checksumAlgorithm: checksumAlgorithm,
            customHTTPClient: .none,
            customArchiver: .none,
            delegate: .none
        )

        // Checks the valid case.
        do {
            let binaryPath = sandbox.appending(component: "binary.zip")
            try fs.writeFileContents(binaryPath, bytes: ByteString([0xAA, 0xBB, 0xCC]))

            let checksum = try binaryArtifactsManager.checksum(forBinaryArtifactAt: binaryPath)
            XCTAssertEqual(checksumAlgorithm.hashes.map { $0.contents }, [[0xAA, 0xBB, 0xCC]])
            XCTAssertEqual(checksum, "ccbbaa")
        }

        // Checks an unsupported extension.
        do {
            let unknownPath = sandbox.appending(component: "unknown")
            XCTAssertThrowsError(try binaryArtifactsManager.checksum(forBinaryArtifactAt: unknownPath), "error expected") { error in
                XCTAssertEqual(error as? StringError, StringError("unexpected file type; supported extensions are: zip"))
            }
        }

        // Checks a supported extension that is not a file (does not exist).
        do {
            let unknownPath = sandbox.appending(component: "missingFile.zip")
            XCTAssertThrowsError(try binaryArtifactsManager.checksum(forBinaryArtifactAt: unknownPath), "error expected") { error in
                XCTAssertEqual(error as? StringError, StringError("file not found at path: \(sandbox.appending(component: "missingFile.zip"))"))
            }
        }

        // Checks a supported extension that is a directory instead of a file.
        do {
            let unknownPath = sandbox.appending(component: "aDirectory.zip")
            try fs.createDirectory(unknownPath)
            XCTAssertThrowsError(try binaryArtifactsManager.checksum(forBinaryArtifactAt: unknownPath), "error expected") { error in
                XCTAssertEqual(error as? StringError, StringError("file not found at path: \(sandbox.appending(component: "aDirectory.zip"))"))
            }
        }
    }

    func testDownloadedArtifactChecksumChange() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let httpClient = HTTPClient(handler: { request, _, completion in
            XCTFail("should not be called")
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "A", type: .binary, url: "https://a.com/a.zip", checksum: "new-checksum"),
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient
            )
        )

        let rootPath = workspace.pathToRoot(withName: "Root")
        let rootRef = PackageReference.root(identity: PackageIdentity(path: rootPath), path: rootPath)

        try workspace.set(
            managedArtifacts: [
                .init(
                    packageRef: rootRef,
                    targetName: "A",
                    source: .remote(
                        url: "https://a.com/a.zip",
                        checksum: "old-checksum"
                    ),
                    path: workspace.artifactsDir.appending(components: "Root", "A.xcframework"),
                    kind: .xcframework
                ),
            ]
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("artifact of binary target 'A' has changed checksum"), severity: .error)
            }
        }
    }

    func testDownloadedArtifactChecksumChangeURLChange() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                let contents: [UInt8]
                switch request.url.lastPathComponent {
                case "a.zip":
                    contents = [0xA1]
                case "b.zip":
                    contents = [0xB1]
                default:
                    throw StringError("unexpected url \(request.url)")
                }

                try fileSystem.writeFileContents(
                    destination,
                    bytes: ByteString(contents),
                    atomically: true
                )

                completion(.success(.okay()))
            } catch {
                completion(.failure(error))
            }
        })

        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "a.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A")
                case "b.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "B")
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "A", type: .binary, url: "https://a.com/b.zip",
                                   checksum: "b1"),
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        let rootPath = workspace.pathToRoot(withName: "Root")
        let rootRef = PackageReference.root(identity: PackageIdentity(path: rootPath), path: rootPath)

        try workspace.set(
            managedArtifacts: [
                .init(
                    packageRef: rootRef,
                    targetName: "A",
                    source: .remote(
                        url: "https://a.com/a.zip",
                        checksum: "a1"
                    ),
                    path: workspace.artifactsDir.appending(components: "Root", "A.xcframework"),
                    kind: .xcframework
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { _, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testArtifactDownloadAddsAcceptHeader() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let downloads = ThreadSafeKeyValueStore<URL, AbsolutePath>()
        var acceptHeaders: [String] = []

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }
                acceptHeaders.append(request.headers.get("accept").first!)

                let contents: [UInt8]
                switch request.url.lastPathComponent {
                    case "a1.zip":
                        contents = [0xA1]
                    default:
                        throw StringError("unexpected url \(request.url)")
                }

                try fileSystem.writeFileContents(
                    destination,
                    bytes: ByteString(contents),
                    atomically: true
                )

                downloads[request.url] = destination
                completion(.success(.okay()))
            } catch {
                completion(.failure(error))
            }
        })

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "a1.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A1")
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/a1.zip",
                            checksum: "a1"
                        ),
                    ]
                ),
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            XCTAssertEqual(acceptHeaders, [
                "application/octet-stream"
            ])
        }
    }

    func testDownloadedArtifactTransitive() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let downloads = ThreadSafeKeyValueStore<URL, AbsolutePath>()

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                let contents: [UInt8]
                switch request.url.lastPathComponent {
                case "a.zip":
                    contents = [0xA]
                default:
                    throw StringError("unexpected url \(request.url)")
                }

                try fileSystem.writeFileContents(
                    destination,
                    bytes: ByteString(contents),
                    atomically: true
                )

                if downloads[request.url] != nil {
                    throw StringError("\(request.url) already requested")
                }
                downloads[request.url] = destination
                completion(.success(.okay()))
            } catch {
                completion(.failure(error))
            }
        })

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "a.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "A")
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }

                if archiver.extractions.get().contains(where: { $0.archivePath == archivePath }) {
                    throw StringError("\(archivePath) already extracted")
                }

                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "A", package: "A"),
                            .product(name: "B", package: "B"),
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./A", requirement: .exact("1.0.0")),
                        .sourceControl(path: "./B", requirement: .exact("1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "A",
                    targets: [
                        MockTarget(name: "A", type: .binary, url: "https://a.com/a.zip", checksum: "0a")
                    ],
                    products: [
                        MockProduct(name: "A", targets: ["A"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "B",
                    targets: [
                        MockTarget(name: "B", dependencies: [
                            .product(name: "C", package: "C"),
                            .product(name: "D", package: "D"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "B", targets: ["B"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./C", requirement: .exact("1.0.0")),
                        .sourceControl(path: "./D", requirement: .exact("1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "C",
                    targets: [
                        MockTarget(name: "C", dependencies: [
                            .product(name: "A", package: "A"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "C", targets: ["C"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./A", requirement: .exact("1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "D",
                    targets: [
                        MockTarget(name: "D", dependencies: [
                            .product(name: "A", package: "A"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "D", targets: ["D"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "./A", requirement: .exact("1.0.0")),
                    ],
                    versions: ["1.0.0"]
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            XCTAssert(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/a")))
            XCTAssertEqual(downloads.map { $0.key.absoluteString }.sorted(), [
                "https://a.com/a.zip"
            ])
            XCTAssertEqual(workspace.checksumAlgorithm.hashes.map{ $0.hexadecimalRepresentation }.sorted(), [
                ByteString([0xA]).hexadecimalRepresentation
            ])
            XCTAssertEqual(archiver.extractions.map { $0.destinationPath.parentDirectory }.sorted(), [
                AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A")
            ])
            XCTAssertEqual(
                downloads.map { $0.value }.sorted(),
                archiver.extractions.map { $0.archivePath }.sorted()
            )
        }

        workspace.checkManagedArtifacts { result in
            result.check(
                packageIdentity: .plain("a"),
                targetName: "A",
                source: .remote(
                    url: "https://a.com/a.zip",
                    checksum: "0a"
                ),
                path: workspace.artifactsDir.appending(components: "a", "A", "A.xcframework")
            )
        }
    }

    func testDownloadedArtifactArchiveExists() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        // this relies on internal knowledge of the destination path construction
        let expectedDownloadDestination = sandbox.appending(components: ".build", "artifacts", "root", "binary", "binary.zip")

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                // this is to test the test's integrity, as it relied on internal knowledge of the destination path construction
                guard expectedDownloadDestination == destination else {
                    throw StringError("expected destination of \(expectedDownloadDestination)")
                }

                let contents: [UInt8]
                switch request.url.lastPathComponent {
                    case "binary.zip":
                        contents = [0x01]
                    default:
                        throw StringError("unexpected url \(request.url)")
                }

                // in-memory fs does not check for this!
                if fileSystem.exists(destination) {
                    throw StringError("\(destination) already exists")
                }

                try fileSystem.writeFileContents(
                    destination,
                    bytes: ByteString(contents),
                    atomically: true
                )

                completion(.success(.okay()))
            } catch {
                completion(.failure(error))
            }
        })

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "binary.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "binary")
                    archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "binary",
                            type: .binary,
                            url: "https://a.com/binary.zip",
                            checksum: "01"
                        ),
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        // write the file to test it gets deleted

        try fs.createDirectory(expectedDownloadDestination.parentDirectory, recursive: true)
        try fs.writeFileContents(
            expectedDownloadDestination,
            bytes: [],
            atomically: true
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }

        workspace.checkManagedArtifacts { result in
            result.check(
                packageIdentity: .plain("root"),
                targetName: "binary",
                source: .remote(
                    url: "https://a.com/binary.zip",
                    checksum: "01"
                ),
                path: workspace.artifactsDir.appending(components: "root", "binary", "binary.xcframework")
            )
        }
    }

    func testDownloadedArtifactConcurrency() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let maxConcurrentRequests = 2
        var concurrentRequests = 0
        let concurrentRequestsLock = NSLock()

        var configuration = HTTPClient.Configuration()
        configuration.maxConcurrentRequests = maxConcurrentRequests
        let httpClient = HTTPClient(configuration: configuration, handler: { request, _, completion in
            defer {
                concurrentRequestsLock.withLock {
                    concurrentRequests -= 1
                }
            }

            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                concurrentRequestsLock.withLock {
                    concurrentRequests += 1
                    if concurrentRequests > maxConcurrentRequests {
                        XCTFail("too many concurrent requests \(concurrentRequests), expected \(maxConcurrentRequests)")
                    }
                }

                // returns a dummy zipfile for the requested artifact
                try fileSystem.writeFileContents(
                    destination,
                    bytes: [0x01],
                    atomically: true
                )

                completion(.success(.okay()))
            } catch {
                completion(.failure(error))
            }
        })

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: archivePath.basenameWithoutExt)
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let packages = try (0 ... maxConcurrentRequests * 10).map { index in
            MockPackage(
                name: "library\(index)",
                targets: [
                    try MockTarget(
                        name: "binary\(index)",
                        type: .binary,
                        url: "https://somwhere.com/binary\(index).zip",
                        checksum: "01"
                    )
                ],
                products: [
                    MockProduct(name: "binary\(index)", targets: ["binary\(index)"]),
                ],
                versions: ["1.0.0"]
            )
        }

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "App",
                    targets: [
                        MockTarget(
                            name: "App",
                            dependencies: packages.map { package in
                                .product(name: package.targets.first!.name, package: package.name)
                            }
                        ),
                    ],
                    products: [],
                    dependencies: packages.map { package in
                        .sourceControl(path: "./\(package.name)", requirement: .exact("1.0.0"))
                    }
                ),
            ],
            packages: packages,
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        try workspace.checkPackageGraph(roots: ["App"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }

        workspace.checkManagedArtifacts { result in
            for package in packages {
                let targetName = package.targets.first!.name
                result.check(
                    packageIdentity: .plain(package.name),
                    targetName: targetName,
                    source: .remote(
                        url: "https://somwhere.com/\(targetName).zip",
                        checksum: "01"
                    ),
                    path: workspace.artifactsDir.appending(components: package.name, targetName, "\(targetName).xcframework")
                )
            }
        }
    }

    func testDownloadedArtifactStripFirstComponent() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let downloads = ThreadSafeKeyValueStore<URL, AbsolutePath>()

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                let contents: [UInt8]
                switch request.url.lastPathComponent {
                case "flat.zip":
                    contents = [0x01]
                case "nested.zip":
                    contents = [0x02]
                case "nested2.zip":
                    contents = [0x03]
                default:
                    throw StringError("unexpected url \(request.url)")
                }

                try fileSystem.writeFileContents(
                    destination,
                    bytes: ByteString(contents),
                    atomically: true
                )

                downloads[request.url] = destination
                completion(.success(.okay()))
            } catch {
                completion(.failure(error))
            }
        })

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                switch archivePath.basename {
                case "flat.zip":
                    try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: "flat")
                    archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                case "nested.zip":
                    let nestedPath = destinationPath.appending(component: "root")
                    try fs.createDirectory(nestedPath)
                    try createDummyXCFramework(fileSystem: fs, path: nestedPath, name: "nested")
                    archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                case "nested2.zip":
                    let nestedPath = destinationPath.appending(component: "root")
                    try fs.createDirectory(nestedPath)
                    try createDummyXCFramework(fileSystem: fs, path: nestedPath, name: "nested2")
                    try fs.writeFileContents(nestedPath.appending(component: ".DS_Store"), bytes: []) // add a file next to the directory

                    archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }

                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "flat",
                            type: .binary,
                            url: "https://a.com/flat.zip",
                            checksum: "01"
                        ),
                        MockTarget(
                            name: "nested",
                            type: .binary,
                            url: "https://a.com/nested.zip",
                            checksum: "02"
                        )
                        ,
                        MockTarget(
                            name: "nested2",
                            type: .binary,
                            url: "https://a.com/nested2.zip",
                            checksum: "03"
                        )
                    ]
                ),
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            XCTAssert(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/root")))
            XCTAssertEqual(downloads.map { $0.key.absoluteString }.sorted(), [
                "https://a.com/flat.zip",
                "https://a.com/nested.zip",
                "https://a.com/nested2.zip",
            ])
            XCTAssertEqual(workspace.checksumAlgorithm.hashes.map{ $0.hexadecimalRepresentation }.sorted(), [
                ByteString([0x01]).hexadecimalRepresentation,
                ByteString([0x02]).hexadecimalRepresentation,
                ByteString([0x03]).hexadecimalRepresentation,
            ])
            XCTAssertEqual(archiver.extractions.map { $0.destinationPath.parentDirectory }.sorted(), [
                AbsolutePath("/tmp/ws/.build/artifacts/extract/root/flat"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/root/nested"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/root/nested2"),
            ])
            XCTAssertEqual(
                downloads.map { $0.value }.sorted(),
                archiver.extractions.map { $0.archivePath }.sorted()
            )
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("root"),
                         targetName: "flat",
                         source: .remote(
                            url: "https://a.com/flat.zip",
                            checksum: "01"
                         ),
                         path: workspace.artifactsDir.appending(components: "root", "flat", "flat.xcframework")
            )
            result.check(packageIdentity: .plain("root"),
                         targetName: "nested",
                         source: .remote(
                            url: "https://a.com/nested.zip",
                            checksum: "02"
                         ),
                         path: workspace.artifactsDir.appending(components: "root", "nested", "nested.xcframework")
            )
            result.check(packageIdentity: .plain("root"),
                         targetName: "nested2",
                         source: .remote(
                            url: "https://a.com/nested2.zip",
                            checksum: "03"
                         ),
                         path: workspace.artifactsDir.appending(components: "root", "nested2", "nested2.xcframework")
            )
        }
    }

    func testArtifactMultipleExtensions() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let downloads = ThreadSafeKeyValueStore<URL, AbsolutePath>()

        // returns a dummy zipfile for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                guard case .download(let fileSystem, let destination) = request.kind else {
                    throw StringError("invalid request \(request.kind)")
                }

                let contents: [UInt8]
                switch request.url.lastPathComponent {
                case "a1.xcframework.zip":
                    contents = [0xA1]
                case "a2.zip.zip":
                    contents = [0xA2]
                default:
                    throw StringError("unexpected url \(request.url)")
                }

                try fileSystem.writeFileContents(
                    destination,
                    bytes: ByteString(contents),
                    atomically: true
                )

                downloads[request.url] = destination
                completion(.success(.okay()))
            } catch {
                completion(.failure(error))
            }
        })

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                let name: String
                switch archivePath.basename {
                case "a1.xcframework.zip":
                    name = "A1"
                case "a2.zip.zip":
                    name = "A2"
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                try createDummyXCFramework(fileSystem: fs, path: destinationPath, name: name)
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/a1.xcframework.zip",
                            checksum: "a1"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            url: "https://a.com/a2.zip.zip",
                            checksum: "a2"
                        )
                    ],
                    products: []
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            XCTAssertEqual(downloads.map { $0.key.absoluteString }.sorted(), [
                "https://a.com/a1.xcframework.zip",
                "https://a.com/a2.zip.zip",
            ])
            XCTAssertEqual(workspace.checksumAlgorithm.hashes.map{ $0.hexadecimalRepresentation }.sorted(), [
                ByteString([0xA1]).hexadecimalRepresentation,
                ByteString([0xA2]).hexadecimalRepresentation,
            ])
            XCTAssertEqual(archiver.extractions.map { $0.destinationPath.parentDirectory }.sorted(), [
                AbsolutePath("/tmp/ws/.build/artifacts/extract/root/A1"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/root/A2"),
            ])
            XCTAssertEqual(
                downloads.map { $0.value }.sorted(),
                archiver.extractions.map { $0.archivePath }.sorted()
            )
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("root"),
                         targetName: "A1",
                         source: .remote(
                            url: "https://a.com/a1.xcframework.zip",
                            checksum: "a1"
                         ),
                         path: workspace.artifactsDir.appending(components: "root", "A1", "A1.xcframework")
            )
            result.check(packageIdentity: .plain("root"),
                         targetName: "A2",
                         source: .remote(
                            url: "https://a.com/a2.zip.zip",
                            checksum: "a2"
                         ),
                         path: workspace.artifactsDir.appending(components: "root", "A2", "A2.xcframework")
            )
        }
    }

    func testLoadRootPackageWithBinaryDependencies() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()


        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/a1.zip",
                            checksum: "a1"
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            url: "https://a.com/a2.xcframework.zip",
                            checksum: "a2"
                        ),
                        MockTarget(
                            name: "A3",
                            type: .binary,
                            url: "https://a.com/a2.zip.zip",
                            checksum: "a3"
                        )
                    ],
                    products: []
                )
            ]
        )

        let observability = ObservabilitySystem.makeForTesting()
        let wks = try workspace.getOrCreateWorkspace()
        XCTAssertNoThrow(try tsc_await {
            wks.loadRootPackage(
                at: workspace.rootsDir.appending(component: "Root"),
                observabilityScope: observability.topScope,
                completion: $0
            )
        })
        XCTAssertNoDiagnostics(observability.diagnostics)
    }

    func testDownloadArchiveIndexFilesHappyPath() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let downloads = ThreadSafeKeyValueStore<URL, AbsolutePath>()
        let hostToolchain = try UserToolchain(destination: .hostDestination())

        let ariFiles = [
            """
            {
                "schemaVersion": "1.0",
                "archives": [
                    {
                        "fileName": "a1.zip",
                        "checksum": "a1",
                        "supportedTriples": ["\(hostToolchain.triple.tripleString)"]
                    }
                ]
            }
            """,
            """
            {
                "schemaVersion": "1.0",
                "archives": [
                    {
                        "fileName": "a2/a2.zip",
                        "checksum": "a2",
                        "supportedTriples": ["\(hostToolchain.triple.tripleString)"]
                    }
                ]
            }
            """
        ]

        let checksumAlgorithm = MockHashAlgorithm() // used in tests
        let ariFilesChecksums = ariFiles.map { checksumAlgorithm.hash($0).hexadecimalRepresentation }

        // returns a dummy file for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            switch request.kind  {
            case .generic:
                do {
                    let contents: String
                    switch request.url.lastPathComponent {
                    case "a1.artifactbundleindex":
                        contents = ariFiles[0]
                    case "a2.artifactbundleindex":
                        contents = ariFiles[1]
                    default:
                        throw StringError("unexpected url \(request.url)")
                    }
                    completion(.success(.okay(body: contents)))
                } catch {
                    completion(.failure(error))
                }
            case .download(let fileSystem, let destination):
                do {
                    let contents: [UInt8]
                    switch request.url.lastPathComponent {
                    case "a1.zip":
                        contents = [0xA1]
                    case "a2.zip":
                        contents = [0xA2]
                    case "b.zip":
                        contents = [0xB0]
                    default:
                        throw StringError("unexpected url \(request.url)")
                    }

                    try fileSystem.writeFileContents(
                        destination,
                        bytes: ByteString(contents),
                        atomically: true
                    )

                    downloads[request.url] = destination
                    completion(.success(.okay()))
                } catch {
                    completion(.failure(error))
                }
            }
        })

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                let name: String
                switch archivePath.basename {
                case "a1.zip":
                    name = "A1"
                case "a2.zip":
                    name = "A2"
                case "b.zip":
                    name = "B"
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                try createDummyArtifactBundle(fileSystem: fs, path: destinationPath, name: name)
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "Foo", dependencies: [
                            .product(name: "A1", package: "A"),
                            .product(name: "A2", package: "A"),
                            "B"
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "./A", requirement: .exact("1.0.0")),
                        .sourceControl(path: "./B", requirement: .exact("1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "A",
                    targets: [
                        MockTarget(
                            name: "A1",
                            type: .binary,
                            url: "https://a.com/a1.artifactbundleindex",
                            checksum: ariFilesChecksums[0]
                        ),
                        MockTarget(
                            name: "A2",
                            type: .binary,
                            url: "https://a.com/a2.artifactbundleindex",
                            checksum: ariFilesChecksums[1]
                        )
                    ],
                    products: [
                        MockProduct(name: "A1", targets: ["A1"]),
                        MockProduct(name: "A2", targets: ["A2"])
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "B",
                    targets: [
                        MockTarget(
                            name: "B",
                            type: .binary,
                            url: "https://b.com/b.zip",
                            checksum: "b0"
                        ),
                    ],
                    products: [
                        MockProduct(name: "B", targets: ["B"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            ),
            checksumAlgorithm: checksumAlgorithm
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            XCTAssert(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/a")))
            XCTAssert(fs.isDirectory(AbsolutePath("/tmp/ws/.build/artifacts/b")))
            XCTAssertEqual(downloads.map { $0.key.absoluteString }.sorted(), [
                "https://a.com/a1.zip",
                "https://a.com/a2/a2.zip",
                "https://b.com/b.zip",
            ])
            XCTAssertEqual(checksumAlgorithm.hashes.map{ $0.hexadecimalRepresentation }.sorted(),
                (
                    ariFiles.map(ByteString.init(encodingAsUTF8:)) +
                    ariFiles.map(ByteString.init(encodingAsUTF8:)) +
                    [
                        ByteString([0xA1]),
                        ByteString([0xA2]),
                        ByteString([0xB0]),
                    ]
                ).map{ $0.hexadecimalRepresentation }.sorted()
            )
            XCTAssertEqual(archiver.extractions.map { $0.destinationPath.parentDirectory }.sorted(), [
                AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A1"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/a/A2"),
                AbsolutePath("/tmp/ws/.build/artifacts/extract/b/B"),
            ])
            XCTAssertEqual(
                downloads.map { $0.value }.sorted(),
                archiver.extractions.map { $0.archivePath }.sorted()
            )
        }

        workspace.checkManagedArtifacts { result in
            result.check(packageIdentity: .plain("a"),
                         targetName: "A1",
                         source: .remote(
                            url: "https://a.com/a1.zip",
                            checksum: "a1"
                         ),
                         path: workspace.artifactsDir.appending(components: "a", "A1", "A1.artifactbundle")
            )
            result.check(packageIdentity: .plain("a"),
                         targetName: "A2",
                         source: .remote(
                            url: "https://a.com/a2/a2.zip",
                            checksum: "a2"
                         ),
                         path: workspace.artifactsDir.appending(components: "a", "A2", "A2.artifactbundle")
            )
            result.check(packageIdentity: .plain("b"),
                         targetName: "B",
                         source: .remote(
                            url: "https://b.com/b.zip",
                            checksum: "b0"
                         ),
                         path: workspace.artifactsDir.appending(components: "b", "B", "B.artifactbundle")
            )
        }

        XCTAssertMatch(workspace.delegate.events, ["downloading binary artifact package: https://a.com/a1.zip"])
        XCTAssertMatch(workspace.delegate.events, ["downloading binary artifact package: https://a.com/a2/a2.zip"])
        XCTAssertMatch(workspace.delegate.events, ["downloading binary artifact package: https://b.com/b.zip"])
        XCTAssertMatch(workspace.delegate.events, ["finished downloading binary artifact package: https://a.com/a1.zip"])
        XCTAssertMatch(workspace.delegate.events, ["finished downloading binary artifact package: https://a.com/a2/a2.zip"])
        XCTAssertMatch(workspace.delegate.events, ["finished downloading binary artifact package: https://b.com/b.zip"])
    }

    func testDownloadArchiveIndexServerError() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        // returns a dummy files for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            completion(.success(.serverError()))
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "A", type: .binary, url: "https://a.com/a.artifactbundleindex", checksum: "does-not-matter"),
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient
            )
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("failed retrieving 'https://a.com/a.artifactbundleindex': badResponseStatusCode(500)"), severity: .error)
            }
        }
    }

    func testDownloadArchiveIndexFileBadChecksum() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let hostToolchain = try UserToolchain(destination: .hostDestination())

        let ari = """
        {
            "schemaVersion": "1.0",
            "archives": [
                {
                    "fileName": "a1.zip",
                    "checksum": "a1",
                    "supportedTriples": ["\(hostToolchain.triple.tripleString)"]
                }
            ]
        }
        """
        let checksumAlgorithm = MockHashAlgorithm() // used in tests
        let ariChecksums = checksumAlgorithm.hash(ari).hexadecimalRepresentation

        // returns a dummy files for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                let contents: String
                switch request.url.lastPathComponent {
                case "a.artifactbundleindex":
                    contents = ari
                default:
                    throw StringError("unexpected url \(request.url)")
                }
                completion(.success(.okay(body: contents)))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "A", type: .binary, url: "https://a.com/a.artifactbundleindex", checksum: "incorrect"),
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient
            )
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("failed retrieving 'https://a.com/a.artifactbundleindex': checksum of downloaded artifact of binary target 'A' (\(ariChecksums)) does not match checksum specified by the manifest (incorrect)"), severity: .error)
            }
        }
    }

    func testDownloadArchiveIndexFileChecksumChanges() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "A", type: .binary, url: "https://a.com/a.artifactbundleindex", checksum: "new-checksum"),
                    ]
                )
            ]
        )

        let rootPath = workspace.pathToRoot(withName: "Root")
        let rootRef = PackageReference.root(identity: PackageIdentity(path: rootPath), path: rootPath)

        try workspace.set(
            managedArtifacts: [
                .init(
                    packageRef: rootRef,
                    targetName: "A",
                    source: .remote(
                        url: "https://a.com/a.artifactbundleindex",
                        checksum: "old-checksum"
                    ),
                    path: workspace.artifactsDir.appending(components: "root", "A.xcframework"),
                    kind: .xcframework
                ),
            ]
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("artifact of binary target 'A' has changed checksum"), severity: .error)
            }
        }
    }

    func testDownloadArchiveIndexFileBadArchivesChecksum() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let hostToolchain = try UserToolchain(destination: .hostDestination())

        let ari = """
        {
            "schemaVersion": "1.0",
            "archives": [
                {
                    "fileName": "a.zip",
                    "checksum": "a",
                    "supportedTriples": ["\(hostToolchain.triple.tripleString)"]
                }
            ]
        }
        """
        let checksumAlgorithm = MockHashAlgorithm() // used in tests
        let ariChecksums = checksumAlgorithm.hash(ari).hexadecimalRepresentation

        // returns a dummy files for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                switch request.kind  {
                case .generic:
                    let contents: String
                    switch request.url.lastPathComponent {
                    case "a.artifactbundleindex":
                        contents = ari
                    default:
                        throw StringError("unexpected url \(request.url)")
                    }

                    completion(.success(.okay(body: contents)))

                case .download(let fileSystem, let destination):
                    let contents: [UInt8]
                    switch request.url.lastPathComponent {
                    case "a.zip":
                        contents = [0x42]
                    default:
                        throw StringError("unexpected url \(request.url)")
                    }

                    try fileSystem.writeFileContents(
                        destination,
                        bytes: ByteString(contents),
                        atomically: true
                    )

                    completion(.success(.okay()))
                }
            } catch {
                completion(.failure(error))
            }
        })

        // create a dummy xcframework directory from the request archive
        let archiver = MockArchiver(handler: { archiver, archivePath, destinationPath, completion in
            do {
                let name: String
                switch archivePath.basename {
                case "a.zip":
                    name = "A.artifactbundle"
                default:
                    throw StringError("unexpected archivePath \(archivePath)")
                }
                try fs.createDirectory(destinationPath.appending(component: name), recursive: false)
                archiver.extractions.append(MockArchiver.Extraction(archivePath: archivePath, destinationPath: destinationPath))
                completion(.success(()))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "A", type: .binary, url: "https://a.com/a.artifactbundleindex", checksum: ariChecksums),
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient,
                archiver: archiver
            )
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("checksum of downloaded artifact of binary target 'A' (42) does not match checksum specified by the manifest (a)"), severity: .error)
            }
        }
    }

    func testDownloadArchiveIndexFileArchiveNotFound() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()
        let hostToolchain = try UserToolchain(destination: .hostDestination())

        let ari = """
        {
            "schemaVersion": "1.0",
            "archives": [
                {
                    "fileName": "not-found.zip",
                    "checksum": "a",
                    "supportedTriples": ["\(hostToolchain.triple.tripleString)"]
                }
            ]
        }
        """
        let checksumAlgorithm = MockHashAlgorithm() // used in tests
        let ariChecksums = checksumAlgorithm.hash(ari).hexadecimalRepresentation

        // returns a dummy files for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                switch request.kind  {
                case .generic:
                        let contents: String
                        switch request.url.lastPathComponent {
                        case "a.artifactbundleindex":
                            contents = ari
                        default:
                            throw StringError("unexpected url \(request.url)")
                        }
                        completion(.success(.okay(body: contents)))

                case .download:
                    completion(.success(.notFound()))
                }
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "A", type: .binary, url: "https://a.com/a.artifactbundleindex", checksum: ariChecksums),
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient
            )
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("failed downloading 'https://a.com/not-found.zip' which is required by binary target 'A': badResponseStatusCode(404)"), severity: .error)
            }
        }
    }

    func testDownloadArchiveIndexTripleNotFound() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let hostToolchain = try UserToolchain(destination: .hostDestination())
        let androidTriple = try Triple("x86_64-unknown-linux-android")
        let notHostTriple = hostToolchain.triple == androidTriple ? .macOS : androidTriple

        let ari = """
        {
            "schemaVersion": "1.0",
            "archives": [
                {
                    "fileName": "a1.zip",
                    "checksum": "a1",
                    "supportedTriples": ["\(notHostTriple.tripleString)"]
                }
            ]
        }
        """
        let checksumAlgorithm = MockHashAlgorithm() // used in tests
        let ariChecksum = checksumAlgorithm.hash(ari).hexadecimalRepresentation

        // returns a dummy files for the requested artifact
        let httpClient = HTTPClient(handler: { request, _, completion in
            do {
                let contents: String
                switch request.url.lastPathComponent {
                case "a.artifactbundleindex":
                    contents = ari
                default:
                    throw StringError("unexpected url \(request.url)")
                }
                completion(.success(.okay(body: contents)))
            } catch {
                completion(.failure(error))
            }
        })

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "A", type: .binary, url: "https://a.com/a.artifactbundleindex", checksum: ariChecksum),
                    ]
                )
            ],
            binaryArtifactsManager: .init(
                httpClient: httpClient
            )
        )

        workspace.checkPackageGraphFailure(roots: ["Root"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .contains("failed retrieving 'https://a.com/a.artifactbundleindex': No supported archive was found for '\(hostToolchain.triple.tripleString)'"), severity: .error)
            }
        }
    }

    func testDuplicateDependencyIdentityWithNameAtRoot() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "FooUtilityPackage"),
                            .product(name: "BarProduct", package: "BarUtilityPackage")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControlWithDeprecatedName(name: "FooUtilityPackage", path: "foo/utility", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControlWithDeprecatedName(name: "BarUtilityPackage", path: "bar/utility", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooUtilityPackage",
                    path: "foo/utility",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                // this package never gets loaded since the dependency declaration identity is the same as "FooPackage"
                MockPackage(
                    name: "BarUtilityPackage",
                    path: "bar/utility",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(
                    diagnostic: "'root' dependency on '\(sandbox.appending(components: "pkgs", "bar", "utility"))' conflicts with dependency on '\(sandbox.appending(components: "pkgs", "foo", "utility"))' which has the same identity 'utility'",
                    severity: .error
                )
            }
        }
    }

    func testDuplicateDependencyIdentityWithoutNameAtRoot() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "FooUtilityPackage"),
                            .product(name: "BarProduct", package: "BarUtilityPackage")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "foo/utility", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar/utility", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooUtilityPackage",
                    path: "foo/utility",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                // this package never gets loaded since the dependency declaration identity is the same as "FooPackage"
                MockPackage(
                    name: "BarUtilityPackage",
                    path: "bar/utility",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(
                    diagnostic: "'root' dependency on '\(sandbox.appending(components: "pkgs", "bar", "utility"))' conflicts with dependency on '\(sandbox.appending(components: "pkgs", "foo", "utility"))' which has the same identity 'utility'",
                    severity: .error
                )
            }
        }
    }

    func testDuplicateExplicitDependencyName_AtRoot() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "FooPackage"),
                            .product(name: "BarProduct", package: "BarPackage")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControlWithDeprecatedName(name: "FooPackage", path: "foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControlWithDeprecatedName(name: "FooPackage", path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    path: "foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                // this package never gets loaded since the dependency declaration name is the same as "FooPackage"
                MockPackage(
                    name: "BarPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(
                    diagnostic: "'root' dependency on '\(sandbox.appending(components: "pkgs", "bar"))' conflicts with dependency on '\(sandbox.appending(components: "pkgs", "foo"))' which has the same explicit name 'FooPackage'",
                    severity: .error
                )
            }
        }
    }

    func testDuplicateManifestNameAtRoot() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            "FooProduct",
                            "BarProduct"
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "MyPackage",
                    path: "foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                MockPackage(
                    name: "MyPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testDuplicateManifestName_ExplicitProductPackage_AtRoot() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "bar"),
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "MyPackage",
                    path: "foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                MockPackage(
                    name: "MyPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testManifestNameAndIdentityConflict_AtRoot_Pre52() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            "FooProduct",
                            "BarProduct"
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "foo",
                    path: "foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                MockPackage(
                    name: "foo",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testManifestNameAndIdentityConflict_AtRoot_Post52_Incorrect() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            "FooProduct",
                            "BarProduct"
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_3
                ),
            ],
            packages: [
                MockPackage(
                    name: "foo",
                    path: "foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                MockPackage(
                    name: "foo",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(
                    diagnostic: "dependency 'FooProduct' in target 'RootTarget' requires explicit declaration; reference the package in the target dependency with '.product(name: \"FooProduct\", package: \"foo\")'",
                    severity: .error
                )
                result.check(
                    diagnostic: "dependency 'BarProduct' in target 'RootTarget' requires explicit declaration; reference the package in the target dependency with '.product(name: \"BarProduct\", package: \"bar\")'",
                    severity: .error
                )
            }
        }
    }

    func testManifestNameAndIdentityConflict_AtRoot_Post52_Correct() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "bar"),
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_3
                ),
            ],
            packages: [
                MockPackage(
                    name: "foo",
                    path: "foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                MockPackage(
                    name: "foo",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testManifestNameAndIdentityConflict_ExplicitDependencyNames_AtRoot() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            "FooProduct",
                            "BarProduct"
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControlWithDeprecatedName(name: "foo", path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "foo",
                    path: "foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                MockPackage(
                    name: "foo",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(
                    diagnostic: "'root' dependency on '\(sandbox.appending(components: "pkgs", "bar"))' conflicts with dependency on '\(sandbox.appending(components: "pkgs", "foo"))' which has the same explicit name 'foo'",
                    severity: .error
                )
            }
        }
    }

    func testManifestNameAndIdentityConflict_ExplicitDependencyNames_ExplicitProductPackage_AtRoot() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "foo"),
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControlWithDeprecatedName(name: "foo", path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_3
                ),
            ],
            packages: [
                MockPackage(
                    name: "foo",
                    path: "foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                MockPackage(
                    name: "foo",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(
                    diagnostic: "'root' dependency on '\(sandbox.appending(components: "pkgs", "bar"))' conflicts with dependency on '\(sandbox.appending(components: "pkgs", "foo"))' which has the same explicit name 'foo'",
                    severity: .error
                )
            }
        }
    }

    func testDuplicateTransitiveIdentityWithNames() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooUtilityProduct", package: "FooUtilityPackage"),
                            .product(name: "BarProduct", package: "BarPackage")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControlWithDeprecatedName(name: "FooUtilityPackage", path: "foo/utility", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControlWithDeprecatedName(name: "BarPackage", path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooUtilityPackage",
                    path: "foo/utility",
                    targets: [
                        MockTarget(name: "FooUtilityTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooUtilityProduct", targets: ["FooUtilityTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "OtherUtilityProduct", package: "OtherUtilityPackage"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControlWithDeprecatedName(name: "OtherUtilityPackage", path: "other/utility", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                // this package never gets loaded since its identity is the same as "FooPackage"
                MockPackage(
                    name: "OtherUtilityPackage",
                    path: "other/utility",
                    targets: [
                        MockTarget(name: "OtherUtilityTarget"),
                    ],
                    products: [
                        MockProduct(name: "OtherUtilityProduct", targets: ["OtherUtilityTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(
                    diagnostic: "'bar' dependency on '\(sandbox.appending(components: "pkgs", "other", "utility"))' conflicts with dependency on '\(sandbox.appending(components: "pkgs", "foo", "utility"))' which has the same identity 'utility'",
                    severity: .error
                )
            }
        }
    }

    func testDuplicateTransitiveIdentityWithoutNames() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooUtilityProduct", package: "utility"),
                            .product(name: "BarProduct", package: "bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "foo/utility", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooUtilityPackage",
                    path: "foo/utility",
                    targets: [
                        MockTarget(name: "FooUtilityTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooUtilityProduct", targets: ["FooUtilityTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "OtherUtilityProduct", package: "utility"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "other-foo/utility", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                // this package never gets loaded since its identity is the same as "FooPackage"
                MockPackage(
                    name: "OtherUtilityPackage",
                    path: "other-foo/utility",
                    targets: [
                        MockTarget(name: "OtherUtilityTarget"),
                    ],
                    products: [
                        MockProduct(name: "OtherUtilityProduct", targets: ["OtherUtilityTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        // 9/2021 this is currently emitting a warning only to support backwards compatibility
        // we will escalate this to an error in a few versions to tighten up the validation
        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(
                    diagnostic: "'bar' dependency on '\(sandbox.appending(components: "pkgs", "other-foo", "utility"))' conflicts with dependency on '\(sandbox.appending(components: "pkgs", "foo", "utility"))' which has the same identity 'utility'. this will be escalated to an error in future versions of SwiftPM.",
                    severity: .warning
                )
                // FIXME: rdar://72940946
                // we need to improve this situation or diagnostics when working on identity
                result.check(
                    diagnostic: "product 'OtherUtilityProduct' required by package 'bar' target 'BarTarget' not found in package 'utility'.",
                    severity: .error
                )
            }
        }
    }

    func testDuplicateTransitiveIdentitySimilarURLs1() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://github.com/foo/foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://github.com/foo/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "https://github.com/foo/foo.git", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                // this package never gets loaded since its identity is the same as "FooPackage"
                MockPackage(
                    name: "FooPackage",
                    url: "https://github.com/foo/foo.git",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testDuplicateTransitiveIdentitySimilarURLs2() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://github.com/foo/foo.git", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://github.com/foo/foo.git",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "http://github.com/foo/foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                // this package never gets loaded since its identity is the same as "FooPackage"
                MockPackage(
                    name: "FooPackage",
                    url: "http://github.com/foo/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testDuplicateTransitiveIdentityGitHubURLs1() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://github.com/foo/foo.git", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://github.com/foo/foo.git",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "git@github.com:foo/foo.git", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                // this package never gets loaded since its identity is the same as "FooPackage"
                MockPackage(
                    name: "FooPackage",
                    url: "git@github.com:foo/foo.git",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testDuplicateTransitiveIdentityGitHubURLs2() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://github.enterprise.com/foo/foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://github.enterprise.com/foo/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "git@github.enterprise.com:foo/foo.git", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                // this package never gets loaded since its identity is the same as "FooPackage"
                MockPackage(
                    name: "FooPackage",
                    url: "git@github.enterprise.com:foo/foo.git",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
        }
    }

    func testDuplicateTransitiveIdentityUnfamiliarURLs() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://github.com/foo/foo.git", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://github.com/foo/foo.git",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "https://github.com/foo-moved/foo.git", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                // this package never gets loaded since its identity is the same as "FooPackage"
                MockPackage(
                    name: "FooPackage",
                    url: "https://github.com/foo-moved/foo.git",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        // 9/2021 this is currently emitting a warning only to support backwards compatibility
        // we will escalate this to an error in a few versions to tighten up the validation
        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(
                    diagnostic: "'bar' dependency on 'https://github.com/foo-moved/foo.git' conflicts with dependency on 'https://github.com/foo/foo.git' which has the same identity 'foo'. this will be escalated to an error in future versions of SwiftPM.",
                    severity: .warning
                )
            }
        }
    }

    func testDuplicateTransitiveIdentityWithSimilarURLs() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "bar"),
                            .product(name: "BazProduct", package: "baz")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://github.com/org/foo.git", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(url: "https://github.com/org/bar.git", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(url: "https://github.com/org/baz.git", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://github.com/org/foo.git",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    url: "https://github.com/org/bar.git",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "Foo"),
                            .product(name: "BazProduct", package: "baz"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "https://github.com/ORG/Foo.git", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(url: "https://github.com/org/baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BazPackage",
                    url: "https://github.com/org/baz.git",
                    targets: [
                        MockTarget(name: "BazTarget"),
                    ],
                    products: [
                        MockProduct(name: "BazProduct", targets: ["BazTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                // URL with different casing
                MockPackage(
                    name: "FooPackage",
                    url: "https://github.com/ORG/Foo.git",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                // URL with no .git extension
                MockPackage(
                    name: "BazPackage",
                    url: "https://github.com/org/baz",
                    targets: [
                        MockTarget(name: "BazTarget"),
                    ],
                    products: [
                        MockProduct(name: "BazProduct", targets: ["BazTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        // 9/2021 this is currently emitting a warning only to support backwards compatibility
        // we will escalate this to an error in a few versions to tighten up the validation
        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics, minSeverity: .info) { result in
                result.checkUnordered(
                    diagnostic: "dependency on 'foo' is represented by similar locations ('https://github.com/org/foo.git' and 'https://github.com/ORG/Foo.git') which are treated as the same canonical location 'github.com/org/foo'.",
                    severity: .info
                )
                result.checkUnordered(
                    diagnostic: "dependency on 'baz' is represented by similar locations ('https://github.com/org/baz.git' and 'https://github.com/org/baz') which are treated as the same canonical location 'github.com/org/baz'.",
                    severity: .info
                )
            }
        }
    }

    func testDuplicateNestedTransitiveIdentityWithNames() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooUtilityProduct", package: "FooUtilityPackage")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControlWithDeprecatedName(name: "FooUtilityPackage", path: "foo/utility", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooUtilityPackage",
                    path: "foo/utility",
                    targets: [
                        MockTarget(name: "FooUtilityTarget", dependencies: [
                            .product(name: "BarProduct", package: "BarPackage")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "FooUtilityProduct", targets: ["FooUtilityTarget"]),
                    ],
                    dependencies: [
                        .sourceControlWithDeprecatedName(name: "BarPackage", path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "OtherUtilityProduct", package: "OtherUtilityPackage"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControlWithDeprecatedName(name: "OtherUtilityPackage", path: "other/utility", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                // this package never gets loaded since its identity is the same as "FooPackage"
                MockPackage(
                    name: "OtherUtilityPackage",
                    path: "other/utility",
                    targets: [
                        MockTarget(name: "OtherUtilityTarget"),
                    ],
                    products: [
                        MockProduct(name: "OtherUtilityProduct", targets: ["OtherUtilityTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                // FIXME: rdar://72940946
                // we need to improve this situation or diagnostics when working on identity
                result.check(
                    diagnostic: "cyclic dependency declaration found: Root -> FooUtilityPackage -> BarPackage -> FooUtilityPackage",
                    severity: .error
                )
            }
        }
    }

    func testDuplicateNestedTransitiveIdentityWithoutNames() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooUtilityProduct", package: "FooUtilityPackage")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(path: "foo/utility", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooUtilityPackage",
                    path: "foo/utility",
                    targets: [
                        MockTarget(name: "FooUtilityTarget", dependencies: [
                            .product(name: "BarProduct", package: "BarPackage")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "FooUtilityProduct", targets: ["FooUtilityTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"],
                    toolsVersion: .v5
                ),
                MockPackage(
                    name: "BarPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "OtherUtilityProduct", package: "OtherUtilityPackage"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(path: "other/utility", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"],
                    toolsVersion: .v5
                ),
                // this package never gets loaded since its identity is the same as "FooPackage"
                MockPackage(
                    name: "OtherUtilityPackage",
                    path: "other/utility",
                    targets: [
                        MockTarget(name: "OtherUtilityTarget"),
                    ],
                    products: [
                        MockProduct(name: "OtherUtilityProduct", targets: ["OtherUtilityTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                // FIXME: rdar://72940946
                // we need to improve this situation or diagnostics when working on identity
                result.check(
                    diagnostic: "cyclic dependency declaration found: Root -> FooUtilityPackage -> BarPackage -> FooUtilityPackage",
                    severity: .error
                )
            }
        }
    }

    func testRootPathConflictsWithTransitiveIdentity() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    path: "foo",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "BarProduct", package: "BarPackage")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControlWithDeprecatedName(name: "BarPackage", path: "bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5
                ),
            ],
            packages: [
                MockPackage(
                    name: "BarPackage",
                    path: "bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "FooPackage"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControlWithDeprecatedName(name: "FooPackage", path: "foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                // this package never gets loaded since its identity is the same as "FooPackage"
                MockPackage(
                    name: "FooPackage",
                    path: "foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["foo"]) { graph, diagnostics in
            testDiagnostics(diagnostics) { result in
                // FIXME: rdar://72940946
                // we need to improve this situation or diagnostics when working on identity
                result.check(
                    diagnostic: "cyclic dependency declaration found: Root -> BarPackage -> Root",
                    severity: .error
                )
            }
        }
    }

    func testResolutionBranchAndVersion() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "http://localhost/org/foo", requirement: .branch("experiment")),
                        .sourceControl(url: "http://localhost/org/bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "http://localhost/org/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "experiment"]
                ),
                MockPackage(
                    name: "BarPackage",
                    url: "http://localhost/org/bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "http://localhost/org/foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                )
            ]
        )

        try workspace.checkPackageGraph(roots: ["Root"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            PackageGraphTester(graph) { result in
                result.check(roots: "Root")
                result.check(packages: "BarPackage", "FooPackage", "Root")
                result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
            }
        }

        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.branch("experiment")))
            result.check(dependency: "bar", at: .checkout(.version("1.0.0")))
        }
    }

    func testBinaryArtifactsInvalidPath() throws {
        try testWithTemporaryDirectory { path in
            let fs = localFileSystem
            let observability = ObservabilitySystem.makeForTesting()

            let foo = path.appending(component: "foo")

            try fs.writeFileContents(foo.appending(component: "Package.swift")) {
                $0 <<<
                    """
                    // swift-tools-version:5.3
                    import PackageDescription
                    let package = Package(
                        name: "Best",
                        targets: [
                            .binaryTarget(name: "best", path: "/best.xcframework")
                        ]
                    )
                    """
            }

            let manifestLoader = ManifestLoader(toolchain: UserToolchain.default)
            let sandbox = path.appending(component: "ws")
            let workspace = try Workspace(
                fileSystem: fs,
                forRootPackage: sandbox,
                customManifestLoader: manifestLoader,
                delegate: MockWorkspaceDelegate()
            )

            try workspace.resolve(root: .init(packages: [foo]), observabilityScope: observability.topScope)
            testDiagnostics(observability.diagnostics) { result in
                result.check(
                    diagnostic: "invalid local path '/best.xcframework' for binary target 'best', path expected to be relative to package root.",
                    severity: .error
                )
            }
        }
    }

    func testManifestLoaderDiagnostics() throws {
        struct TestLoader: ManifestLoaderProtocol {
            let error: Error?

            init (error: Error?) {
                self.error = error
            }

            func load(
                manifestPath: AbsolutePath,
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
                completion: @escaping (Result<Manifest, Error>) -> Void
            ) {
                if let error = self.error {
                    callbackQueue.async {
                        completion(.failure(error))
                    }
                } else {
                    callbackQueue.async {
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
                }
            }

            func resetCache() throws {}
            func purgeCache() throws {}
        }

        let fs = InMemoryFileSystem()
        let observability = ObservabilitySystem.makeForTesting()

        // write a manifest
        try fs.writeFileContents(.root.appending(component: Manifest.filename), bytes: "")
        try ToolsVersionSpecificationWriter.rewriteSpecification(manifestDirectory: .root, toolsVersion: .current, fileSystem: fs)

        do {
            // no error
            let delegate = MockWorkspaceDelegate()
            let workspace = try Workspace(
                fileSystem: fs,
                forRootPackage: .root,
                customManifestLoader: TestLoader(error: .none),
                delegate: delegate
            )
            try workspace.loadPackageGraph(rootPath: .root, observabilityScope: observability.topScope)

            XCTAssertNotNil(delegate.manifest)
            XCTAssertNoDiagnostics(observability.diagnostics)
            XCTAssertNoDiagnostics(delegate.manifestLoadingDiagnostics ?? [])
        }

        do {
            // actual error
            let delegate = MockWorkspaceDelegate()
            let workspace = try Workspace(
                fileSystem: fs,
                forRootPackage: .root,
                customManifestLoader: TestLoader(error: StringError("boom")),
                delegate: delegate
            )
            try workspace.loadPackageGraph(rootPath: .root, observabilityScope: observability.topScope)

            XCTAssertNil(delegate.manifest)
            testDiagnostics(delegate.manifestLoadingDiagnostics ?? []) { result in
                result.check(diagnostic: .equal("boom"), severity: .error)
            }
            testDiagnostics(delegate.manifestLoadingDiagnostics ?? []) { result in
                result.check(diagnostic: .equal("boom"), severity: .error)
            }
        }
    }

    func testBasicResolutionFromSourceControl() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "MyPackage",
                    targets: [
                        MockTarget(
                            name: "MyTarget1",
                            dependencies: [
                                .product(name: "Foo", package: "foo")
                            ]),
                        MockTarget(
                            name: "MyTarget2",
                            dependencies: [
                                .product(name: "Bar", package: "bar")
                            ]),
                    ],
                    products: [
                        MockProduct(name: "MyProduct", targets: ["MyTarget1", "MyTarget2"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "http://localhost/org/foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(url: "http://localhost/org/bar", requirement: .upToNextMajor(from: "2.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    url: "http://localhost/org/foo",
                    targets: [
                        MockTarget(name: "Foo")
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "1.2.0", "1.3.0", "1.4.0", "1.5.0", "1.5.1"]
                ),
                MockPackage(
                    name: "Bar",
                    url: "http://localhost/org/bar",
                    targets: [
                        MockTarget(name: "Bar")
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["2.0.0", "2.1.0", "2.2.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["MyPackage"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            PackageGraphTester(graph) { result in
                result.check(roots: "MyPackage")
                result.check(packages: "Bar", "Foo", "MyPackage")
                result.check(targets: "Foo", "Bar", "MyTarget1", "MyTarget2")
                result.checkTarget("MyTarget1") { result in result.check(dependencies: "Foo") }
                result.checkTarget("MyTarget2") { result in result.check(dependencies: "Bar") }
            }
        }

        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.5.1")))
            result.check(dependency: "bar", at: .checkout(.version("2.2.0")))
        }

        // Check the load-package callbacks.
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for root package: \(sandbox.appending(components: "roots", "MyPackage"))"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for root package: \(sandbox.appending(components: "roots", "MyPackage"))"])
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for remoteSourceControl package: http://localhost/org/foo"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for remoteSourceControl package: http://localhost/org/foo"])
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for remoteSourceControl package: http://localhost/org/bar"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for remoteSourceControl package: http://localhost/org/bar"])
    }

    func testBasicTransitiveResolutionFromSourceControl() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "MyPackage",
                    targets: [
                        MockTarget(
                            name: "MyTarget1",
                            dependencies: [
                                .product(name: "Foo", package: "foo")
                            ]),
                        MockTarget(
                            name: "MyTarget2",
                            dependencies: [
                                .product(name: "Bar", package: "bar")
                            ]),
                    ],
                    products: [
                        MockProduct(name: "MyProduct", targets: ["MyTarget1", "MyTarget2"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "http://localhost/org/foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(url: "http://localhost/org/bar", requirement: .upToNextMajor(from: "2.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    url: "http://localhost/org/foo",
                    targets: [
                        MockTarget(
                            name: "Foo",
                            dependencies: [
                                .product(name: "Baz", package: "baz")
                            ]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "http://localhost/org/baz", requirement: .range("2.0.0" ..< "4.0.0")),
                    ],
                    versions: ["1.0.0", "1.1.0"]
                ),
                MockPackage(
                    name: "Bar",
                    url: "http://localhost/org/bar",
                    targets: [
                        MockTarget(
                            name: "Bar",
                            dependencies: [
                                .product(name: "Baz", package: "baz")
                            ]),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "http://localhost/org/baz", requirement: .upToNextMajor(from: "3.0.0")),
                    ],
                    versions: ["2.0.0", "2.1.0"]
                ),
                MockPackage(
                    name: "Baz",
                    url: "http://localhost/org/baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "2.0.0", "2.1.0", "3.0.0", "3.1.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["MyPackage"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            PackageGraphTester(graph) { result in
                result.check(roots: "MyPackage")
                result.check(packages: "Bar", "Baz", "Foo", "MyPackage")
                result.check(targets: "Foo", "Bar", "Baz", "MyTarget1", "MyTarget2")
                result.checkTarget("MyTarget1") { result in result.check(dependencies: "Foo") }
                result.checkTarget("MyTarget2") { result in result.check(dependencies: "Bar") }
                result.checkTarget("Foo") { result in result.check(dependencies: "Baz") }
                result.checkTarget("Bar") { result in result.check(dependencies: "Baz") }
            }
        }

        workspace.checkManagedDependencies { result in
            result.check(dependency: "foo", at: .checkout(.version("1.1.0")))
            result.check(dependency: "bar", at: .checkout(.version("2.1.0")))
            result.check(dependency: "baz", at: .checkout(.version("3.1.0")))
        }

        // Check the load-package callbacks.
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for root package: \(sandbox.appending(components: "roots", "MyPackage"))"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for root package: \(sandbox.appending(components: "roots", "MyPackage"))"])
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for remoteSourceControl package: http://localhost/org/foo"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for remoteSourceControl package: http://localhost/org/foo"])
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for remoteSourceControl package: http://localhost/org/bar"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for remoteSourceControl package: http://localhost/org/bar"])
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for remoteSourceControl package: http://localhost/org/baz"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for remoteSourceControl package: http://localhost/org/baz"])
    }

    func testBasicResolutionFromRegistry() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "MyPackage",
                    targets: [
                        MockTarget(
                            name: "MyTarget1",
                            dependencies: [
                                .product(name: "Foo", package: "org.foo")
                            ]),
                        MockTarget(
                            name: "MyTarget2",
                            dependencies: [
                                .product(name: "Bar", package: "org.bar")
                            ]),
                    ],
                    products: [
                        MockProduct(name: "MyProduct", targets: ["MyTarget1", "MyTarget2"]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .registry(identity: "org.bar", requirement: .upToNextMajor(from: "2.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    identity: "org.foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "1.2.0", "1.3.0", "1.4.0", "1.5.0", "1.5.1"]
                ),
                MockPackage(
                    name: "Bar",
                    identity: "org.bar",
                    targets: [
                        MockTarget(name: "Bar"),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    versions: ["2.0.0", "2.1.0", "2.2.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["MyPackage"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            PackageGraphTester(graph) { result in
                result.check(roots: "MyPackage")
                result.check(packages: "Bar", "Foo", "MyPackage")
                result.check(targets: "Foo", "Bar", "MyTarget1", "MyTarget2")
                result.checkTarget("MyTarget1") { result in result.check(dependencies: "Foo") }
                result.checkTarget("MyTarget2") { result in result.check(dependencies: "Bar") }
            }
        }

        workspace.checkManagedDependencies { result in
            result.check(dependency: "org.foo", at: .registryDownload("1.5.1"))
            result.check(dependency: "org.bar", at: .registryDownload("2.2.0"))
        }

        // Check the load-package callbacks.
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for root package: \(sandbox.appending(components: "roots", "MyPackage"))"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for root package: \(sandbox.appending(components: "roots", "MyPackage"))"])
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for registry package: org.foo"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for registry package: org.foo"])
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for registry package: org.bar"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for registry package: org.bar"])
    }

    func testBasicTransitiveResolutionFromRegistry() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "MyPackage",
                    targets: [
                        MockTarget(
                            name: "MyTarget1",
                            dependencies: [
                                .product(name: "Foo", package: "org.foo")
                            ]),
                        MockTarget(
                            name: "MyTarget2",
                            dependencies: [
                                .product(name: "Bar", package: "org.bar")
                            ]),
                    ],
                    products: [
                        MockProduct(name: "MyProduct", targets: ["MyTarget1", "MyTarget2"]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .registry(identity: "org.bar", requirement: .upToNextMajor(from: "2.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    identity: "org.foo",
                    targets: [
                        MockTarget(
                            name: "Foo",
                            dependencies: [
                                .product(name: "Baz", package: "org.baz")
                            ]),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    dependencies: [
                        .registry(identity: "org.baz", requirement: .range("2.0.0" ..< "4.0.0")),
                    ],
                    versions: ["1.0.0", "1.1.0"]
                ),
                MockPackage(
                    name: "Bar",
                    identity: "org.bar",
                    targets: [
                        MockTarget(
                            name: "Bar",
                            dependencies: [
                                .product(name: "Baz", package: "org.baz")
                            ]),
                    ],
                    products: [
                        MockProduct(name: "Bar", targets: ["Bar"]),
                    ],
                    dependencies: [
                        .registry(identity: "org.baz", requirement: .upToNextMajor(from: "3.0.0")),
                    ],
                    versions: ["1.0.0", "1.1.0", "2.0.0", "2.1.0"]
                ),
                MockPackage(
                    name: "Baz",
                    identity: "org.baz",
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "2.0.0", "2.1.0", "3.0.0", "3.1.0"]
                ),
            ]
        )

        try workspace.checkPackageGraph(roots: ["MyPackage"]) { graph, diagnostics in
            XCTAssertNoDiagnostics(diagnostics)
            PackageGraphTester(graph) { result in
                result.check(roots: "MyPackage")
                result.check(packages: "Bar", "Baz", "Foo", "MyPackage")
                result.check(targets: "Foo", "Bar", "Baz", "MyTarget1", "MyTarget2")
                result.checkTarget("MyTarget1") { result in result.check(dependencies: "Foo") }
                result.checkTarget("MyTarget2") { result in result.check(dependencies: "Bar") }
                result.checkTarget("Foo") { result in result.check(dependencies: "Baz") }
                result.checkTarget("Bar") { result in result.check(dependencies: "Baz") }
            }
        }

        workspace.checkManagedDependencies { result in
            result.check(dependency: "org.foo", at: .registryDownload("1.1.0"))
            result.check(dependency: "org.bar", at: .registryDownload("2.1.0"))
            result.check(dependency: "org.baz", at: .registryDownload("3.1.0"))
        }

        // Check the load-package callbacks.
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for root package: \(sandbox.appending(components: "roots", "MyPackage"))"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for root package: \(sandbox.appending(components: "roots", "MyPackage"))"])
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for registry package: org.foo"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for registry package: org.foo"])
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for registry package: org.bar"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for registry package: org.bar"])
        XCTAssertMatch(workspace.delegate.events, ["will load manifest for registry package: org.baz"])
        XCTAssertMatch(workspace.delegate.events, ["did load manifest for registry package: org.baz"])
    }

    // no dups
    func testResolutionMixedRegistryAndSourceControl1() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    path: "root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "org.bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://git/org/foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .registry(identity: "org.bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://git/org/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "1.2.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    identity: "org.bar",
                    targets: [
                        MockTarget(name: "BarTarget"),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    versions: ["1.0.0", "1.1.0"]
                ),
                MockPackage(
                    name: "FooPackage",
                    identity: "org.foo",
                    alternativeURLs: ["https://git/org/foo"],
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "1.2.0"]
                )
            ]
        )

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .disabled

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                XCTAssertNoDiagnostics(diagnostics)
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "foo", at: .checkout(.version("1.2.0")))
                result.check(dependency: "org.bar", at: .registryDownload("1.1.0"))
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .identity

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                XCTAssertNoDiagnostics(diagnostics)
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .checkout(.version("1.2.0")))
                result.check(dependency: "org.bar", at: .registryDownload("1.1.0"))
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .swizzle

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                XCTAssertNoDiagnostics(diagnostics)
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .registryDownload("1.2.0"))
                result.check(dependency: "org.bar", at: .registryDownload("1.1.0"))
            }
        }
    }

    // duplicate package at root level
    func testResolutionMixedRegistryAndSourceControl2() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    path: "root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://git/org/foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://git/org/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "FooPackage",
                    identity: "org.foo",
                    alternativeURLs: ["https://git/org/foo"],
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .disabled

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        multiple products named 'FooProduct' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                    result.check(
                        diagnostic: .contains("""
                        multiple targets named 'FooTarget' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .identity

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: "'root' dependency on 'org.foo' conflicts with dependency on 'https://git/org/foo' which has the same identity 'org.foo'",
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .swizzle

            // TODO: this error message should be improved
            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: "'root' dependency on 'org.foo' conflicts with dependency on 'org.foo' which has the same identity 'org.foo'",
                        severity: .error
                    )
                }
            }
        }
    }


    // mixed graph root --> dep1 scm
    //                  --> dep2 scm --> dep1 registry
    func testResolutionMixedRegistryAndSourceControl3() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    path: "root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://git/org/foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(url: "https://git/org/bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://git/org/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "2.0.0", "2.1.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    url: "https://git/org/bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "org.foo")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0", "1.1.0", "2.0.0", "2.1.0"]
                ),
                MockPackage(
                    name: "FooPackage",
                    identity: "org.foo",
                    alternativeURLs: ["https://git/org/foo"],
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "2.0.0", "2.1.0"]
                ),
            ]
        )

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .disabled

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        multiple products named 'FooProduct' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                    result.check(
                        diagnostic: .contains("""
                        multiple targets named 'FooTarget' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .identity

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        'bar' dependency on 'org.foo' conflicts with dependency on 'https://git/org/foo' which has the same identity 'org.foo'.
                        """),
                        severity: .warning
                    )
                }

                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .checkout(.version("1.1.0")))
                result.check(dependency: "bar", at: .checkout(.version("1.1.0")))
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .swizzle

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                XCTAssertNoDiagnostics(diagnostics)

                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .registryDownload("1.1.0"))
                result.check(dependency: "bar", at: .checkout(.version("1.1.0")))
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .swizzle

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                XCTAssertNoDiagnostics(diagnostics)
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .registryDownload("1.1.0"))
                result.check(dependency: "bar", at: .checkout(.version("1.1.0")))
            }
        }
    }

    // mixed graph root --> dep1 scm
    //                  --> dep2 registry --> dep1 registry
    func testResolutionMixedRegistryAndSourceControl4() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    path: "root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "org.bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://git/org/foo", requirement: .upToNextMajor(from: "1.1.0")),
                        .registry(identity: "org.bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://git/org/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "1.2.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    identity: "org.bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "org.foo")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.2.0")),
                    ],
                    versions: ["1.0.0", "1.1.0"]
                ),
                MockPackage(
                    name: "FooPackage",
                    identity: "org.foo",
                    alternativeURLs: ["https://git/org/foo"],
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "1.2.0"]
                ),
            ]
        )

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .disabled

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        multiple products named 'FooProduct' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                    result.check(
                        diagnostic: .contains("""
                        multiple targets named 'FooTarget' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .identity

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        'org.bar' dependency on 'org.foo' conflicts with dependency on 'https://git/org/foo' which has the same identity 'org.foo'.
                        """),
                        severity: .warning
                    )
                }
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .checkout(.version("1.2.0")))
                result.check(dependency: "org.bar", at: .registryDownload("1.1.0"))
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .swizzle

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                XCTAssertNoDiagnostics(diagnostics)
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .registryDownload("1.2.0"))
                result.check(dependency: "org.bar", at: .registryDownload("1.1.0"))
            }
        }
    }

    // mixed graph root --> dep1 scm
    //                  --> dep2 scm --> dep1 registry incompatible version
    func testResolutionMixedRegistryAndSourceControl5() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    path: "root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://git/org/foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .sourceControl(url: "https://git/org/bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://git/org/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    url: "https://git/org/bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "org.foo")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "2.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "FooPackage",
                    identity: "org.foo",
                    alternativeURLs: ["https://git/org/foo"],
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .disabled

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        multiple products named 'FooProduct' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                    result.check(
                        diagnostic: .contains("""
                        multiple targets named 'FooTarget' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .identity

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic:
                        """
                        Dependencies could not be resolved because root depends on 'org.foo' 1.0.0..<2.0.0 and root depends on 'bar' 1.0.0..<2.0.0.
                        'bar' >= 1.0.0 practically depends on 'org.foo' 2.0.0..<3.0.0 because no versions of 'bar' match the requirement 1.0.1..<2.0.0 and 'bar' 1.0.0 depends on 'org.foo' 2.0.0..<3.0.0.
                        """,
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .swizzle

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic:
                        """
                        Dependencies could not be resolved because root depends on 'org.foo' 1.0.0..<2.0.0 and root depends on 'bar' 1.0.0..<2.0.0.
                        'bar' >= 1.0.0 practically depends on 'org.foo' 2.0.0..<3.0.0 because no versions of 'bar' match the requirement 1.0.1..<2.0.0 and 'bar' 1.0.0 depends on 'org.foo' 2.0.0..<3.0.0.
                        """,
                        severity: .error
                    )
                }
            }
        }
    }

    // mixed graph root --> dep1 registry
    //                  --> dep2 registry --> dep1 scm
    func testResolutionMixedRegistryAndSourceControl6() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    path: "root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "org.foo"),
                            .product(name: "BarProduct", package: "org.bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.1.0")),
                        .registry(identity: "org.bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    identity: "org.foo",
                    alternativeURLs: ["https://git/org/foo"],
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "1.2.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    identity: "org.bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "https://git/org/foo", requirement: .upToNextMajor(from: "1.2.0")),
                    ],
                    versions: ["1.0.0", "1.1.0"]
                ),
                MockPackage(
                    name: "FooPackage",
                    url: "https://git/org/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "1.1.0", "1.2.0"]
                ),
            ]
        )

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .disabled

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        multiple products named 'FooProduct' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                    result.check(
                        diagnostic: .contains("""
                        multiple targets named 'FooTarget' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .identity

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        'org.bar' dependency on 'https://git/org/foo' conflicts with dependency on 'org.foo' which has the same identity 'org.foo'.
                        """),
                        severity: .warning
                    )
                }
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .registryDownload("1.2.0"))
                result.check(dependency: "org.bar", at: .registryDownload("1.1.0"))
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .swizzle

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                XCTAssertNoDiagnostics(diagnostics)
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .registryDownload("1.2.0"))
                result.check(dependency: "org.bar", at: .registryDownload("1.1.0"))
            }
        }
    }

    // mixed graph root --> dep1 registry
    //                  --> dep2 registry --> dep1 scm incompatible version
    func testResolutionMixedRegistryAndSourceControl7() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    path: "root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "org.foo"),
                            .product(name: "BarProduct", package: "org.bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .registry(identity: "org.bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    identity: "org.foo",
                    alternativeURLs: ["https://git/org/foo"],
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    identity: "org.bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "https://git/org/foo", requirement: .upToNextMajor(from: "2.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "FooPackage",
                    url: "https://git/org/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .disabled

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        multiple products named 'FooProduct' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                    result.check(
                        diagnostic: .contains("""
                        multiple targets named 'FooTarget' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .identity

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic:
                        """
                        Dependencies could not be resolved because root depends on 'org.foo' 1.0.0..<2.0.0 and root depends on 'org.bar' 1.0.0..<2.0.0.
                        'org.bar' >= 1.0.0 practically depends on 'org.foo' 2.0.0..<3.0.0 because no versions of 'org.bar' match the requirement 1.0.1..<2.0.0 and 'org.bar' 1.0.0 depends on 'org.foo' 2.0.0..<3.0.0.
                        """,
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .swizzle

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic:
                        """
                        Dependencies could not be resolved because root depends on 'org.foo' 1.0.0..<2.0.0 and root depends on 'org.bar' 1.0.0..<2.0.0.
                        'org.bar' >= 1.0.0 practically depends on 'org.foo' 2.0.0..<3.0.0 because no versions of 'org.bar' match the requirement 1.0.1..<2.0.0 and 'org.bar' 1.0.0 depends on 'org.foo' 2.0.0..<3.0.0.
                        """,
                        severity: .error
                    )
                }
            }
        }
    }

    // mixed graph root --> dep1 registry --> dep3 scm
    //                  --> dep2 registry --> dep3 registry
    func testResolutionMixedRegistryAndSourceControl8() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    path: "root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "org.foo"),
                            .product(name: "BarProduct", package: "org.bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .registry(identity: "org.bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    identity: "org.foo",
                    targets: [
                        MockTarget(name: "FooTarget", dependencies: [
                            .product(name: "BazProduct", package: "baz")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "https://git/org/baz", requirement: .upToNextMajor(from: "1.1.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    identity: "org.bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "BazProduct", package: "org.baz")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .registry(identity: "org.baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BazPackage",
                    url: "https://git/org/baz",
                    targets: [
                        MockTarget(name: "BazTarget"),
                    ],
                    products: [
                        MockProduct(name: "BazProduct", targets: ["BazTarget"]),
                    ],
                    versions: ["1.0.0", "1.1.0"]
                ),
                MockPackage(
                    name: "BazPackage",
                    identity: "org.baz",
                    alternativeURLs: ["https://git/org/baz"],
                    targets: [
                        MockTarget(name: "BazTarget"),
                    ],
                    products: [
                        MockProduct(name: "BazProduct", targets: ["BazTarget"]),
                    ],
                    versions: ["1.0.0", "1.1.0"]
                ),
            ]
        )

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .disabled

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        multiple products named 'BazProduct' in: 'baz', 'org.baz'
                        """),
                        severity: .error
                    )
                    result.check(
                        diagnostic: .contains("""
                        multiple targets named 'BazTarget' in: 'baz', 'org.baz'
                        """),
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .identity

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        'org.foo' dependency on 'https://git/org/baz' conflicts with dependency on 'org.baz' which has the same identity 'org.baz'.
                        """),
                        severity: .warning
                    )
                }
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "BazPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "BazTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                    result.checkTarget("FooTarget") { result in result.check(dependencies: "BazProduct") }
                    result.checkTarget("BarTarget") { result in result.check(dependencies: "BazProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .registryDownload("1.0.0"))
                result.check(dependency: "org.bar", at: .registryDownload("1.0.0"))
                result.check(dependency: "org.baz", at: .registryDownload("1.1.0"))
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .swizzle

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                XCTAssertNoDiagnostics(diagnostics)
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "BazPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "BazTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                    result.checkTarget("FooTarget") { result in result.check(dependencies: "BazProduct") }
                    result.checkTarget("BarTarget") { result in result.check(dependencies: "BazProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .registryDownload("1.0.0"))
                result.check(dependency: "org.bar", at: .registryDownload("1.0.0"))
                result.check(dependency: "org.baz", at: .registryDownload("1.1.0"))
            }
        }
    }

    // mixed graph root --> dep1 registry --> dep3 scm
    //                  --> dep2 registry --> dep3 registry incompatible version
    func testResolutionMixedRegistryAndSourceControl9() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    path: "root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "org.foo"),
                            .product(name: "BarProduct", package: "org.bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                        .registry(identity: "org.bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    identity: "org.foo",
                    targets: [
                        MockTarget(name: "FooTarget", dependencies: [
                            .product(name: "BazProduct", package: "baz")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    dependencies: [
                        .sourceControl(url: "https://git/org/baz", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BarPackage",
                    identity: "org.bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "BazProduct", package: "org.baz")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .registry(identity: "org.baz", requirement: .upToNextMajor(from: "2.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BazPackage",
                    url: "https://git/org/baz",
                    targets: [
                        MockTarget(name: "BazTarget"),
                    ],
                    products: [
                        MockProduct(name: "BazProduct", targets: ["BazTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "BazPackage",
                    identity: "org.baz",
                    alternativeURLs: ["https://git/org/baz"],
                    targets: [
                        MockTarget(name: "BazTarget"),
                    ],
                    products: [
                        MockProduct(name: "BazProduct", targets: ["BazTarget"]),
                    ],
                    versions: ["1.0.0", "2.0.0"]
                ),
            ]
        )

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .disabled

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        multiple products named 'BazProduct' in: 'baz', 'org.baz'
                        """),
                        severity: .error
                    )
                    result.check(
                        diagnostic: .contains("""
                        multiple targets named 'BazTarget' in: 'baz', 'org.baz'
                        """),
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .identity

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: """
                        Dependencies could not be resolved because root depends on 'org.foo' 1.0.0..<2.0.0 and root depends on 'org.bar' 1.0.0..<2.0.0.
                        'org.bar' is incompatible with 'org.foo' because 'org.foo' 1.0.0 depends on 'org.baz' 1.0.0..<2.0.0 and no versions of 'org.foo' match the requirement 1.0.1..<2.0.0.
                        'org.bar' >= 1.0.0 practically depends on 'org.baz' 2.0.0..<3.0.0 because no versions of 'org.bar' match the requirement 1.0.1..<2.0.0 and 'org.bar' 1.0.0 depends on 'org.baz' 2.0.0..<3.0.0.
                        """,
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .swizzle

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: """
                        Dependencies could not be resolved because root depends on 'org.foo' 1.0.0..<2.0.0 and root depends on 'org.bar' 1.0.0..<2.0.0.
                        'org.bar' is incompatible with 'org.foo' because 'org.foo' 1.0.0 depends on 'org.baz' 1.0.0..<2.0.0 and no versions of 'org.foo' match the requirement 1.0.1..<2.0.0.
                        'org.bar' >= 1.0.0 practically depends on 'org.baz' 2.0.0..<3.0.0 because no versions of 'org.bar' match the requirement 1.0.1..<2.0.0 and 'org.bar' 1.0.0 depends on 'org.baz' 2.0.0..<3.0.0.
                        """,
                        severity: .error
                    )
                }
            }
        }
    }

    // mixed graph root --> dep1 scm branch
    //                  --> dep2 registry --> dep1 registry
    func testResolutionMixedRegistryAndSourceControl10() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Root",
                    path: "root",
                    targets: [
                        MockTarget(name: "RootTarget", dependencies: [
                            .product(name: "FooProduct", package: "foo"),
                            .product(name: "BarProduct", package: "org.bar")
                        ]),
                    ],
                    products: [],
                    dependencies: [
                        .sourceControl(url: "https://git/org/foo", requirement: .branch("experiment")),
                        .registry(identity: "org.bar", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    toolsVersion: .v5_6
                ),
            ],
            packages: [
                MockPackage(
                    name: "FooPackage",
                    url: "https://git/org/foo",
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["experiment"]
                ),
                MockPackage(
                    name: "BarPackage",
                    identity: "org.bar",
                    targets: [
                        MockTarget(name: "BarTarget", dependencies: [
                            .product(name: "FooProduct", package: "org.foo")
                        ]),
                    ],
                    products: [
                        MockProduct(name: "BarProduct", targets: ["BarTarget"]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ],
                    versions: ["1.0.0"]
                ),
                MockPackage(
                    name: "FooPackage",
                    identity: "org.foo",
                    alternativeURLs: ["https://git/org/foo"],
                    targets: [
                        MockTarget(name: "FooTarget"),
                    ],
                    products: [
                        MockProduct(name: "FooProduct", targets: ["FooTarget"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ]
        )

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .disabled

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        multiple products named 'FooProduct' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                    result.check(
                        diagnostic: .contains("""
                        multiple targets named 'FooTarget' in: 'foo', 'org.foo'
                        """),
                        severity: .error
                    )
                }
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .identity

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        'org.bar' dependency on 'org.foo' conflicts with dependency on 'https://git/org/foo' which has the same identity 'org.foo'.
                        """),
                        severity: .warning
                    )
                }
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .checkout(.branch("experiment")))
                result.check(dependency: "org.bar", at: .registryDownload("1.0.0"))
            }
        }

        // reset
        try workspace.closeWorkspace()

        do {
            workspace.sourceControlToRegistryDependencyTransformation = .swizzle

            try workspace.checkPackageGraph(roots: ["root"]) { graph, diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(
                        diagnostic: .contains("""
                        'org.bar' dependency on 'org.foo' conflicts with dependency on 'https://git/org/foo' which has the same identity 'org.foo'.
                        """),
                        severity: .warning
                    )
                }
                PackageGraphTester(graph) { result in
                    result.check(roots: "Root")
                    result.check(packages: "BarPackage", "FooPackage", "Root")
                    result.check(targets: "FooTarget", "BarTarget", "RootTarget")
                    result.checkTarget("RootTarget") { result in result.check(dependencies: "BarProduct", "FooProduct") }
                }
            }

            workspace.checkManagedDependencies { result in
                result.check(dependency: "org.foo", at: .checkout(.branch("experiment"))) // we cannot swizzle branch based deps
                result.check(dependency: "org.bar", at: .registryDownload("1.0.0"))
            }
        }
    }

    func testCustomPackageContainerProvider() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let customFS = InMemoryFileSystem()
        // write a manifest
        try customFS.writeFileContents(.root.appending(component: Manifest.filename), bytes: "")
        try ToolsVersionSpecificationWriter.rewriteSpecification(manifestDirectory: .root, toolsVersion: .current, fileSystem: customFS)
        // write the sources
        let sourcesDir = AbsolutePath("/Sources")
        let targetDir = sourcesDir.appending(component: "Baz")
        try customFS.createDirectory(targetDir, recursive: true)
        try customFS.writeFileContents(targetDir.appending(component: "file.swift"), bytes: "")

        let bazURL = try XCTUnwrap(URL(string: "https://example.com/baz"))
        let bazPackageReference = PackageReference(identity: PackageIdentity(url: bazURL), kind: .remoteSourceControl(bazURL))
        let bazContainer = MockPackageContainer(package: bazPackageReference, dependencies: ["1.0.0": []], fileSystem: customFS, customRetrievalPath: .root)

        let fooPath = AbsolutePath("/tmp/ws/Foo")
        let fooPackageReference = PackageReference(identity: PackageIdentity(path: fooPath), kind: .root(fooPath))
        let fooContainer = MockPackageContainer(package: fooPackageReference)

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "Foo",
                    targets: [
                        MockTarget(name: "Foo", dependencies: ["Bar"]),
                        MockTarget(name: "Bar", dependencies: [.product(name: "Baz", package: "baz")]),
                        MockTarget(name: "BarTests", dependencies: ["Bar"], type: .test),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo", "Bar"]),
                    ],
                    dependencies: [
                        .sourceControl(url: bazURL, requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Baz",
                    url: bazURL.absoluteString,
                    targets: [
                        MockTarget(name: "Baz"),
                    ],
                    products: [
                        MockProduct(name: "Baz", targets: ["Baz"]),
                    ],
                    versions: ["1.0.0"]
                ),
            ],
            customPackageContainerProvider: MockPackageContainerProvider(containers: [fooContainer, bazContainer])
        )

        let deps: [MockDependency] = [
            .sourceControl(url: bazURL, requirement: .exact("1.0.0")),
        ]
        try workspace.checkPackageGraph(roots: ["Foo"], deps: deps) { graph, diagnostics in
            PackageGraphTester(graph) { result in
                result.check(roots: "Foo")
                result.check(packages: "Baz", "Foo")
                result.check(targets: "Bar", "Baz", "Foo")
                result.check(testModules: "BarTests")
                result.checkTarget("Foo") { result in result.check(dependencies: "Bar") }
                result.checkTarget("Bar") { result in result.check(dependencies: "Baz") }
                result.checkTarget("BarTests") { result in result.check(dependencies: "Bar") }
            }
            XCTAssertNoDiagnostics(diagnostics)
        }
        workspace.checkManagedDependencies { result in
            result.check(dependency: "baz", at: .custom(Version(1, 0, 0), .root))
        }
    }

    func testRegistryMissingConfigurationErrors() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let registryClient = try makeRegistryClient(
            packageIdentity: .plain("org.foo"),
            packageVersion: "1.0.0",
            fileSystem: fs,
            configuration: .init()
        )

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "MyPackage",
                    targets: [
                        MockTarget(
                            name: "MyTarget",
                            dependencies: [
                                .product(name: "Foo", package: "org.foo")
                            ]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    identity: "org.foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"]
                )
            ],
            registryClient: registryClient
        )

        workspace.checkPackageGraphFailure(roots: ["MyPackage"]) { diagnostics in
            testDiagnostics(diagnostics) { result in
                result.check(diagnostic: .equal("No registry configured for 'org' scope"), severity: .error)
            }
        }
    }

    func testRegistryReleasesServerErrors() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "MyPackage",
                    targets: [
                        MockTarget(
                            name: "MyTarget",
                            dependencies: [
                                .product(name: "Foo", package: "org.foo")
                            ]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    identity: "org.foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"]
                )
            ]
        )

        do {
            let registryClient = try makeRegistryClient(
                packageIdentity: .plain("org.foo"),
                packageVersion: "1.0.0",
                fileSystem: fs,
                releasesRequestHandler: { _, _ , completion in
                    completion(.failure(StringError("boom")))
                }
            )

            try workspace.closeWorkspace()
            workspace.registryClient = registryClient
            workspace.checkPackageGraphFailure(roots: ["MyPackage"]) { diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(diagnostic: .equal("Failed fetching releases from registry: boom"), severity: .error)
                }
            }
        }

        do {
            let registryClient = try makeRegistryClient(
                packageIdentity: .plain("org.foo"),
                packageVersion: "1.0.0",
                fileSystem: fs,
                releasesRequestHandler: { _, _ , completion in
                    completion(.success(.serverError()))
                }
            )

            try workspace.closeWorkspace()
            workspace.registryClient = registryClient
            workspace.checkPackageGraphFailure(roots: ["MyPackage"]) { diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(diagnostic: .equal("Failed fetching releases from registry: Invalid registry response status '500', expected '200'"), severity: .error)
                }
            }
        }
    }

    func testRegistryReleaseChecksumServerErrors() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "MyPackage",
                    targets: [
                        MockTarget(
                            name: "MyTarget",
                            dependencies: [
                                .product(name: "Foo", package: "org.foo")
                            ]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    identity: "org.foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"]
                )
            ]
        )

        do {
            let registryClient = try makeRegistryClient(
                packageIdentity: .plain("org.foo"),
                packageVersion: "1.0.0",
                fileSystem: fs,
                versionMetadataRequestHandler: { _, _ , completion in
                    completion(.failure(StringError("boom")))
                }
            )

            try workspace.closeWorkspace()
            workspace.registryClient = registryClient
            workspace.checkPackageGraphFailure(roots: ["MyPackage"]) { diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(diagnostic: .equal("Failed fetching release checksum from registry: boom"), severity: .error)
                }
            }
        }

        do {
            let registryClient = try makeRegistryClient(
                packageIdentity: .plain("org.foo"),
                packageVersion: "1.0.0",
                fileSystem: fs,
                versionMetadataRequestHandler: { _, _ , completion in
                    completion(.success(.serverError()))
                }
            )

            try workspace.closeWorkspace()
            workspace.registryClient = registryClient
            workspace.checkPackageGraphFailure(roots: ["MyPackage"]) { diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(diagnostic: .equal("Failed fetching release checksum from registry: Invalid registry response status '500', expected '200'"), severity: .error)
                }
            }
        }
    }

    func testRegistryManifestServerErrors() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "MyPackage",
                    targets: [
                        MockTarget(
                            name: "MyTarget",
                            dependencies: [
                                .product(name: "Foo", package: "org.foo")
                            ]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    identity: "org.foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"]
                )
            ]
        )

        do {
            let registryClient = try makeRegistryClient(
                packageIdentity: .plain("org.foo"),
                packageVersion: "1.0.0",
                fileSystem: fs,
                manifestRequestHandler: { _, _ , completion in
                    completion(.failure(StringError("boom")))
                }
            )

            try workspace.closeWorkspace()
            workspace.registryClient = registryClient
            workspace.checkPackageGraphFailure(roots: ["MyPackage"]) { diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(diagnostic: .equal("Failed retrieving manifest from registry: boom"), severity: .error)
                }
            }
        }

        do {
            let registryClient = try makeRegistryClient(
                packageIdentity: .plain("org.foo"),
                packageVersion: "1.0.0",
                fileSystem: fs,
                manifestRequestHandler: { _, _ , completion in
                    completion(.success(.serverError()))
                }
            )

            try workspace.closeWorkspace()
            workspace.registryClient = registryClient
            workspace.checkPackageGraphFailure(roots: ["MyPackage"]) { diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(diagnostic: .equal("Failed retrieving manifest from registry: Invalid registry response status '500', expected '200'"), severity: .error)
                }
            }
        }
    }

    func testRegistryDownloadServerErrors() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "MyPackage",
                    targets: [
                        MockTarget(
                            name: "MyTarget",
                            dependencies: [
                                .product(name: "Foo", package: "org.foo")
                            ]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    identity: "org.foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"]
                )
            ]
        )

        do {
            let registryClient = try makeRegistryClient(
                packageIdentity: .plain("org.foo"),
                packageVersion: "1.0.0",
                fileSystem: fs,
                downloadArchiveRequestHandler: { _, _ , completion in
                    completion(.failure(StringError("boom")))
                }
            )

            try workspace.closeWorkspace()
            workspace.registryClient = registryClient
            workspace.checkPackageGraphFailure(roots: ["MyPackage"]) { diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(diagnostic: .equal("Failed downloading source archive from registry: boom"), severity: .error)
                }
            }
        }

        do {
            let registryClient = try makeRegistryClient(
                packageIdentity: .plain("org.foo"),
                packageVersion: "1.0.0",
                fileSystem: fs,
                downloadArchiveRequestHandler: { _, _ , completion in
                    completion(.success(.serverError()))
                }
            )

            try workspace.closeWorkspace()
            workspace.registryClient = registryClient
            workspace.checkPackageGraphFailure(roots: ["MyPackage"]) { diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(diagnostic: .equal("Failed downloading source archive from registry: Invalid registry response status '500', expected '200'"), severity: .error)
                }
            }
        }
    }

    func testRegistryArchiveErrors() throws {
        let sandbox = AbsolutePath("/tmp/ws/")
        let fs = InMemoryFileSystem()

        let workspace = try MockWorkspace(
            sandbox: sandbox,
            fileSystem: fs,
            roots: [
                MockPackage(
                    name: "MyPackage",
                    targets: [
                        MockTarget(
                            name: "MyTarget",
                            dependencies: [
                                .product(name: "Foo", package: "org.foo")
                            ]),
                    ],
                    dependencies: [
                        .registry(identity: "org.foo", requirement: .upToNextMajor(from: "1.0.0")),
                    ]
                ),
            ],
            packages: [
                MockPackage(
                    name: "Foo",
                    identity: "org.foo",
                    targets: [
                        MockTarget(name: "Foo"),
                    ],
                    products: [
                        MockProduct(name: "Foo", targets: ["Foo"]),
                    ],
                    versions: ["1.0.0"]
                )
            ]
        )

        do {
            let registryClient = try makeRegistryClient(
                packageIdentity: .plain("org.foo"),
                packageVersion: "1.0.0",
                fileSystem: fs,
                archiver: MockArchiver(handler: { archiver, from, to, completion in
                    completion(.failure(StringError("boom")))
                })
            )

            try workspace.closeWorkspace()
            workspace.registryClient = registryClient
            workspace.checkPackageGraphFailure(roots: ["MyPackage"]) { diagnostics in
                testDiagnostics(diagnostics) { result in
                    result.check(diagnostic: .regex("failed extracting '.*[\\\\/]registry[\\\\/]downloads[\\\\/]org[\\\\/]foo[\\\\/]1.0.0.zip' to '.*[\\\\/]registry[\\\\/]downloads[\\\\/]org[\\\\/]foo[\\\\/]1.0.0': boom"), severity: .error)
                }
            }
        }
    }

    func makeRegistryClient(
        packageIdentity: PackageIdentity,
        packageVersion: Version,
        fileSystem: FileSystem,
        configuration: PackageRegistry.RegistryConfiguration? = .none,
        identityResolver: IdentityResolver? = .none,
        fingerprintStorage: PackageFingerprintStorage? = .none,
        fingerprintCheckingMode: FingerprintCheckingMode = .strict,
        authorizationProvider: AuthorizationProvider? = .none,
        releasesRequestHandler: HTTPClient.Handler? = .none,
        versionMetadataRequestHandler: HTTPClient.Handler? = .none,
        manifestRequestHandler: HTTPClient.Handler? = .none,
        downloadArchiveRequestHandler: HTTPClient.Handler? = .none,
        archiver: Archiver? = .none
    ) throws -> RegistryClient {
        let jsonEncoder = JSONEncoder.makeWithDefaults()

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
            switch request.kind {
            case .download(let fileSystem, let destination):
                // creates a dummy zipfile which is required by the archiver step
                try! fileSystem.createDirectory(destination.parentDirectory, recursive: true)
                try! fileSystem.writeFileContents(destination, string: "")
            default:
                preconditionFailure("invalid request")
            }

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
        let fingerprintStorage = fingerprintStorage ?? MockPackageFingerprintStorage()

        return RegistryClient(
            configuration: configuration!,
            fingerprintStorage: fingerprintStorage,
            fingerprintCheckingMode: fingerprintCheckingMode,
            authorizationProvider: authorizationProvider?.httpAuthorizationHeader(for:),
            customHTTPClient: HTTPClient(configuration: .init(), handler: { request, progress , completion in
                switch request.url {
                // request to get package releases
                case URL(string: "http://localhost/\(packageScope)/\(packageName)")!:
                    releasesRequestHandler(request, progress, completion)
                // request to get package version metadata
                case URL(string: "http://localhost/\(packageScope)/\(packageName)/\(packageVersion)")!:
                    versionMetadataRequestHandler(request, progress, completion)
                // request to get package manifest
                case URL(string: "http://localhost/\(packageScope)/\(packageName)/\(packageVersion)/Package.swift")!:
                    manifestRequestHandler(request, progress, completion)
                // request to get download the version source archive
                case URL(string: "http://localhost/\(packageScope)/\(packageName)/\(packageVersion).zip")!:
                    downloadArchiveRequestHandler(request, progress, completion)
                default:
                    completion(.failure(StringError("unexpected url \(request.url)")))
                }
            }),
            customArchiverProvider: { _ in archiver }
        )
    }
}

func createDummyXCFramework(fileSystem: FileSystem, path: AbsolutePath, name: String) throws {
    let path = path.appending(component: "\(name).xcframework")
    try fileSystem.createDirectory(path, recursive: true)
    try fileSystem.writeFileContents(
        path.appending(component: "info.plist"),
        string: """
            <?xml version="1.0" encoding="UTF-8"?>
            <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
            <plist version="1.0">
            <dict>
                <key>AvailableLibraries</key>
                <array></array>
                <key>CFBundlePackageType</key>
                <string>XFWK</string>
                <key>XCFrameworkFormatVersion</key>
                <string>1.0</string>
            </dict>
            </plist>
            """
    )
}

func createDummyArtifactBundle(fileSystem: FileSystem, path: AbsolutePath, name: String) throws {
    let path = path.appending(component: "\(name).artifactbundle")
    try fileSystem.createDirectory(path, recursive: true)
    try fileSystem.writeFileContents(
        path.appending(component: "info.json"),
        string: """
        {
            "schemaVersion": "1.0",
            "artifacts": {}
        }
        """
    )
}

struct DummyError: LocalizedError, Equatable {
    public var errorDescription: String? { "dummy error" }
}
