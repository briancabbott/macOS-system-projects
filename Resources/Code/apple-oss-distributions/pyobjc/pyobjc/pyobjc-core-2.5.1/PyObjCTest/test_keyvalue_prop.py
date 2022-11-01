from PyObjCTools.TestSupport import *
import objc

NSObject = objc.lookUpClass('NSObject')

class TestKVOPropHelper (NSObject):
    def init(self):
        self = super(TestKVOPropHelper, self).init()
        if self is None: return None

        self.__helper = None
        self.accessing = []
        return self

    def get_helper(self):
        self.accessing.append(('get_helper',))
        return self.__helper

    def set_helper(self, value):
        self.accessing.append(('set_helper', value))
        self.__helper = value

    helper = property(get_helper, set_helper)


class TestKVOProp (TestCase):
    def testKVOProperty(self):
        o = TestKVOPropHelper.alloc().init()
        self.assertIsInstance(o, TestKVOPropHelper)

        self.assertEqual(len(o.accessing), 0)
        o.helper = 42
        self.assertEqual(len(o.accessing), 1)
        self.assertEqual(o.accessing[-1], ('set_helper', 42))

        self.assertEqual(o.helper, 42)
        self.assertEqual(len(o.accessing), 2)
        self.assertEqual(o.accessing[-1], ('get_helper',))

        o.accessing[:] = []

        self.assertEqual(len(o.accessing), 0)
        o.setValue_forKey_(42, 'helper')
        self.assertEqual(len(o.accessing), 1)
        self.assertEqual(o.accessing[-1], ('set_helper', 42))

        self.assertEqual(o.valueForKey_('helper'), 42)
        self.assertEqual(len(o.accessing), 2)
        self.assertEqual(o.accessing[-1], ('get_helper',))

    def testKVOWillChange(self):
        o = TestKVOPropHelper.alloc().init()
        self.assertIsInstance(o, TestKVOPropHelper)

        self.assertEqual(len(o.accessing), 0)
        o.willChangeValueForKey_('helper')
        o.helper = 42
        o.didChangeValueForKey_('helper')
        self.assertEqual(len(o.accessing), 1)
        self.assertEqual(o.accessing[-1], ('set_helper', 42))

if __name__ == "__main__":
    main()
