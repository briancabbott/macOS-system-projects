import PackageDescription

let package = Package(
    name: "JSONC",
    dependencies: [
        .Package(url: "https://github.com/PureSwift/CJSONC.git", majorVersion: 1),
        .Package(url: "https://github.com/PureSwift/SwiftFoundation.git", majorVersion: 1)
    ],
    targets: [
        Target(
            name: "UnitTests",
            dependencies: [.Target(name: "JSONC")]),
        Target(
            name: "JSONC")
    ]
)