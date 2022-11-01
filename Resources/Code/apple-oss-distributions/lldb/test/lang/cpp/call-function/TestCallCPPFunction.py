"""
Tests calling a function by basename
"""

import lldb
from lldbtest import *
import lldbutil

class CallCPPFunctionTestCase(TestBase):
    
    mydir = TestBase.compute_mydir(__file__)
    
    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    def test_with_dsym_and_run_command(self):
        """Test calling a function by basename"""
        self.buildDsym()
        self.call_cpp_function()

    @dwarf_test
    def test_with_dwarf_and_run_command(self):
        """Test calling a function by basename"""
        self.buildDwarf()
        self.call_cpp_function()

    def setUp(self):
        TestBase.setUp(self)
        self.line = line_number('main.cpp', '// breakpoint')
    
    def call_cpp_function(self):
        """Test calling a function by basename"""
        self.runCmd("file a.out", CURRENT_EXECUTABLE_SET)

        lldbutil.run_break_set_by_file_and_line (self, "main.cpp", self.line, num_expected_locations=1, loc_exact=True)

        self.runCmd("process launch", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list",
                    STOPPED_DUE_TO_BREAKPOINT,
                    substrs = ['stopped', 'stop reason = breakpoint'])

        self.expect("expression -- a_function_to_call()",
                    startstr = "(int) $0 = 0")

if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
