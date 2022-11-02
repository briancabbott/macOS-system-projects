public struct BF {
    public static let datasize = 65536
    public let code:[CChar]
    public let jump:Dictionary<Int,Int>
    public var data = [CChar](repeating: CChar(0), count:Self.datasize);
    public var ibuf = [CChar]()
    public var obuf = [CChar]()
    public var (pc, sp) = (0, 0)
    public init?(_ src:String) {
        self.code =  src.utf8.map{CChar($0)}
        var stak:[Int] = []
        var jump:Dictionary<Int,Int> = [:]
        for (i,c) in code.enumerated() {
            let u = UnicodeScalar(Int(c))
            if u == "[" {
                stak.append(i)
            } else if u == "]" {
                if stak.isEmpty { /* error = "too many ]s" */ return nil }
                let f = stak.removeLast()
                jump[f] = i
                jump[i] = f
            }
        }
        if !stak.isEmpty { /* error = "too many [s" */ return nil }
        self.jump = jump
    }
    public mutating func reset() {
        data = [CChar](repeating: CChar(0), count:Self.datasize);
        ibuf = [CChar]()
        obuf = [CChar]()
        (pc, sp) = (0, 0)
    }
    public mutating func step() -> Bool {
        guard 0 <= pc && pc < code.count else { return false }
        switch UnicodeScalar(Int(code[pc])) {
        case ">": sp += 1
        case "<": sp -= 1
        case "+": data[sp] += 1
        case "-": data[sp] -= 1
        case "[": if data[sp] == CChar(0) { pc = jump[pc]! }
        case "]": if data[sp] != CChar(0) { pc = jump[pc]! }
        case ".": obuf.append(data[sp])
        case ",":
            if ibuf.isEmpty {
                return false
            } else {
                data[sp] = ibuf.removeFirst()
            }
        default:
            return false
        }
        pc += 1
        return true;
    }
    public mutating func run(input:String = "") -> String {
        reset()
        ibuf = input.utf8.map{CChar($0)}
        while self.step() {}
        obuf.append(CChar(0)) // \0 Terminate
        return String(cString:&obuf)
    }
    public static func compile(src:String) -> String {
        var lines = [
            "import Darwin",
            "var data = [CChar](repeating: CChar(0), count:\(Self.datasize))",
            "var (sp, pc) = (0, 0)",
        ];
        for c in src {
            switch c {
            case ">": lines.append("sp+=1")
            case "<": lines.append("sp-=1")
            case "+": lines.append("data[sp]+=1")
            case "-": lines.append("data[sp]-=1")
            case "[": lines.append("while data[sp] != CChar(0) {")
            case "]": lines.append("}")
            case ".": lines.append("putchar(Int32(data[sp]))")
            case ",": lines.append(
                "data[sp] = {c in CChar(c < 0 ? 0 : c)}(getchar())"
                )
            default:
                continue
            }
        }
        lines.append("")
        return lines.joined(separator: "\n")
    }
}
