from PyObjCTools.TestSupport import *
import objc

from Foundation import *

import sys
if sys.version_info[0] == 3:
    def cmp(a, b):
        if a < b:
            return -1
        elif b < a:
            return 1
        return 0

class TestNSArrayInteraction(TestCase):
    def testRepeatedAllocInit(self):
        for i in range(1,1000):
            a = NSArray.alloc().init()

    def testIndices(self):
        x = NSArray.arrayWithArray_( [b"foo".decode('ascii'), b"bar".decode('ascii'), b"baz".decode('ascii')] )

        self.assertEqual( x.indexOfObject_(b"bar".decode('ascii')), 1 )

        self.assertRaises( IndexError, x.objectAtIndex_, 100)

    def testEnumeration(self):
        x = NSArray.arrayWithArray_([1, 2, b"foo".decode('ascii'), b"bar".decode('ascii'), b"".decode('ascii'), b"baz".decode('ascii')])
        y = []

        for o in x:
            y.append(o)

        self.assertEqual(len(x), len(y))

    def testContains(self):
        x = NSArray.arrayWithArray_( [b"foo".decode('ascii'), b"bar".decode('ascii'), b"baz".decode('ascii')] )
        self.assertEqual( x.count(), 3 )
        self.assertEqual( len(x), 3 )

        self.assertTrue( x.containsObject_(b"foo".decode('ascii')) )
        self.assertTrue( not x.containsObject_(b"dumbledorf".decode('ascii')) )

        self.assertTrue( b"foo".decode('ascii') in x )
        self.assertTrue( not b"dumbledorf".decode('ascii') in x )

    def testIn(self):
        x = NSMutableArray.array()
        for i in range(0, 100):
            x.addObject_(i)

        y = []
        for i in x:
            y.append( i )

        z = []
        for i in range(0, 100):
            z.append( i )

        self.assertEqual(x, y)
        self.assertEqual(x, z)
        self.assertEqual(y, z)

        for i in range(0, 100):
            self.assertTrue( i in x )

        self.assertTrue( 101 not in x )
        self.assertTrue( None not in x )
        self.assertTrue( b"foo bar".decode('ascii') not in x )

    def assertSlicesEqual(self,  x, y, z):
        self.assertEqual( x, x[:] )
        self.assertEqual( y, y[:] )
        self.assertEqual( z, z[:] )

        self.assertEqual( x[25:75], y[25:75] )
        self.assertEqual( x[25:75], z[25:75] )
        self.assertEqual( y[25:75], z[25:75] )

        self.assertEqual( x[:15], y[:15] )
        self.assertEqual( x[:15], z[:15] )
        self.assertEqual( y[:15], z[:15] )

        self.assertEqual( x[15:], y[15:] )
        self.assertEqual( x[15:], z[15:] )
        self.assertEqual( y[15:], z[15:] )

        self.assertEqual( x[-15:], y[-15:] )
        self.assertEqual( x[-15:], z[-15:] )
        self.assertEqual( y[-15:], z[-15:] )

        self.assertEqual( x[-15:30], y[-15:30] )
        self.assertEqual( x[-15:30], z[-15:30] )
        self.assertEqual( y[-15:30], z[-15:30] )

        self.assertEqual( x[-15:-5], y[-15:-5] )
        self.assertEqual( x[-15:-5], z[-15:-5] )
        self.assertEqual( y[-15:-5], z[-15:-5] )

    def testSlice(self):
        x = NSMutableArray.array()
        for i in range(0, 100):
            x.addObject_(i)

        y = []
        for i in x:
            y.append( i )

        z = []
        for i in range(0, 100):
            z.append( i )

        self.assertSlicesEqual(x, y, z)

        k = range(300, 50)
        x[20:30] = k
        y[20:30] = k
        z[20:30] = k

        self.assertSlicesEqual(x, y, z)

        # Note that x[1] = x works in python, but not for a bridged NS*Array*.
        # Not sure if there is anything we can do about that.
        x[1] = x[:]
        y[1] = y[:]
        z[1] = z[:]

        self.assertSlicesEqual(x, y, z)

        del x[-15:-5]
        del y[-15:-5]
        del z[-15:-5]

        self.assertSlicesEqual(x, y, z)

    def test_mixSliceNDice(self):
        # This test failes on Python 2.2, that is expected.
        x = list(range(0, 10))
        y = NSMutableArray.arrayWithArray_( range(0, 10) )

        y[2:4] = x[1:5]
        x[2:8] = y[3:7]
        y[2:4] = y[1:8]

    def test_subScripts(self):
        x = list(range(0, 10))
        y = NSMutableArray.arrayWithArray_(x)

        self.assertEqual( x[0], y[0] )
        self.assertEqual( x[2], y[2] )

        self.assertEqual( x[-1], y[-1] )
        self.assertEqual( x[-5], y[-5] )

        self.assertRaises( IndexError, x.__getitem__, 100)
        self.assertRaises( IndexError, x.__getitem__, -100)

    def test_varargConstruction(self):
        w = NSArray.arrayWithObjects_(1,2,3,4)
        x = NSArray.alloc().initWithObjects_(1,2,3,4)

        y = NSArray.arrayWithObjects_count_([1,2,3,4,5,6], 4)
        z = NSArray.alloc().initWithObjects_count_([1,2,3,4,5,6], 4)

        self.assertEqual(len(w), 4)
        self.assertEqual(len(x), 4)
        self.assertEqual(len(y), 4)
        self.assertEqual(len(z), 4)

        self.assertEqual(w[0], 1)
        self.assertEqual(x[1], 2)
        self.assertEqual(y[2], 3)
        self.assertEqual(z[3], 4)

    def test_varargConstruction2(self):
        w = NSMutableArray.arrayWithObjects_(1,2,3,4, None)
        x = NSMutableArray.alloc().initWithObjects_(1,2,3,4, None)
        y = NSMutableArray.arrayWithObjects_count_([1,2,3,4,5,6], 4)
        z = NSMutableArray.alloc().initWithObjects_count_([1,2,3,4,5,6], 4)

        self.assertEqual(len(w), 4)
        self.assertEqual(len(x), 4)
        self.assertEqual(len(y), 4)
        self.assertEqual(len(z), 4)

        self.assertEqual(w[0], 1)
        self.assertEqual(x[1], 2)
        self.assertEqual(y[2], 3)
        self.assertEqual(z[3], 4)

