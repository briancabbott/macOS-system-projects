
from PyObjCTools.TestSupport import *
from CoreText import *

try:
    unicode
except NameError:
    unicode = str

try:
    long
except NameError:
    long = int

class TestCTTextTab (TestCase):
    def testTypes(self):
        self.assertIsCFType(CTTextTabRef)

    def testConstants(self):
        self.assertIsInstance(kCTTabColumnTerminatorsAttributeName, unicode)

    def testFunctions(self):
        self.assertIsInstance(CTTextTabGetTypeID(), (int, long))

        tab = CTTextTabCreate(kCTCenterTextAlignment, 10.5, {
            kCTTabColumnTerminatorsAttributeName: CFCharacterSetCreateWithCharactersInString(None, b".".decode('latin1'))
        })
        self.assertIsInstance(tab, CTTextTabRef)

        v = CTTextTabGetAlignment(tab)
        self.assertEqual(v , kCTCenterTextAlignment)

        v = CTTextTabGetLocation(tab)
        self.assertEqual(v , 10.5)

        v = CTTextTabGetOptions(tab)
        self.assertIsInstance(v, dict)


if __name__ == "__main__":
    main()
