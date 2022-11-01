"""
Tests for the proxy of Python numbers

NOTE: Decimal conversion is not tested, the required proxy is part of
the Foundation bindings :-(
"""
from __future__ import unicode_literals
import sys, os
from PyObjCTools.TestSupport import *
from PyObjCTest.fnd import NSNumber, NSNumberFormatter
from PyObjCTest.pythonnumber import OC_TestNumber
import objc


if sys.version_info[0] == 3:
    unicode = str
    long = int

OC_PythonNumber = objc.lookUpClass("OC_PythonNumber")
try:
    NSCFNumber = objc.lookUpClass("__NSCFNumber")
except objc.error:
    NSCFNumber = objc.lookUpClass("NSCFNumber")


NSOrderedAscending = -1
NSOrderedSame = 0
NSOrderedDescending = 1

class TestNSNumber (TestCase):
    # These testcases check the behaviour of NSNumber, these
    # are mostly here to verify that NSNumbers behave as
    # we expect them to.

    def testClass(self):
        for m in ('numberWithInt_', 'numberWithFloat_', 'numberWithDouble_', 'numberWithShort_'):
            v = getattr(NSNumber, m)(0)
            self.assertIsInstance(v, NSNumber)
            self.assertIsNotInstance(v, OC_PythonNumber)
            self.assertIs(OC_TestNumber.numberClass_(v), NSCFNumber)

    def testDecimal(self):
        NSDecimalNumber = objc.lookUpClass("NSDecimalNumber")
        v = NSDecimalNumber.numberWithInt_(10)
        self.assertIsInstance(v, NSDecimalNumber)

        from objc._pythonify import numberWrapper
        o = numberWrapper(v)
        self.assertIs(o, v)

    def testLongValue(self):
        v = NSNumber.numberWithUnsignedLongLong_(2 ** 63 + 5000)
        self.assertIsInstance(v, long)

        if os_release() <= (10, 5):
            self.assertEqual(v.description(), str(-2**63+5000))
        else:
            self.assertEqual(v.description(), str(2**63+5000))

        self.assertIsNot(type(v), long)

        self.assertRaises(AttributeError, setattr, v, 'x', 42)

    def testEdgeCases(self):
        from objc._pythonify import numberWrapper

        n = objc.lookUpClass('NSObject').alloc().init()

        with filterWarnings("error", RuntimeWarning):
            self.assertRaises(RuntimeWarning, numberWrapper, n)

        with filterWarnings("ignore", RuntimeWarning):
            self.assertIs(numberWrapper(n), n)


        # Fake number class, to ensure that all of
        # numberWrapper can be tested with a 64-bit runtime
        class Number (objc.lookUpClass("NSObject")):
            def objCType(self):
                return objc._C_INT

            def longValue(self):
                return 42

        n = Number.alloc().init()
        v = numberWrapper(n)
        self.assertEqual(v, 42)
        self.assertIs(v.__pyobjc_object__, n)

    def testPickling(self):
        v = {
            'long': NSNumber.numberWithUnsignedLongLong_(2 ** 63 + 5000),
            'int':  NSNumber.numberWithInt_(42),
            'float': NSNumber.numberWithDouble_(2.0),
        }
        import pickle
        data = pickle.dumps(v)

        w = pickle.loads(data)
        if os_release() <= (10, 5):
            self.assertEqual(w, {
                'long': -2**63 + 5000,
                'int': 42,
                'float': 2.0,
            })
        else:
            self.assertEqual(w, {
                'long': 2**63 + 5000,
                'int': 42,
                'float': 2.0,
            })

        for o in v.values():
            self.assertTrue(hasattr(o, '__pyobjc_object__'))

        for o in w.values():
            self.assertFalse(hasattr(o, '__pyobjc_object__'))

    def testShortConversions(self):
        v = NSNumber.numberWithShort_(42)

        self.assertEqual(v.stringValue(), '42')

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsFloat_(v), 42.0)
        self.assertEqual(OC_TestNumber.numberAsDouble_(v), 42.0)


    def testIntConversions(self):
        v = NSNumber.numberWithInt_(42)

        self.assertEqual(v.stringValue(), '42')

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsFloat_(v), 42.0)
        self.assertEqual(OC_TestNumber.numberAsDouble_(v), 42.0)

        # Negative values
        v = NSNumber.numberWithInt_(-42)

        self.assertEqual(v.stringValue(), '-42')

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 214)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 65494)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 4294967254)

        if sys.maxsize == (2 ** 31) -1:
            self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 4294967254)
        else:
            self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 18446744073709551574)

        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 18446744073709551574)
        self.assertEqual(OC_TestNumber.numberAsFloat_(v), -42.0)
        self.assertEqual(OC_TestNumber.numberAsDouble_(v), -42.0)

        # Overflow
        v = NSNumber.numberWithInt_(892455)

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), 39)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), -25049)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 39)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 40487)

    def testDoubleConversions(self):
        v = NSNumber.numberWithDouble_(75.5)
        self.assertEqual(v.stringValue(), '75.5')

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsFloat_(v), 75.5)
        self.assertEqual(OC_TestNumber.numberAsDouble_(v), 75.5)

        # Negative values
        v = NSNumber.numberWithDouble_(-127.6)
        self.assertEqual(v.stringValue(), '-127.6')

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), -127)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), -127)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), -127)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), -127)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), -127)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 129)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 65409)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 4294967169)

        if sys.maxsize == (2 ** 31) -1:
            self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 4294967169)
        else:
            self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 18446744073709551488)

        # The first entry in the tuple below is incorrect, that happens to be what
        # is returned by NSNumber on some platforms (in particular, any Python where
        # the python framework itself is linked against the 10.4 SDK)
        #
        #   double v = -127.6;
        #   unsigned long long lv = v;
        #   printf("%llu\n", lv);
        #

        self.assertIn(
                OC_TestNumber.numberAsUnsignedLongLong_(v),
                    (18446744073709551489, 18446744073709551488))

        self.assertEqual(OC_TestNumber.numberAsDouble_(v), -127.6)

        # Overflow
        v = NSNumber.numberWithDouble_(float(2**64 + 99))

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)

        if sys.byteorder == 'big':
            self.assertEqual(OC_TestNumber.numberAsChar_(v), -1)
            self.assertEqual(OC_TestNumber.numberAsShort_(v), -1)
            self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 255)
            self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 65535)
        else:
            self.assertEqual(OC_TestNumber.numberAsChar_(v), 0)
            self.assertEqual(OC_TestNumber.numberAsShort_(v), 0)
            self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 0)
            self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 0)

    def testCompare(self):
        self.assertEqual(OC_TestNumber.compareA_andB_(NSNumber.numberWithLong_(0), NSNumber.numberWithLong_(1)), NSOrderedAscending)
        self.assertEqual(OC_TestNumber.compareA_andB_(NSNumber.numberWithLong_(0), NSNumber.numberWithUnsignedLongLong_(2**40)), NSOrderedAscending)
        self.assertEqual(OC_TestNumber.compareA_andB_(NSNumber.numberWithLong_(0), NSNumber.numberWithDouble_(42.0)), NSOrderedAscending)

        self.assertEqual(OC_TestNumber.compareA_andB_(NSNumber.numberWithLong_(0), NSNumber.numberWithLong_(-1)), NSOrderedDescending)
        self.assertEqual(OC_TestNumber.compareA_andB_(NSNumber.numberWithLong_(0), NSNumber.numberWithLongLong_(-2**60)), NSOrderedDescending)
        self.assertEqual(OC_TestNumber.compareA_andB_(NSNumber.numberWithLong_(0), NSNumber.numberWithDouble_(-42.0)), NSOrderedDescending)

        self.assertEqual(OC_TestNumber.compareA_andB_(NSNumber.numberWithLong_(0), NSNumber.numberWithLong_(0)), NSOrderedSame)
        self.assertEqual(OC_TestNumber.compareA_andB_(NSNumber.numberWithLong_(0), NSNumber.numberWithDouble_(0.0)), NSOrderedSame)
        self.assertEqual(OC_TestNumber.compareA_andB_(NSNumber.numberWithLong_(0), NSNumber.numberWithLongLong_(0)), NSOrderedSame)

    def testDescription(self):
        v = OC_TestNumber.numberDescription_(NSNumber.numberWithInt_(0))
        self.assertIsInstance(v, unicode)
        self.assertEqual(v, "0")

        v = OC_TestNumber.numberDescription_(NSNumber.numberWithLongLong_(2**60))
        self.assertIsInstance(v, unicode)
        self.assertEqual(v, unicode(str(2**60)))

        v = OC_TestNumber.numberDescription_(NSNumber.numberWithLongLong_(-2**60))
        self.assertIsInstance(v, unicode)
        self.assertEqual(v, unicode(str(-2**60)))

        v = OC_TestNumber.numberDescription_(NSNumber.numberWithDouble_(264.0))
        self.assertIsInstance(v, unicode)
        self.assertEqual(v, "264")


