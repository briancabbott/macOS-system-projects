//
//  VoteCommands.swift
//  Note
//
//  Created by Devran on 09.02.22.
//

import Foundation
import SwiftUI

struct VoteCommands: Commands {
    @FocusedValue(\.document) private var document
    @FocusedValue(\.undoManager) private var undoManager
    
    var body: some Commands {
        CommandMenu("Votes") {
            if let documentMaybe = document,
               let document = documentMaybe,
               let undoManagerMaybe = undoManager,
               let undoManager = undoManagerMaybe
            {
                Button("Set Votes to 9000") {
                    document.replacePositiveVotes(newValue: 9000, undoManager: undoManager)
                }
                .disabled(document.post.isPositiveVotesLocked)
                
                Button("Set Votes to 0") {
                    document.replacePositiveVotes(newValue: 0, undoManager: undoManager)
                }
                .disabled(document.post.isPositiveVotesLocked)
            }
        }
    }
}
