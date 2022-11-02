// swift-tools-version:4.0
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "Interval",
    products: [
        .library(
          name: "Interval",
          type: .dynamic,
          targets: ["Interval"]),
    ],
    dependencies: [
      //.package(url: "https://github.com/dankogai/swift-floatingpointmath.git", from: "0.0.7")
      .package(url: "https://github.com/apple/swift-numerics", from: "0.0.8"),
    ],
    targets: [
        .target(
           name: "Interval",
           dependencies: [
               .product(name: "Numerics", package: "swift-numerics")
           ]),
        .target(
            name: "IntervalRun",
            dependencies: ["Interval"]),
        .testTarget(
            name: "IntervalTests",
            dependencies: ["Interval"]),
    ]
)
