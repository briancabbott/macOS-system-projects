"""
Test lldb data formatter subsystem.
"""

import os, time
import unittest2
import lldb
from lldbtest import *
import lldbutil

class LibcxxVectorDataFormatterTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    def test_with_dsym_and_run_command(self):
        """Test data formatter commands."""
        self.buildDsym()
        self.data_formatter_commands()

    @skipIfLinux # No standard locations for libc++ on Linux, so skip for now 
    @dwarf_test
    def test_with_dwarf_and_run_command(self):
        """Test data formatter commands."""
        self.buildDwarf()
        self.data_formatter_commands()

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)
        # Find the line number to break at.
        self.line = line_number('main.cpp', '// Set break point at this line.')
        self.line2 = line_number('main.cpp', '// Set second break point at this line.')

    def data_formatter_commands(self):
        """Test that that file and class static variables display correctly."""
        self.runCmd("file a.out", CURRENT_EXECUTABLE_SET)

        lldbutil.run_break_set_by_file_and_line (self, "main.cpp", self.line, num_expected_locations=-1)
        lldbutil.run_break_set_by_file_and_line (self, "main.cpp", self.line2, num_expected_locations=-1)

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

        # empty vectors (and storage pointers SHOULD BOTH BE NULL..)
        self.expect("frame variable numbers",
            substrs = ['numbers = size=0'])

        self.runCmd("n")
        
        # first value added
        self.expect("frame variable numbers",
                    substrs = ['numbers = size=1',
                               '[0] = 1',
                               '}'])

        # add some more data
        self.runCmd("n");self.runCmd("n");self.runCmd("n");
    
        self.expect("frame variable numbers",
                    substrs = ['numbers = size=4',
                               '[0] = 1',
                               '[1] = 12',
                               '[2] = 123',
                               '[3] = 1234',
                               '}'])

        self.expect("p numbers",
                    substrs = ['$', 'size=4',
                               '[0] = 1',
                               '[1] = 12',
                               '[2] = 123',
                               '[3] = 1234',
                               '}'])

        
        # check access to synthetic children
        self.runCmd("type summary add --summary-string \"item 0 is ${var[0]}\" std::int_vect int_vect")
        self.expect('frame variable numbers',
                    substrs = ['item 0 is 1']);
        
        self.runCmd("type summary add --summary-string \"item 0 is ${svar[0]}\" std::int_vect int_vect")
        self.expect('frame variable numbers',
                    substrs = ['item 0 is 1']);
        # move on with synths
        self.runCmd("type summary delete std::int_vect")
        self.runCmd("type summary delete int_vect")

        # add some more data
        self.runCmd("n");self.runCmd("n");self.runCmd("n");

        self.expect("frame variable numbers",
                    substrs = ['numbers = size=7',
                               '[0] = 1',
                               '[1] = 12',
                               '[2] = 123',
                               '[3] = 1234',
                               '[4] = 12345',
                               '[5] = 123456',
                               '[6] = 1234567',
                               '}'])
            
        self.expect("p numbers",
                    substrs = ['$', 'size=7',
                               '[0] = 1',
                               '[1] = 12',
                               '[2] = 123',
                               '[3] = 1234',
                               '[4] = 12345',
                               '[5] = 123456',
                               '[6] = 1234567',
                               '}'])

        # check access-by-index
        self.expect("frame variable numbers[0]",
                    substrs = ['1']);
        self.expect("frame variable numbers[1]",
                    substrs = ['12']);
        self.expect("frame variable numbers[2]",
                    substrs = ['123']);
        self.expect("frame variable numbers[3]",
                    substrs = ['1234']);

        # clear out the vector and see that we do the right thing once again
        self.runCmd("n")

        self.expect("frame variable numbers",
            substrs = ['numbers = size=0'])

        self.runCmd("n")

        # first value added
        self.expect("frame variable numbers",
                    substrs = ['numbers = size=1',
                               '[0] = 7',
                               '}'])

        # check if we can display strings
        self.runCmd("c")

        self.expect("frame variable strings",
            substrs = ['goofy',
                       'is',
                       'smart'])

        self.expect("p strings",
                    substrs = ['goofy',
                               'is',
                               'smart'])

        # test summaries based on synthetic children
        self.runCmd("type summary add std::string_vect string_vect --summary-string \"vector has ${svar%#} items\" -e")
        self.expect("frame variable strings",
                    substrs = ['vector has 3 items',
                               'goofy',
                               'is',
                               'smart'])

        self.expect("p strings",
                    substrs = ['vector has 3 items',
                               'goofy',
                               'is',
                               'smart'])

        self.runCmd("n")

        self.expect("frame variable strings",
                    substrs = ['vector has 4 items'])
        
        # check access-by-index
        self.expect("frame variable strings[0]",
                    substrs = ['goofy']);
        self.expect("frame variable strings[1]",
                    substrs = ['is']);

        self.runCmd("n")

        self.expect("frame variable strings",
            substrs = ['vector has 0 items'])

if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
