'''
Some simple tests to check that the framework is properly wrapped.
'''
import objc
from PyObjCTools.TestSupport import *
import WebKit

try:
    unicode
except NameError:
    unicode = str

try:
    long
except NameError:
    long = int

class TestWebKit (TestCase):
    def testClasses(self):
        self.assert_( hasattr(WebKit, 'WebResource') )
        self.assert_( isinstance(WebKit.WebResource, objc.objc_class) )

        self.assert_( hasattr(WebKit, 'DOMHTMLObjectElement') )
        self.assert_( isinstance(WebKit.DOMHTMLObjectElement, objc.objc_class) )

    def testValues(self):
        self.assert_( hasattr(WebKit, 'DOM_CSS_PERCENTAGE') )
        self.assert_( isinstance(WebKit.DOM_CSS_PERCENTAGE, (int, long)) )
        self.assertEquals(WebKit.DOM_CSS_PERCENTAGE, 2)

        self.assert_( hasattr(WebKit, 'DOM_CSS_VALUE_LIST') )
        self.assert_( isinstance(WebKit.DOM_CSS_VALUE_LIST, (int, long)) )
        self.assertEquals(WebKit.DOM_CSS_VALUE_LIST, 2)

        self.assert_( hasattr(WebKit, 'WebViewInsertActionDropped') )
        self.assert_( isinstance(WebKit.WebViewInsertActionDropped, (int, long)) )


    def testVariables(self):
        self.assert_( hasattr(WebKit, 'DOMRangeException') )
        self.assert_( isinstance(WebKit.DOMRangeException, unicode) )

    @onlyOn32Bit
    def testFunctions(self):
        self.assert_( hasattr(WebKit, 'WebConvertNSImageToCGImageRef') )
        self.assert_(isinstance(WebKit.WebConvertNSImageToCGImageRef, objc.function) )

    def testProtocols(self):
        objc.protocolNamed('DOMEventListener')
        objc.protocolNamed('DOMEventTarget')
        objc.protocolNamed('DOMNodeFilter')
        objc.protocolNamed('DOMXPathNSResolver')
        objc.protocolNamed('WebDocumentRepresentation')
        objc.protocolNamed('WebDocumentSearching')
        objc.protocolNamed('WebDocumentText')
        objc.protocolNamed('WebDocumentView')
        objc.protocolNamed('WebOpenPanelResultListener')
        objc.protocolNamed('WebPlugInViewFactory')
        objc.protocolNamed('WebPolicyDecisionListener')


if __name__ == "__main__":
    main()
