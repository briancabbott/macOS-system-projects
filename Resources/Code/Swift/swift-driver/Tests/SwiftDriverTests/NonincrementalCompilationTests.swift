//===------------ NonincrementalCompilationTests.swift - Swift Testing ----===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//
import XCTest
import TSCBasic

@_spi(Testing) import SwiftDriver
import SwiftOptions
import TestUtilities


/// Testing the machinery for incremental compilation with nonincremental test cases.
final class NonincrementalCompilationTests: XCTestCase {
  func testBuildRecordReading() throws {
    let buildRecord = try XCTUnwrap(
      BuildRecord(contents: Inputs.buildRecord,
                                  failedToReadOutOfDateMap: {XCTFail($0 ?? "Failed to read map")}))
    XCTAssertEqual(buildRecord.swiftVersion,
                   "Apple Swift version 5.1 (swiftlang-1100.0.270.13 clang-1100.0.33.7)")
    XCTAssertEqual(buildRecord.argsHash, "abbbfbcaf36b93e58efaadd8271ff142")

    XCTAssertEqual(buildRecord.buildStartTime,
                   TimePoint(seconds: 1570318779, nanoseconds: 32358000))
    XCTAssertEqual(buildRecord.buildEndTime,
                   TimePoint(seconds: 1570318779, nanoseconds: 32358010))
    try XCTAssertEqual(buildRecord.inputInfos,
                       [
                        VirtualPath(path: "/Volumes/AS/repos/swift-driver/sandbox/sandbox/sandbox/file2.swift"):
                          InputInfo(status: .needsCascadingBuild,
                                    previousModTime: TimePoint(seconds: 1570318778, nanoseconds: 0)),
                        VirtualPath(path: "/Volumes/AS/repos/swift-driver/sandbox/sandbox/sandbox/main.swift"):
                          InputInfo(status: .upToDate,
                                    previousModTime: TimePoint(seconds: 1570083660, nanoseconds: 0)),
                        VirtualPath(path: "/Volumes/gazorp.swift"):
                          InputInfo(status: .needsNonCascadingBuild,
                                    previousModTime: .zero),
                       ])
  }

  func testBuildRecordWithoutOptionsReading() throws {
    let buildRecord = try XCTUnwrap(
      BuildRecord(
        contents: Inputs.buildRecordWithoutOptions,
        failedToReadOutOfDateMap: {XCTFail($0 ?? "Failed to read map")}))
    XCTAssertEqual(buildRecord.swiftVersion,
                   "Apple Swift version 5.1 (swiftlang-1100.0.270.13 clang-1100.0.33.7)")
    XCTAssertEqual(buildRecord.argsHash, nil)

    XCTAssertEqual(buildRecord.buildStartTime,
                   TimePoint(seconds: 1570318779, nanoseconds: 32358000))
    XCTAssertEqual(buildRecord.buildEndTime,
                   TimePoint(seconds: 1570318779, nanoseconds: 32358010))
    try XCTAssertEqual(buildRecord.inputInfos,
                       [
                        VirtualPath(path: "/Volumes/AS/repos/swift-driver/sandbox/sandbox/sandbox/file2.swift"):
                          InputInfo(status: .needsCascadingBuild,
                                    previousModTime: TimePoint(seconds: 1570318778, nanoseconds: 0)),
                        VirtualPath(path: "/Volumes/AS/repos/swift-driver/sandbox/sandbox/sandbox/main.swift"):
                          InputInfo(status: .upToDate,
                                    previousModTime: TimePoint(seconds: 1570083660, nanoseconds: 0)),
                        VirtualPath(path: "/Volumes/gazorp.swift"):
                          InputInfo(status: .needsNonCascadingBuild,
                                    previousModTime: .zero)
                       ])
  }

