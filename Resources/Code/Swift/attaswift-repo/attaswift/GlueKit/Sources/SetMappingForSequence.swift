//
//  SetMappingForSequence.swift
//  GlueKit
//
//  Created by Károly Lőrentey on 2016-10-07.
//  Copyright © 2015–2017 Károly Lőrentey.
//

extension ObservableSetType {
    public func flatMap<Result: Sequence>(_ key: @escaping (Element) -> Result) -> AnyObservableSet<Result.Iterator.Element> where Result.Iterator.Element: Hashable {
        return SetMappingForSequence<Self, Result>(parent: self, key: key).anyObservableSet
    }
}

class SetMappingForSequence<Parent: ObservableSetType, Result: Sequence>: SetMappingBase<Result.Iterator.Element>
where Result.Iterator.Element: Hashable {
    typealias Element = Result.Iterator.Element

    private struct ParentSink: UniqueOwnedSink {
        typealias Owner = SetMappingForSequence
        
        unowned(unsafe) let owner: Owner
        
        func receive(_ update: SetUpdate<Parent.Element>) {
            owner.apply(update)
        }
    }
    
    let parent: Parent
    let key: (Parent.Element) -> Result

    init(parent: Parent, key: @escaping (Parent.Element) -> Result) {
        self.parent = parent
        self.key = key
        super.init()
        for e in parent.value {
            for new in key(e) {
                _ = self.insert(new)
            }
        }
        parent.add(ParentSink(owner: self))
    }

    deinit {
        parent.remove(ParentSink(owner: self))
    }

    func apply(_ update: SetUpdate<Parent.Element>) {
        switch update {
        case .beginTransaction:
            beginTransaction()
        case .change(let change):
            var transformedChange = SetChange<Element>()
            for e in change.removed {
                for old in key(e) {
                    if self.remove(old) {
                        transformedChange.remove(old)
                    }
                }
            }
            for e in change.inserted {
                for new in key(e) {
                    if self.insert(new) {
                        transformedChange.insert(new)
                    }
                }
            }
            if !transformedChange.isEmpty {
                sendChange(transformedChange)
            }
        case .endTransaction:
            endTransaction()
        }
    }
}
