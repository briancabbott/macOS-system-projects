//===--------------- ModuleDependencyScanning.swift -----------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

import TSCBasic // <<<
import protocol TSCBasic.FileSystem
import struct TSCBasic.AbsolutePath
import struct TSCBasic.Diagnostic
import var TSCBasic.localFileSystem
import var TSCBasic.stdoutStream

import SwiftOptions
import struct Foundation.Data
import class Foundation.JSONEncoder
import class Foundation.JSONDecoder
import var Foundation.EXIT_SUCCESS

extension Diagnostic.Message {
  static func warn_scanner_frontend_fallback() -> Diagnostic.Message {
    .warning("Fallback to `swift-frontend` dependency scanner invocation")
  }
  static func scanner_diagnostic_error(_ message: String) -> Diagnostic.Message {
    .error("Dependency scanning failure: \(message)")
  }
  static func scanner_diagnostic_warn(_ message: String) -> Diagnostic.Message {
    .warning(message)
  }
  static func scanner_diagnostic_note(_ message: String) -> Diagnostic.Message {
    .note(message)
  }
  static func scanner_diagnostic_remark(_ message: String) -> Diagnostic.Message {
    .remark(message)
  }
}

public extension Driver {
  /// Precompute the dependencies for a given Swift compilation, producing a
  /// dependency graph including all Swift and C module files and
  /// source files.
  mutating func dependencyScanningJob() throws -> Job {
    let (inputs, commandLine) = try dependencyScannerInvocationCommand()

    // Construct the scanning job.
    return Job(moduleName: moduleOutputInfo.name,
               kind: .scanDependencies,
               tool: try toolchain.resolvedTool(.swiftCompiler),
               commandLine: commandLine,
               displayInputs: inputs,
               inputs: inputs,
               primaryInputs: [],
               outputs: [TypedVirtualPath(file: .standardOutput, type: .jsonDependencies)])
  }

  /// Generate a full command-line invocation to be used for the dependency scanning action
  /// on the target module.
  @_spi(Testing) mutating func dependencyScannerInvocationCommand()
  throws -> ([TypedVirtualPath],[Job.ArgTemplate]) {
    // Aggregate the fast dependency scanner arguments
    var inputs: [TypedVirtualPath] = []
    var commandLine: [Job.ArgTemplate] = swiftCompilerPrefixArgs.map { Job.ArgTemplate.flag($0) }
    commandLine.appendFlag("-frontend")
    commandLine.appendFlag("-scan-dependencies")
    try addCommonFrontendOptions(commandLine: &commandLine, inputs: &inputs,
                                 bridgingHeaderHandling: .ignored,
                                 moduleDependencyGraphUse: .dependencyScan)
    // FIXME: MSVC runtime flags

    // Pass in external target dependencies to be treated as placeholder dependencies by the scanner
    if let externalTargetPaths = externalTargetModuleDetailsMap?.mapValues({ $0.path }) {
      let dependencyPlaceholderMapFile =
      try serializeExternalDependencyArtifacts(externalTargetPaths: externalTargetPaths)
      commandLine.appendFlag("-placeholder-dependency-module-map-file")
      commandLine.appendPath(dependencyPlaceholderMapFile)
    }

    // Pass on the input files
    commandLine.append(contentsOf: inputFiles.map { .path($0.file) })
    return (inputs, commandLine)
  }

  /// Serialize a map of placeholder (external) dependencies for the dependency scanner.
   private func serializeExternalDependencyArtifacts(externalTargetPaths: ExternalTargetModulePathMap)
   throws -> VirtualPath {
     var placeholderArtifacts: [SwiftModuleArtifactInfo] = []
     // Explicit external targets
     for (moduleId, binaryModulePath) in externalTargetPaths {
       let modPath = TextualVirtualPath(path: VirtualPath.absolute(binaryModulePath).intern())
       placeholderArtifacts.append(
           SwiftModuleArtifactInfo(name: moduleId.moduleName,
                                   modulePath: modPath))
     }
     let encoder = JSONEncoder()
     encoder.outputFormatting = [.prettyPrinted]
     let contents = try encoder.encode(placeholderArtifacts)
     return VirtualPath.createUniqueTemporaryFileWithKnownContents(.init("\(moduleOutputInfo.name)-external-modules.json"),
                                                                   contents)
   }

