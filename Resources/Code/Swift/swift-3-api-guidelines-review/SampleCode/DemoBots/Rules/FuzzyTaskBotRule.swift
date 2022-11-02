/*
    Copyright (C) 2015 Apple Inc. All Rights Reserved.
    See LICENSE.txt for this sample’s licensing information
    
    Abstract:
    `FuzzyTaskBotRule` is a `GKRule` subclass that asserts a `fact` if and only if its `grade()` function returns a non-zero value. Subclasses for the specific rules used in the game can be found in Rules.swift.
*/

import GameplayKit

class FuzzyTaskBotRule: GKRule {
    // MARK: Properties
    
    var snapshot: EntitySnapshot!
    
    func grade() -> Float { return 0.0 }
    
    let fact: Fact
    
    // MARK: Initializers
    
    init(fact: Fact) {
        self.fact = fact
        
        super.init()
        
        // Set the salience so that 'fuzzy' rules will evaluate first.
        salience = Int.max
    }
    
    // MARK: GPRule Overrides
    
    override func evaluatePredicateWith(system: GKRuleSystem) -> Bool {
        snapshot = system.state["snapshot"] as! EntitySnapshot
        
        if grade() >= 0.0 {
            return true
        }
        
        return false
    }
    
    override func performActionWith(system: GKRuleSystem) {
        system.assertFact(fact.rawValue, grade: grade())
    }
}
