
from PyObjCTools.TestSupport import *
from WebKit import *

class TestDOMHTMLHRElement (TestCase):
    def testMethods(self):
        self.assertResultIsBOOL(DOMHTMLHRElement.noShade)
        self.assertArgIsBOOL(DOMHTMLHRElement.setNoShade_, 0)

if __name__ == "__main__":
    main()
