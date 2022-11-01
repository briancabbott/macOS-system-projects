from PyObjCTools.TestSupport import *
from CoreFoundation import *

try:
    unicode
except NameError:
    unicode = str


try:
    long
except NameError:
    long = int


class TestNumberFormatter (TestCase):
    def testTypes(self):
        self.assertIsCFType(CFNumberFormatterRef)

    def testTypeID(self):
        self.assertIsInstance(CFNumberFormatterGetTypeID(), (int, long))

    def testFuncs(self):
        locale = CFLocaleCopyCurrent()
        fmt = CFNumberFormatterCreate(None, locale, kCFNumberFormatterDecimalStyle)
        self.assertIsInstance(fmt, CFNumberFormatterRef)
        v = CFNumberFormatterGetLocale(fmt)
        self.assertIs(v, locale)
        v = CFNumberFormatterGetStyle(fmt)
        self.assertEqual(v , kCFNumberFormatterDecimalStyle)
        v = CFNumberFormatterGetFormat(fmt)
        self.assertIsInstance(v, unicode)
        CFNumberFormatterSetFormat(fmt, v[:-2])
        v2 = CFNumberFormatterGetFormat(fmt)
        self.assertEqual(v2 , v[:-2])
        v = CFNumberFormatterCreateStringWithNumber(None, fmt, 42.5)
        self.assertIsInstance(v, unicode)
        self.assertEqual(v , b'42.5'.decode('ascii'))
        num, rng = CFNumberFormatterCreateNumberFromString(None, fmt, b"42.0a".decode('ascii'), (0, 5), 0)
        self.assertEqual(num , 42.0)
        self.assertEqual(rng , (0, 4))
        num, rng = CFNumberFormatterCreateNumberFromString(None, fmt, b"42.0a".decode('ascii'), (0, 5), kCFNumberFormatterParseIntegersOnly)
        self.assertEqual(num , 42)
        self.assertEqual(rng , (0, 2))
        v = CFNumberFormatterCopyProperty(fmt, kCFNumberFormatterCurrencyCode)
        self.assertIsInstance(v, unicode)
        CFNumberFormatterSetProperty(fmt, kCFNumberFormatterCurrencyCode, b"HFL".decode('ascii'))

        self.assertResultIsCFRetained(CFNumberFormatterCopyProperty)
        v = CFNumberFormatterCopyProperty(fmt, kCFNumberFormatterCurrencyCode)
        self.assertEqual(v , b"HFL".decode('ascii'))
        self.assertArgIsOut(CFNumberFormatterGetDecimalInfoForCurrencyCode, 1)
        self.assertArgIsOut(CFNumberFormatterGetDecimalInfoForCurrencyCode, 2)
        ok, frac, rnd = CFNumberFormatterGetDecimalInfoForCurrencyCode("EUR", None, None)
        self.assertEqual(ok, True)
        self.assertEqual(frac, 2)
        self.assertEqual(rnd, 0.0)

    def testConstants(self):
        self.assertEqual(kCFNumberFormatterNoStyle, 0)
        self.assertEqual(kCFNumberFormatterDecimalStyle, 1)
        self.assertEqual(kCFNumberFormatterCurrencyStyle, 2)
        self.assertEqual(kCFNumberFormatterPercentStyle, 3)
        self.assertEqual(kCFNumberFormatterScientificStyle, 4)
        self.assertEqual(kCFNumberFormatterSpellOutStyle, 5)

        self.assertEqual(kCFNumberFormatterParseIntegersOnly, 1)

        self.assertEqual(kCFNumberFormatterRoundCeiling, 0)
        self.assertEqual(kCFNumberFormatterRoundFloor, 1)
        self.assertEqual(kCFNumberFormatterRoundDown, 2)
        self.assertEqual(kCFNumberFormatterRoundUp, 3)
        self.assertEqual(kCFNumberFormatterRoundHalfEven, 4)
        self.assertEqual(kCFNumberFormatterRoundHalfDown, 5)
        self.assertEqual(kCFNumberFormatterRoundHalfUp, 6)

        self.assertEqual(kCFNumberFormatterPadBeforePrefix, 0)
        self.assertEqual(kCFNumberFormatterPadAfterPrefix, 1)
        self.assertEqual(kCFNumberFormatterPadBeforeSuffix, 2)
        self.assertEqual(kCFNumberFormatterPadAfterSuffix, 3)

        self.assertIsInstance(kCFNumberFormatterCurrencyCode, unicode)
        self.assertIsInstance(kCFNumberFormatterDecimalSeparator, unicode)
        self.assertIsInstance(kCFNumberFormatterCurrencyDecimalSeparator, unicode)
        self.assertIsInstance(kCFNumberFormatterAlwaysShowDecimalSeparator, unicode)
        self.assertIsInstance(kCFNumberFormatterGroupingSeparator, unicode)
        self.assertIsInstance(kCFNumberFormatterUseGroupingSeparator, unicode)
        self.assertIsInstance(kCFNumberFormatterPercentSymbol, unicode)
        self.assertIsInstance(kCFNumberFormatterZeroSymbol, unicode)
        self.assertIsInstance(kCFNumberFormatterNaNSymbol, unicode)
        self.assertIsInstance(kCFNumberFormatterInfinitySymbol, unicode)
        self.assertIsInstance(kCFNumberFormatterMinusSign, unicode)
        self.assertIsInstance(kCFNumberFormatterPlusSign, unicode)
        self.assertIsInstance(kCFNumberFormatterCurrencySymbol, unicode)
        self.assertIsInstance(kCFNumberFormatterExponentSymbol, unicode)
        self.assertIsInstance(kCFNumberFormatterMinIntegerDigits, unicode)
        self.assertIsInstance(kCFNumberFormatterMaxIntegerDigits, unicode)
        self.assertIsInstance(kCFNumberFormatterMinFractionDigits, unicode)
        self.assertIsInstance(kCFNumberFormatterMaxFractionDigits, unicode)
        self.assertIsInstance(kCFNumberFormatterGroupingSize, unicode)
        self.assertIsInstance(kCFNumberFormatterSecondaryGroupingSize, unicode)
        self.assertIsInstance(kCFNumberFormatterRoundingMode, unicode)
        self.assertIsInstance(kCFNumberFormatterRoundingIncrement, unicode)
        self.assertIsInstance(kCFNumberFormatterFormatWidth, unicode)
        self.assertIsInstance(kCFNumberFormatterPaddingPosition, unicode)
        self.assertIsInstance(kCFNumberFormatterPaddingCharacter, unicode)
        self.assertIsInstance(kCFNumberFormatterDefaultFormat, unicode)
        self.assertIsInstance(kCFNumberFormatterMultiplier, unicode)
        self.assertIsInstance(kCFNumberFormatterPositivePrefix, unicode)
        self.assertIsInstance(kCFNumberFormatterPositiveSuffix, unicode)
        self.assertIsInstance(kCFNumberFormatterNegativePrefix, unicode)
        self.assertIsInstance(kCFNumberFormatterNegativeSuffix, unicode)
        self.assertIsInstance(kCFNumberFormatterPerMillSymbol, unicode)
        self.assertIsInstance(kCFNumberFormatterInternationalCurrencySymbol, unicode)

    @min_os_level('10.5')
    def testConstants10_5(self):
        self.assertIsInstance(kCFNumberFormatterCurrencyGroupingSeparator, unicode)
        self.assertIsInstance(kCFNumberFormatterIsLenient, unicode)
        self.assertIsInstance(kCFNumberFormatterUseSignificantDigits, unicode)
        self.assertIsInstance(kCFNumberFormatterMinSignificantDigits, unicode)
        self.assertIsInstance(kCFNumberFormatterMaxSignificantDigits, unicode)

if __name__ == "__main__":
    main()
