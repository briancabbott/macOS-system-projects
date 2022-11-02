//
//  IndexSet+Codable.swift
//  Codable


import Foundation

extension IndexSet : Codable {
    private enum CodingKeys : Int, CodingKey {
        case indexes
    }

    private enum RangeCodingKeys : Int, CodingKey {
        case location
        case length
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        var indexesContainer = try container.nestedUnkeyedContainer(forKey: .indexes)
        self.init()

        while !indexesContainer.isAtEnd {
            let rangeContainer = try indexesContainer.nestedContainer(keyedBy: RangeCodingKeys.self)
            let startIndex = try rangeContainer.decode(Int.self, forKey: .location)
            let count = try rangeContainer.decode(Int.self, forKey: .length)
            self.insert(integersIn: startIndex ..< (startIndex + count))
        }
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        var indexesContainer = container.nestedUnkeyedContainer(forKey: .indexes)

        for range in self.rangeView {
            var rangeContainer = indexesContainer.nestedContainer(keyedBy: RangeCodingKeys.self)
            try rangeContainer.encode(range.startIndex, forKey: .location)
            try rangeContainer.encode(range.count, forKey: .length)
        }
    }
}
