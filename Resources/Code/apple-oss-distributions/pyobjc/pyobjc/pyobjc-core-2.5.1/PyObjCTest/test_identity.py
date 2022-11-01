# Some tests that verify that object identity is correctly retained
# accross the bridge
#
# TODO:
# - Add unittests here for every class that is treated specially by
#   the bridge.
# - Add unittests that test for "real-world" scenarios (writing stuff
#   to plists, ...)
# - Implement the required functionality
from __future__ import unicode_literals

import sys, os
import objc
from PyObjCTest.identity import *
from PyObjCTools.TestSupport import *

if sys.version_info[0] == 3:
    unicode = str

class TestPythonRoundTrip (TestCase):
    # TODO: verify

    def testBasicPython(self):

        container = OC_TestIdentity.alloc().init()

        for v in (self, object(), list, sum):
            container.setStoredObject_(v)
            self.assertTrue(v is container.storedObject(), repr(v))


    def testPythonContainer(self):

        container = OC_TestIdentity.alloc().init()

        for v in ( [1, 2,3], (1,2,3), {1:2, 3:4} ):
            container.setStoredObject_(v)
            self.assertTrue(v is container.storedObject(), repr(v))

    def dont_testPythonStrings(self):
        # XXX: this test would always fail, this is by design.

        container = OC_TestIdentity.alloc().init()

        for v in ( "Hello world", b"Hello world" ):
            container.setStoredObject_(v)
            self.assertTrue(v is container.storedObject(), repr(v))

    def dont_testPythonNumber(self):
        # XXX: this test would always fail, need to move some code to C
        # to fix (but not now)
        container = OC_TestIdentity.alloc().init()

        for v in (99999, sys.maxsize * 3, 10.0):
            container.setStoredObject_(v)
            self.assertTrue(v is container.storedObject, repr(v))


class ObjCRoundTrip (TestCase):
    # TODO: NSProxy

    def testNSObject(self):
        container = OC_TestIdentity.alloc().init()

        cls = objc.lookUpClass("NSObject")
        container.setStoredObjectToResultOf_on_("new", cls)

        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

        container.setStoredObjectToResultOf_on_("class", cls)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

    def testNSString(self):

        container = OC_TestIdentity.alloc().init()

        cls = objc.lookUpClass("NSObject")
        container.setStoredObjectToResultOf_on_("description", cls)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))
        self.assertTrue(isinstance(v, unicode))

        cls = objc.lookUpClass("NSMutableString")
        container.setStoredObjectToResultOf_on_("new", cls)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))
        self.assertTrue(isinstance(v, unicode))

    def testProtocol(self):
        container = OC_TestIdentity.alloc().init()

        container.setStoredObjectToAProtocol()
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))
        self.assertTrue(isinstance(v, objc.formal_protocol))

    if sys.maxsize < 2 ** 32 and os_release() < (10, 7):
        def testObject(self):
            container = OC_TestIdentity.alloc().init()
            cls = objc.lookUpClass("Object")
            container.setStoredObjectAnInstanceOfClassic_(cls)
            v = container.storedObject()
            self.assertTrue(container.isSameObjectAsStored_(v), repr(v))
            self.assertTrue(isinstance(v, cls))

    def testNSNumber(self):
        container = OC_TestIdentity.alloc().init()

        container.setStoredObjectToInteger_(10)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

        container.setStoredObjectToInteger_(-40)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

        container.setStoredObjectToUnsignedInteger_(40)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

        container.setStoredObjectToUnsignedInteger_(2 ** 32 - 4)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

        if sys.maxsize < 2 ** 32:
            container.setStoredObjectToLongLong_(sys.maxsize * 2)
            v = container.storedObject()
            self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

        container.setStoredObjectToLongLong_(-sys.maxsize)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

        container.setStoredObjectToUnsignedLongLong_(sys.maxsize * 2)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

        container.setStoredObjectToFloat_(10.0)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

        container.setStoredObjectToDouble_(9999.0)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

    def testNSDecimalNumber(self):
        container = OC_TestIdentity.alloc().init()
        cls = objc.lookUpClass("NSDecimalNumber")
        container.setStoredObjectToResultOf_on_("zero", cls)
        v = container.storedObject()
        self.assertTrue(container.isSameObjectAsStored_(v), repr(v))