class TestNSArraySpecialMethods(TestCase):
    #Test calling 'difficult' methods from Python

    def test_initWithObjects_count_(self):
        a = NSArray.alloc().initWithObjects_count_((b'a'.decode('ascii'),b'b'.decode('ascii'),b'c'.decode('ascii'),b'd'.decode('ascii')), 3)
        self.assertEqual(a, [b'a'.decode('ascii'),b'b'.decode('ascii'),b'c'.decode('ascii')])

        import warnings
        warnings.filterwarnings('ignore',
                category=objc.UninitializedDeallocWarning)

        try:
            self.assertRaises(ValueError, NSArray.alloc().initWithObjects_count_, (b'a'.decode('ascii'),b'b'.decode('ascii')), 3)

        finally:
            del warnings.filters[0]


    def test_arrayWithObjects_count_(self):
        a = NSArray.arrayWithObjects_count_((b'a'.decode('ascii'),b'b'.decode('ascii'),b'c'.decode('ascii'),b'd'.decode('ascii')), 3)
        self.assertEqual(a, [b'a'.decode('ascii'),b'b'.decode('ascii'),b'c'.decode('ascii')])

        self.assertRaises(ValueError, NSArray.arrayWithObjects_count_, (b'a'.decode('ascii'),b'b'.decode('ascii')), 3)

    def test_arrayByAddingObjects_count_(self):
        return

        a = NSArray.arrayWithArray_((b'a'.decode('ascii'), b'b'.decode('ascii'), b'c'.decode('ascii')))
        self.assertEqual(a, (b'a'.decode('ascii'), b'b'.decode('ascii'), b'c'.decode('ascii')))

        b = a.arrayByAddingObjects_count_((b'd'.decode('ascii'), b'e'.decode('ascii'), b'f'.decode('ascii')), 3)
        self.assertEqual(a, (b'a'.decode('ascii'), b'b'.decode('ascii'), b'c'.decode('ascii')))
        self.assertEqual(b, (b'a'.decode('ascii'), b'b'.decode('ascii'), b'c'.decode('ascii'), b'd'.decode('ascii'), b'e'.decode('ascii'), b'f'.decode('ascii')))

        self.assertRaises(ValueError, a.arrayByAddingObjects_count_, (b'a'.decode('ascii'),b'b'.decode('ascii')), 3)
    def test_sortedArrayUsingFunction_context_(self):
        a = NSArray.arrayWithArray_((b'a'.decode('ascii'), b'b'.decode('ascii'), b'c'.decode('ascii')))
        self.assertEqual(a, (b'a'.decode('ascii'), b'b'.decode('ascii'), b'c'.decode('ascii')))

        def cmpfunc(l, r, c):
            return -cmp(l,r)

        b = a.sortedArrayUsingFunction_context_(cmpfunc, b'hello'.decode('ascii'))
        self.assertEqual(a, (b'a'.decode('ascii'), b'b'.decode('ascii'), b'c'.decode('ascii')))
        self.assertEqual(b, (b'c'.decode('ascii'), b'b'.decode('ascii'), b'a'.decode('ascii')))

    def test_sortedArrayUsingFunction_context_hint_(self):
        a = NSArray.arrayWithArray_((b'a'.decode('ascii'), b'b'.decode('ascii'), b'c'.decode('ascii')))
        self.assertEqual(a, (b'a'.decode('ascii'), b'b'.decode('ascii'), b'c'.decode('ascii')))

        def cmpfunc(l, r, c):
            return -cmp(l,r)

        b = a.sortedArrayUsingFunction_context_hint_(cmpfunc, b'hello'.decode('ascii'), a.sortedArrayHint())
        self.assertEqual(a, (b'a'.decode('ascii'), b'b'.decode('ascii'), b'c'.decode('ascii')))
        self.assertEqual(b, (b'c'.decode('ascii'), b'b'.decode('ascii'), b'a'.decode('ascii')))

