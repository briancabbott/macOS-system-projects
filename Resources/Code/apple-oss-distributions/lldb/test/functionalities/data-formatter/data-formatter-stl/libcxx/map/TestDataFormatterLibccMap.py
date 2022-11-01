"""
Test lldb data formatter subsystem.
"""

import os, time
import unittest2
import lldb
from lldbtest import *
import lldbutil

class LibcxxMapDataFormatterTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    def test_with_dsym_and_run_command(self):
        """Test data formatter commands."""
        self.buildDsym()
        self.data_formatter_commands()

    @skipIfFreeBSD # llvm.org/pr17231
    @skipIfLinux # No standard locations for libc++ on Linux, so skip for now 
    @dwarf_test
    def test_with_dwarf_and_run_command(self):
        """Test data formatter commands."""
        self.buildDwarf()
        self.data_formatter_commands()

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)

    def data_formatter_commands(self):
        """Test that that file and class static variables display correctly."""
        self.runCmd("file a.out", CURRENT_EXECUTABLE_SET)

        lldbutil.run_break_set_by_source_regexp (self, "Set break point at this line.")

        self.runCmd("run", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['stopped',
                       'stop reason = breakpoint'])

        # This is the function to remove the custom formats in order to have a
        # clean slate for the next test case.
        def cleanup():
            self.runCmd('type format clear', check=False)
            self.runCmd('type summary clear', check=False)
            self.runCmd('type filter clear', check=False)
            self.runCmd('type synth clear', check=False)
            self.runCmd("settings set target.max-children-count 256", check=False)

        # Execute the cleanup function during test case tear down.
        self.addTearDownHook(cleanup)

        self.expect('image list', substrs = self.getLibcPlusPlusLibs())

        self.runCmd("frame variable ii --show-types")
                
        self.expect('frame variable ii',
            substrs = ['size=0',
                       '{}'])

        self.runCmd("continue");

        self.expect('frame variable ii',
                    substrs = ['size=2',
                               '[0] = ',
                               'first = 0',
                               'second = 0',
                               '[1] = ',
                               'first = 1',
                               'second = 1'])

        self.runCmd("continue");

        self.expect('frame variable ii',
                    substrs = ['size=4',
                               '[2] = ',
                               'first = 2',
                               'second = 0',
                               '[3] = ',
                               'first = 3',
                               'second = 1'])

        self.runCmd("continue");

        self.expect("frame variable ii",
                    substrs = ['size=8',
                               '[5] = ',
                               'first = 5',
                               'second = 0',
                               '[7] = ',
                               'first = 7',
                               'second = 1'])

        self.expect("p ii",
                    substrs = ['size=8',
                               '[5] = ',
                               'first = 5',
                               'second = 0',
                               '[7] = ',
                               'first = 7',
                               'second = 1'])

        # check access-by-index
        self.expect("frame variable ii[0]",
                    substrs = ['first = 0',
                               'second = 0']);
        self.expect("frame variable ii[3]",
                    substrs = ['first =',
                               'second =']);

        # check that MightHaveChildren() gets it right
        self.assertTrue(self.frame().FindVariable("ii").MightHaveChildren(), "ii.MightHaveChildren() says False for non empty!")

        # check that the expression parser does not make use of
        # synthetic children instead of running code
        # TOT clang has a fix for this, which makes the expression command here succeed
        # since this would make the test fail or succeed depending on clang version in use
        # this is safer commented for the time being
        #self.expect("expression ii[8]", matching=False, error=True,
        #            substrs = ['1234567'])

        self.runCmd("continue");
        
        self.expect('frame variable ii',
                    substrs = ['size=0',
                               '{}'])
        
        self.runCmd("frame variable si --show-types")

        self.expect('frame variable si',
                    substrs = ['size=0',
                               '{}'])

        self.runCmd("continue");

        self.expect('frame variable si',
                    substrs = ['size=1',
                               '[0] = ',
                               'first = \"zero\"',
                               'second = 0'])

        self.runCmd("continue");

        self.expect("frame variable si",
                    substrs = ['size=4',
                               '[0] = ',
                               'first = \"zero\"',
                               'second = 0',
                                '[1] = ',
                                'first = \"one\"',
                                'second = 1',
                                '[2] = ',
                                'first = \"two\"',
                                'second = 2',
                                '[3] = ',
                                'first = \"three\"',
                                'second = 3'])

        self.expect("p si",
                    substrs = ['size=4',
                               '[0] = ',
                               'first = \"zero\"',
                               'second = 0',
                               '[1] = ',
                               'first = \"one\"',
                               'second = 1',
                               '[2] = ',
                               'first = \"two\"',
                               'second = 2',
                               '[3] = ',
                               'first = \"three\"',
                               'second = 3'])

        # check that MightHaveChildren() gets it right
        self.assertTrue(self.frame().FindVariable("si").MightHaveChildren(), "si.MightHaveChildren() says False for non empty!")

        # check access-by-index
        self.expect("frame variable si[0]",
                    substrs = ['first = ', 'one',
                               'second = 1']);
        
        # check that the expression parser does not make use of
        # synthetic children instead of running code
        # TOT clang has a fix for this, which makes the expression command here succeed
        # since this would make the test fail or succeed depending on clang version in use
        # this is safer commented for the time being
        #self.expect("expression si[0]", matching=False, error=True,
        #            substrs = ['first = ', 'zero'])

        self.runCmd("continue");
        
        self.expect('frame variable si',
                    substrs = ['size=0',
                               '{}'])

        self.runCmd("continue");
        self.runCmd("frame variable is --show-types")
        
        self.expect('frame variable is',
                    substrs = ['size=0',
                               '{}'])

        self.runCmd("continue");

        self.expect("frame variable is",
                    substrs = ['size=4',
                               '[0] = ',
                               'second = \"goofy\"',
                               'first = 85',
                               '[1] = ',
                               'second = \"is\"',
                               'first = 1',
                               '[2] = ',
                               'second = \"smart\"',
                               'first = 2',
                               '[3] = ',
                               'second = \"!!!\"',
                               'first = 3'])
        
        self.expect("p is",
                    substrs = ['size=4',
                               '[0] = ',
                               'second = \"goofy\"',
                               'first = 85',
                               '[1] = ',
                               'second = \"is\"',
                               'first = 1',
                               '[2] = ',
                               'second = \"smart\"',
                               'first = 2',
                               '[3] = ',
                               'second = \"!!!\"',
                               'first = 3'])

        # check that MightHaveChildren() gets it right
        self.assertTrue(self.frame().FindVariable("is").MightHaveChildren(), "is.MightHaveChildren() says False for non empty!")

        # check access-by-index
        self.expect("frame variable is[0]",
                    substrs = ['first = ',
                               'second =']);
        
        # check that the expression parser does not make use of
        # synthetic children instead of running code
        # TOT clang has a fix for this, which makes the expression command here succeed
        # since this would make the test fail or succeed depending on clang version in use
        # this is safer commented for the time being
        #self.expect("expression is[0]", matching=False, error=True,
        #            substrs = ['first = ', 'goofy'])

        self.runCmd("continue");
        
        self.expect('frame variable is',
                    substrs = ['size=0',
                               '{}'])

        self.runCmd("continue");
        self.runCmd("frame variable ss --show-types")
        
        self.expect('frame variable ss',
                    substrs = ['size=0',
                               '{}'])

        self.runCmd("continue");

        self.expect("frame variable ss",
                    substrs = ['size=3',
                               '[0] = ',
                               'second = \"hello\"',
                               'first = \"ciao\"',
                               '[1] = ',
                               'second = \"house\"',
                               'first = \"casa\"',
                               '[2] = ',
                               'second = \"cat\"',
                               'first = \"gatto\"'])
        
        self.expect("p ss",
                    substrs = ['size=3',
                               '[0] = ',
                               'second = \"hello\"',
                               'first = \"ciao\"',
                               '[1] = ',
                               'second = \"house\"',
                               'first = \"casa\"',
                               '[2] = ',
                               'second = \"cat\"',
                               'first = \"gatto\"'])

        # check that MightHaveChildren() gets it right
        self.assertTrue(self.frame().FindVariable("ss").MightHaveChildren(), "ss.MightHaveChildren() says False for non empty!")

        # check access-by-index
        self.expect("frame variable ss[2]",
                    substrs = ['gatto', 'cat']);
        
        # check that the expression parser does not make use of
        # synthetic children instead of running code
        # TOT clang has a fix for this, which makes the expression command here succeed
        # since this would make the test fail or succeed depending on clang version in use
        # this is safer commented for the time being
        #self.expect("expression ss[3]", matching=False, error=True,
        #            substrs = ['gatto'])

        self.runCmd("continue");
        
        self.expect('frame variable ss',
                    substrs = ['size=0',
                               '{}'])

if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
