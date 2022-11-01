"""
Test thread states.
"""

import os, time
import unittest2
import lldb
from lldbtest import *
import lldbutil

class ThreadStateTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    @expectedFailureDarwin("rdar://15367566")
    def test_state_after_breakpoint_with_dsym(self):
        """Test thread state after breakpoint."""
        self.buildDsym(dictionary=self.getBuildFlags(use_cpp11=False))
        self.thread_state_after_breakpoint_test()

    @expectedFailureFreeBSD('llvm.org/pr15824')
    @dwarf_test
    @expectedFailureDarwin("rdar://15367566")
    def test_state_after_breakpoint_with_dwarf(self):
        """Test thread state after breakpoint."""
        self.buildDwarf(dictionary=self.getBuildFlags(use_cpp11=False))
        self.thread_state_after_breakpoint_test()

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    def test_state_after_continue_with_dsym(self):
        """Test thread state after continue."""
        self.buildDsym(dictionary=self.getBuildFlags(use_cpp11=False))
        self.thread_state_after_continue_test()

    @dwarf_test
    def test_state_after_continue_with_dwarf(self):
        """Test thread state after continue."""
        self.buildDwarf(dictionary=self.getBuildFlags(use_cpp11=False))
        self.thread_state_after_continue_test()

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    def test_state_after_expression_with_dsym(self):
        """Test thread state after expression."""
        self.buildDsym(dictionary=self.getBuildFlags(use_cpp11=False))
        self.thread_state_after_continue_test()

    @dwarf_test
    def test_state_after_expression_with_dwarf(self):
        """Test thread state after expression."""
        self.buildDwarf(dictionary=self.getBuildFlags(use_cpp11=False))
        self.thread_state_after_continue_test()

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    @unittest2.expectedFailure("llvm.org/pr16172") # thread states not properly maintained
    def test_process_interrupt_with_dsym(self):
        """Test process interrupt."""
        self.buildDsym(dictionary=self.getBuildFlags(use_cpp11=False))
        self.process_interrupt_test()

    @dwarf_test
    @unittest2.expectedFailure("llvm.org/pr16712") # thread states not properly maintained
    def test_process_interrupt_with_dwarf(self):
        """Test process interrupt."""
        self.buildDwarf(dictionary=self.getBuildFlags(use_cpp11=False))
        self.process_interrupt_test()

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    @unittest2.expectedFailure("llvm.org/pr15824") # thread states not properly maintained
    def test_process_state_with_dsym(self):
        """Test thread states (comprehensive)."""
        self.buildDsym(dictionary=self.getBuildFlags(use_cpp11=False))
        self.thread_states_test()

    @dwarf_test
    @unittest2.expectedFailure("llvm.org/pr15824") # thread states not properly maintained
    def test_process_state_with_dwarf(self):
        """Test thread states (comprehensive)."""
        self.buildDwarf(dictionary=self.getBuildFlags(use_cpp11=False))
        self.thread_states_test()

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)
        # Find the line numbers for our breakpoints.
        self.break_1 = line_number('main.c', '// Set first breakpoint here')
        self.break_2 = line_number('main.c', '// Set second breakpoint here')

    def thread_state_after_breakpoint_test(self):
        """Test thread state after breakpoint."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        # This should create a breakpoint in the main thread.
        lldbutil.run_break_set_by_file_and_line (self, "main.c", self.break_1, num_expected_locations=1)

        # The breakpoint list should show 1 breakpoint with 1 location.
        self.expect("breakpoint list -f", "Breakpoint location shown correctly",
            substrs = ["1: file = 'main.c', line = %d, locations = 1" % self.break_1])

        # Run the program.
        self.runCmd("run", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['stopped',
                       '* thread #1',
                       'stop reason = breakpoint'])

        # Get the target process
        target = self.dbg.GetSelectedTarget()
        process = target.GetProcess()

        # Get the number of threads
        num_threads = process.GetNumThreads()

        self.assertTrue(num_threads == 1, 'Number of expected threads and actual threads do not match.')

        # Get the thread object
        thread = process.GetThreadAtIndex(0)

        # Make sure the thread is in the stopped state.
        self.assertTrue(thread.IsStopped(), "Thread state isn't \'stopped\' during breakpoint 1.")
        self.assertFalse(thread.IsSuspended(), "Thread state is \'suspended\' during breakpoint 1.")

        # Kill the process
        self.runCmd("process kill")

    def thread_state_after_continue_test(self):
        """Test thread state after continue."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        # This should create a breakpoint in the main thread.
        lldbutil.run_break_set_by_file_and_line (self, "main.c", self.break_1, num_expected_locations=1)
        lldbutil.run_break_set_by_file_and_line (self, "main.c", self.break_2, num_expected_locations=1)

        # The breakpoint list should show 1 breakpoints with 1 location.
        self.expect("breakpoint list -f", "Breakpoint location shown correctly",
            substrs = ["1: file = 'main.c', line = %d, locations = 1" % self.break_1])

        # Run the program.
        self.runCmd("run", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['stopped',
                       '* thread #1',
                       'stop reason = breakpoint'])

        # Get the target process
        target = self.dbg.GetSelectedTarget()
        process = target.GetProcess()

        # Get the number of threads
        num_threads = process.GetNumThreads()

        self.assertTrue(num_threads == 1, 'Number of expected threads and actual threads do not match.')

        # Get the thread object
        thread = process.GetThreadAtIndex(0)

        # Continue, the inferior will go into an infinite loop waiting for 'g_test' to change.
        self.dbg.SetAsync(True)
        self.runCmd("continue")
        time.sleep(1)

        # Check the thread state. It should be running.
        self.assertFalse(thread.IsStopped(), "Thread state is \'stopped\' when it should be running.")
        self.assertFalse(thread.IsSuspended(), "Thread state is \'suspended\' when it should be running.")

        # Go back to synchronous interactions
        self.dbg.SetAsync(False)

        # Kill the process
        self.runCmd("process kill")

    def thread_state_after_expression_test(self):
        """Test thread state after expression."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        # This should create a breakpoint in the main thread.
        lldbutil.run_break_set_by_file_and_line (self, "main.c", self.break_1, num_expected_locations=1)
        lldbutil.run_break_set_by_file_and_line (self, "main.c", self.break_2, num_expected_locations=1)

        # The breakpoint list should show 1 breakpoints with 1 location.
        self.expect("breakpoint list -f", "Breakpoint location shown correctly",
            substrs = ["1: file = 'main.c', line = %d, locations = 1" % self.break_1])

        # Run the program.
        self.runCmd("run", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['stopped',
                       '* thread #1',
                       'stop reason = breakpoint'])

        # Get the target process
        target = self.dbg.GetSelectedTarget()
        process = target.GetProcess()

        # Get the number of threads
        num_threads = process.GetNumThreads()

        self.assertTrue(num_threads == 1, 'Number of expected threads and actual threads do not match.')

        # Get the thread object
        thread = process.GetThreadAtIndex(0)

        # Get the inferior out of its loop
        self.runCmd("expression g_test = 1")

        # Check the thread state
        self.assertTrue(thread.IsStopped(), "Thread state isn't \'stopped\' after expression evaluation.")
        self.assertFalse(thread.IsSuspended(), "Thread state is \'suspended\' after expression evaluation.")

        # Let the process run to completion
        self.runCmd("process continue")


    def process_interrupt_test(self):
        """Test process interrupt and continue."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        # This should create a breakpoint in the main thread.
        lldbutil.run_break_set_by_file_and_line (self, "main.c", self.break_1, num_expected_locations=1)

        # The breakpoint list should show 1 breakpoints with 1 location.
        self.expect("breakpoint list -f", "Breakpoint location shown correctly",
            substrs = ["1: file = 'main.c', line = %d, locations = 1" % self.break_1])

        # Run the program.
        self.runCmd("run", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['stopped',
                       '* thread #1',
                       'stop reason = breakpoint'])

        # Get the target process
        target = self.dbg.GetSelectedTarget()
        process = target.GetProcess()

        # Get the number of threads
        num_threads = process.GetNumThreads()

        self.assertTrue(num_threads == 1, 'Number of expected threads and actual threads do not match.')

        # Continue, the inferior will go into an infinite loop waiting for 'g_test' to change.
        self.dbg.SetAsync(True)
        self.runCmd("continue")
        time.sleep(1)

        # Go back to synchronous interactions
        self.dbg.SetAsync(False)

        # Stop the process
        self.runCmd("process interrupt")

        # The stop reason of the thread should be signal.
        self.expect("process status", STOPPED_DUE_TO_SIGNAL,
            substrs = ['stopped',
                       '* thread #1',
                       'stop reason = signal'])

        # Get the inferior out of its loop
        self.runCmd("expression g_test = 1")

        # Run to completion
        self.runCmd("continue")

    def thread_states_test(self):
        """Test thread states (comprehensive)."""
        exe = os.path.join(os.getcwd(), "a.out")
        self.runCmd("file " + exe, CURRENT_EXECUTABLE_SET)

        # This should create a breakpoint in the main thread.
        lldbutil.run_break_set_by_file_and_line (self, "main.c", self.break_1, num_expected_locations=1)
        lldbutil.run_break_set_by_file_and_line (self, "main.c", self.break_2, num_expected_locations=1)

        # The breakpoint list should show 2 breakpoints with 1 location each.
        self.expect("breakpoint list -f", "Breakpoint location shown correctly",
            substrs = ["1: file = 'main.c', line = %d, locations = 1" % self.break_1,
                       "2: file = 'main.c', line = %d, locations = 1" % self.break_2])

        # Run the program.
        self.runCmd("run", RUN_SUCCEEDED)

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['stopped',
                       '* thread #1',
                       'stop reason = breakpoint'])

        # Get the target process
        target = self.dbg.GetSelectedTarget()
        process = target.GetProcess()

        # Get the number of threads
        num_threads = process.GetNumThreads()

        self.assertTrue(num_threads == 1, 'Number of expected threads and actual threads do not match.')

        # Get the thread object
        thread = process.GetThreadAtIndex(0)

        # Make sure the thread is in the stopped state.
        self.assertTrue(thread.IsStopped(), "Thread state isn't \'stopped\' during breakpoint 1.")
        self.assertFalse(thread.IsSuspended(), "Thread state is \'suspended\' during breakpoint 1.")

        # Continue, the inferior will go into an infinite loop waiting for 'g_test' to change.
        self.dbg.SetAsync(True)
        self.runCmd("continue")
        time.sleep(1)

        # Check the thread state. It should be running.
        self.assertFalse(thread.IsStopped(), "Thread state is \'stopped\' when it should be running.")
        self.assertFalse(thread.IsSuspended(), "Thread state is \'suspended\' when it should be running.")

        # Go back to synchronous interactions
        self.dbg.SetAsync(False)

        # Stop the process
        self.runCmd("process interrupt")

        # The stop reason of the thread should be signal.
        self.expect("process status", STOPPED_DUE_TO_SIGNAL,
            substrs = ['stopped',
                       '* thread #1',
                       'stop reason = signal'])

        # Check the thread state
        self.assertTrue(thread.IsStopped(), "Thread state isn't \'stopped\' after process stop.")
        self.assertFalse(thread.IsSuspended(), "Thread state is \'suspended\' after process stop.")

        # Get the inferior out of its loop
        self.runCmd("expression g_test = 1")

        # Check the thread state
        self.assertTrue(thread.IsStopped(), "Thread state isn't \'stopped\' after expression evaluation.")
        self.assertFalse(thread.IsSuspended(), "Thread state is \'suspended\' after expression evaluation.")

        # The stop reason of the thread should be signal.
        self.expect("process status", STOPPED_DUE_TO_SIGNAL,
            substrs = ['stopped',
                       '* thread #1',
                       'stop reason = signal'])

        # Run to breakpoint 2
        self.runCmd("continue")

        # The stop reason of the thread should be breakpoint.
        self.expect("thread list", STOPPED_DUE_TO_BREAKPOINT,
            substrs = ['stopped',
                       '* thread #1',
                       'stop reason = breakpoint'])

        # Make sure both threads are stopped
        self.assertTrue(thread.IsStopped(), "Thread state isn't \'stopped\' during breakpoint 2.")
        self.assertFalse(thread.IsSuspended(), "Thread state is \'suspended\' during breakpoint 2.")

        # Run to completion
        self.runCmd("continue")

        # At this point, the inferior process should have exited.
        self.assertTrue(process.GetState() == lldb.eStateExited, PROCESS_EXITED)

if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
