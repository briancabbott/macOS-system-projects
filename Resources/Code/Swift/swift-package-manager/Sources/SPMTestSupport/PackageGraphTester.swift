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

import XCTest

import PackageModel
import PackageGraph

public func PackageGraphTester(_ graph: PackageGraph, _ result: (PackageGraphResult) -> Void) {
    result(PackageGraphResult(graph))
}

public final class PackageGraphResult {
    public let graph: PackageGraph

    public init(_ graph: PackageGraph) {
        self.graph = graph
    }

    // TODO: deprecate / transition to PackageIdentity
    public func check(roots: String..., file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(graph.rootPackages.map{$0.manifest.displayName }.sorted(), roots.sorted(), file: file, line: line)
    }

    public func check(roots: PackageIdentity..., file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(graph.rootPackages.map{$0.identity }.sorted(), roots.sorted(), file: file, line: line)
    }

    // TODO: deprecate / transition to PackageIdentity
    public func check(packages: String..., file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(graph.packages.map {$0.manifest.displayName }.sorted(), packages.sorted(), file: file, line: line)
    }

    public func check(packages: PackageIdentity..., file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(graph.packages.map {$0.identity }.sorted(), packages.sorted(), file: file, line: line)
    }

    public func check(targets: String..., file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(
            graph.allTargets
                .filter{ $0.type != .test }
                .map{ $0.name }
                .sorted(), targets.sorted(), file: file, line: line)
    }

    public func check(products: String..., file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(Set(graph.allProducts.map { $0.name }), Set(products), file: file, line: line)
    }

    public func check(reachableTargets: String..., file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(Set(graph.reachableTargets.map { $0.name }), Set(reachableTargets), file: file, line: line)
    }

    public func check(reachableProducts: String..., file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(Set(graph.reachableProducts.map { $0.name }), Set(reachableProducts), file: file, line: line)
    }

    public func check(
        reachableBuildTargets: String...,
        in environment: BuildEnvironment,
        file: StaticString = #file,
        line: UInt = #line
    ) throws {
        let targets = Set(try self.reachableBuildTargets(in: environment).map({ $0.name }))
        XCTAssertEqual(targets, Set(reachableBuildTargets), file: file, line: line)
    }

    public func check(
        reachableBuildProducts: String...,
        in environment: BuildEnvironment,
        file: StaticString = #file,
        line: UInt = #line
    ) throws {
        let products = Set(try self.reachableBuildProducts(in: environment).map({ $0.name }))
        XCTAssertEqual(products, Set(reachableBuildProducts), file: file, line: line)
    }

    public func checkTarget(
        _ name: String,
        file: StaticString = #file,
        line: UInt = #line,
        body: (ResolvedTargetResult) -> Void
    ) {
        guard let target = find(target: name) else {
            return XCTFail("Target \(name) not found", file: file, line: line)
        }
        body(ResolvedTargetResult(target))
    }

    public func checkProduct(
        _ name: String,
        file: StaticString = #file,
        line: UInt = #line,
        body: (ResolvedProductResult) -> Void
    ) {
        guard let target = find(product: name) else {
            return XCTFail("Product \(name) not found", file: file, line: line)
        }
        body(ResolvedProductResult(target))
    }

    public func check(testModules: String..., file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(
            graph.allTargets
                .filter{ $0.type == .test }
                .map{ $0.name }
                .sorted(), testModules.sorted(), file: file, line: line)
    }

    public func find(target: String) -> ResolvedTarget? {
        return graph.allTargets.first(where: { $0.name == target })
    }

    public func find(product: String) -> ResolvedProduct? {
        return graph.allProducts.first(where: { $0.name == product })
    }

    private func reachableBuildTargets(in environment: BuildEnvironment) throws -> Set<ResolvedTarget> {
        let inputTargets = graph.inputPackages.lazy.flatMap { $0.targets }
        let recursiveBuildTargetDependencies = try inputTargets
            .flatMap { try $0.recursiveDependencies(satisfying: environment) }
            .compactMap { $0.target }
        return Set(inputTargets).union(recursiveBuildTargetDependencies)
    }

