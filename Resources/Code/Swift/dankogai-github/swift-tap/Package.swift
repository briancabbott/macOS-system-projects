// swift-tools-version:4.0
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "TAP",
    products: [
        .library(
            name: "TAP",
            targets: ["TAP"]),
    ],
    dependencies: [],
    targets: [
      .target(
        name: "TAP",
        dependencies: [],
        path: "Sources/TAP"),
      .target(
        name: "main",
        dependencies: ["TAP"],
        path: "Sources/TAPSample"),
      .testTarget(
            name: "TAPTests",
            dependencies: ["TAP"],
            path: "Sources/Tests"),
    ]
)