class TestPyNumber (TestCase):
    # Basic tests of the proxy methods

    def testClasses(self):
        # Ensure that python numbers are proxied using the right proxy type
        for v in (0, 1, 2**32+1, 2**64+1, 42.5):
            self.assertIs(OC_TestNumber.numberClass_(v), OC_PythonNumber)

        # The booleans True and False must be proxied as the corresponding
        # NSNumber constants, otherwise lowlevel Cocoa/CoreFoundation code
        # get's upset.
        try:
            boolClass = objc.lookUpClass('__NSCFBoolean')
        except objc.error:
            boolClass = objc.lookUpClass('NSCFBoolean')

        for v in (True, False):
            self.assertIs(OC_TestNumber.numberClass_(v), boolClass)
            self.assertIs(objc.repythonify(v), v)


    def testPythonIntConversions(self):
        # Conversions to other values. Note that values are converted
        # using C casts, without any exceptions when converting a
        # negative value to an unsigned one and without exceptions for
        # overflow.
        v = 42

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsFloat_(v), 42.0)
        self.assertEqual(OC_TestNumber.numberAsDouble_(v), 42.0)

        # Negative values
        v = -42

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 214)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 65494)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 4294967254)

        if sys.maxsize == (2 ** 31) -1:
            self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 4294967254)
        else:
            self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 18446744073709551574)

        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 18446744073709551574)
        self.assertEqual(OC_TestNumber.numberAsFloat_(v), -42.0)
        self.assertEqual(OC_TestNumber.numberAsDouble_(v), -42.0)

        # Overflow
        v = 892455

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), 39)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), -25049)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 39)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 40487)

    def testPythonLongConversions(self):
        if sys.version_info[0] == 2:
            v = long(42)
            self.assertIsInstance(v, long)
        else:
            v = 42

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 42)
        self.assertEqual(OC_TestNumber.numberAsFloat_(v), 42.0)
        self.assertEqual(OC_TestNumber.numberAsDouble_(v), 42.0)

        # Negative values
        if sys.version_info[0] == 2:
            v = long(-42)
            self.assertIsInstance(v, long)
        else:
            v = -42

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), -42)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 214)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 65494)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 4294967254)

        if sys.maxsize == (2 ** 31) -1:
            self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 4294967254)
        else:
            self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 18446744073709551574)

        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 18446744073709551574)
        self.assertEqual(OC_TestNumber.numberAsFloat_(v), -42.0)
        self.assertEqual(OC_TestNumber.numberAsDouble_(v), -42.0)

        # Overflow
        if sys.version_info[0] == 2:
            v = long(892455)
            self.assertIsInstance(v, long)
        else:
            v = 892455

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), 39)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), -25049)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 39)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 40487)

        # Very much overflow
        v = 2 ** 64 + 1
        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 1)

    def testDoubleConversions(self):
        v = 75.5

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 75)
        self.assertEqual(OC_TestNumber.numberAsFloat_(v), 75.5)
        self.assertEqual(OC_TestNumber.numberAsDouble_(v), 75.5)

        # Negative values
        v = -127.6

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)
        self.assertEqual(OC_TestNumber.numberAsChar_(v), -127)
        self.assertEqual(OC_TestNumber.numberAsShort_(v), -127)
        self.assertEqual(OC_TestNumber.numberAsInt_(v), -127)
        self.assertEqual(OC_TestNumber.numberAsLong_(v), -127)
        self.assertEqual(OC_TestNumber.numberAsLongLong_(v), -127)

        self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 129)
        self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 65409)
        self.assertEqual(OC_TestNumber.numberAsUnsignedInt_(v), 4294967169)

        if sys.maxsize == (2 ** 31) -1:
            self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 4294967169)
        else:
            self.assertEqual(OC_TestNumber.numberAsUnsignedLong_(v), 18446744073709551489)

        if sys.byteorder == 'big':
            self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 4294967169)
        else:
            self.assertEqual(OC_TestNumber.numberAsUnsignedLongLong_(v), 18446744073709551489)

        self.assertEqual(OC_TestNumber.numberAsDouble_(v), -127.6)

        # Overflow
        v = float(2**64 + 99)

        self.assertEqual(OC_TestNumber.numberAsBOOL_(v), 1)

        if sys.byteorder == 'big':
            self.assertEqual(OC_TestNumber.numberAsChar_(v), -1)
            self.assertEqual(OC_TestNumber.numberAsShort_(v), -1)
            self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 255)
            self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 65535)
        else:
            self.assertEqual(OC_TestNumber.numberAsChar_(v), 0)
            self.assertEqual(OC_TestNumber.numberAsShort_(v), 0)
            self.assertEqual(OC_TestNumber.numberAsUnsignedChar_(v), 0)
            self.assertEqual(OC_TestNumber.numberAsUnsignedShort_(v), 0)

    def testCompare(self):
        self.assertEqual(OC_TestNumber.compareA_andB_(0, 1), NSOrderedAscending)
        self.assertEqual(OC_TestNumber.compareA_andB_(0, 2**64), NSOrderedAscending)
        self.assertEqual(OC_TestNumber.compareA_andB_(0, 42.0), NSOrderedAscending)

        self.assertEqual(OC_TestNumber.compareA_andB_(0, -1), NSOrderedDescending)
        self.assertEqual(OC_TestNumber.compareA_andB_(0, -2**64), NSOrderedDescending)
        self.assertEqual(OC_TestNumber.compareA_andB_(0, -42.0), NSOrderedDescending)

        self.assertEqual(OC_TestNumber.compareA_andB_(0, 0), NSOrderedSame)
        self.assertEqual(OC_TestNumber.compareA_andB_(0, 0.0), NSOrderedSame)
        if sys.version_info[0] == 2:
            self.assertEqual(OC_TestNumber.compareA_andB_(0, long(0)), NSOrderedSame)

    def testNumberEqual(self):
        self.assertFalse(OC_TestNumber.number_isEqualTo_(0, 1))
        self.assertFalse(OC_TestNumber.number_isEqualTo_(0, 2**64))
        self.assertFalse(OC_TestNumber.number_isEqualTo_(0, 42.0))

        self.assertFalse(OC_TestNumber.number_isEqualTo_(0, -1))
        self.assertFalse(OC_TestNumber.number_isEqualTo_(0, -2**64))
        self.assertFalse(OC_TestNumber.number_isEqualTo_(0, -42.0))

        self.assertTrue(OC_TestNumber.number_isEqualTo_(0, 0))
        self.assertTrue(OC_TestNumber.number_isEqualTo_(0, 0.0))
        if sys.version_info[0] == 2:
            self.assertTrue(OC_TestNumber.number_isEqualTo_(0, long(0)))

    def testDescription(self):
        v = OC_TestNumber.numberDescription_(0)
        self.assertIsInstance(v, unicode)
        self.assertEqual(v, "0")

        v = OC_TestNumber.numberDescription_(2**64)
        self.assertIsInstance(v, unicode)
        self.assertEqual(v, unicode(repr(2**64)))

        v = OC_TestNumber.numberDescription_(-2**64)
        self.assertIsInstance(v, unicode)
        self.assertEqual(v, unicode(repr(-2**64)))

        v = OC_TestNumber.numberDescription_(264.0)
        self.assertIsInstance(v, unicode)
        self.assertEqual(v, "264.0")

        v = OC_TestNumber.numberDescription_(False)
        self.assertIsInstance(v, unicode)
        self.assertEqual(v, "0")

        v = OC_TestNumber.numberDescription_(True)
        self.assertIsInstance(v, unicode)
        self.assertEqual(v, "1")

