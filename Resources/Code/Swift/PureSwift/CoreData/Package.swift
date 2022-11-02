// swift-tools-version:5.2
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let package = Package(
    name: "CoreData",
    products: [
        .library(
            name: "CoreData",
            targets: ["CoreData"]
        ),
    ],
    dependencies: [
        .package(
            url: "https://github.com/PureSwift/XMLCoder.git",
            .branch("master")
        ),
    ],
    targets: [
        .target(
            name: "CoreData",
            dependencies: []
        ),
        .target(
            name: "CoreDataXML",
            dependencies: [
                "XMLCoder"
            ]
        ),
        .testTarget(
            name: "CoreDataTests",
            dependencies: ["CoreData"]
        ),
    ]
)
