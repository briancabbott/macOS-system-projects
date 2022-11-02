// Copyright © 2017 Károly Lőrentey.
// This file is part of Attabench: https://github.com/attaswift/Benchmarking
// For licensing information, see the file LICENSE.md in the Git repository above.

import Foundation
import BenchmarkIPC

protocol OutputProtocol {
    func begin(task: String, size: Int) throws
    func progress(task: String, size: Int, time: TimeInterval) throws
    func finish(task: String, size: Int, time: TimeInterval) throws
}

extension TimeInterval {
    var humanReadableString: String {
        let sign = self.sign == .minus ? "-" : ""

        var value = self.magnitude
        if value == 0 { return "\(sign)0s" }
        if value.isInfinite { return "\(sign)infinity" }

        let units = ["s", "ms", "µs", "ns", "ps", "fs", "as", "zs", "ys"]
        var scale = 0
        while value + 0.0005 < 1 {
            value *= 1000
            scale += 1
        }
        guard scale < units.count else {
            return String(format: "%.3gs", self)
        }
        return String(format: "%@%.3g%@", sign, value, units[scale])
    }
}

internal struct OutputFile {
    let fileHandle: FileHandle
    var fileDescriptor: Int32 { return fileHandle.fileDescriptor }
    
    init(_ fileHandle: FileHandle) {
        self.fileHandle = fileHandle
    }
    
    var isATeletype: Bool {
        return isatty(self.fileHandle.fileDescriptor) == 1
    }
    
    func write(_ data: Data) throws {
        try data.withUnsafeBytes { (p: UnsafePointer<UInt8>) -> Void in
            var p = p
            var c = data.count
            while c > 0 {
                let r = Darwin.write(fileHandle.fileDescriptor, UnsafeRawPointer(p), c)
                if r == -1 {
                    throw POSIXError(POSIXErrorCode(rawValue: errno)!)
                }
                precondition(r > 0 && r <= c)
                c -= r
                p += r
            }
        }
    }
    
    func write(_ string: String) throws {
        guard let data = string.data(using: .utf8) else {
            throw CocoaError(CocoaError.Code.fileWriteInapplicableStringEncoding)
        }
        try self.write(data)
    }
}

internal struct PrettyOutput: OutputProtocol {
    private let output: OutputFile
    private let isatty: Bool

    enum TerminalEffect: String {
        case none         = "\u{1b}[0m"
        
        case black        = "\u{1b}[0;30m"
        case darkGray     = "\u{1b}[2;37m"
        case gray         = "\u{1b}[2;97m"
        case lightGray    = "\u{1b}[0;90m"
        case white        = "\u{1b}[0;37m"
        case brightWhite  = "\u{1b}[0;97m"
        
        case dimRed       = "\u{1b}[2;31m"
        case darkRed      = "\u{1b}[2;91m"
        case red          = "\u{1b}[0;31m"
        case brightRed    = "\u{1b}[0;91m"
        
        case dimGreen     = "\u{1b}[2;32m"
        case darkGreen    = "\u{1b}[2;92m"
        case green        = "\u{1b}[0;32m"
        case brightGreen  = "\u{1b}[0;92m"
        
        case dimYellow    = "\u{1b}[2;33m"
        case darkYellow   = "\u{1b}[2;93m"
        case yellow       = "\u{1b}[0;33m"
        case brightYellow = "\u{1b}[0;93m"
        
        case dimBlue      = "\u{1b}[2;34m"
        case darkBlue     = "\u{1b}[2;94m"
        case blue         = "\u{1b}[0;34m"
        case brightBlue   = "\u{1b}[0;94m"
        
        case dimPurple    = "\u{1b}[2;35m"
        case darkPurple   = "\u{1b}[2;95m"
        case purple       = "\u{1b}[0;35m"
        case brightPurple = "\u{1b}[0;95m"
        
        case dimCyan      = "\u{1b}[2;36m"
        case darkCyan     = "\u{1b}[2;96m"
        case cyan         = "\u{1b}[0;36m"
        case brightCyan   = "\u{1b}[0;96m"
    }
    
    init(to output: OutputFile) {
        self.output = output
        self.isatty = output.isATeletype
    }
    
    private func highlight(_ value: Any, _ effect: TerminalEffect = .brightWhite) -> String {
        guard isatty else { return "\(value)" }
        return "\(effect.rawValue)\(value)\(TerminalEffect.none.rawValue)"
    }
    
    private func t(_ value: Any) -> String { return highlight(value, .brightGreen) }
    private func s(_ value: Any) -> String { return highlight(value, .brightYellow) }
    private func dim(_ value: Any) -> String { return highlight(value, .white) }
    private func i(_ value: TimeInterval) -> String { return value.humanReadableString }
    
    func begin(task: String, size: Int) throws {
        try output.write("\(dim("Measuring")) \(t(task)) \(dim("for size")) \(s(size))\(dim("..."))")
    }
    func progress(task: String, size: Int, time: TimeInterval) throws {
        if time > 0.1 {
            try output.write(" \(highlight(i(time), .gray))")
        }
        else {
            try output.write(".")
        }
    }
    func finish(task: String, size: Int, time: TimeInterval) throws {
        try output.write(" \(highlight("min:", .red)) \(highlight(i(time), .brightRed))\n")
    }
}

internal struct JSONOutput: OutputProtocol {
    let output: OutputFile
    let encoder: JSONEncoder
    
    init(to output: OutputFile) {
        self.output = output
        self.encoder = JSONEncoder()
        self.encoder.nonConformingFloatEncodingStrategy = .throw
    }
    
    private func send(_ report: Report) throws {
        var data = try encoder.encode(report)
        data.append(0x0a) // newline
        try output.write(data)
    }
    
    func begin(task: String, size: Int) throws {
        try send(.begin(task: task, size: size))
    }
    func progress(task: String, size: Int, time: TimeInterval) throws {
        // Do nothing
    }
    func finish(task: String, size: Int, time: TimeInterval) throws {
        try send(.finish(task: task, size: size, time: time))
    }
}