class TestInteractions (TestCase):
    # Test interactions between Python and NSNumber numbers

    def testMixedCompare(self):
        # compare for:
        #   - python number to nsnumber
        #   - nsnumber to python number
        # For: (bool, int, long, float) vs (char, short, ...)
        methods = [
                'numberWithInt_',
                'numberWithChar_',
                'numberWithLong_',
                'numberWithDouble_',
            ]

        self.assertEqual(OC_TestNumber.compareA_andB_(42, 42), NSOrderedSame)
        for m in methods:
            self.assertEqual(OC_TestNumber.compareA_andB_(getattr(NSNumber, m)(42), 42), NSOrderedSame)
            self.assertEqual(OC_TestNumber.compareA_andB_(42, getattr(NSNumber, m)(42)), NSOrderedSame)

        self.assertEqual(OC_TestNumber.compareA_andB_(42, 99), NSOrderedAscending)
        for m in methods:
            self.assertEqual(OC_TestNumber.compareA_andB_(getattr(NSNumber, m)(42), 99), NSOrderedAscending)
            self.assertEqual(OC_TestNumber.compareA_andB_(42, getattr(NSNumber, m)(99)), NSOrderedAscending)

    def testMixedEquals(self):
        # isEqualToNumber for:
        #   - python number to nsnumber
        #   - nsnumber to python number
        # For: (bool, int, long, float) vs (char, short, ...)
        self.assertTrue(OC_TestNumber.number_isEqualTo_(0, NSNumber.numberWithInt_(0)))
        self.assertTrue(OC_TestNumber.number_isEqualTo_(0, NSNumber.numberWithLong_(0)))
        self.assertTrue(OC_TestNumber.number_isEqualTo_(0, NSNumber.numberWithFloat_(0)))
        self.assertTrue(OC_TestNumber.number_isEqualTo_(NSNumber.numberWithInt_(0), 0))
        self.assertTrue(OC_TestNumber.number_isEqualTo_(NSNumber.numberWithLong_(0), 0))
        self.assertTrue(OC_TestNumber.number_isEqualTo_(NSNumber.numberWithFloat_(0), 0))

        self.assertFalse(OC_TestNumber.number_isEqualTo_(42, NSNumber.numberWithInt_(0)))
        self.assertFalse(OC_TestNumber.number_isEqualTo_(42, NSNumber.numberWithLong_(0)))
        self.assertFalse(OC_TestNumber.number_isEqualTo_(42, NSNumber.numberWithFloat_(0)))
        self.assertFalse(OC_TestNumber.number_isEqualTo_(NSNumber.numberWithInt_(0), 42))
        self.assertFalse(OC_TestNumber.number_isEqualTo_(NSNumber.numberWithLong_(0), 42))
        self.assertFalse(OC_TestNumber.number_isEqualTo_(NSNumber.numberWithFloat_(0), 42))


class TestNumberFormatter (TestCase):
    # Test behaviour of an NSNumberFormatter, both with
    # Python numbers and NSNumbers
    def testFormatting(self):
        formatter = NSNumberFormatter.alloc().init()

        n = NSNumber.numberWithInt_(42)
        p = 42
        self.assertEqual(formatter.stringForObjectValue_(n), formatter.stringForObjectValue_(p))

        n = NSNumber.numberWithInt_(-42)
        p = -42
        self.assertEqual(formatter.stringForObjectValue_(n), formatter.stringForObjectValue_(p))


        n = NSNumber.numberWithDouble_(10.42)
        p = 10.42
        self.assertEqual(formatter.stringForObjectValue_(n), formatter.stringForObjectValue_(p))

if __name__ == "__main__":
    main()