class TestNSMutableArrayInteraction(TestCase):

    def testRemoveObjects(self):
        a = NSMutableArray.arrayWithArray_(range(10))

        self.assertEqual(len(a), 10)
        self.assertEqual(a[0], 0)
        self.assertEqual(a[1], 1)
        self.assertEqual(a[2], 2)

        a.removeObjectsFromIndices_numIndices_([2, 4, 6, 8], 3)

        self.assertEqual(len(a), 7)
        self.assertEqual(a, (0, 1, 3, 5, 7, 8, 9))


    def testReplaceObjects(self):
        if objc.platform == 'MACOSX' or hasattr(NSMutableArray, 'replaceObjectsInRange_withObjects_count_'):

            a = NSMutableArray.arrayWithArray_(range(4))
            self.assertEqual(a, (0, 1, 2, 3))

            a.replaceObjectsInRange_withObjects_count_(
                (1,2), [b"a".decode('ascii'), b"b".decode('ascii'), b"c".decode('ascii'), b"d".decode('ascii')], 3)

            self.assertEqual(a, (0, b"a".decode('ascii'), b"b".decode('ascii'), b"c".decode('ascii'), 3))

    def testSortInvalid(self):
        # Invalid calls to sortUsingFunction:context:
        def cmp(a, b):
            return -1

        a = NSMutableArray.arrayWithArray_(range(4))
        self.assertEqual(a, (0, 1, 2, 3))

        t = objc.getVerbose()
        objc.setVerbose(0)
        try:
            self.assertRaises(TypeError, a.sortUsingFunction_context_, dir)
            self.assertRaises(TypeError, a.sortUsingFunction_context_, dir, 1, 2)
            self.assertRaises(TypeError, a.sortUsingFunction_context_, lambda *args: cmp(*args), b'a'.decode('ascii'))
        finally:
            objc.setVerbose(t)

    def dont_testSort2(self):
        # sortUsingFunction:context:range: isn't documented an hence shouldn't be tested
        a = NSMutableArray.arrayWithArray_(range(10))
        self.assertEqual(a, (0, 1, 2, 3, 4, 5, 6, 7, 8, 9))

        if objc.platform == 'MACOSX' or hasattr(a, 'sortUsingFunction_context_range_'):
            def cmpfunc(l, r, c):
                return -cmp(l,r)

            a.sortUsingFunction_context_range_(cmpfunc, b"a".decode('ascii'), (4, 4))

            self.assertEqual(a, (0, 1, 2, 3, 7, 6, 5, 4, 8, 9))

    def testSort3(self):
        # check the sort method, list interface compatibility

        a = NSMutableArray.arrayWithArray_(range(4))
        self.assertEqual(a, (0, 1, 2, 3))

        def cmpfunc(l, r):
            return -cmp(l,r)

        a.sort(cmpfunc)

        self.assertEqual(a, (3, 2, 1, 0))

        a.sort()
        self.assertEqual(a, (0, 1, 2, 3))

    def testSort1(self):
        a = NSMutableArray.arrayWithArray_(range(4))
        self.assertEqual(a, (0, 1, 2, 3))

        def cmpfunc(l, r, c):
            return -cmp(l,r)

        a.sortUsingFunction_context_(cmpfunc, b"a".decode('ascii'))

        self.assertEqual(a, (3, 2, 1, 0))

    def dont_testSort2(self):
        a = NSMutableArray.arrayWithArray_(range(10))
        self.assertEqual(a, (0, 1, 2, 3, 4, 5, 6, 7, 8, 9))

        if objc.platform == 'MACOSX' or hasattr(a, 'sortUsingFunction_context_range_'):
            def cmpfunc(l, r, c):
                return -cmp(l,r)

            a.sortUsingFunction_context_range_(cmpfunc, b"a".decode('ascii'), (4, 4))

            self.assertEqual(a, (0, 1, 2, 3, 7, 6, 5, 4, 8, 9))

    def testSort3(self):
        # check the sort method, list interface compatibility

        a = NSMutableArray.arrayWithArray_(range(4))
        self.assertEqual(a, (0, 1, 2, 3))


        def cmpfunc(l, r):
            return -cmp(l,r)

        a.sort(cmpfunc=cmpfunc)

        self.assertEqual(a, (3, 2, 1, 0))

        a.sort()

        self.assertEqual(a, (0, 1, 2, 3))

        map={
            0: "nul",
            1: "een",
            2: "twee",
            3: "drie",
        }


        def keyfunc(l):
            return  map[l]

        a.sort(key=keyfunc)
        self.assertEqual(a, (3, 1, 0, 2))

        a.sort(key=keyfunc, reverse=True)
        self.assertEqual(a, (2, 0, 1, 3))

        a.sort(reverse=True)
        self.assertEqual(a, (3, 2, 1, 0))

    def getObjectsRange(self):
        o = NSArray.arrayWithArray_(range(4, 8))
        v =  o.getObjects_range_((1,2))
        self.assertEqual(v, (5,6))

    def test_unsupportedMethods(self):
        #
        # Check that calling unsupported methods results in a TypeError
        #
        # NOTE: Some of these don't even exist on GNUstep
        o = NSArray.arrayWithArray_(range(4))
        self.assertRaises(TypeError, o.getObjects_)

        #
        #if objc.platform == 'MACOSX' or hasattr(o, 'apply_context_'):
        #    self.assertRaises(TypeError, o.apply_context_, lambda x, y:None, 0)


    def testInsert(self):
        o = NSMutableArray.arrayWithArray_(range(4))
        self.assertEqual(list(o), list(range(4)))

        self.assertEqual(o[0], 0)
        o.insert(0, "foo")
        self.assertEqual(o[0], "foo")
        self.assertEqual(o[1], 0)
        self.assertEqual(len(o), 5)

        # FIXME: test the entire interface of list.insert

