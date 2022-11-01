from PyObjCTools.TestSupport import *
import objc

from Foundation import *

class TestNSSetInteraction(TestCase):
    def __testRepeatedAllocInit( self ):
        for i in range(1,1000):
            a = NSSet.alloc().init()

    def __testContains( self ):
        x = NSSet.setWithArray_( ["foo", "bar", "baz"] )

        self.assert_( "foo" in x )
        self.assert_( "notfoo" not in x )

    def __testIteration( self ):
        x = NSSet.setWithArray_( ["foo", "bar", "baz"] )

        for i in x:
            self.assert_( i in x )
            self.assert_( x.containsObject_( i ) )

    def test_varargsConstruction(self):
        w = NSSet.setWithObjects_(0,1,2,3,None)
        x = NSSet.alloc().initWithObjects_(0,1,2,3,None)
        y = NSSet.setWithObjects_count_(range(10), 4)
        z = NSSet.alloc().initWithObjects_count_(range(10), 4)
        #a = NSSet.alloc().initWithObjects_count_(range(4), None)

        self.assert_(len(w) == 4)
        self.assert_(len(x) == 4)
        self.assert_(len(y) == 4)
        self.assert_(len(z) == 4)
        #self.assert_(len(a) == 4)

        self.assert_(0 in w)
        self.assert_(1 in x)
        self.assert_(2 in y)
        self.assert_(3 in z)
        #self.assert_(3 in a)

    def test_varargsConstruction2(self):
        w = NSMutableSet.setWithObjects_(0,1,2,3,None)
        x = NSMutableSet.alloc().initWithObjects_(0,1,2,3,None)
        y = NSMutableSet.setWithObjects_count_(range(10), 4)
        z = NSMutableSet.alloc().initWithObjects_count_(range(10), 4)

        self.assert_(len(w) == 4)
        self.assert_(len(x) == 4)
        self.assert_(len(y) == 4)
        self.assert_(len(z) == 4)

        self.assert_(0 in w)
        self.assert_(1 in x)
        self.assert_(2 in y)
        self.assert_(3 in z)

class TestVariadic (TestCase):
    def testSetWithObjects(self):
        o = NSSet.setWithObjects_()
        self.assertEqual(len(o), 0)
        self.assert_(isinstance(o, NSSet))

        o = NSSet.setWithObjects_(1,2,3)
        self.assertEqual(len(o), 3)
        self.assert_(isinstance(o, NSSet))
        self.assert_(1 in o)
        self.assert_(2 in o)
        self.assert_(3 in o)

        o = NSMutableSet.setWithObjects_()
        self.assertEqual(len(o), 0)
        self.assert_(isinstance(o, NSMutableSet))

        o = NSMutableSet.setWithObjects_(1,2,3)
        self.assertEqual(len(o), 3)
        self.assert_(isinstance(o, NSMutableSet))
        self.assert_(1 in o)
        self.assert_(2 in o)
        self.assert_(3 in o)

    def testInitWithObjects(self):
        o = NSSet.alloc().initWithObjects_()
        self.assertEqual(len(o), 0)
        self.assert_(isinstance(o, NSSet))

        o = NSSet.alloc().initWithObjects_(1,2,3)
        self.assertEqual(len(o), 3)
        self.assert_(isinstance(o, NSSet))
        self.assert_(1 in o)
        self.assert_(2 in o)
        self.assert_(3 in o)

        o = NSMutableSet.alloc().initWithObjects_()
        self.assertEqual(len(o), 0)
        self.assert_(isinstance(o, NSMutableSet))

        o = NSMutableSet.alloc().initWithObjects_(1,2,3)
        self.assertEqual(len(o), 3)
        self.assert_(isinstance(o, NSMutableSet))
        self.assert_(1 in o)
        self.assert_(2 in o)
        self.assert_(3 in o)

    def testSetWithObjectsCount(self):
        o = NSSet.setWithObjects_count_([1,2,3], 3)
        self.assertEqual(len(o), 3)
        self.assert_(isinstance(o, NSSet))
        self.assert_(1 in o)
        self.assert_(2 in o)
        self.assert_(3 in o)
        self.assert_(4 not in o)

        o = NSSet.setWithObjects_count_([1,2,3], 0)
        self.assertEqual(len(o), 0)
        self.assert_(isinstance(o, NSSet))

        o = NSMutableSet.setWithObjects_count_([1,2,3], 3)
        self.assertEqual(len(o), 3)
        self.assert_(isinstance(o, NSMutableSet))
        self.assert_(1 in o)
        self.assert_(2 in o)
        self.assert_(3 in o)

        o = NSMutableSet.setWithObjects_count_([1,2,3], 0)
        self.assertEqual(len(o), 0)
        self.assert_(isinstance(o, NSMutableSet))

    def testInitWithObjectsCount(self):
        o = NSSet.alloc().initWithObjects_count_([1,2,3], 3)
        self.assertEqual(len(o), 3)
        self.assert_(isinstance(o, NSSet))
        self.assert_(1 in o)
        self.assert_(2 in o)
        self.assert_(3 in o)
        self.assert_(4 not in o)

        o = NSSet.alloc().initWithObjects_count_([1,2,3], 0)
        self.assertEqual(len(o), 0)
        self.assert_(isinstance(o, NSSet))

        o = NSMutableSet.alloc().initWithObjects_count_([1,2,3], 3)
        self.assertEqual(len(o), 3)
        self.assert_(isinstance(o, NSMutableSet))
        self.assert_(1 in o)
        self.assert_(2 in o)
        self.assert_(3 in o)

        o = NSMutableSet.alloc().initWithObjects_count_([1,2,3], 0)
        self.assertEqual(len(o), 0)
        self.assert_(isinstance(o, NSMutableSet))

    def testMethods(self):
        self.assertResultIsBOOL(NSSet.containsObject_)
        self.assertResultIsBOOL(NSSet.intersectsSet_)
        self.assertResultIsBOOL(NSSet.isEqualToSet_)
        self.assertResultIsBOOL(NSSet.isSubsetOfSet_)

        self.assertArgIsIn(NSSet.setWithObjects_count_, 0)
        self.assertArgSizeInArg(NSSet.setWithObjects_count_, 0, 1)
        self.assertArgIsIn(NSSet.initWithObjects_count_, 0)
        self.assertArgSizeInArg(NSSet.initWithObjects_count_, 0, 1)

        self.assertArgIsBOOL(NSSet.initWithSet_copyItems_, 1)

    @min_os_level('10.6')
    def testMethods10_6(self):
        self.assertArgIsBlock(NSSet.enumerateObjectsUsingBlock_, 0, b'v@o^' + objc._C_NSBOOL)
        self.assertArgIsBlock(NSSet.enumerateObjectsWithOptions_usingBlock_, 1, b'v@o^' + objc._C_NSBOOL)

        self.assertArgIsBlock(NSSet.objectsPassingTest_, 0, objc._C_NSBOOL + b'@o^' + objc._C_NSBOOL)
        self.assertArgIsBlock(NSSet.objectsWithOptions_passingTest_, 1, objc._C_NSBOOL + b'@o^' + objc._C_NSBOOL)



if __name__ == '__main__':
    main()
