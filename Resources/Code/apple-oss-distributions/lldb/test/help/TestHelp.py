"""
Test some lldb help commands.

See also CommandInterpreter::OutputFormattedHelpText().
"""

import os, time
import unittest2
import lldb
from lldbtest import *

class HelpCommandTestCase(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    def test_simplehelp(self):
        """A simple test of 'help' command and its output."""
        self.expect("help",
            startstr = 'The following is a list of built-in, permanent debugger commands')

        self.expect("help", matching=False,
                    substrs = ['next'])
        
        self.expect("help -a", matching=True,
                    substrs = ['next'])
    
    def test_help_on_help(self):
        """Testing the help on the help facility."""
        self.expect("help help", matching=True,
                    substrs = ['--show-aliases',
                               '--hide-user-commands'])

    def version_number_string(self):
        """Helper function to find the version number string of lldb."""
        plist = os.path.join(os.environ["LLDB_SRC"], "resources", "LLDB-Info.plist")
        try:
            CFBundleVersionSegFound = False
            with open(plist, 'r') as f:
                for line in f:
                    if CFBundleVersionSegFound:
                        version_line = line.strip()
                        import re
                        m = re.match("<string>(.*)</string>", version_line)
                        if m:
                            version = m.group(1)
                            return version
                        else:
                            # Unsuccessful, let's juts break out of the for loop.
                            break

                    if line.find("<key>CFBundleVersion</key>") != -1:
                        # Found our match.  The next line contains our version
                        # string, for example:
                        # 
                        #     <string>38</string>
                        CFBundleVersionSegFound = True

        except:
            # Just fallthrough...
            import traceback
            traceback.print_exc()
            pass

        # Use None to signify that we are not able to grok the version number.
        return None


    def test_help_arch(self):
        """Test 'help arch' which should list of supported architectures."""
        self.expect("help arch",
            substrs = ['arm', 'x86_64', 'i386'])

    def test_help_version(self):
        """Test 'help version' and 'version' commands."""
        self.expect("help version",
            substrs = ['Show version of LLDB debugger.'])
        version_str = self.version_number_string()
        import re
        match = re.match('[0-9]+', version_str)
        if sys.platform.startswith("darwin"):
            search_regexp = ['lldb-' + (version_str if match else '[0-9]+')]
        else:
            search_regexp = ['lldb version (\d|\.)+.*$']

        self.expect("version",
            patterns = search_regexp)

    def test_help_should_not_crash_lldb(self):
        """Command 'help disasm' should not crash lldb."""
        self.runCmd("help disasm", check=False)
        self.runCmd("help unsigned-integer")

    def test_help_should_not_hang_emacsshell(self):
        """Command 'settings set term-width 0' should not hang the help command."""
        self.expect("settings set term-width 0",
                    COMMAND_FAILED_AS_EXPECTED, error=True,
            substrs = ['error: 0 is out of range, valid values must be between'])
        # self.runCmd("settings set term-width 0")
        self.expect("help",
            startstr = 'The following is a list of built-in, permanent debugger commands')

    def test_help_breakpoint_set(self):
        """Test that 'help breakpoint set' does not print out redundant lines of:
        'breakpoint set [-s <shlib-name>] ...'."""
        self.expect("help breakpoint set", matching=False,
            substrs = ['breakpoint set [-s <shlib-name>]'])

    def test_help_image_dump_symtab_should_not_crash(self):
        """Command 'help image dump symtab' should not crash lldb."""
        # 'image' is an alias for 'target modules'.
        self.expect("help image dump symtab",
            substrs = ['dump symtab',
                       'sort-order'])

    def test_help_image_du_sym_is_ambiguous(self):
        """Command 'help image du sym' is ambiguous and spits out the list of candidates."""
        self.expect("help image du sym",
                    COMMAND_FAILED_AS_EXPECTED, error=True,
            substrs = ['error: ambiguous command image du sym',
                       'symfile',
                       'symtab'])

    def test_help_image_du_line_should_work(self):
        """Command 'help image du line' is not ambiguous and should work."""
        # 'image' is an alias for 'target modules'.
        self.expect("help image du line",
            substrs = ['Dump the line table for one or more compilation units'])

    def test_help_target_variable_syntax(self):
        """Command 'help target variable' should display <variable-name> ..."""
        self.expect("help target variable",
            substrs = ['<variable-name> [<variable-name> [...]]'])

    def test_help_watchpoint_and_its_args(self):
        """Command 'help watchpoint', 'help watchpt-id', and 'help watchpt-id-list' should work."""
        self.expect("help watchpoint",
            substrs = ['delete', 'disable', 'enable', 'list'])
        self.expect("help watchpt-id",
            substrs = ['<watchpt-id>'])
        self.expect("help watchpt-id-list",
            substrs = ['<watchpt-id-list>'])

    def test_help_watchpoint_set(self):
        """Test that 'help watchpoint set' prints out 'expression' and 'variable'
        as the possible subcommands."""
        self.expect("help watchpoint set",
            substrs = ['The following subcommands are supported:'],
            patterns = ['expression +--',
                        'variable +--'])


if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lambda: lldb.SBDebugger.Terminate())
    unittest2.main()
