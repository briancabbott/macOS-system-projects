//
//  IndexPath+Codable.swift
//  Codable


import Foundation

extension IndexPath : Codable {
    private enum CodingKeys : Int, CodingKey {
        case indexes
    }

    public init(from decoder: Decoder) throws {
        let container = try decoder.container(keyedBy: CodingKeys.self)
        var indexesContainer = try container.nestedUnkeyedContainer(forKey: .indexes)

        var indexes = [Int]()
        if let count = indexesContainer.count {
            indexes.reserveCapacity(count)
        }

        while !indexesContainer.isAtEnd {
            let index = try indexesContainer.decode(Int.self)
            indexes.append(index)
        }

        self.init(indexes: indexes)
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        var indexesContainer = container.nestedUnkeyedContainer(forKey: .indexes)

        for index in self {
            try indexesContainer.encode(index)
        }
    }
}