  /// Returns false if the lib is available and ready to use
  private func initSwiftScanLib() throws -> Bool {
    // If `-nonlib-dependency-scanner` was specified or the libSwiftScan library cannot be found,
    // attempt to fallback to using `swift-frontend -scan-dependencies` invocations for dependency
    // scanning.
    var fallbackToFrontend = parsedOptions.hasArgument(.driverScanDependenciesNonLib)
    let scanLibPath = try Self.getScanLibPath(of: toolchain, hostTriple: hostTriple, env: env)
    if try interModuleDependencyOracle
        .verifyOrCreateScannerInstance(fileSystem: fileSystem,
                                       swiftScanLibPath: scanLibPath) == false {
      fallbackToFrontend = true
      // This warning is mostly useful for debugging the driver, so let's hide it
      // when libSwiftDriver is used, instead of a swift-driver executable.
      if !integratedDriver {
        diagnosticEngine.emit(.warn_scanner_frontend_fallback())
      }
    }
    return fallbackToFrontend
  }

  private func sanitizeCommandForLibScanInvocation(_ command: inout [String]) {
    // Remove the tool executable to only leave the arguments. When passing the
    // command line into libSwiftScan, the library is itself the tool and only
    // needs to parse the remaining arguments.
    command.removeFirst()
    // We generate full swiftc -frontend -scan-dependencies invocations in order to also be
    // able to launch them as standalone jobs. Frontend's argument parser won't recognize
    // -frontend when passed directly.
    if command.first == "-frontend" {
      command.removeFirst()
    }
  }

  mutating func performImportPrescan() throws -> InterModuleDependencyImports {
    let preScanJob = try importPreScanningJob()
    let forceResponseFiles = parsedOptions.hasArgument(.driverForceResponseFiles)
    let imports: InterModuleDependencyImports

    let isSwiftScanLibAvailable = !(try initSwiftScanLib())
    if isSwiftScanLibAvailable {
      let cwd = workingDirectory ?? fileSystem.currentWorkingDirectory!
      var command = try itemizedJobCommand(of: preScanJob,
                                           useResponseFiles: .disabled,
                                           using: executor.resolver)
      sanitizeCommandForLibScanInvocation(&command)
      imports =
        try interModuleDependencyOracle.getImports(workingDirectory: cwd,
                                                   moduleAliases: moduleOutputInfo.aliases,
                                                   commandLine: command)

    } else {
      // Fallback to legacy invocation of the dependency scanner with
      // `swift-frontend -scan-dependencies -import-prescan`
      imports =
        try self.executor.execute(job: preScanJob,
                                  capturingJSONOutputAs: InterModuleDependencyImports.self,
                                  forceResponseFiles: forceResponseFiles,
                                  recordedInputModificationDates: recordedInputModificationDates)
    }
    return imports
  }

  mutating internal func performDependencyScan() throws -> InterModuleDependencyGraph {
    let scannerJob = try dependencyScanningJob()
    let forceResponseFiles = parsedOptions.hasArgument(.driverForceResponseFiles)
    let dependencyGraph: InterModuleDependencyGraph
    
    if parsedOptions.contains(.v) {
      let arguments: [String] = try executor.resolver.resolveArgumentList(for: scannerJob,
                                                                          useResponseFiles: .disabled)
      stdoutStream <<< arguments.map { $0.spm_shellEscaped() }.joined(separator: " ") <<< "\n"
      stdoutStream.flush()
    }

    let isSwiftScanLibAvailable = !(try initSwiftScanLib())
    if isSwiftScanLibAvailable {
      let cwd = workingDirectory ?? fileSystem.currentWorkingDirectory!
      var command = try itemizedJobCommand(of: scannerJob,
                                           useResponseFiles: .disabled,
                                           using: executor.resolver)
      sanitizeCommandForLibScanInvocation(&command)
      dependencyGraph =
        try interModuleDependencyOracle.getDependencies(workingDirectory: cwd,
                                                        moduleAliases: moduleOutputInfo.aliases,
                                                        commandLine: command)
      let possibleDiags = try interModuleDependencyOracle.getScannerDiagnostics()
      if let diags = possibleDiags {
        for diagnostic in diags {
          switch diagnostic.severity {
          case .error:
            diagnosticEngine.emit(.scanner_diagnostic_error(diagnostic.message))
          case .warning:
            diagnosticEngine.emit(.scanner_diagnostic_warn(diagnostic.message))
          case .note:
            diagnosticEngine.emit(.scanner_diagnostic_note(diagnostic.message))
          case .remark:
            diagnosticEngine.emit(.scanner_diagnostic_remark(diagnostic.message))
          case .ignored:
            diagnosticEngine.emit(.scanner_diagnostic_error(diagnostic.message))
          }
        }
      }
    } else {
      // Fallback to legacy invocation of the dependency scanner with
      // `swift-frontend -scan-dependencies`
      dependencyGraph =
        try self.executor.execute(job: scannerJob,
                                  capturingJSONOutputAs: InterModuleDependencyGraph.self,
                                  forceResponseFiles: forceResponseFiles,
                                  recordedInputModificationDates: recordedInputModificationDates)
    }
    return dependencyGraph
  }

