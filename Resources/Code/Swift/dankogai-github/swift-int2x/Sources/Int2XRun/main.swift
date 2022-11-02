import Int2X

func fact<T:FixedWidthInteger>(_ n:T)->T {
    return n == 0 ? 1 : (1...Int(n)).map{ T($0) }.reduce(1, *)
}

let ua = Int2XConfig.useAccelerate ? [false, true] : [false]

for a in ua {
    if 1 < ua.count {
        #if os(macOS) || os(iOS)
        Int2XConfig.useAccelerate = a
        #endif
    }
    let v = fact(Int256(56)) / fact(Int256(21))
    print(v)
}

for a in ua {
    if 1 < ua.count {
        #if os(macOS) || os(iOS)
        Int2XConfig.useAccelerate = a
        #endif
    }
    let v = fact(Int512(97)) / fact(Int512(56))
    print(v)
}

for a in ua {
    if 1 < ua.count {
        #if os(macOS) || os(iOS)
        Int2XConfig.useAccelerate = a
        #endif
    }
    let v = fact(Int1024(170)) / fact(Int1024(97))
    print(v)
}
