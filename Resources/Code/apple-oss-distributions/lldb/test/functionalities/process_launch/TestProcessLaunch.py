"""
Test lldb process launch flags.
"""

import os, time
import unittest2
import lldb
from lldbtest import *

class ProcessLaunchTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)
        # disable "There is a running process, kill it and restart?" prompt
        self.runCmd("settings set auto-confirm true")
        self.addTearDownHook(lambda: self.runCmd("settings clear auto-confirm"))

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    def test_io_with_dsym (self):
        """Test that process launch I/O redirection flags work properly."""
        self.buildDsym ()
        self.process_io_test ()

    @dwarf_test
    def test_io_with_dwarf (self):
        """Test that process launch I/O redirection flags work properly."""
        self.buildDwarf ()
        self.process_io_test ()

    @not_remote_testsuite_ready
    def process_io_test (self):
        """Test that process launch I/O redirection flags work properly."""
        exe = os.path.join (os.getcwd(), "a.out")
        self.expect("file " + exe,
                    patterns = [ "Current executable set to .*a.out" ])


        in_file = os.path.join (os.getcwd(), "input-file.txt")
        out_file = os.path.join (os.getcwd(), "output-test.out")
        err_file = os.path.join (os.getcwd(), "output-test.err")


        # Make sure the output files do not exist before launching the process
        try:
            os.remove (out_file)
        except OSError:
            pass

        try:
            os.remove (err_file)
        except OSError:
            pass

        launch_command = "process launch -i " + in_file + " -o " + out_file + " -e " + err_file

        self.expect (launch_command,
                     patterns = [ "Process .* launched: .*a.out" ])


        success = True
        err_msg = ""

        # Check to see if the 'stdout' file was created
        try:
            out_f = open (out_file)
        except IOError:
            success = False
            err_msg = err_msg + "   ERROR: stdout file was not created.\n"
        else:
            # Check to see if the 'stdout' file contains the right output
            line = out_f.readline ();
            if line != "This should go to stdout.\n":
                success = False
                err_msg = err_msg + "    ERROR: stdout file does not contain correct output.\n"
                out_f.close();

        # Try to delete the 'stdout' file
        try:
            os.remove (out_file)
        except OSError:
            pass

        # Check to see if the 'stderr' file was created
        try:
            err_f = open (err_file)
        except IOError:
            success = False
            err_msg = err_msg + "     ERROR:  stderr file was not created.\n"
        else:
            # Check to see if the 'stderr' file contains the right output
            line = err_f.readline ()
            if line != "This should go to stderr.\n":
                success = False
                err_msg = err_msg + "    ERROR: stderr file does not contain correct output.\n\
"
                err_f.close()

        # Try to delete the 'stderr' file
        try:
            os.remove (err_file)
        except OSError:
            pass

        if not success:
            self.fail (err_msg)

    d = {'CXX_SOURCES' : 'print_cwd.cpp'}

    @unittest2.skipUnless(sys.platform.startswith("darwin"), "requires Darwin")
    @dsym_test
    def test_set_working_dir_with_dsym (self):
        """Test that '-w dir' sets the working dir when running the inferior."""
        self.buildDsym(dictionary=self.d)
        self.setTearDownCleanup(self.d)
        self.my_working_dir_test()

    @skipIfFreeBSD # llvm.org/pr16684
    @dwarf_test
    def test_set_working_dir_with_dwarf (self):
        """Test that '-w dir' sets the working dir when running the inferior."""
        self.buildDwarf(dictionary=self.d)
        self.setTearDownCleanup(self.d)
        self.my_working_dir_test()

    # rdar://problem/9056462
    # The process launch flag '-w' for setting the current working directory not working?
    @not_remote_testsuite_ready
    def my_working_dir_test (self):
        """Test that '-w dir' sets the working dir when running the inferior."""
        exe = os.path.join (os.getcwd(), "a.out")
        self.runCmd("file " + exe)

        mywd = 'my_working_dir'
        out_file_name = "my_working_dir_test.out"
        err_file_name = "my_working_dir_test.err"

        my_working_dir_path = os.path.join(os.getcwd(), mywd)
        out_file_path = os.path.join(my_working_dir_path, out_file_name)
        err_file_path = os.path.join(my_working_dir_path, err_file_name)

        # Make sure the output files do not exist before launching the process
        try:
            os.remove (out_file_path)
            os.remove (err_file_path)
        except OSError:
            pass

        # Check that we get an error when we have a nonexisting path
        launch_command = "process launch -w %s -o %s -e %s" % (my_working_dir_path + 'z',
                                                               out_file_path,
                                                               err_file_path)

        self.expect(launch_command, error=True,
                patterns = ["error:.* No such file or directory: %sz" % my_working_dir_path])

        # Really launch the process
        launch_command = "process launch -w %s -o %s -e %s" % (my_working_dir_path,
                                                               out_file_path,
                                                               err_file_path)

        self.expect(launch_command,
                    patterns = [ "Process .* launched: .*a.out" ])

        success = True
        err_msg = ""

        # Check to see if the 'stdout' file was created
        try:
            out_f = open(out_file_path)
        except IOError:
            success = False
            err_msg = err_msg + "ERROR: stdout file was not created.\n"
        else:
            # Check to see if the 'stdout' file contains the right output
            line = out_f.readline();
            if self.TraceOn():
                print "line:", line
            if not re.search(mywd, line):
                success = False
                err_msg = err_msg + "The current working directory was not set correctly.\n"
                out_f.close();

        # Try to delete the 'stdout' and 'stderr' files
        try:
            os.remove(out_file_path)
            os.remove(err_file_path)
            pass
        except OSError:
            pass

        if not success:
            self.fail(err_msg)


if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()