  mutating internal func performBatchDependencyScan(moduleInfos: [BatchScanModuleInfo])
  throws -> [ModuleDependencyId: [InterModuleDependencyGraph]] {
    let batchScanningJob = try batchDependencyScanningJob(for: moduleInfos)
    let forceResponseFiles = parsedOptions.hasArgument(.driverForceResponseFiles)
    let moduleVersionedGraphMap: [ModuleDependencyId: [InterModuleDependencyGraph]]

    let isSwiftScanLibAvailable = !(try initSwiftScanLib())
    if isSwiftScanLibAvailable {
      let cwd = workingDirectory ?? fileSystem.currentWorkingDirectory!
      var command = try itemizedJobCommand(of: batchScanningJob,
                                           useResponseFiles: .disabled,
                                           using: executor.resolver)
      sanitizeCommandForLibScanInvocation(&command)
      moduleVersionedGraphMap =
        try interModuleDependencyOracle.getBatchDependencies(workingDirectory: cwd,
                                                             moduleAliases: moduleOutputInfo.aliases,
                                                             commandLine: command,
                                                             batchInfos: moduleInfos)
    } else {
      // Fallback to legacy invocation of the dependency scanner with
      // `swift-frontend -scan-dependencies`
      moduleVersionedGraphMap = try executeLegacyBatchScan(moduleInfos: moduleInfos,
                                                           batchScanningJob: batchScanningJob,
                                                           forceResponseFiles: forceResponseFiles)
    }
    return moduleVersionedGraphMap
  }

  // Perform a batch scan by invoking the command-line dependency scanner and decoding the resulting
  // JSON.
  fileprivate func executeLegacyBatchScan(moduleInfos: [BatchScanModuleInfo],
                                          batchScanningJob: Job,
                                          forceResponseFiles: Bool)
  throws -> [ModuleDependencyId: [InterModuleDependencyGraph]] {
    let batchScanResult =
      try self.executor.execute(job: batchScanningJob,
                                forceResponseFiles: forceResponseFiles,
                                recordedInputModificationDates: recordedInputModificationDates)
    let success = batchScanResult.exitStatus == .terminated(code: EXIT_SUCCESS)
    guard success else {
      throw JobExecutionError.jobFailedWithNonzeroExitCode(
        type(of: executor).computeReturnCode(exitStatus: batchScanResult.exitStatus),
        try batchScanResult.utf8stderrOutput())
    }
    // Decode the resulting dependency graphs and build a dictionary from a moduleId to
    // a set of dependency graphs that were built for it
    let moduleVersionedGraphMap =
      try moduleInfos.reduce(into: [ModuleDependencyId: [InterModuleDependencyGraph]]()) {
        let moduleId: ModuleDependencyId
        let dependencyGraphPath: VirtualPath
        switch $1 {
          case .swift(let swiftModuleBatchScanInfo):
            moduleId = .swift(swiftModuleBatchScanInfo.swiftModuleName)
            dependencyGraphPath = try VirtualPath(path: swiftModuleBatchScanInfo.output)
          case .clang(let clangModuleBatchScanInfo):
            moduleId = .clang(clangModuleBatchScanInfo.clangModuleName)
            dependencyGraphPath = try VirtualPath(path: clangModuleBatchScanInfo.output)
        }
        let contents = try fileSystem.readFileContents(dependencyGraphPath)
        let decodedGraph = try JSONDecoder().decode(InterModuleDependencyGraph.self,
                                                    from: Data(contents.contents))
        if $0[moduleId] != nil {
          $0[moduleId]!.append(decodedGraph)
        } else {
          $0[moduleId] = [decodedGraph]
        }
      }
    return moduleVersionedGraphMap
  }

  /// Precompute the set of module names as imported by the current module
  mutating private func importPreScanningJob() throws -> Job {
    // Aggregate the fast dependency scanner arguments
    var inputs: [TypedVirtualPath] = []
    var commandLine: [Job.ArgTemplate] = swiftCompilerPrefixArgs.map { Job.ArgTemplate.flag($0) }
    commandLine.appendFlag("-frontend")
    commandLine.appendFlag("-scan-dependencies")
    commandLine.appendFlag("-import-prescan")
    try addCommonFrontendOptions(commandLine: &commandLine, inputs: &inputs,
                                 bridgingHeaderHandling: .ignored,
                                 moduleDependencyGraphUse: .dependencyScan)
    // FIXME: MSVC runtime flags

    // Pass on the input files
    commandLine.append(contentsOf: inputFiles.map { .path($0.file) })

    // Construct the scanning job.
    return Job(moduleName: moduleOutputInfo.name,
               kind: .scanDependencies,
               tool: try toolchain.resolvedTool(.swiftCompiler),
               commandLine: commandLine,
               displayInputs: inputs,
               inputs: inputs,
               primaryInputs: [],
               outputs: [TypedVirtualPath(file: .standardOutput, type: .jsonDependencies)])
  }

