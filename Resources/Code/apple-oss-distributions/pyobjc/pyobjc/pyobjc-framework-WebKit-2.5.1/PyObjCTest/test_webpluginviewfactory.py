
from PyObjCTools.TestSupport import *
from WebKit import *

try:
    unicode
except NameError:
    unicode = str

class TestWebPluginViewFactory (TestCase):
    def testConstants(self):
        self.assertIsInstance(WebPlugInBaseURLKey, unicode)
        self.assertIsInstance(WebPlugInAttributesKey, unicode)
        self.assertIsInstance(WebPlugInContainerKey, unicode)
        self.assertIsInstance(WebPlugInContainingElementKey, unicode)

if __name__ == "__main__":
    main()
