import PackageDescription

let package = Package(
    name: "MySQL",
    dependencies: [
        .Package(url: "https://github.com/PureSwift/CMySQL.git", majorVersion: 1),
        .Package(url: "https://github.com/PureSwift/SwiftFoundation.git", majorVersion: 1)
    ],
    targets: [
        Target(
            name: "UnitTests",
            dependencies: [.Target(name: "MySQL")]),
        Target(
            name: "MySQL")
    ]
)