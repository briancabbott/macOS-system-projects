/*
 This source file is part of the Swift.org open source project
 
 Copyright (c) 2014 - 2017 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception
 
 See http://swift.org/LICENSE.txt for license information
 See http://swift.org/CONTRIBUTORS.txt for Swift project authors 
*/

import TSCBasic
import Foundation

// deprecated 12/21, moved to SwiftPM
@available(*, deprecated, message: "moved into SwiftPM")
public enum PkgConfigError: Swift.Error, CustomStringConvertible {
    case couldNotFindConfigFile(name: String)
    case parsingError(String)
    case prohibitedFlags(String)

    public var description: String {
        switch self {
        case .couldNotFindConfigFile(let name):
            return "couldn't find pc file for \(name)"
        case .parsingError(let error):
            return "parsing error(s): \(error)"
        case .prohibitedFlags(let flags):
            return "prohibited flag(s): \(flags)"
        }
    }
}

// deprecated 12/21, moved to SwiftPM
@available(*, deprecated, message: "moved into SwiftPM")
public struct PCFileFinder {
    /// DiagnosticsEngine to emit warnings
    let diagnostics: DiagnosticsEngine

    /// Cached results of locations `pkg-config` will search for `.pc` files
    /// FIXME: This shouldn't use a static variable, since the first lookup
    /// will cache the result of whatever `brewPrefix` was passed in.  It is
    /// also not threadsafe.
    public private(set) static var pkgConfigPaths: [AbsolutePath]? // FIXME: @testable(internal)
    private static var shouldEmitPkgConfigPathsDiagnostic = false

    /// The built-in search path list.
    ///
    /// By default, this is combined with the search paths inferred from
    /// `pkg-config` itself.
    static let searchPaths = [
        AbsolutePath("/usr/local/lib/pkgconfig"),
        AbsolutePath("/usr/local/share/pkgconfig"),
        AbsolutePath("/usr/lib/pkgconfig"),
        AbsolutePath("/usr/share/pkgconfig"),
    ]

    /// Get search paths from `pkg-config` itself to locate `.pc` files.
    ///
    /// This is needed because on Linux machines, the search paths can be different
    /// from the standard locations that we are currently searching.
    public init(diagnostics: DiagnosticsEngine, brewPrefix: AbsolutePath?) {
        self.diagnostics = diagnostics
        if PCFileFinder.pkgConfigPaths == nil {
            do {
                let pkgConfigPath: String
                if let brewPrefix = brewPrefix {
                    pkgConfigPath = brewPrefix.appending(components: "bin", "pkg-config").pathString
                } else {
                    pkgConfigPath = "pkg-config"
                }
                let searchPaths = try Process.checkNonZeroExit(
                args: pkgConfigPath, "--variable", "pc_path", "pkg-config").spm_chomp()
                PCFileFinder.pkgConfigPaths = searchPaths.split(separator: ":").map({ AbsolutePath(String($0)) })
            } catch {
                PCFileFinder.shouldEmitPkgConfigPathsDiagnostic = true
                PCFileFinder.pkgConfigPaths = []
            }
        }
    }
    
    /// Reset the cached `pkgConfigPaths` property, so that it will be evaluated
    /// again when instantiating a `PCFileFinder()`.  This is intended only for
    /// use by testing.  This is a temporary workaround for the use of a static
    /// variable by this class.
    internal static func resetCachedPkgConfigPaths() {
        PCFileFinder.pkgConfigPaths = nil
    }

    public func locatePCFile(
        name: String,
        customSearchPaths: [AbsolutePath],
        fileSystem: FileSystem
    ) throws -> AbsolutePath {
        // FIXME: We should consider building a registry for all items in the
        // search paths, which is likely to be substantially more efficient if
        // we end up searching for a reasonably sized number of packages.
        for path in OrderedSet(customSearchPaths + PCFileFinder.pkgConfigPaths! + PCFileFinder.searchPaths) {
            let pcFile = path.appending(component: name + ".pc")
            if fileSystem.isFile(pcFile) {
                return pcFile
            }
        }
        if PCFileFinder.shouldEmitPkgConfigPathsDiagnostic {
            PCFileFinder.shouldEmitPkgConfigPathsDiagnostic = false
            diagnostics.emit(warning: "failed to retrieve search paths with pkg-config; maybe pkg-config is not installed")
        }
        throw PkgConfigError.couldNotFindConfigFile(name: name)
    }
}