  /// Precompute the dependencies for a given collection of modules using swift frontend's batch scanning mode
  mutating private func batchDependencyScanningJob(for moduleInfos: [BatchScanModuleInfo]) throws -> Job {
    var inputs: [TypedVirtualPath] = []

    // Aggregate the fast dependency scanner arguments
    var commandLine: [Job.ArgTemplate] = swiftCompilerPrefixArgs.map { Job.ArgTemplate.flag($0) }

    // The dependency scanner automatically operates in batch mode if -batch-scan-input-file
    // is present.
    commandLine.appendFlag("-frontend")
    commandLine.appendFlag("-scan-dependencies")
    try addCommonFrontendOptions(commandLine: &commandLine, inputs: &inputs,
                                 bridgingHeaderHandling: .ignored,
                                 moduleDependencyGraphUse: .dependencyScan)

    let batchScanInputFilePath = try serializeBatchScanningModuleArtifacts(moduleInfos: moduleInfos)
    commandLine.appendFlag("-batch-scan-input-file")
    commandLine.appendPath(batchScanInputFilePath)

    // This action does not require any input files, but all frontend actions require
    // at least one input so pick any input of the current compilation.
    let inputFile = inputFiles.first { $0.type == .swift }
    commandLine.appendPath(inputFile!.file)
    inputs.append(inputFile!)

    // This job's outputs are defined as a set of dependency graph json files
    let outputs: [TypedVirtualPath] = try moduleInfos.map {
      switch $0 {
        case .swift(let swiftModuleBatchScanInfo):
          return TypedVirtualPath(file: try VirtualPath.intern(path: swiftModuleBatchScanInfo.output),
                                  type: .jsonDependencies)
        case .clang(let clangModuleBatchScanInfo):
          return TypedVirtualPath(file: try VirtualPath.intern(path: clangModuleBatchScanInfo.output),
                                  type: .jsonDependencies)
      }
    }

    // Construct the scanning job.
    return Job(moduleName: moduleOutputInfo.name,
               kind: .scanDependencies,
               tool: try toolchain.resolvedTool(.swiftCompiler),
               commandLine: commandLine,
               displayInputs: inputs,
               inputs: inputs,
               primaryInputs: [],
               outputs: outputs)
  }

  /// Serialize a collection of modules into an input format expected by the batch module dependency scanner.
  func serializeBatchScanningModuleArtifacts(moduleInfos: [BatchScanModuleInfo])
  throws -> VirtualPath {
    let encoder = JSONEncoder()
    encoder.outputFormatting = [.prettyPrinted]
    let contents = try encoder.encode(moduleInfos)
    return VirtualPath.createUniqueTemporaryFileWithKnownContents(.init("\(moduleOutputInfo.name)-batch-module-scan.json"),
                                                                  contents)
  }

  fileprivate func itemizedJobCommand(of job: Job, useResponseFiles: ResponseFileHandling,
                                      using resolver: ArgsResolver) throws -> [String] {
    let (args, _) = try resolver.resolveArgumentList(for: job,
                                                     useResponseFiles: useResponseFiles)
    return args
  }
}

@_spi(Testing) public extension Driver {
  static func getScanLibPath(of toolchain: Toolchain, hostTriple: Triple,
                             env: [String: String]) throws -> AbsolutePath {
    if hostTriple.isWindows {
      // no matter if we are in a build tree or an installed tree, the layout is
      // always: `bin/_InternalSwiftScan.dll`
      return try getRootPath(of: toolchain, env: env)
                    .appending(component: "bin")
                    .appending(component: "_InternalSwiftScan.dll")
    }

    let sharedLibExt: String
    if hostTriple.isMacOSX {
      sharedLibExt = ".dylib"
    } else {
      sharedLibExt = ".so"
    }
    let libScanner = "lib_InternalSwiftScan\(sharedLibExt)"
    // We first look into position in toolchain
    let libPath
     = try getRootPath(of: toolchain, env: env).appending(component: "lib")
      .appending(component: "swift")
      .appending(component: hostTriple.osNameUnversioned)
      .appending(component: libScanner)
    if localFileSystem.exists(libPath) {
        return libPath
    }
    // In case we are using a compiler from the build dir, we should also try
    // this path.
    return try getRootPath(of: toolchain, env: env).appending(component: "lib")
      .appending(component: libScanner)
  }

  static func getRootPath(of toolchain: Toolchain, env: [String: String])
  throws -> AbsolutePath {
    if let overrideString = env["SWIFT_DRIVER_SWIFT_SCAN_TOOLCHAIN_PATH"] {
      return try AbsolutePath(validating: overrideString)
    }
    return try toolchain.getToolPath(.swiftCompiler)
      .parentDirectory // bin
      .parentDirectory // toolchain root
  }
}
