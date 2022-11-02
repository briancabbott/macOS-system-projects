// Should we use CGSize instead?
public struct Size {
    public var width: Int
    public var height: Int
    
    public static let zero = Size(width: 0, height: 0)
    
    public init(width: Int, height: Int) {
        self.width = width
        self.height = height
    }
}

extension Size: Equatable {
}
