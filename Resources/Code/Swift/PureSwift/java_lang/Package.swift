// swift-tools-version:4.0
import PackageDescription

let package = Package(
    name: "java_lang",
    products: [
        .library(
            name: "java_lang",
            targets: ["java_lang"]
        ),
    ],
    dependencies: [
        .package(
            url: "https://github.com/PureSwift/java_swift.git",
            .branch("master")
        ),
    ],
    targets: [
        .target(
            name: "java_lang",
            dependencies: [
                "java_swift",
            ],
            path: "./Sources"
        )
    ]
)