class TestVariadic (TestCase):
    def testArrayWithObjects(self):
        a = NSArray.arrayWithObjects_(b"foo".decode('ascii'), b"bar".decode('ascii'), None)
        self.assertEqual(a, (b"foo".decode('ascii'), b"bar".decode('ascii')))
        self.assertIsInstance(a, NSArray)

        a = NSMutableArray.arrayWithObjects_(b"foo".decode('ascii'), b"bar".decode('ascii'), None)
        self.assertEqual(a, [b"foo".decode('ascii'), b"bar".decode('ascii')])
        self.assertIsInstance(a, NSMutableArray)

    def testInitWithObjecs(self):
        a = NSArray.alloc().initWithObjects_(b"foo".decode('ascii'), b"bar".decode('ascii'), None)
        self.assertEqual(a, (b"foo".decode('ascii'), b"bar".decode('ascii')))
        self.assertIsInstance(a, NSArray)

        a = NSMutableArray.alloc().initWithObjects_(b"foo".decode('ascii'), b"bar".decode('ascii'), None)
        self.assertEqual(a, [b"foo".decode('ascii'), b"bar".decode('ascii')])
        self.assertIsInstance(a, NSMutableArray)

class TestNSArray (TestCase):
    def testMethods(self):
        self.assertResultIsBOOL(NSArray.isEqualToArray_)
        self.assertResultIsBOOL(NSArray.containsObject_)
        self.assertResultIsBOOL(NSArray.writeToFile_atomically_)
        self.assertArgIsBOOL(NSArray.writeToFile_atomically_, 1)
        self.assertResultIsBOOL(NSArray.writeToURL_atomically_)
        self.assertArgIsBOOL(NSArray.writeToURL_atomically_, 1)

        self.assertArgIsSEL(NSArray.makeObjectsPerformSelector_, 0, b'v@:')
        self.assertArgIsSEL(NSArray.makeObjectsPerformSelector_withObject_, 0, b'v@:@')

        self.assertArgIsBOOL(NSArray.initWithArray_copyItems_, 1)

        self.assertArgIsIn(NSArray.arrayWithObjects_count_, 0)
        self.assertArgSizeInArg(NSArray.arrayWithObjects_count_, 0, 1)
        self.assertArgIsIn(NSArray.initWithObjects_count_, 0)
        self.assertArgSizeInArg(NSArray.initWithObjects_count_, 0, 1)

        self.assertArgIsIn(NSMutableArray.removeObjectsFromIndices_numIndices_, 0)
        self.assertArgSizeInArg(NSMutableArray.removeObjectsFromIndices_numIndices_, 0, 1)

        self.assertArgIsFunction(NSArray.sortedArrayUsingFunction_context_, 0, b'l@@@', False)
        self.assertArgHasType(NSArray.sortedArrayUsingFunction_context_, 1, b'@')
        self.assertArgIsFunction(NSArray.sortedArrayUsingFunction_context_hint_, 0, b'l@@@', False)
        self.assertArgHasType(NSArray.sortedArrayUsingFunction_context_hint_, 1, b'@')
        self.assertArgIsSEL(NSArray.sortedArrayUsingSelector_, 0, b'i@:@')

        self.assertArgIsFunction(NSMutableArray.sortUsingFunction_context_, 0, b'l@@@', False)
        self.assertArgHasType(NSMutableArray.sortUsingFunction_context_, 1, b'@')

        self.assertArgIsSEL(NSMutableArray.sortUsingSelector_, 0, b'i@:@')

        self.assertIsNullTerminated(NSArray.arrayWithObjects_)
        self.assertIsNullTerminated(NSArray.initWithObjects_)

    @min_os_level('10.6')
    def testMethods10_6(self):
        self.assertArgIsBlock(NSArray.enumerateObjectsUsingBlock_, 0,
                b'v@'+objc._C_NSUInteger+b'o^'+objc._C_NSBOOL)
        self.assertArgIsBlock(NSArray.enumerateObjectsWithOptions_usingBlock_, 1,
                b'v@'+objc._C_NSUInteger+b'o^'+objc._C_NSBOOL)
        self.assertArgIsBlock(NSArray.enumerateObjectsAtIndexes_options_usingBlock_, 2,
                b'v@'+objc._C_NSUInteger+b'o^'+objc._C_NSBOOL)

        self.assertArgIsBlock(NSArray.indexOfObjectPassingTest_, 0,
                objc._C_NSBOOL + b'@'+objc._C_NSUInteger+b'o^'+objc._C_NSBOOL)
        self.assertArgIsBlock(NSArray.indexOfObjectWithOptions_passingTest_, 1,
                objc._C_NSBOOL + b'@'+objc._C_NSUInteger+b'o^'+objc._C_NSBOOL)
        self.assertArgIsBlock(NSArray.indexOfObjectAtIndexes_options_passingTest_, 2,
                objc._C_NSBOOL + b'@'+objc._C_NSUInteger+b'o^'+objc._C_NSBOOL)

        self.assertArgIsBlock(NSArray.indexesOfObjectsPassingTest_, 0,
                objc._C_NSBOOL + b'@'+objc._C_NSUInteger+b'o^'+objc._C_NSBOOL)
        self.assertArgIsBlock(NSArray.indexesOfObjectsWithOptions_passingTest_, 1,
                objc._C_NSBOOL + b'@'+objc._C_NSUInteger+b'o^'+objc._C_NSBOOL)
        self.assertArgIsBlock(NSArray.indexesOfObjectsAtIndexes_options_passingTest_, 2,
                objc._C_NSBOOL + b'@'+objc._C_NSUInteger+b'o^'+objc._C_NSBOOL)

        self.assertArgIsBlock(NSArray.sortedArrayUsingComparator_,
                0, b'l@@')
        self.assertArgIsBlock(NSArray.sortedArrayWithOptions_usingComparator_,
                1, b'l@@')
        self.assertArgIsBlock(NSArray.indexOfObject_inSortedRange_options_usingComparator_,
                3, b'l@@')
        self.assertArgHasType(NSArray.indexOfObject_inSortedRange_options_usingComparator_, 1, NSRange.__typestr__)



        self.assertArgIsBlock(NSMutableArray.sortUsingComparator_,
                0, objc._C_NSInteger + b'@@')
        self.assertArgIsBlock(NSMutableArray.sortWithOptions_usingComparator_,
                1, objc._C_NSInteger + b'@@')



    @min_os_level('10.6')
    def testConstants10_6(self):
        self.assertEqual(NSBinarySearchingFirstEqual, 1 << 8)
        self.assertEqual(NSBinarySearchingLastEqual, 1 << 9)
        self.assertEqual(NSBinarySearchingInsertionIndex, 1 << 10)

if __name__ == '__main__':
    main()
