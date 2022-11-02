// imported and re-exported
//
// cf. https://github.com/apple/swift/blob/master/docs/Modules.rst#modules-can-re-export-other-modules

// import everything
@_exported import BigNum
@_exported import Complex
@_exported import Interval
// import just a protocol
@_exported import protocol FloatingPointMath.FloatingPointMath

// placeholder
public class PONS {}
//

extension BigRational: IntervalElement {}
extension BigFloat   : IntervalElement {}