class ObjCtoPython (TestCase):
    # TODO: NSProxy

    def assertFetchingTwice(self, container, message = None):
        v1 = container.storedObject()
        v2 = container.storedObject()

        self.assertTrue(v1 is v2, message)

    def testNSObject(self):
        container = OC_TestIdentity.alloc().init()

        cls = objc.lookUpClass("NSObject")
        container.setStoredObjectToResultOf_on_("new", cls)

        self.assertFetchingTwice(container, repr(cls))

        container.setStoredObjectToResultOf_on_("new", cls)
        v1 = container.storedObject()
        container.setStoredObjectToResultOf_on_("new", cls)
        v2 = container.storedObject()
        self.assertTrue(v1 is not v2, "different objects")

    def dont_testNSString(self):
        # This would always fail, NSStrings get a new proxy everytime
        # they cross the bridge, otherwise it would be unnecessarily hard
        # to get at the current value of NSMutableStrings.

        container = OC_TestIdentity.alloc().init()

        cls = objc.lookUpClass("NSObject")
        container.setStoredObjectToResultOf_on_("description", cls)
        self.assertFetchingTwice(container, "string")

        cls = objc.lookUpClass("NSMutableString")
        container.setStoredObjectToResultOf_on_("new", cls)
        self.assertFetchingTwice(container, "mutable string")

    def testProtocol(self):
        container = OC_TestIdentity.alloc().init()

        container.setStoredObjectToAProtocol()
        v = container.storedObject()
        self.assertFetchingTwice(container, "protocol")

    if sys.maxsize < 2 ** 32 and os_release() < (10, 7):
        def testObject(self):
            container = OC_TestIdentity.alloc().init()

            cls = objc.lookUpClass("Object")
            container.setStoredObjectAnInstanceOfClassic_(cls)
            self.assertFetchingTwice(container, "object")

    def dont_testNSNumber(self):
        # No unique proxies yet, due to the way these proxies are
        # implemented. This needs to be fixed, but that does not have
        # a high priority.
        container = OC_TestIdentity.alloc().init()

        container.setStoredObjectToInteger_(10)
        self.assertFetchingTwice(container, "int 10")

        container.setStoredObjectToInteger_(-40)
        self.assertFetchingTwice(container, "int -40")

        container.setStoredObjectToUnsignedInteger_(40)
        self.assertFetchingTwice(container, "unsigned int 40")

        container.setStoredObjectToUnsignedInteger_(sys.maxsize * 2)
        self.assertFetchingTwice(container, "unsigned int sys.maxsize*2")

        container.setStoredObjectToLongLong_(sys.maxsize * 2)
        self.assertFetchingTwice(container, "long long sys.maxsize*2")

        container.setStoredObjectToLongLong_(-sys.maxsize)
        self.assertFetchingTwice(container, "long long -sys.maxsize")

        container.setStoredObjectToUnsignedLongLong_(sys.maxsize * 2)
        self.assertFetchingTwice(container, "unsigned long long sys.maxsize*2")

        container.setStoredObjectToFloat_(10.0)
        self.assertFetchingTwice(container, "float")

        container.setStoredObjectToDouble_(9999.0)
        self.assertFetchingTwice(container, "double")

    def testNSDecimalNumber(self):
        container = OC_TestIdentity.alloc().init()

        cls = objc.lookUpClass("NSDecimalNumber")
        container.setStoredObjectToResultOf_on_("zero", cls)
        self.assertFetchingTwice(container, "decimal")

class PythonToObjC (TestCase):
    # TODO: verify

    def testPlainObjects(self):
        container = OC_TestIdentity.alloc().init()

        for v in (self, object(), list, sum):
            container.setStoredObject_(v)

            self.assertTrue(container.isSameObjectAsStored_(v), repr(v))


    def testContainers(self):
        container = OC_TestIdentity.alloc().init()

        for v in ([1,2,3], (1,2,3), {1:2, 3:4}):
            container.setStoredObject_(v)

            self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

    def dont_testStrings(self):
        # These get converted, not proxied
        container = OC_TestIdentity.alloc().init()

        for v in (b"hello world", "a unicode string"):
            container.setStoredObject_(v)

            self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

    def dont_testNumbers(self):
        # These get converted, not proxied
        container = OC_TestIdentity.alloc().init()

        for v in (1, 666666, sys.maxsize*3, 1.0,):
            container.setStoredObject_(v)

            self.assertTrue(container.isSameObjectAsStored_(v), repr(v))

class TestSerializingDataStructures (TestCase):
    # OC_Python{Array,Dictionary} used to contain specialized
    # identity-preservation code. It is unclear why this was added, it might
    # have to do with writing data to plist files.
    # This TestCase tries to trigger the problem that caused the addition of
    # said code, although unsuccesfully...

    def tearDown(self):
        if os.path.exists("/tmp/pyPyObjCTest.identity"):
            os.unlink("/tmp/pyPyObjCTest.identity")

    def testMakePlist(self):
        container = OC_TestIdentity.alloc().init()

        value = [ 1, 2, 3, [ "hello", ["world", ("in", 9 ) ], True, {"aap":3}]]
        value.append(value[3])

        container.setStoredObject_(value)
        container.writeStoredObjectToFile_("/tmp/pyPyObjCTest.identity")

        value = {
            "hello": [ 1, 2, 3],
            "world": {
                "nl": "wereld",
                "de": "Welt",
            }
        }
        container.setStoredObject_(value)
        container.writeStoredObjectToFile_("/tmp/pyPyObjCTest.identity")



if __name__ == "__main__":
    main()
