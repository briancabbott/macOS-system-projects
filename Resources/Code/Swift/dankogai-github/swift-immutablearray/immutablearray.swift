//
//  immutablearray.swift
//  immutablearray
//
//  Created by Dan Kogai on 6/29/14.
//  Copyright (c) 2014 Dan Kogai. All rights reserved.
//

class ImmutableArray<T> {
    typealias Element = T
    let vault:()->T[]
    let count:Int
    init() { vault = {[]}; count = 0 }
    init(_ array:T[]) {
        let copy = array.copy()
        vault = {copy}
        count = array.count
    }
    init(_ elements:T...) {
        vault = {elements}
        count = elements.count
    }
}
extension ImmutableArray: Collection {
    var startIndex:Int { return vault().startIndex }
    var endIndex:Int { return vault().endIndex }
    subscript (index:Int) -> T {
        return vault()[index]
    }
    subscript (range:Range<Int>) -> ImmutableArray<T> {
        return ImmutableArray(Array(vault()[range]))
    }
    func generate() -> IndexingGenerator<Array<T>> {
        return vault().generate()
    }
}
extension ImmutableArray : Printable, DebugPrintable {
    var description:String {
        return vault().description
    }
    var debugDescription:String {
        return vault().debugDescription
    }
}
@infix func ==<T: Equatable>(
    lhs:ImmutableArray<T>, rhs:ImmutableArray<T>
    )->Bool {
    return lhs.vault() == rhs.vault()
}
@infix func ==<T: Equatable>(
    lhs:ImmutableArray<T>, rhs:Array<T>
    )->Bool {
        return lhs.vault() == rhs
}
@infix func ==<T: Equatable>(
    lhs:Array<T>, rhs:ImmutableArray<T>
    )->Bool {
        return lhs == rhs.vault()
}
@infix func !=<T: Equatable>(
    lhs:ImmutableArray<T>, rhs:ImmutableArray<T>
    )->Bool {
        return lhs.vault() != rhs.vault()
}
@infix func !=<T: Equatable>(
    lhs:ImmutableArray<T>, rhs:Array<T>
    )->Bool {
        return lhs.vault() != rhs
}
@infix func !=<T: Equatable>(
    lhs:Array<T>, rhs:ImmutableArray<T>
    )->Bool {
        return lhs != rhs.vault()
}
extension ImmutableArray {
    var isEmpty:Bool { return count == 0 }
    func sort(block:(T,T)->Bool) -> ImmutableArray<T> {
        let newarray = vault()
        newarray.sort(block)
        return ImmutableArray(newarray)
    }
    func filter(block:(T)->Bool) -> ImmutableArray<T> {
        return ImmutableArray(vault().filter(block))
    }
    func map<U>(block:(T)->U) -> ImmutableArray<U> {
        return ImmutableArray<U>(vault().map(block))
    }
    func reduce<U>(start:U, block:(U,T)->U) -> U {
        return vault().reduce(start,block)
    }
    func reverse() -> ImmutableArray<T> {
        return ImmutableArray(vault().reverse())
    }
}
extension Array {
    func immutable() -> ImmutableArray<T> {
        return ImmutableArray(self)
    }
}
