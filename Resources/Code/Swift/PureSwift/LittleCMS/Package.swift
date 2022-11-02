// swift-tools-version:3.0.2
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "LittleCMS",
    targets: [
        Target(name: "LittleCMS")
    ],
    dependencies: [
        .Package(url: "https://github.com/PureSwift/CLCMS.git", majorVersion: 1)
    ]
)