/// Informations to track circular dependencies and other PkgConfig issues
// deprecated 12/21, moved to SwiftPM
@available(*, deprecated, message: "moved into SwiftPM")
public class LoadingContext {
    public init() {
        pkgConfigStack = [String]()
    }

    public var pkgConfigStack: [String]
}

/// Information on an individual `pkg-config` supported package.
/// // deprecated 12/21, moved to SwiftPM
@available(*, deprecated, message: "moved into SwiftPM")
public struct PkgConfig {
    /// The name of the package.
    public let name: String

    /// The path to the definition file.
    public let pcFile: AbsolutePath

    /// The list of C compiler flags in the definition.
    public let cFlags: [String]

    /// The list of libraries to link.
    public let libs: [String]

    /// DiagnosticsEngine to emit diagnostics
    let diagnostics: DiagnosticsEngine

    /// Load the information for the named package.
    ///
    /// It will search `fileSystem` for the pkg config file in the following order:
    /// * Paths defined in `PKG_CONFIG_PATH` environment variable
    /// * Paths defined in `additionalSearchPaths` argument
    /// * Built-in search paths (see `PCFileFinder.searchPaths`)
    ///
    /// - parameter name: Name of the pkg config file (without file extension).
    /// - parameter additionalSearchPaths: Additional paths to search for pkg config file.
    /// - parameter fileSystem: The file system to use, defaults to local file system.
    ///
    /// - throws: PkgConfigError
    public init(
        name: String,
        additionalSearchPaths: [AbsolutePath] = [],
        diagnostics: DiagnosticsEngine,
        fileSystem: FileSystem = localFileSystem,
        brewPrefix: AbsolutePath?,
        loadingContext: LoadingContext = LoadingContext()
    ) throws {
        loadingContext.pkgConfigStack.append(name)

        if let path = try? AbsolutePath(validating: name) {
            guard fileSystem.isFile(path) else { throw PkgConfigError.couldNotFindConfigFile(name: name) }
            self.name = path.basenameWithoutExt
            self.pcFile = path
        } else {
            self.name = name
            let pkgFileFinder = PCFileFinder(diagnostics: diagnostics, brewPrefix: brewPrefix)
            self.pcFile = try pkgFileFinder.locatePCFile(
                name: name,
                customSearchPaths: PkgConfig.envSearchPaths + additionalSearchPaths,
                fileSystem: fileSystem)
        }

        self.diagnostics = diagnostics
        var parser = PkgConfigParser(pcFile: pcFile, fileSystem: fileSystem)
        try parser.parse()

        func getFlags(from dependencies: [String]) throws -> (cFlags: [String], libs: [String]) {
            var cFlags = [String]()
            var libs = [String]()
            
            for dep in dependencies {
                if let index = loadingContext.pkgConfigStack.firstIndex(of: dep) {
                    diagnostics.emit(warning: "circular dependency detected while parsing \(loadingContext.pkgConfigStack[0]): \(loadingContext.pkgConfigStack[index..<loadingContext.pkgConfigStack.count].joined(separator: " -> ")) -> \(dep)")
                    continue
                }

                // FIXME: This is wasteful, we should be caching the PkgConfig result.
                let pkg = try PkgConfig(
                    name: dep,
                    additionalSearchPaths: additionalSearchPaths,
                    diagnostics: diagnostics,
                    fileSystem: fileSystem,
                    brewPrefix: brewPrefix,
                    loadingContext: loadingContext
                )

                cFlags += pkg.cFlags
                libs += pkg.libs
            }

            return (cFlags: cFlags, libs: libs)
        }

        let dependencyFlags = try getFlags(from: parser.dependencies)
        let privateDependencyFlags = try getFlags(from: parser.privateDependencies)

        self.cFlags = parser.cFlags + dependencyFlags.cFlags + privateDependencyFlags.cFlags
        self.libs = parser.libs + dependencyFlags.libs

        loadingContext.pkgConfigStack.removeLast();
    }

