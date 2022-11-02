//
//  Data+Codable.swift
//  Codable


import Foundation

extension Data : Codable {
    public init(from decoder: Decoder) throws {
        // FIXME: This is a hook for bypassing a conditional conformance implementation to apply a strategy (see SR-5206). Remove this once conditional conformance is available.
        do {
            let singleValueContainer = try decoder.singleValueContainer()
            if let decoder = singleValueContainer as? _JSONDecoder {
                switch decoder.options.dataDecodingStrategy {
                case .deferredToData:
                    break /* fall back to default implementation below; this would recurse */

                default:
                    // _JSONDecoder has a hook for Datas; this won't recurse since we're not going to defer back to Data in _JSONDecoder.
                    self = try singleValueContainer.decode(Data.self)
                    return
                }
            }
        } catch { /* fall back to default implementation below */ }

        var container = try decoder.unkeyedContainer()

        // It's more efficient to pre-allocate the buffer if we can.
        if let count = container.count {
            self = Data(count: count)

            // Loop only until count, not while !container.isAtEnd, in case count is underestimated (this is misbehavior) and we haven't allocated enough space.
            // We don't want to write past the end of what we allocated.
            for i in 0 ..< count {
                let byte = try container.decode(UInt8.self)
                self[i] = byte
            }
        } else {
            self = Data()
        }

        while !container.isAtEnd {
            var byte = try container.decode(UInt8.self)
            self.append(&byte, count: 1)
        }
    }

    public func encode(to encoder: Encoder) throws {
        // FIXME: This is a hook for bypassing a conditional conformance implementation to apply a strategy (see SR-5206). Remove this once conditional conformance is available.
        // We are allowed to request this container as long as we don't encode anything through it when we need the unkeyed container below.
        var singleValueContainer = encoder.singleValueContainer()
        if let encoder = singleValueContainer as? _JSONEncoder {
            switch encoder.options.dataEncodingStrategy {
            case .deferredToData:
                break /* fall back to default implementation below; this would recurse */

            default:
                // _JSONEncoder has a hook for Datas; this won't recurse since we're not going to defer back to Data in _JSONEncoder.
                try singleValueContainer.encode(self)
                return
            }
        }

        var container = encoder.unkeyedContainer()

        // Since enumerateBytes does not rethrow, we need to catch the error, stow it away, and rethrow if we stopped.
        var caughtError: Error? = nil
        self.enumerateBytes { (buffer: UnsafeBufferPointer<UInt8>, byteIndex: Data.Index, stop: inout Bool) in
            do {
                try container.encode(contentsOf: buffer)
            } catch {
                caughtError = error
                stop = true
            }
        }

        if let error = caughtError {
            throw error
        }
    }
}