  func testReadBinarySourceFileDependencyGraph() throws {
    let absolutePath = try XCTUnwrap(Fixture.fixturePath(at: RelativePath("Incremental"),
                                                         for: "main.swiftdeps"))
    let typedFile = TypedVirtualPath( file: VirtualPath.absolute(absolutePath).intern(), type: .swiftDeps)
    try MockIncrementalCompilationSynchronizer.withInternedStringTable { internedStringTable in
      let graph = try XCTUnwrap(
        try SourceFileDependencyGraph(
          contentsOf: typedFile,
          on: localFileSystem,
          internedStringTable: internedStringTable))
      XCTAssertEqual(graph.majorVersion, 1)
      XCTAssertEqual(graph.minorVersion, 0)
      XCTAssertEqual(graph.compilerVersionString, "Swift version 5.3-dev (LLVM f516ac602c, Swift c39f31febd)")
      graph.verify()
      var saw0 = false
      var saw1 = false
      var saw2 = false
      graph.forEachNode { node in
        switch (node.sequenceNumber, node.key.designator) {
        case let (0, .sourceFileProvide(name: name)):
          saw0 = true
          XCTAssertEqual(node.key.aspect, .interface)
          XCTAssertEqual(name.lookup(in: internedStringTable), "main.swiftdeps")
          XCTAssertEqual(node.fingerprint?.lookup(in: internedStringTable), "ec443bb982c3a06a433bdd47b85eeba2")
          XCTAssertEqual(node.defsIDependUpon, [2])
          XCTAssertEqual(node.definitionVsUse, .definition)
        case let (1, .sourceFileProvide(name: name)):
          saw1 = true
          XCTAssertEqual(node.key.aspect, .implementation)
          XCTAssertEqual(name.lookup(in: internedStringTable), "main.swiftdeps")
          XCTAssertEqual(node.fingerprint?.lookup(in: internedStringTable), "ec443bb982c3a06a433bdd47b85eeba2")
          XCTAssertEqual(node.defsIDependUpon, [])
          XCTAssertEqual(node.definitionVsUse, .definition)
        case let (2, .topLevel(name: name)):
          saw2 = true
          XCTAssertEqual(node.key.aspect, .interface)
          XCTAssertEqual(name.lookup(in: internedStringTable), "a")
          XCTAssertNil(node.fingerprint)
          XCTAssertEqual(node.defsIDependUpon, [])
          XCTAssertEqual(node.definitionVsUse, .use)
        default:
          XCTFail()
        }
      }
      XCTAssertTrue(saw0)
      XCTAssertTrue(saw1)
      XCTAssertTrue(saw2)
    }
  }

  func testReadComplexSourceFileDependencyGraph() throws {
    let absolutePath = try XCTUnwrap(Fixture.fixturePath(at: RelativePath("Incremental"),
                                                         for: "hello.swiftdeps"))
    try MockIncrementalCompilationSynchronizer.withInternedStringTable{ internedStringTable in
      let graph = try XCTUnwrap(
        try SourceFileDependencyGraph(
          contentsOf: TypedVirtualPath(file: VirtualPath.absolute(absolutePath).intern(), type: .swiftDeps),
          on: localFileSystem,
          internedStringTable: internedStringTable))
      XCTAssertEqual(graph.majorVersion, 1)
      XCTAssertEqual(graph.minorVersion, 0)
      XCTAssertEqual(graph.compilerVersionString, "Swift version 5.3-dev (LLVM 4510748e505acd4, Swift 9f07d884c97eaf4)")
      graph.verify()
      
      // Check that a node chosen at random appears as expected.
      var foundNode = false
      graph.forEachNode { node in
        if case let .member(context: context, name: name) = node.key.designator,
           node.sequenceNumber == 25
        {
          XCTAssertFalse(foundNode)
          foundNode = true
          XCTAssertEqual(node.key.aspect, .interface)
          XCTAssertEqual(context.lookup(in: internedStringTable), "5hello1BV")
          XCTAssertEqual(name.lookup(in: internedStringTable), "init")
          XCTAssertEqual(node.defsIDependUpon, [])
          XCTAssertEqual(node.definitionVsUse, .use)
        }
      }
      XCTAssertTrue(foundNode)
      
      // Check that an edge chosen at random appears as expected.
      var foundEdge = false
      graph.forEachArc { defNode, useNode in
        if defNode.sequenceNumber == 0 && useNode.sequenceNumber == 10 {
          switch (defNode.key.designator, useNode.key.designator) {
          case let (.sourceFileProvide(name: defName),
                    .potentialMember(context: useContext)):
            XCTAssertFalse(foundEdge)
            foundEdge = true
            
            XCTAssertEqual(defName.lookup(in: internedStringTable), "/Users/owenvoorhees/Desktop/hello.swiftdeps")
            XCTAssertEqual(defNode.fingerprint?.lookup(in: internedStringTable), "38b457b424090ac2e595be0e5f7e3b5b")
            
            XCTAssertEqual(useContext.lookup(in: internedStringTable), "5hello1AC")
            XCTAssertEqual(useNode.fingerprint?.lookup(in: internedStringTable), "b83bbc0b4b0432dbfabff6556a3a901f")
            
          default:
            XCTFail()
          }
        }
      }
      XCTAssertTrue(foundEdge)
    }
  }

