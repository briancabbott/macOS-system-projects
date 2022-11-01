//
//  ContentView.swift
//  Note
//
//  Created by Devran on 09.02.22.
//

import SwiftUI

struct ContentView: View {
    @ObservedObject var document: NoteDocument
    @Environment(\.undoManager) var undoManager

    var body: some View {
        VStack(alignment: .leading) {
            Toggle("Votes locked?", isOn: $document.post.isPositiveVotesLocked)
            Text(document.post.isPositiveVotesLocked ? "Votes locked." : "Votes not locked.")
            Divider()
            Text("\(document.post.positiveVotes) Positive votes")
            HStack {
                Button("-") {
                    document.decreasePositiveVotes(undoManager: undoManager)
                }.disabled(document.post.isPositiveVotesLocked)
                
                Button("+") {
                    document.increasePositiveVotes(undoManager: undoManager)
                }.disabled(document.post.isPositiveVotesLocked)
            }
        }
        .frame(width: 700, height: 400)
        .padding()
        .focusedSceneValue(\.document, document)
        .focusedSceneValue(\.undoManager, undoManager)
        .toolbar {
            ToolbarItemGroup(placement: .navigation) {
                HStack {
                    undoButton
                    redoButton
                }
            }
        }
    }
    
    var undoButton: some View {
        Button(action: {
            undoManager?.undo()
        }) {
            Image(systemName: "arrow.uturn.backward.circle.fill")
        }
        .disabled(!(undoManager?.canUndo ?? false))
    }
    
    var redoButton: some View {
        Button(action: {
            undoManager?.redo()
        }) {
            Image(systemName: "arrow.uturn.forward.circle.fill")
        }
        .disabled(!(undoManager?.canRedo ?? false))
    }
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
        ContentView(document: NoteDocument())
    }
}
