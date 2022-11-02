//
//  SwiftExtensions.swift
//  JSONSchema
//
//  Created by Alsey Coleman Miller on 12/16/17.
//

public extension Array where Element: Hashable {
    
    public var isUnique: Bool {
        
        var hashes = [Int]()
        hashes.reserveCapacity(count)
        
        for element in self {
            
            let hash = element.hashValue
            
            guard hashes.contains(hash) == false
                else { return false }
            
            hashes.append(hash)
        }
        
        return true
    }
}