  func testExtractSourceFileDependencyGraphFromSwiftModule() throws {
    let absolutePath = try XCTUnwrap(Fixture.fixturePath(at: RelativePath("Incremental"),
                                                         for: "hello.swiftmodule"))
    let data = try localFileSystem.readFileContents(absolutePath)
    try MockIncrementalCompilationSynchronizer.withInternedStringTable { internedStringTable in
      let graph = try XCTUnwrap(
        try SourceFileDependencyGraph(internedStringTable: internedStringTable,
                                      data: data,
                                      fromSwiftModule: true))
      XCTAssertEqual(graph.majorVersion, 1)
      XCTAssertEqual(graph.minorVersion, 0)
      XCTAssertEqual(graph.compilerVersionString, "Apple Swift version 5.3-dev (LLVM 240312aa7333e90, Swift 15bf0478ad7c47c)")
      graph.verify()
      
      // Check that a node chosen at random appears as expected.
      var foundNode = false
      graph.forEachNode { node in
        if case .nominal(context: "5hello3FooV".intern(in: internedStringTable)) = node.key.designator,
           node.sequenceNumber == 4
        {
          XCTAssertFalse(foundNode)
          foundNode = true
          XCTAssertEqual(node.key.aspect, .interface)
          XCTAssertEqual(node.defsIDependUpon, [0])
          XCTAssertEqual(node.definitionVsUse, .definition)
        }
      }
      XCTAssertTrue(foundNode)
    }
  }

  func testDateConversion() {
    let sn = TimePoint(seconds: 0, nanoseconds: 8000)
    XCTAssertEqual(sn.seconds, 0)
    XCTAssertEqual(sn.nanoseconds, 8000)
  }

  func testZeroDuration() {
    XCTAssertEqual(TimePoint.zero, TimePoint.seconds(0))
    XCTAssertEqual(TimePoint.zero, TimePoint.nanoseconds(0))
  }

  func testDurationSecondsArithmetic() {
    let x = TimePoint.seconds(1)
    XCTAssertEqual(TimePoint.zero + x, x)
    XCTAssertEqual(x + TimePoint.zero, x)
    XCTAssertEqual(x - TimePoint.zero, x)

    let y = TimePoint.nanoseconds(1)
    let z = TimePoint.nanoseconds(2_000_000)
    XCTAssertEqual(x + (y + z), (x + y) + z)
  }

  func testDurationComparison() {
    let x = TimePoint.seconds(1)
    let y = TimePoint.nanoseconds(500)

    XCTAssertEqual(x < y, !(x >= y))
  }

  func testDurationOverflow() {
    XCTAssertEqual(TimePoint.nanoseconds(1_000_000_000), TimePoint.seconds(1))
    XCTAssertEqual(TimePoint.nanoseconds(500_000_000) + TimePoint.nanoseconds(500_000_000), TimePoint.seconds(1))
    XCTAssertEqual(TimePoint.nanoseconds(1_500_000_000) + TimePoint.nanoseconds(500_000_000), TimePoint.seconds(2))
  }

