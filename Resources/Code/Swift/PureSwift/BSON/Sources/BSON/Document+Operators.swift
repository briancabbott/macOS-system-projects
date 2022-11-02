//
//  Document+Operators.swift
//  BSON
//
//  Created by Robbert Brandsma on 17-08-16.
//
//

import Foundation

extension Document : Equatable {
    /// Compares two Documents to be equal to each other
    ///
    /// TODO: Implement fast comparison here
    public static func ==(lhs: Document, rhs: Document) -> Bool {
        guard lhs.count == rhs.count else {
            return false
        }
        
        if let lhsIsArray = lhs.isArray, let rhsIsArray = rhs.isArray {
            guard lhsIsArray == rhsIsArray else {
                return false
            }
        }
        
        for (key, value) in lhs {
            guard let val = rhs[key] else {
                return false
            }
            
            if let val = val as? Document, let value = value as? Document {
                guard val == value else {
                    return false
                }
            } else {
                guard val.makeBinary() == value.makeBinary() else {
                    return false
                }
            }
        }
        
        return true
    }
    
    /// Returns true if `lhs` and `rhs` store the same serialized data.
    /// Implies that `lhs` == `rhs`.
    public static func ===(lhs: Document, rhs: Document) -> Bool {
        return lhs.storage == rhs.storage
    }
}

extension Document {
    /// Appends `rhs` to `lhs` overwriting the keys from `lhs` when necessary
    ///
    /// - returns: The modified `lhs`
    public static func +(lhs: Document, rhs: Document) -> Document {
        var new = lhs
        new += rhs
        return new
    }
    
    /// Appends `rhs` to `lhs` overwriting the keys from `lhs` when necessary
    public static func +=(lhs: inout Document, rhs: Document) {
        let rhsIsSmaller = lhs.count > rhs.count
        let smallest = rhsIsSmaller ? rhs : lhs
        let other = rhsIsSmaller ? lhs : rhs
        
        let otherKeys = other.keys
        for key in smallest.keys {
            if otherKeys.contains(key) {
                lhs.removeValue(forKey: key)
            }
        }
        
        lhs.storage.append(contentsOf: rhs.storage)
        lhs.searchTree = IndexTrieNode(0)
    }
}
