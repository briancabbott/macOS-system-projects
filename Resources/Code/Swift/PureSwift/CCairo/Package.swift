// swift-tools-version:3.0.2
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "CCairo",
    pkgConfig: "cairo",
    providers: [.Brew("cairo"), .Apt("libcairo-dev")]
)
