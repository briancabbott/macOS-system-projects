"""
Test watchpoint modify command to set condition on a watchpoint.
"""

import os, time
import unittest2
import lldb
from lldbtest import *
import lldbutil

class WatchpointConditionCmdTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)
        # Our simple source filename.
        self.source = 'main.cpp'
        # Find the line number to break inside main().
        self.line = line_number(self.source, '// Set break point at this line.')
        # And the watchpoint variable declaration line number.
        self.decl = line_number(self.source, '// Watchpoint variable declaration.')
        # Build dictionary to have unique executable names for each test method.
        self.exe_name = self.testMethodName
        self.d = {'CXX_SOURCES': self.source, 'EXE': self.exe_name}

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    def test_watchpoint_cond_with_dsym(self):
        """Test watchpoint condition."""
        self.buildDsym(dictionary=self.d)
        self.setTearDownCleanup(dictionary=self.d)
        self.watchpoint_condition()

    @expectedFailureFreeBSD('llvm.org/pr16706') # Watchpoints fail on FreeBSD
    @dwarf_test
    def test_watchpoint_cond_with_dwarf(self):
        """Test watchpoint condition."""
        self.buildDwarf(dictionary=self.d)
        self.setTearDownCleanup(dictionary=self.d)
        self.watchpoint_condition()

    def watchpoint_condition(self):
        """Do watchpoint condition 'global==5'."""
        exe = os.path.join(os.getcwd(), self.exe_name)
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        # Add a breakpoint to set a watchpoint when stopped on the breakpoint.
        lldbutil.run_break_set_by_file_and_line (self, None, self.line, num_expected_locations=1)

        # Run the program.
        self.runCmd("run", RUN_SUCCEEDED)

        # We should be stopped again due to the breakpoint.
        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['stopped',
                       'stop reason = breakpoint'])

        # Now let's set a write-type watchpoint for 'global'.
        # With a condition of 'global==5'.
        self.expect("watchpoint set variable -w write global", WATCHPOINT_CREATED,
            substrs = ['Watchpoint created', 'size = 4', 'type = w',
                       '%s:%d' % (self.source, self.decl)])

        self.runCmd("watchpoint modify -c 'global==5'")

        # Use the '-v' option to do verbose listing of the watchpoint.
        # The hit count should be 0 initially.
        self.expect("watchpoint list -v",
            substrs = ['hit_count = 0', 'global==5'])

        self.runCmd("process continue")

        # We should be stopped again due to the watchpoint (write type).
        # The stop reason of the thread should be watchpoint.
        self.expect("thread backtrace", STOPPED_DUE_TO_WATCHPOINT,
            substrs = ['stop reason = watchpoint'])
        self.expect("frame variable --show-globals global",
            substrs = ['(int32_t)', 'global = 5'])

        # Use the '-v' option to do verbose listing of the watchpoint.
        # The hit count should now be 2.
        self.expect("watchpoint list -v",
            substrs = ['hit_count = 5'])


if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
