//
//  Date+Codable.swift
//  Codable


import Foundation

extension Date : Codable {
    public init(from decoder: Decoder) throws {
        // FIXME: This is a hook for bypassing a conditional conformance implementation to apply a strategy (see SR-5206). Remove this once conditional conformance is available.
        let container = try decoder.singleValueContainer()
        if let decoder = container as? _JSONDecoder {
            switch decoder.options.dateDecodingStrategy {
            case .deferredToDate:
                break /* fall back to default implementation below; this would recurse */

            default:
                // _JSONDecoder has a hook for Dates; this won't recurse since we're not going to defer back to Date in _JSONDecoder.
                self = try container.decode(Date.self)
                return
            }
        }

        let timestamp = try container.decode(Double.self)
        self = Date(timeIntervalSinceReferenceDate: timestamp)
    }

    public func encode(to encoder: Encoder) throws {
        // FIXME: This is a hook for bypassing a conditional conformance implementation to apply a strategy (see SR-5206). Remove this once conditional conformance is available.
        // We are allowed to request this container as long as we don't encode anything through it when we need the keyed container below.
        var container = encoder.singleValueContainer()
        if let encoder = container as? _JSONEncoder {
            switch encoder.options.dateEncodingStrategy {
            case .deferredToDate:
                break /* fall back to default implementation below; this would recurse */

            default:
                // _JSONEncoder has a hook for Dates; this won't recurse since we're not going to defer back to Date in _JSONEncoder.
                try container.encode(self)
                return
            }
        }

        try container.encode(self.timeIntervalSinceReferenceDate)
    }
}
