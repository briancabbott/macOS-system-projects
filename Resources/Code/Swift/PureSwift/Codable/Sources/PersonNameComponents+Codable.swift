//
//  PersonNameComponents+Codable.swift
//  Codable


import Foundation

@available(OSX 10.11, iOS 9.0, *)
extension PersonNameComponents : Codable {
    private enum CodingKeys : Int, CodingKey {
        case namePrefix
        case givenName
        case middleName
        case familyName
        case nameSuffix
        case nickname
    }

    public init(from decoder: Decoder) throws {
        self.init()

        let container = try decoder.container(keyedBy: CodingKeys.self)
        self.namePrefix = try container.decodeIfPresent(String.self, forKey: .namePrefix)
        self.givenName  = try container.decodeIfPresent(String.self, forKey: .givenName)
        self.middleName = try container.decodeIfPresent(String.self, forKey: .middleName)
        self.familyName = try container.decodeIfPresent(String.self, forKey: .familyName)
        self.nameSuffix = try container.decodeIfPresent(String.self, forKey: .nameSuffix)
        self.nickname   = try container.decodeIfPresent(String.self, forKey: .nickname)
    }

    public func encode(to encoder: Encoder) throws {
        var container = encoder.container(keyedBy: CodingKeys.self)
        if let np = self.namePrefix { try container.encode(np, forKey: .namePrefix) }
        if let gn = self.givenName  { try container.encode(gn, forKey: .givenName) }
        if let mn = self.middleName { try container.encode(mn, forKey: .middleName) }
        if let fn = self.familyName { try container.encode(fn, forKey: .familyName) }
        if let ns = self.nameSuffix { try container.encode(ns, forKey: .nameSuffix) }
        if let nn = self.nickname   { try container.encode(nn, forKey: .nickname) }
    }
}
