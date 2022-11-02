// swift-tools-version:5.6
import PackageDescription

let package = Package(
    name: "swift-distributed-tracing-baggage",
    products: [
        .library(
            name: "InstrumentationBaggage",
            targets: [
                "InstrumentationBaggage",
            ]
        ),
    ],
    dependencies: [
        .package(url: "https://github.com/apple/swift-docc-plugin", from: "1.0.0"),
    ],
    targets: [
        .target(name: "InstrumentationBaggage"),

        // ==== --------------------------------------------------------------------------------------------------------
        // MARK: Tests

        .testTarget(
            name: "InstrumentationBaggageTests",
            dependencies: [
                .target(name: "InstrumentationBaggage"),
            ]
        ),
    ]
)
