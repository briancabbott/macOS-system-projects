//
//  CryptoLinux.swift
//
//
//  Created by Alsey Coleman Miller on 5/6/22.
//

import Foundation
import SystemPackage
import Socket

/// Manages a socket to the [Linux Kernel Crypto API](https://www.kernel.org/doc/html/v5.10/crypto/userspace-if.html#)
///
/// The Linux kernel crypto API is accessible from user space. Currently, the following ciphers are accessible:
///
/// - Message digest including keyed message digest (HMAC, CMAC)
/// - Symmetric ciphers
/// - AEAD ciphers
/// - Random Number Generators
public struct CryptoLinux {
    
    // MARK: - Properties
    
    public static var path: String { "/proc/crypto" }
    
    internal let ciphers: [Cipher]
    
    // MARK: - Initializaton
    
    /// Reads the list of availble ciphers from `/proc/crypto`.
    @available(macOS, unavailable)
    public init() throws {
        let data = try Data(contentsOf: URL(fileURLWithPath: Self.path), options: [.mappedIfSafe])
        guard let string = String(data: data, encoding: .utf8) else {
            throw CocoaError(.fileReadCorruptFile)
        }
        try self.init(string)
    }
    
    internal init(
        _ cipherList: String,
        log: ((String) -> ())? = nil
    ) throws {
        // parse ciphers
        var decoder = CipherDecoder()
        decoder.log = log
        let ciphers = try decoder.decode(cipherList)
        self.init(ciphers)
    }
    
    internal init(_ ciphers: [Cipher]) {
        self.ciphers = ciphers
    }
    
    // MARK: - Methods
    
    
}

// MARK: - Sequence

extension CryptoLinux: Sequence {
    
    public typealias Element = Cipher
    
    public func makeIterator() -> IndexingIterator<CryptoLinux> {
        return IndexingIterator(_elements: self)
    }
}

// MARK: - Collection

extension CryptoLinux: Collection {
    
    public var isEmpty: Bool {
        ciphers.isEmpty
    }
    
    public var count: Int {
        ciphers.count
    }
    
    public func index(after index: Int) -> Int {
        index + 1
    }
    
    public var startIndex: Int {
        0
    }
    
    public var endIndex: Int {
        count
    }
}

// MARK: - RandomAccessCollection

extension CryptoLinux: RandomAccessCollection {
    
    public subscript (index: Int) -> Cipher {
        ciphers[index]
    }
    
    public subscript(bounds: Range<Int>) -> Slice<CryptoLinux> {
        return Slice<CryptoLinux>(base: self, bounds: bounds)
    }
}
