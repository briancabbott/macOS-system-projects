// SwiftEvolve/SwiftEvolveTool.swift - swift-evolve driver
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
// -----------------------------------------------------------------------------
///
/// This file handles the top-level logic for swift-evolve.
///
// -----------------------------------------------------------------------------

import Foundation
import SwiftSyntax
import SwiftSyntaxParser
import TSCBasic

public class SwiftEvolveTool {
  public enum Step: Hashable {
    case parse(arguments: [String])
    case seed(options: Options)
    case plan(seed: UInt64, options: Options)
    case evolve(planFile: AbsolutePath, options: Options)
    case exit(code: Int32)
    
    public struct Options: Hashable {
      let command: String
      let files: [AbsolutePath]
      let rulesFile: AbsolutePath?
      let replace: Bool
      let verbose: Bool

      public init(command: String, files: [AbsolutePath], rulesFile: AbsolutePath?, replace: Bool, verbose: Bool) {
        self.command = command
        self.files = files
        self.rulesFile = rulesFile
        self.replace = replace
        self.verbose = verbose
      }
    }
  }
  
  var nextStep: Step
  fileprivate var parsedSourceFiles: [AbsolutePath: SourceFileSyntax] = [:]
  
  public init(arguments: [String]) {
    nextStep = .parse(arguments: arguments)
  }
}

extension SwiftEvolveTool.Step {
  var name: String {
    switch self {
    case .parse:
      return "Parsing arguments"
    case .seed:
      return "Seeding"
    case .plan:
      return "Planning"
    case .evolve:
      return "Evolving"
    case .exit:
      return "Exiting"
    }
  }
}

public extension SwiftEvolveTool.Step.Options {
  func setMinimumLogTypeToPrint() {
    LogType.minimumToPrint = verbose ? .debug : .info
  }
}

extension SwiftEvolveTool {
  public func run() -> Never {
    while true {
      log(type: .info, "\(nextStep.name): \(nextStep)")

      do {
        switch nextStep {
        case let .parse(arguments: arguments):
          nextStep = try Step(arguments: arguments)
          
        case let .seed(options: options):
          nextStep = .plan(seed: makeSeed(), options: options)
        
        case let .plan(seed: seed, options: options):
          nextStep = .evolve(planFile: try plan(withSeed: seed, options: options), options: options)
        
        case let .evolve(planFile: planFile, options: options):
          try evolve(withPlanFile: planFile, options: options)
          nextStep = .exit(code: 0)
        
        case .exit(code: let code):
          exit(code)
        }
      }
      catch {
        logError(error)
        nextStep = .exit(code: 1)
      }
    }
  }
  
  func makeSeed() -> UInt64 {
    return LinearCongruentialGenerator.makeSeed()
  }

  func plan(withSeed seed: UInt64, options: Step.Options) throws -> AbsolutePath {
    let planner = Planner(
      rng: LinearCongruentialGenerator(seed: seed),
      rules: try readRules(options: options),
      includeLineAndColumn: options.verbose
    )

    for file in options.files {
      let parsed = try parsedSource(at: file)
      try planner.planEvolution(in: parsed, at: URL(file))
    }

    let planFile = localFileSystem.currentWorkingDirectory!.appending(component: "evolution.json")
    
    let jsonEncoder = JSONEncoder()
    if #available(macOS 10.13, *) {
      jsonEncoder.outputFormatting = [.prettyPrinted, .sortedKeys]
    }
    let data = try jsonEncoder.encode(planner.plan)

    try data.write(to: URL(planFile))

    return planFile
  }

  func evolve(withPlanFile planFile: AbsolutePath, options: Step.Options) throws {
    let data = try Data(contentsOf: URL(planFile))
    let plan = try JSONDecoder().decode([PlannedEvolution].self, from: data)

    let evolver = Evolver(plan: plan)

    for file in options.files {
      let parsed = try parsedSource(at: file)
      let evolved = evolver.evolve(in: parsed, at: URL(file))

      if options.replace {
        try withTemporaryFile(dir: file.parentDirectory, prefix: "",
                          suffix: file.basename,
                          deleteOnClose: true) { tempFile in
          tempFile.fileHandle.write(evolved.description)
          
          _ = try withErrorContext(
            url: URL(file),
            debugDescription: "replaceItemAt(file, withItemAt: tempFile, ...)"
          ) {
            try FileManager.default.replaceItemAt(
              URL(file), withItemAt: URL(tempFile.path),
              backupItemName: file.basename + "~",
              options: .withoutDeletingBackupItem
            )
          }
        }
      }
      else {
        print(evolved, terminator: "")
      }
    }
  }
}

extension SwiftEvolveTool {
  fileprivate func readRules(options: Step.Options) throws -> EvolutionRules {
    guard let path = options.rulesFile else {
      return EvolutionRules()
    }
    let jsonData = try Data(contentsOf: URL(path))
    return try JSONDecoder().decode(EvolutionRules.self, from: jsonData)
  }
  
  fileprivate func parsedSource(at path: AbsolutePath) throws -> SourceFileSyntax {
    if let preparsed = parsedSourceFiles[path] {
      return preparsed
    }

    let parsed = try SyntaxParser.parse(URL(path))
    parsedSourceFiles[path] = parsed
    return parsed
  }
}

extension URL {
  init(_ path: AbsolutePath) {
    self.init(fileURLWithPath: path.pathString)
  }
}

fileprivate func logError(_ error: Error) {
  log(type: .error, error)
  
  let e = error as NSError
  log(type: .debug, "Localized Description:", e.localizedDescription)
  log(type: .debug, "Debug Description:", e.debugDescription)
  log(type: .debug, "User Info:", e.userInfo)
  
  if let le = error as? LocalizedError {
    if let fr = le.failureReason { log(type: .debug, "Failure Reason:", fr) }
    if let rs = le.recoverySuggestion { log(type: .debug, "Recovery Suggestion:", rs) }
  }
  
  if let e = error as? CocoaError, let u = e.underlying {
    log(type: .error, "Underlying Error:")
    logError(u)
  }
}
