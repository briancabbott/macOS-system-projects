/*
 This source file is part of the Swift.org open source project

 Copyright (c) 2021-2022 Apple Inc. and the Swift project authors
 Licensed under Apache License v2.0 with Runtime Library Exception

 See https://swift.org/LICENSE.txt for license information
 See https://swift.org/CONTRIBUTORS.txt for Swift project authors
*/

import Foundation

/// An auxiliary aside element interpreted from a block quote.
///
/// Asides are written as a block quote starting with a special plain-text tag,
/// such as `note:` or `tip:`:
///
/// ```markdown
/// > Tip: This is a `tip` aside.
/// > It may have a presentation similar to a block quote, but with a
/// > different meaning, as it doesn't quote speech.
/// ```
public struct Aside {
    /// Describes the different kinds of aside.
    public struct Kind: RawRepresentable, CaseIterable, Equatable {
        /// A "note" aside.
        public static let note = Kind(rawValue: "Note")!
        
        /// A "tip" aside.
        public static let tip = Kind(rawValue: "Tip")!
        
        /// An "important" aside.
        public static let important = Kind(rawValue: "Important")!
        
        /// An "experiment" aside.
        public static let experiment = Kind(rawValue: "Experiment")!
        
        /// A "warning" aside.
        public static let warning = Kind(rawValue: "Warning")!
        
        /// An "attention" aside.
        public static let attention = Kind(rawValue: "Attention")!
        
        /// An "author" aside.
        public static let author = Kind(rawValue: "Author")!
        
        /// An "authors" aside.
        public static let authors = Kind(rawValue: "Authors")!
        
        /// A "bug" aside.
        public static let bug = Kind(rawValue: "Bug")!
        
        /// A "complexity" aside.
        public static let complexity = Kind(rawValue: "Complexity")!
        
        /// A "copyright" aside.
        public static let copyright = Kind(rawValue: "Copyright")!
        
        /// A "date" aside.
        public static let date = Kind(rawValue: "Date")!
        
        /// An "invariant" aside.
        public static let invariant = Kind(rawValue: "Invariant")!
        
        /// A "mutatingVariant" aside.
        public static let mutatingVariant = Kind(rawValue: "MutatingVariant")!
        
        /// A "nonMutatingVariant" aside.
        public static let nonMutatingVariant = Kind(rawValue: "NonMutatingVariant")!
        
        /// A "postcondition" aside.
        public static let postcondition = Kind(rawValue: "Postcondition")!
        
        /// A "precondition" aside.
        public static let precondition = Kind(rawValue: "Precondition")!
        
        /// A "remark" aside.
        public static let remark = Kind(rawValue: "Remark")!
        
        /// A "requires" aside.
        public static let requires = Kind(rawValue: "Requires")!
        
        /// A "since" aside.
        public static let since = Kind(rawValue: "Since")!
        
        /// A "todo" aside.
        public static let todo = Kind(rawValue: "ToDo")!
        
        /// A "version" aside.
        public static let version = Kind(rawValue: "Version")!
        
        /// A "throws" aside.
        public static let `throws` = Kind(rawValue: "Throws")!
        
        /// A collection of preconfigured aside kinds.
        public static var allCases: [Aside.Kind] {
            [
                note,
                tip,
                important,
                experiment,
                warning,
                attention,
                author,
                authors,
                bug,
                complexity,
                copyright,
                date,
                invariant,
                mutatingVariant,
                nonMutatingVariant,
                postcondition,
                precondition,
                remark,
                requires,
                since,
                todo,
                version,
                `throws`,
            ]
        }
        
        /// The underlying raw string value.
        public var rawValue: String
        
        /// Creates an aside kind with the specified raw value.
        /// - Parameter rawValue: The string the aside displays as its title.
        public init?(rawValue: String) {
            self.rawValue = rawValue
        }
    }
    
    /// The kind of aside interpreted from the initial text of the ``BlockQuote``.
    public var kind: Kind

    /// The block elements of the aside taken from the ``BlockQuote``,
    /// excluding the initial text tag.
    public var content: [BlockMarkup]

    /// Create an aside from a block quote.
    public init(_ blockQuote: BlockQuote) {
        // Try to find an initial `tag:` text at the beginning.
        guard var initialText = blockQuote.child(through: [
            (0, Paragraph.self),
            (0, Text.self),
        ]) as? Text,
        let firstColonIndex = initialText.string.firstIndex(where: { $0 == ":" }) else {
            // Otherwise, default to a note aside.
            self.kind = .note
            self.content = Array(blockQuote.blockChildren)
            return
        }
        self.kind = Kind(rawValue: String(initialText.string[..<firstColonIndex]))!

        // Trim off the aside tag prefix.
        let trimmedText = initialText.string[initialText.string.index(after: firstColonIndex)...].drop {
            $0 == " " || $0 == "\t"
        }
        initialText.string = String(trimmedText)

        let newBlockQuote = initialText.parent!.parent! as! BlockQuote
        self.content = Array(newBlockQuote.blockChildren)
    }
}