  func testReadAndWriteBuildRecord() throws {
    let version = "Apple Swift version 5.1 (swiftlang-1100.0.270.13 clang-1100.0.33.7)"
    let options = "abbbfbcaf36b93e58efaadd8271ff142"
    let file2: AbsolutePath = AbsolutePath("/Volumes/AS/repos/swift-driver/sandbox/sandbox/sandbox/file2.swift")
    let main: AbsolutePath = AbsolutePath("/Volumes/AS/repos/swift-driver/sandbox/sandbox/sandbox/main.swift")
    let gazorp: AbsolutePath = AbsolutePath("/Volumes/gazorp.swift")
    let inputString =
      """
      version: "\(version)"
      options: "\(options)"
      build_start_time: [1570318779, 32357931]
      build_end_time: [1580318779, 33357858]
      inputs:
        "\(file2.nativePathString(escaped: true))": !dirty [1570318778, 0]
        "\(main.nativePathString(escaped: true))": [1570083660, 0]
        "\(gazorp.nativePathString(escaped: true))": !private [0, 0]

      """
    let buildRecord = try XCTUnwrap (BuildRecord(contents: inputString, failedToReadOutOfDateMap: {_ in}))
    XCTAssertEqual(buildRecord.swiftVersion, version)
    XCTAssertEqual(buildRecord.argsHash, options)
    XCTAssertEqual(buildRecord.inputInfos.count, 3)
    XCTAssertEqual(buildRecord.buildStartTime,
                   TimePoint(seconds: 1570318779, nanoseconds: 32357931))
    XCTAssertEqual(buildRecord.buildEndTime,
                   TimePoint(seconds: 1580318779, nanoseconds: 33357858))

    XCTAssertEqual(try buildRecord.inputInfos[VirtualPath(path: file2.pathString)]!.status,
                   .needsCascadingBuild)
    XCTAssertEqual(try XCTUnwrap(buildRecord.inputInfos[VirtualPath(path: file2.pathString)]).previousModTime,
                   TimePoint(seconds: 1570318778, nanoseconds: 0))
    XCTAssertEqual(try XCTUnwrap(buildRecord.inputInfos[VirtualPath(path: gazorp.pathString)]).status,
                   .needsNonCascadingBuild)
    XCTAssertEqual(try XCTUnwrap(buildRecord.inputInfos[VirtualPath(path: gazorp.pathString)]).previousModTime,
                   .zero)
    XCTAssertEqual(try XCTUnwrap(buildRecord.inputInfos[VirtualPath(path: main.pathString)]).status,
                   .upToDate)
    XCTAssertEqual(try XCTUnwrap(buildRecord.inputInfos[VirtualPath(path: main.pathString)]).previousModTime,
                   TimePoint(seconds: 1570083660, nanoseconds: 0))

    let outputString = try XCTUnwrap (buildRecord.encode(currentArgsHash: options,
                                                         diagnosticEngine: DiagnosticsEngine()))
    XCTAssertEqual(inputString, outputString)
  }
  /// The date conversions are not exact
  func isCloseEnough(_ a: [Int], _ b: [Int]) -> Bool {
    a[0] == b[0] && abs(a[1] - b[1]) <= 100
  }


  /// Run a test with two files, main and other.swift, passing on the additional arguments
  /// expecting certain diagnostics.
  private func runDriver(
    with otherArgs: [String],
    expecting expectations: [Diagnostic.Message],
    alsoExpectingWhenAutolinking autolinkExpectations: [Diagnostic.Message] = []
  ) throws {
    guard let sdkArguments = try Driver.sdkArgumentsForTesting() else {
      throw XCTSkip("cannot get sdk arguments on this platform")
    }
    try withTemporaryDirectory { path in
      let main = path.appending(component: "main.swift")
      try localFileSystem.writeFileContents(main) {
        $0 <<< "let foo = 1"
      }
      let other = path.appending(component: "other.swift")
      try localFileSystem.writeFileContents(other) {
        $0 <<< "let bar = 2"
      }
      try assertDriverDiagnostics(args: [
        "swiftc", "-module-name", "theModule", "-working-directory", path.pathString,
        main.pathString, other.pathString
      ] + otherArgs + sdkArguments) {driver, verifier in
        verifier.forbidUnexpected(.error, .warning, .note, .remark, .ignored)

        expectations.forEach {verifier.expect($0)}
        if driver.isAutolinkExtractJobNeeded {
          autolinkExpectations.forEach {verifier.expect($0)}
        }

        let jobs = try driver.planBuild()
        try driver.run(jobs: jobs)
      }
    }
  }

  func testShowJobLifecycleAndIncremental() throws {
    // Legacy MacOS driver output:
    //    Adding standard job to task queue: {compile: main.o <= main.swift}
    //    Added to TaskQueue: {compile: main.o <= main.swift}
    //    Adding standard job to task queue: {compile: other.o <= other.swift}
    //    Added to TaskQueue: {compile: other.o <= other.swift}
    //    Job finished: {compile: main.o <= main.swift}
    //    Job finished: {compile: other.o <= other.swift}

    try runDriver( with: [
      "-c",
      "-driver-show-job-lifecycle",
      "-driver-show-incremental",
    ],
    expecting: [
      .remark("Starting Compiling main.swift"),
      .remark("Finished Compiling main.swift"),
      .remark("Starting Compiling other.swift"),
      .remark("Finished Compiling other.swift"),
    ],
    alsoExpectingWhenAutolinking: [
      .remark("Starting Extracting autolink information for module theModule"),
      .remark("Finished Extracting autolink information for module theModule"),
    ])

  }
  func testNoIncremental() throws {
    try runDriver( with: [
      "-c",
      "-incremental",
    ],
    expecting: [
      .warning("ignoring -incremental (currently requires an output file map)")
    ])
    // Legacy driver output:
    //    <unknown>:0: warning: ignoring -incremental (currently requires an output file map)
  }
}
