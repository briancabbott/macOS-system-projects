from PyObjCTools.TestSupport import *
import JavaScriptCore

class TestJavaScriptCore (TestCase):

    @expectedFailure
    def testMissing(self):
        self.fail("Tests for JavaScriptCore are missing")

if __name__ == "__main__":
    main()
