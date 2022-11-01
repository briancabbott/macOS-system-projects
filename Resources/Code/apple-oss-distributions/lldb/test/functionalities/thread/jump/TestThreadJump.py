"""
Test jumping to different places.
"""

import os, time
import unittest2
import lldb
from lldbtest import *
import lldbutil

class ThreadJumpTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    def test_with_dsym(self):
        """Test thread jump handling."""
        self.buildDsym(dictionary=self.getBuildFlags())
        self.thread_jump_test()

    @dwarf_test
    def test_with_dwarf(self):
        """Test thread jump handling."""
        self.buildDwarf(dictionary=self.getBuildFlags())
        self.thread_jump_test()

    def do_min_test(self, start, jump, var, value):
        self.runCmd("j %i" % start)                     # jump to the start marker
        self.runCmd("thread step-in")                   # step into the min fn
        self.runCmd("j %i" % jump)                      # jump to the branch we're interested in
        self.runCmd("thread step-out")                  # return out
        self.runCmd("thread step-over")                 # assign to the global
        self.expect("expr %s" % var, substrs = [value]) # check it

    def thread_jump_test(self):
        """Test thread exit handling."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        # Find the line numbers for our breakpoints.
        self.mark1 = line_number('main.cpp', '// 1st marker')
        self.mark2 = line_number('main.cpp', '// 2nd marker')
        self.mark3 = line_number('main.cpp', '// 3rd marker')
        self.mark4 = line_number('main.cpp', '// 4th marker')
        self.mark5 = line_number('other.cpp', '// other marker')

        lldbutil.run_break_set_by_file_and_line (self, "main.cpp", self.mark3, num_expected_locations=1)
        self.runCmd("run", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint 1.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT + " 1",
            substrs = ['stopped',
                       '* thread #1',
                       'stop reason = breakpoint 1'])

        self.do_min_test(self.mark3, self.mark1, "i", "4"); # Try the int path, force it to return 'a'
        self.do_min_test(self.mark3, self.mark2, "i", "5"); # Try the int path, force it to return 'b'
        self.do_min_test(self.mark4, self.mark1, "j", "7"); # Try the double path, force it to return 'a'
        self.do_min_test(self.mark4, self.mark2, "j", "8"); # Try the double path, force it to return 'b'

        # Try jumping to another function in a different file.
        self.runCmd("thread jump --file other.cpp --line %i --force" % self.mark5)
        self.expect("process status",
            substrs = ["at other.cpp:%i" % self.mark5])

        # Try jumping to another function (without forcing)
        self.expect("j main.cpp:%i" % self.mark1, COMMAND_FAILED_AS_EXPECTED, error = True,
            substrs = ["error"])

if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