    private func reachableBuildProducts(in environment: BuildEnvironment) throws -> Set<ResolvedProduct> {
        let recursiveBuildProductDependencies = try graph.inputPackages
            .lazy
            .flatMap { $0.targets }
            .flatMap { try $0.recursiveDependencies(satisfying: environment) }
            .compactMap { $0.product }
        return Set(graph.inputPackages.flatMap { $0.products }).union(recursiveBuildProductDependencies)
    }
}

public final class ResolvedTargetResult {
    private let target: ResolvedTarget

    init(_ target: ResolvedTarget) {
        self.target = target
    }

    public func check(dependencies: String..., file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(Set(dependencies), Set(target.dependencies.map({ $0.name })), file: file, line: line)
    }

    public func checkDependency(
        _ name: String,
        file: StaticString = #file,
        line: UInt = #line,
        body: (ResolvedTargetDependencyResult) -> Void
    ) {
        guard let dependency = target.dependencies.first(where: { $0.name == name }) else {
            return XCTFail("Dependency \(name) not found", file: file, line: line)
        }
        body(ResolvedTargetDependencyResult(dependency))
    }

    public func check(type: Target.Kind, file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(type, target.type, file: file, line: line)
    }

    public func checkDeclaredPlatforms(_ platforms: [String: String], file: StaticString = #file, line: UInt = #line) {
        let targetPlatforms = Dictionary(uniqueKeysWithValues: target.platforms.declared.map({ ($0.platform.name, $0.version.versionString) }))
        XCTAssertEqual(platforms, targetPlatforms, file: file, line: line)
    }

    public func checkDerivedPlatforms(_ platforms: [String: String], file: StaticString = #file, line: UInt = #line) {
        let targetPlatforms = Dictionary(uniqueKeysWithValues: target.platforms.derived.map({ ($0.platform.name, $0.version.versionString) }))
        XCTAssertEqual(platforms, targetPlatforms, file: file, line: line)
    }

    public func checkDerivedPlatformOptions(_ platform: PackageModel.Platform, options: [String], file: StaticString = #file, line: UInt = #line) {
        let platform = target.platforms.getDerived(for: platform)
        XCTAssertEqual(platform?.options, options, file: file, line: line)
    }
}

public final class ResolvedTargetDependencyResult {
    private let dependency: ResolvedTarget.Dependency

    init(_ dependency: ResolvedTarget.Dependency) {
        self.dependency = dependency
    }

    public func checkConditions(satisfy environment: BuildEnvironment, file: StaticString = #file, line: UInt = #line) {
        XCTAssert(dependency.conditions.allSatisfy({ $0.satisfies(environment) }), file: file, line: line)
    }

    public func checkConditions(
        dontSatisfy environment: BuildEnvironment,
        file: StaticString = #file,
        line: UInt = #line
    ) {
        XCTAssert(!dependency.conditions.allSatisfy({ $0.satisfies(environment) }), file: file, line: line)
    }
}

public final class ResolvedProductResult {
    private let product: ResolvedProduct

    init(_ product: ResolvedProduct) {
        self.product = product
    }

    public func check(targets: String..., file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(Set(targets), Set(product.targets.map({ $0.name })), file: file, line: line)
    }

    public func check(type: ProductType, file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(type, product.type, file: file, line: line)
    }

    public func checkDeclaredPlatforms(_ platforms: [String: String], file: StaticString = #file, line: UInt = #line) {
        let targetPlatforms = Dictionary(uniqueKeysWithValues: product.platforms.declared.map({ ($0.platform.name, $0.version.versionString) }))
        XCTAssertEqual(platforms, targetPlatforms, file: file, line: line)
    }

    public func checkDerivedPlatforms(_ platforms: [String: String], file: StaticString = #file, line: UInt = #line) {
        let targetPlatforms = Dictionary(uniqueKeysWithValues: product.platforms.derived.map({ ($0.platform.name, $0.version.versionString) }))
        XCTAssertEqual(platforms, targetPlatforms, file: file, line: line)
    }

    public func checkDerivedPlatformOptions(_ platform: PackageModel.Platform, options: [String], file: StaticString = #file, line: UInt = #line) {
        let platform = product.platforms.getDerived(for: platform)
        XCTAssertEqual(platform?.options, options, file: file, line: line)
    }
}

extension ResolvedTarget.Dependency {
    public var name: String {
        switch self {
        case .target(let target, _):
            return target.name
        case .product(let product, _):
            return product.name
        }
    }
}
