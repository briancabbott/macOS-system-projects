//
//  BufferedSet.swift
//  GlueKit
//
//  Created by Károly Lőrentey on 2016-11-02.
//  Copyright © 2015–2017 Károly Lőrentey.
//

extension ObservableSetType {
    public func buffered() -> AnyObservableSet<Element> {
        if isBuffered {
            return anyObservableSet
        }
        return BufferedObservableSet(self).anyObservableSet
    }
}

internal class BufferedObservableSet<Content: ObservableSetType>: _BaseObservableSet<Content.Element> {
    typealias Element = Content.Element
    typealias Change = SetChange<Element>

    private struct BufferedSink: UniqueOwnedSink {
        typealias Owner = BufferedObservableSet
        
        unowned(unsafe) let owner: Owner
        
        func receive(_ update: SetUpdate<Content.Element>) {
            owner.applyUpdate(update)
        }
    }
    
    private let _content: Content
    private var _value: Set<Element>
    private var _pendingChange: Change? = nil

    init(_ content: Content) {
        _content = content
        _value = content.value
        super.init()
        _content.add(BufferedSink(owner: self))
    }

    deinit {
        _content.remove(BufferedSink(owner: self))
    }

    func applyUpdate(_ update: SetUpdate<Element>) {
        switch update {
        case .beginTransaction:
            beginTransaction()
        case .change(let change):
            if _pendingChange != nil {
                _pendingChange!.merge(with: change)
            }
            else {
                _pendingChange = change
            }
        case .endTransaction:
            if let change = _pendingChange {
                _value.apply(change)
                _pendingChange = nil
                sendChange(change)
            }
            endTransaction()
        }
    }

    override var isBuffered: Bool {
        return true
    }

    override var count: Int {
        return _value.count
    }

    override var value: Set<Element> {
        return _value
    }

    override func contains(_ member: Element) -> Bool {
        return _value.contains(member)
    }

    override func isSubset(of other: Set<Element>) -> Bool {
        return _value.isSubset(of: other)
    }

    override func isSuperset(of other: Set<Element>) -> Bool {
        return _value.isSuperset(of: other)
    }
}
