//
//  URL+Codable.swift
//  Codable


import Foundation

extension URL : Codable {
    private enum CodingKeys : Int, CodingKey {
        case base
        case relative
    }

    public init(from decoder: Decoder) throws {
        // FIXME: This is a hook for bypassing a conditional conformance implementation to apply a strategy (see SR-5206). Remove this once conditional conformance is available.
        do {
            // We are allowed to request this container as long as we don't decode anything through it when we need the keyed container below.
            let singleValueContainer = try decoder.singleValueContainer()
            if singleValueContainer is _JSONDecoder {
                // _JSONDecoder has a hook for URLs; this won't recurse since we're not going to defer back to URL in _JSONDecoder.
                self = try singleValueContainer.decode(URL.self)
                return
            }
        } catch { /* Fall back to default implementation below. */ }

        let container = try decoder.container(keyedBy: CodingKeys.self)
        let relative = try container.decode(String.self, forKey: .relative)
        let base = try container.decodeIfPresent(URL.self, forKey: .base)

        guard let url = URL(string: relative, relativeTo: base) else {
            throw DecodingError.dataCorrupted(DecodingError.Context(codingPath: decoder.codingPath,
                                                                    debugDescription: "Invalid URL string."))
        }

        self = url
    }

    public func encode(to encoder: Encoder) throws {
        // FIXME: This is a hook for bypassing a conditional conformance implementation to apply a strategy (see SR-5206). Remove this once conditional conformance is available.
        // We are allowed to request this container as long as we don't encode anything through it when we need the keyed container below.
        var singleValueContainer = encoder.singleValueContainer()
        if singleValueContainer is _JSONEncoder {
            // _JSONEncoder has a hook for URLs; this won't recurse since we're not going to defer back to URL in _JSONEncoder.
            try singleValueContainer.encode(self)
            return
        }

        var container = encoder.container(keyedBy: CodingKeys.self)
        try container.encode(self.relativeString, forKey: .relative)
        if let base = self.baseURL {
            try container.encode(base, forKey: .base)
        }
    }
}
