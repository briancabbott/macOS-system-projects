// swift-tools-version:5.1

import PackageDescription

#if canImport(Combine)
  let extraPackages     : [ PackageDescription.Package.Dependency ] = []
  let extraDependencies : [ Target.Dependency ] = []
#else
  let extraPackages     : [ PackageDescription.Package.Dependency ] = [
    .package(url: "https://github.com/broadwaylamb/OpenCombine.git",
             from: "0.5.0")
  ]
  let extraDependencies : [ Target.Dependency ] = [ "OpenCombine" ]
#endif

let package = Package(
  
  name: "SwiftWebUI",
  
   platforms: [
     .macOS(.v10_15), .iOS(.v13)
   ],
  
  products: [
    .library   (name: "SwiftWebUI", targets: [ "SwiftWebUI" ]),
    .executable(name: "HolyCow",    targets: [ "HolyCow"    ])
  ],
  
  dependencies: [
    /* .package(url: "https://github.com/apple/swift-nio.git",
             from: "2.13.0"),
    .package(url: "https://github.com/SwiftWebResources/SemanticUI-Swift.git",
             from: "2.3.4"), */
    .package(url: "https://github.com/MaxDesiatov/Runtime",
             .branch("wasi-build")),
    .package(url: "https://github.com/kateinoigakukun/JavaScriptKit", .revision("1edcf70")),
  ] + extraPackages,
  
  targets: [
    .target(name: "SwiftWebUI",
            dependencies: [
                /* "NIO", "NIOHTTP1", "NIOConcurrencyHelpers",  */
                "Runtime", /* "SemanticUI" */
                "JavaScriptKit"
            ] + extraDependencies),
    .target(name: "HolyCow", dependencies: [ "SwiftWebUI", "JavaScriptKit" ] + extraDependencies)
  ]
)
