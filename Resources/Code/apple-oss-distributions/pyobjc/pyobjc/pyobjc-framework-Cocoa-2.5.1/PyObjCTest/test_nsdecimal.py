#
# Some tests for NSDecimal (C type) and NSDecimalNumber (Objective-C class)
#
from PyObjCTools.TestSupport import *
from Foundation import *
import operator
import objc
import sys

try:
    long
except NameError:
    long = int

if 0:
    class TestNSDecimal (TestCase):
        def testConstants(self):
            self.assertEqual(NSRoundPlain, 0)
            self.assertEqual(NSRoundDown, 1)
            self.assertEqual(NSRoundUp, 2)
            self.assertEqual(NSRoundBankers, 3)

            self.assertEqual(NSCalculationNoError, 0)
            self.assertEqual(NSCalculationLossOfPrecision, 1)
            self.assertEqual(NSCalculationUnderflow, 2)
            self.assertEqual(NSCalculationOverflow, 3)
            self.assertEqual(NSCalculationDivideByZero, 4)

            self.assertEqual(NSDecimalMaxSize, 8)
            self.assertEqual(NSDecimalNoScale, (2**15)-1)

        def testCreation(self):
            o = NSDecimal(b"1.25".decode('ascii'))
            self.assert_(isinstance(o, NSDecimal))
            self.assertEqual(str(o), b"1.25".decode('ascii'))

            o = NSDecimal(12345, -2, objc.YES)
            self.assert_(isinstance(o, NSDecimal))
            self.assertEqual(str(o), b"-123.45".decode('ascii'))

            o = NSDecimal()
            self.assert_(isinstance(o, NSDecimal))
            self.assert_(str(o) in (b"0".decode('ascii'), b"0.0".decode('ascii')))

            o = NSDecimal(1234)
            self.assert_(isinstance(o, NSDecimal))
            self.assertEqual(str(o), b"1234".decode('ascii'))

            o = NSDecimal(-1234)
            self.assert_(isinstance(o, NSDecimal))
            self.assertEqual(str(o), b"-1234".decode('ascii'))

            o = NSDecimal(long(1234))
            self.assert_(isinstance(o, NSDecimal))
            self.assertEqual(str(o), b"1234".decode('ascii'))

            o = NSDecimal(long(-1234))
            self.assert_(isinstance(o, NSDecimal))
            self.assertEqual(str(o), b"-1234".decode('ascii'))

            o = NSDecimal(1 << 64 - 1)

            # Explicit conversion is supported, but might not do
            # what a naive user expects...
            o = NSDecimal(1.1)
            self.assert_(isinstance(o, NSDecimal))
            self.assertEqual(str(o), repr(1.1))

            self.assertRaises(OverflowError, NSDecimal, 1 << 128)
            self.assertRaises(OverflowError, NSDecimal, -1 << 128)

        def testFunction(self):
            o = NSDecimal(b"1.5".decode('ascii'))
            p = NSDecimal(12345, -2, objc.YES)
            r = NSDecimal(b"-121.95".decode('ascii'))
            q = NSDecimal()

            NSDecimalAdd(q, o, p, NSRoundPlain)

            self.assertEqual(str(q), str(r))

            v = NSDecimalIsNotANumber(o)
            self.assertIs(v, False)
            v = NSDecimal()
            NSDecimalCopy(v, o)
            self.assertEqual(str(v), str(o))

            NSDecimalCompact(v)

            i = NSDecimalCompare(o, p)
            self.assertIsInstance(i, (int, long))

            NSDecimalRound(v, o, 0, NSRoundBankers)
            self.assertEqual(str(v), '2')

            t = NSDecimalNormalize(v, o, NSRoundBankers)
            self.assertEqual(t, NSCalculationNoError)
            self.assertEqual(str(v), '2.0')

            t = NSDecimalPower(v, o, 3, NSRoundBankers)
            self.assertEqual(t, NSCalculationNoError)
            self.assertEqual(str(v), '3.375')

            t = NSDecimalString(v, None)
            self.assertEqual(t, b'3.375'.decode('ascii'))

        def testCompare(self):
            small = NSDecimal(b"1".decode('ascii'))
            small2 = NSDecimal(b"1".decode('ascii'))
            large = NSDecimal(b"42".decode('ascii'))

            self.assert_(small == small2)
            self.assert_(not (small == large))
            self.assert_(not (small != small2))
            self.assert_(small < large)
            self.assert_(not(large < small))
            self.assert_(not(small < small))
            self.assert_(small <= large)
            self.assert_(small <= small)
            self.assert_(not(large <= small))
            self.assert_(large > small)
            self.assert_(not(small > large))
            self.assert_(not(large > large))
            self.assert_(large >= small)
            self.assert_(large >= large)
            self.assert_(not(small >= large))

        def testConversion(self):
            o = NSDecimal(b"1234.44".decode('ascii'))
            self.assertEqual(o.as_int(), 1234)

            o = NSDecimal(b"1.5".decode('ascii'))
            self.assertEqual(o.as_float(), 1.5)

            self.assertRaises(TypeError, int, o)
            self.assertRaises(TypeError, float, o)

        def testCreateFromFloat(self):
            o = NSDecimal(1.1)
            self.assertAlmostEquals(o.as_float(), 1.1)

        if not hasattr(TestCase, 'assertAlmostEquals'):
            def assertAlmostEquals(self, val1, val2, eta=0.000001):
                self.assert_(abs(val1 - val2) < eta)


    class TestNSDecimalNumber (TestCase):
        def testCreation1(self):
            o = NSDecimalNumber.decimalNumberWithString_(b"1.1234".decode('ascii'))
            self.assertEqual(o.description(), b"1.1234".decode('ascii'))

            p = o.decimalValue()
            self.assert_(isinstance(p, NSDecimal))
            self.assertEqual(str(p), b"1.1234".decode('ascii'))

        def testCreation2(self):
            p = NSDecimal(b"1.1234".decode('ascii'))
            o = NSDecimalNumber.decimalNumberWithDecimal_(p)
            self.assertEqual(o.description(), b"1.1234".decode('ascii'))

        def testCreation3(self):
            p = NSDecimal(b"1.1234".decode('ascii'))
            o = NSDecimalNumber.alloc().initWithDecimal_(p)
            self.assertEqual(o.description(), b"1.1234".decode('ascii'))

    class NSDecimalOperators (TestCase):
        def testCoerce(self):
            r = NSDecimal(1)

            v = coerce(r, r)
            self.assertEqual(v, (r, r))

            v = coerce(r, 2)
            self.assertEqual(v, (r, NSDecimal(2)))

            v = coerce(2, r)
            self.assertEqual(v, (NSDecimal(2), r))

            v = coerce(r, sys.maxint+2)
            self.assertEqual(v, (r, NSDecimal(sys.maxint+2)))

            v = coerce(sys.maxint+2, r)
            self.assertEqual(v, (NSDecimal(sys.maxint+2), r))

            t = NSDecimal(4).__pyobjc_object__
            self.assert_(isinstance(t, NSObject))
            v = coerce(t, r)
            self.assertEqual(v, (NSDecimal(4), r))

            v = coerce(r, t)
            self.assertEqual(v, (r, NSDecimal(4)))

            self.assertRaises(TypeError, coerce, 1.0, r)
            self.assertRaises(TypeError, coerce, r, 1.0)
            self.assertRaises(TypeError, coerce, "1.0", r)
            self.assertRaises(TypeError, coerce, r, "1.0")
            self.assertRaises(TypeError, coerce, b"1.0".decode('ascii'), r)
            self.assertRaises(TypeError, coerce, r, b"1.0".decode('ascii'))
            self.assertRaises(TypeError, coerce, (), r)
            self.assertRaises(TypeError, coerce, r, ())


        def testAddition(self):
            r = NSDecimal()
            o = NSDecimal(1)
            p = NSDecimal(2)

            O = o.__pyobjc_object__
            P = p.__pyobjc_object__
            self.assert_(isinstance(P, NSObject))
            self.assert_(isinstance(O, NSObject))

            NSDecimalAdd(r, o, p, NSRoundPlain)
            self.assertEqual(o+p, r)
            self.assertEqual(o+P, r)
            self.assertEqual(O+p, r)
            self.assertEqual(o+2, r)
            self.assertEqual(o+long(2), r)
            self.assertEqual(p+1, r)
            self.assertEqual(1+p, r)

            self.assertRaises(TypeError, operator.add, o, 1.2)
            self.assertRaises(TypeError, operator.add, 1.2, o)
            self.assertRaises(TypeError, operator.add, o, "1.2")
            self.assertRaises(TypeError, operator.add, "1.2", o)
            self.assertRaises(TypeError, operator.add, o, b"1.2".decode('ascii'))
            self.assertRaises(TypeError, operator.add, b"1.2".decode('ascii'), o)
            self.assertRaises(TypeError, operator.add, o, [])
            self.assertRaises(TypeError, operator.add, [], o)

        def testSubtraction(self):
            r = NSDecimal()
            o = NSDecimal(1)
            p = NSDecimal(2)

            P = p.__pyobjc_object__
            O = o.__pyobjc_object__
            self.assert_(isinstance(P, NSObject))
            self.assert_(isinstance(O, NSObject))


            NSDecimalSubtract(r, o, p, NSRoundPlain)
            self.assertEqual(o-p, r)
            self.assertEqual(O-p, r)
            self.assertEqual(o-P, r)
            self.assertEqual(o-2, r)
            self.assertEqual(o-long(2), r)
            self.assertEqual(1-p, r)
            self.assertEqual(1-p, r)

            self.assertRaises(TypeError, operator.sub, o, 1.2)
            self.assertRaises(TypeError, operator.sub, 1.2, o)
            self.assertRaises(TypeError, operator.sub, o, "1.2")
            self.assertRaises(TypeError, operator.sub, "1.2", o)
            self.assertRaises(TypeError, operator.sub, o, b"1.2".decode('ascii'))
            self.assertRaises(TypeError, operator.sub, b"1.2".decode('ascii'), o)
            self.assertRaises(TypeError, operator.sub, o, ())
            self.assertRaises(TypeError, operator.sub, (), o)

        def testMultiplication(self):
            r = NSDecimal()
            o = NSDecimal(2)
            p = NSDecimal(3)

            P = p.__pyobjc_object__
            O = o.__pyobjc_object__
            self.assert_(isinstance(P, NSObject))
            self.assert_(isinstance(O, NSObject))

            NSDecimalMultiply(r, o, p, NSRoundPlain)
            self.assertEqual(o*p, r)
            self.assertEqual(O*p, r)
            self.assertEqual(o*P, r)
            self.assertEqual(o*3, r)
            self.assertEqual(o*long(3), r)
            self.assertEqual(2*p, r)
            self.assertEqual(2*p, r)

            self.assertRaises(TypeError, operator.mul, o, 1.2)
            self.assertRaises(TypeError, operator.mul, 1.2, o)

            NSDecimalMultiplyByPowerOf10(r, o, 4, NSRoundPlain)
            self.assertEqual(r, NSDecimal(20000))

        def testDivision(self):
            r = NSDecimal()
            o = NSDecimal(2)
            p = NSDecimal(3)

            P = p.__pyobjc_object__
            O = o.__pyobjc_object__
            self.assert_(isinstance(P, NSObject))
            self.assert_(isinstance(O, NSObject))

            NSDecimalDivide(r, o, p, NSRoundPlain)
            self.assertEqual(o/p, r)
            self.assertEqual(O/p, r)
            self.assertEqual(o/P, r)
            self.assertEqual(o/3, r)
            self.assertEqual(o/long(3), r)
            self.assertEqual(2/p, r)
            self.assertEqual(2/p, r)

            self.assertRaises(TypeError, operator.div, o, 1.2)
            self.assertRaises(TypeError, operator.div, 1.2, o)

        def testPositive(self):
            o = NSDecimal(2)
            p = NSDecimal(-2)

            self.assertEqual(+o, o)
            self.assertEqual(+p, p)

        def testNegative(self):
            o = NSDecimal(2)
            p = NSDecimal(-2)

            self.assertEqual(-o, p)
            self.assertEqual(-p, o)

        def testAbs(self):
            o = NSDecimal(2)
            p = NSDecimal(-2)

            self.assertEqual(abs(o), o)
            self.assertEqual(abs(p), o)

        def testBitwise(self):
            o = NSDecimal(2)
            p = NSDecimal(3)

            self.assertRaises(TypeError, operator.and_, o, p)
            self.assertRaises(TypeError, operator.or_, o, p)
            self.assertRaises(TypeError, operator.not_, o, p)

        def testPow(self):
            o = NSDecimal(2)
            p = NSDecimal(3)

            self.assertRaises(TypeError, pow, o, p)
            self.assertRaises(TypeError, pow, o, 2)
            self.assertRaises(TypeError, pow, 2, o)

        def testDivMod(self):
            o = NSDecimal(2)
            p = NSDecimal(3)

            self.assertRaises(TypeError, divmod, o, p)
            self.assertRaises(TypeError, divmod, o, 2)
            self.assertRaises(TypeError, divmod, 2, o)

        def testInplaceAddition(self):
            r = NSDecimal()
            o = NSDecimal(1)
            p = NSDecimal(2)

            P = p.__pyobjc_object__
            self.assert_(isinstance(P, NSObject))

            NSDecimalAdd(r, o, p, NSRoundPlain)

            o = NSDecimal(1)
            o += p
            self.assertEqual(o, r)

            o = NSDecimal(1)
            o += P
            self.assertEqual(o, r)

            o = NSDecimal(1)
            o += 2
            self.assertEqual(o, r)

            o = NSDecimal(1)
            o += long(2)
            self.assertEqual(o, r)

            o = 1
            o += p
            self.assertEqual(o, r)

            o = long(1)
            o += p
            self.assertEqual(o, r)

            try:
                o = 1.2
                o += p
                self.fail()
            except TypeError:
                pass

            try:
                o = NSDecimal(1)
                o += 1.2
                self.fail()
            except TypeError:
                pass

        def testInplaceSubtraction(self):
            r = NSDecimal()
            o = NSDecimal(1)
            p = NSDecimal(2)

            P = p.__pyobjc_object__
            self.assert_(isinstance(P, NSObject))

            NSDecimalSubtract(r, o, p, NSRoundPlain)

            o = NSDecimal(1)
            o -= p
            self.assertEqual(o, r)

            o = NSDecimal(1)
            o -= P
            self.assertEqual(o, r)


            o = NSDecimal(1)
            o -= 2
            self.assertEqual(o, r)

            o = NSDecimal(1)
            o -= 2
            self.assertEqual(o, r)

            o = 1
            o -= p
            self.assertEqual(o, r)

            o = 1
            o -= p
            self.assertEqual(o, r)

        def testInplaceMultiplication(self):
            r = NSDecimal()
            o = NSDecimal(2)
            p = NSDecimal(3)

            P = p.__pyobjc_object__
            self.assert_(isinstance(P, NSObject))

            NSDecimalMultiply(r, o, p, NSRoundPlain)

            o = NSDecimal(2)
            o *= p
            self.assertEqual(o, r)

            o = NSDecimal(2)
            o *= P
            self.assertEqual(o, r)

            o = NSDecimal(2)
            o *= 3
            self.assertEqual(o, r)

            o = NSDecimal(2)
            o *= long(3)
            self.assertEqual(o, r)

            o = 2
            o *= p
            self.assertEqual(o, r)

            o = long(2)
            o *= p
            self.assertEqual(o, r)

        def testInplaceDivision(self):
            r = NSDecimal()
            o = NSDecimal(2)
            p = NSDecimal(3)

            P = p.__pyobjc_object__
            self.assert_(isinstance(P, NSObject))

            NSDecimalDivide(r, o, p, NSRoundPlain)

            o = NSDecimal(2)
            o /= p
            self.assertEqual(o, r)

            o = NSDecimal(2)
            o /= P
            self.assertEqual(o, r)

            o = NSDecimal(2)
            o /= 3
            self.assertEqual(o, r)

            o = NSDecimal(2)
            o /= long(3)
            self.assertEqual(o, r)

            o = 2
            o /= p
            self.assertEqual(o, r)

            o = long(2)
            o /= p
            self.assertEqual(o, r)

    class NSDecimalNumberOperators (TestCase):
        def testAddition(self):
            r = NSDecimal()
            o = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(1))
            p = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(2))

            NSDecimalAdd(r, o.decimalValue(), p.decimalValue(), NSRoundPlain)
            self.assertEqual((o+p), r)
            self.assertEqual((o+2), r)
            self.assertEqual((o+long(2)), r)
            self.assertEqual((1+p), r)
            self.assertEqual((1+p), r)

            self.assertRaises(TypeError, operator.add, o, 1.2)
            self.assertRaises(TypeError, operator.add, 1.2, o)

            o = NSDecimalNumber.zero()
            self.assertRaises(TypeError, operator.add, o, 1.2)

        def testSubtraction(self):
            r = NSDecimal()
            o = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(1))
            p = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(2))

            NSDecimalSubtract(r, o.decimalValue(), p.decimalValue(), NSRoundPlain)
            self.assertEqual((o-p), r)
            self.assertEqual((o-2), r)
            self.assertEqual((o-long(2)), r)
            self.assertEqual((1-p), r)
            self.assertEqual((1-p), r)

            self.assertRaises(TypeError, operator.sub, o, 1.2)
            self.assertRaises(TypeError, operator.sub, 1.2, o)

        def testMultiplication(self):
            r = NSDecimal()
            o = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(2))
            p = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(3))

            NSDecimalMultiply(r, o.decimalValue(), p.decimalValue(), NSRoundPlain)
            self.assertEqual((o*p), r)
            self.assertEqual((o*3), r)
            self.assertEqual((o*long(3)), r)
            self.assertEqual((2*p), r)
            self.assertEqual((2*p), r)

            self.assertRaises(TypeError, operator.mul, o, 1.2)
            self.assertRaises(TypeError, operator.mul, 1.2, o)

        def testDivision(self):
            r = NSDecimal()
            o = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(2))
            p = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(3))

            NSDecimalDivide(r, o.decimalValue(), p.decimalValue(), NSRoundPlain)
            self.assertEqual((o/p), r)
            self.assertEqual((o/3), r)
            self.assertEqual((o/long(3)), r)
            self.assertEqual((2/p), r)
            self.assertEqual((2/p), r)

            self.assertRaises(TypeError, operator.div, o, 1.2)
            self.assertRaises(TypeError, operator.div, 1.2, o)

        def testPositive(self):
            o = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(2))
            p = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(-2))

            self.assertEqual((+o), o.decimalValue())
            self.assertEqual((+p), p.decimalValue())

        def testNegative(self):
            o = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(2))
            p = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(-2))

            self.assertEqual((-o), p.decimalValue())
            self.assertEqual((-p), o.decimalValue())

        def testAbs(self):
            o = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(2))
            p = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(-2))

            self.assertEqual(abs(o), o.decimalValue())
            self.assertEqual(abs(p), o.decimalValue())

        def testBitwise(self):
            o = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(2))
            p = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(-2))

            self.assertRaises(TypeError, operator.and_, o, p)
            self.assertRaises(TypeError, operator.or_, o, p)
            self.assertRaises(TypeError, operator.not_, o, p)

        def testPow(self):
            o = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(2))
            p = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(-2))

            self.assertRaises(TypeError, pow, o, p)
            self.assertRaises(TypeError, pow, o, 2)
            self.assertRaises(TypeError, pow, 2, o)

        def testDivMod(self):
            o = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(2))
            p = NSDecimalNumber.decimalNumberWithDecimal_(NSDecimal(-2))

            self.assertRaises(TypeError, divmod, o, p)
            self.assertRaises(TypeError, divmod, o, 2)
            self.assertRaises(TypeError, divmod, 2, o)


if __name__ == "__main__":
    main()
