import PackageDescription

let package = Package(
    name: "CPNG",
    pkgConfig: "libpng",
    providers: [.Brew("libpng"), .Apt("libpng")]
)
