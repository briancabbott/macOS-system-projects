import PackageDescription

let package = Package(
    name: "CNetlink",
    targets: [
        Target(
            name: "CNetlink"
        )
    ],
    exclude: ["Xcode", "Carthage"]
)
