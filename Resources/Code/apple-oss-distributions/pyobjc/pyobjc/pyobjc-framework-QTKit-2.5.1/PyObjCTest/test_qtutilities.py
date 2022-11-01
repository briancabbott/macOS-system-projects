
from PyObjCTools.TestSupport import *
from QTKit import *

try:
    unicode
except NameError:
    unicode = str

try:
    long
except NameError:
    long = int

class TestQTUtilities (TestCase):
    def testFunctions(self):
        v = QTStringForOSType(15490)
        self.assertIsInstance(v, unicode)

        w = QTOSTypeForString(v)
        self.assertIsInstance(w, (int, long))
        self.assertEqual(w, 15490)


if __name__ == "__main__":
    main()