    private static var envSearchPaths: [AbsolutePath] {
        if let configPath = ProcessEnv.vars["PKG_CONFIG_PATH"] {
            return configPath.split(separator: ":").map({ AbsolutePath(String($0)) })
        }
        return []
    }
}

// FIXME: This is only internal so it can be unit tested.
//
/// Parser for the `pkg-config` `.pc` file format.
///
/// See: https://www.freedesktop.org/wiki/Software/pkg-config/
// deprecated 12/21, moved to SwiftPM
@available(*, deprecated, message: "moved into SwiftPM")
public struct PkgConfigParser {
    public let pcFile: AbsolutePath
    private let fileSystem: FileSystem
    public private(set) var variables = [String: String]()
    public private(set) var dependencies = [String]()
    public private(set) var privateDependencies = [String]()
    public private(set) var cFlags = [String]()
    public private(set) var libs = [String]()

    public init(pcFile: AbsolutePath, fileSystem: FileSystem) {
        precondition(fileSystem.isFile(pcFile))
        self.pcFile = pcFile
        self.fileSystem = fileSystem
    }

    public mutating func parse() throws {
        func removeComment(line: String) -> String {
            if let commentIndex = line.firstIndex(of: "#") {
                return String(line[line.startIndex..<commentIndex])
            }
            return line
        }

        // Add pcfiledir variable. This is the path of the directory containing this pc file.
        variables["pcfiledir"] = pcFile.parentDirectory.pathString

        // Add pc_sysrootdir variable. This is the path of the sysroot directory for pc files.
        variables["pc_sysrootdir"] = ProcessEnv.vars["PKG_CONFIG_SYSROOT_DIR"] ?? "/"

        let fileContents = try fileSystem.readFileContents(pcFile)
        // FIXME: Should we error out instead if content is not UTF8 representable?
        for line in fileContents.validDescription?.components(separatedBy: "\n") ?? [] {
            // Remove commented or any trailing comment from the line.
            let uncommentedLine = removeComment(line: line)
            // Ignore any empty or whitespace line.
            guard let line = uncommentedLine.spm_chuzzle() else { continue }

            if line.contains(":") {
                // Found a key-value pair.
                try parseKeyValue(line: line)
            } else if line.contains("=") {
                // Found a variable.
                let (name, maybeValue) = line.spm_split(around: "=")
                let value = maybeValue?.spm_chuzzle() ?? ""
                variables[name.spm_chuzzle() ?? ""] = try resolveVariables(value)
            } else {
                // Unexpected thing in the pc file, abort.
                throw PkgConfigError.parsingError("Unexpected line: \(line) in \(pcFile)")
            }
        }
    }

    private mutating func parseKeyValue(line: String) throws {
        precondition(line.contains(":"))
        let (key, maybeValue) = line.spm_split(around: ":")
        let value = try resolveVariables(maybeValue?.spm_chuzzle() ?? "")
        switch key {
        case "Requires":
            dependencies = try parseDependencies(value)
        case "Requires.private":
            privateDependencies = try parseDependencies(value)
        case "Libs":
            libs = try splitEscapingSpace(value)
        case "Cflags":
            cFlags = try splitEscapingSpace(value)
        default:
            break
        }
    }

