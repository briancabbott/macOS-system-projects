// swift-tools-version:4.0
import PackageDescription

let package = Package(
    name: "CJavaVM",
    products: [
        .library(
            name: "CJavaVM",
            targets: ["CJavaVM"]
        ),
    ],
    targets: [
        .target(
            name: "CJavaVM"
        )
    ]
)