// swift-tools-version:5.6
import PackageDescription

let package = Package(
    name: "swift-distributed-tracing-baggage-core",
    products: [
        .library(
            name: "CoreBaggage",
            targets: [
                "CoreBaggage",
            ]
        ),
    ],
    dependencies: [
        .package(url: "https://github.com/apple/swift-docc-plugin", from: "1.0.0"),
    ],
    targets: [
        .target(
            name: "CoreBaggage",
            dependencies: []
        ),

        // ==== --------------------------------------------------------------------------------------------------------
        // MARK: Tests

        .testTarget(
            name: "CoreBaggageTests",
            dependencies: [
                "CoreBaggage",
            ]
        ),
    ]
)
