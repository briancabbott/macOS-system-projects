import os
import string


repos = [

    "https://github.com/apple/example-package-deckofplayingcards",
    "https://github.com/apple/example-package-playingcard",
    "https://github.com/apple/swift",
    "https://github.com/apple/swift-3-api-guidelines-review",
    "https://github.com/apple/swift-algorithms",
    "https://github.com/apple/swift-argument-parser",
    "https://github.com/apple/swift-async-algorithms",
    "https://github.com/apple/swift-atomics",
    "https://github.com/apple/swift-book",
    "https://github.com/apple/swift-clang",
    "https://github.com/apple/swift-clang-tools-extra",
    "https://github.com/apple/swift-cluster-membership",
    "https://github.com/apple/swift-cmark",
    "https://github.com/apple/swift-collections",
    "https://github.com/apple/swift-collections-benchmark",
    "https://github.com/apple/swift-community-hosted-continuous-integration",
    "https://github.com/apple/swift-compiler-rt",
    "https://github.com/apple/swift-corelibs-foundation",
    "https://github.com/apple/swift-corelibs-libdispatch",
    "https://github.com/apple/swift-corelibs-xctest",
    "https://github.com/apple/swift-crypto",
    "https://github.com/apple/swift-distributed-actors",
    "https://github.com/apple/swift-distributed-tracing",
    "https://github.com/apple/swift-distributed-tracing-baggage",
    "https://github.com/apple/swift-distributed-tracing-baggage-core",
    "https://github.com/apple/swift-docc",
    "https://github.com/apple/swift-docc-plugin",
    "https://github.com/apple/swift-docc-render",
    "https://github.com/apple/swift-docc-render-artifact",
    "https://github.com/apple/swift-docc-symbolkit",
    "https://github.com/apple/swift-docker",
    "https://github.com/apple/swift-driver",
    "https://github.com/apple/swift-evolution",
    "https://github.com/apple/swift-evolution-staging",
    "https://github.com/apple/swift-experimental-string-processing",
    "https://github.com/apple/swift-format",
    "https://github.com/apple/swift-http-structured-headers",
    "https://github.com/apple/swift-installer-scripts",
    "https://github.com/apple/swift-integration-tests",
    "https://github.com/apple/swift-internals",
    "https://github.com/apple/swift-issues",
    "https://github.com/apple/swift-libcxx",
    "https://github.com/apple/swift-llbuild",
    "https://github.com/apple/swift-llbuild2",
    "https://github.com/apple/swift-lldb",
    "https://github.com/apple/swift-llvm",
    "https://github.com/apple/swift-llvm-bindings",
    "https://github.com/apple/swift-lmdb",
    "https://github.com/apple/swift-log",
    "https://github.com/apple/swift-markdown",
    "https://github.com/apple/swift-metrics",
    "https://github.com/apple/swift-metrics-extras",
    "https://github.com/apple/swift-nio",
    "https://github.com/apple/swift-nio-examples",
    "https://github.com/apple/swift-nio-extras",
    "https://github.com/apple/swift-nio-http2",
    "https://github.com/apple/swift-nio-imap",
    "https://github.com/apple/swift-nio-nghttp2-support",
    "https://github.com/apple/swift-nio-ssh",
    "https://github.com/apple/swift-nio-ssl",
    "https://github.com/apple/swift-nio-ssl-support",
    "https://github.com/apple/swift-nio-transport-services",
    "https://github.com/apple/swift-nio-zlib-support",
    "https://github.com/apple/swift-numerics",
    "https://github.com/apple/swift-org-website",
    "https://github.com/apple/swift-package-collection-generator",
    "https://github.com/apple/swift-package-manager",
    "https://github.com/apple/swift-package-registry-compatibility-test-suite",
    "https://github.com/apple/swift-protobuf",
    "https://github.com/apple/swift-protobuf-plugin",
    "https://github.com/apple/swift-protobuf-test-conformance",
    "https://github.com/apple/swift-sample-distributed-actors-transport",
    "https://github.com/apple/swift-se0270-range-set",
    "https://github.com/apple/swift-se0288-is-power",
    "https://github.com/apple/swift-service-discovery",
    "https://github.com/apple/swift-source-compat-suite",
    "https://github.com/apple/swift-standard-library-preview",
    "https://github.com/apple/swift-statsd-client",
    "https://github.com/apple/swift-stress-tester",
    "https://github.com/apple/swift-syntax",
    "https://github.com/apple/swift-system",
    "https://github.com/apple/swift-tools-support-async",
    "https://github.com/apple/swift-tools-support-core",
    "https://github.com/apple/swift-xcode-playground-support",
    "https://github.com/apple/swiftpm-on-llbuild2",
    "https://github.com/jpsim/Yams"
]

for repo in repos:
    repo_file = repo.split("/")[-1]
    print(repo_file)
    repo_exists = os.path.exists("./" + repo_file)
    print("repo_exists ", repo_exists)
    if not repo_exists:
        print("git clone " + repo)

    os.chdir(repo_file)
    os.system("git pull")
    os.chdir("..")
    print("")