    /// Parses `Requires: ` string into array of dependencies.
    ///
    /// The dependency string has seperator which can be (multiple) space or a
    /// comma.  Additionally each there can be an optional version constaint to
    /// a dependency.
    private func parseDependencies(_ depString: String) throws -> [String] {
        let operators = ["=", "<", ">", "<=", ">="]
        let separators = [" ", ","]

        // Look at a char at an index if present.
        func peek(idx: Int) -> Character? {
            guard idx <= depString.count - 1 else { return nil }
            return depString[depString.index(depString.startIndex, offsetBy: idx)]
        }

        // This converts the string which can be separated by comma or spaces
        // into an array of string.
        func tokenize() -> [String] {
            var tokens = [String]()
            var token = ""
            for (idx, char) in depString.enumerated() {
                // Encountered a seperator, use the token.
                if separators.contains(String(char)) {
                    // If next character is a space skip.
                    if let peeked = peek(idx: idx+1), peeked == " " { continue }
                    // Append to array of tokens and reset token variable.
                    tokens.append(token)
                    token = ""
                } else {
                    token += String(char)
                }
            }
            // Append the last collected token if present.
            if !token.isEmpty { tokens += [token] }
            return tokens
        }

        var deps = [String]()
        var it = tokenize().makeIterator()
        while let arg = it.next() {
            // If we encounter an operator then we need to skip the next token.
            if operators.contains(arg) {
                // We should have a version number next, skip.
                guard it.next() != nil else {
                    throw PkgConfigError.parsingError("""
                        Expected version number after \(deps.last.debugDescription) \(arg) in \"\(depString)\" in \
                        \(pcFile)
                        """)
                }
            } else {
                // Otherwise it is a dependency.
                deps.append(arg)
            }
        }
        return deps
    }

    /// Perform variable expansion on the line by processing the each fragment
    /// of the string until complete.
    ///
    /// Variables occur in form of ${variableName}, we search for a variable
    /// linearly in the string and if found, lookup the value of the variable in
    /// our dictionary and replace the variable name with its value.
    private func resolveVariables(_ line: String) throws -> String {
        // Returns variable name, start index and end index of a variable in a string if present.
        // We make sure it of form ${name} otherwise it is not a variable.
        func findVariable(_ fragment: String)
            -> (name: String, startIndex: String.Index, endIndex: String.Index)? {
            guard let dollar = fragment.firstIndex(of: "$"),
                  dollar != fragment.endIndex && fragment[fragment.index(after: dollar)] == "{",
                  let variableEndIndex = fragment.firstIndex(of: "}")
            else { return nil }
            return (String(fragment[fragment.index(dollar, offsetBy: 2)..<variableEndIndex]), dollar, variableEndIndex)
        }

        var result = ""
        var fragment = line
        while !fragment.isEmpty {
            // Look for a variable in our current fragment.
            if let variable = findVariable(fragment) {
                // Append the contents before the variable.
                result += fragment[fragment.startIndex..<variable.startIndex]
                guard let variableValue = variables[variable.name] else {
                    throw PkgConfigError.parsingError(
                        "Expected a value for variable '\(variable.name)' in \(pcFile). Variables: \(variables)")
                }
                // Append the value of the variable.
                result += variableValue
                // Update the fragment with post variable string.
                fragment = String(fragment[fragment.index(after: variable.endIndex)...])
            } else {
                // No variable found, just append rest of the fragment to result.
                result += fragment
                fragment = ""
            }
        }
        return String(result)
    }

    /// Split line on unescaped spaces.
    ///
    /// Will break on space in "abc def" and "abc\\ def" but not in "abc\ def"
    /// and ignore multiple spaces such that "abc def" will split into ["abc",
    /// "def"].
    private func splitEscapingSpace(_ line: String) throws -> [String] {
        var splits = [String]()
        var fragment = [Character]()

        func saveFragment() {
            if !fragment.isEmpty {
                splits.append(String(fragment))
                fragment.removeAll()
            }
        }

        var it = line.makeIterator()
        // Indicates if we're in a quoted fragment, we shouldn't append quote.
        var inQuotes = false
        while let char = it.next() {
            if char == "\"" {
                inQuotes = !inQuotes
            } else if char == "\\" {
                if let next = it.next() {
                    fragment.append(next)
                }
            } else if char == " " && !inQuotes {
                saveFragment()
            } else {
                fragment.append(char)
            }
        }
        guard !inQuotes else {
            throw PkgConfigError.parsingError(
                "Text ended before matching quote was found in line: \(line) file: \(pcFile)")
        }
        saveFragment()
        return splits
    }
}

