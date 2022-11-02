import has_alias
@_exported import struct_with_operators

public func numeric(x: MyInt64) {}
public func conditional(x: AliasWrapper.Boolean) {}
public func longInt(x: Int.EspeciallyMagicalInt) {}

public func numericArray(x: IntSlice) {}


public protocol ExtraIncrementable {
  prefix func +++(inout base: Self)
}

extension SpecialInt : ExtraIncrementable {}

public protocol DefaultInitializable {
  init()
}

extension SpecialInt : DefaultInitializable {}
