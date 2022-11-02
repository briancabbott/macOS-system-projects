# TestResilience.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
# Licensed under Apache License v2.0 with Runtime Library Exception
#
# See https://swift.org/LICENSE.txt for license information
# See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
#
# ------------------------------------------------------------------------------
"""
Test that resilient APIs work regardless of the combination of library and executable
"""
import subprocess
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil
import os
import os.path
import time
import unittest2

import sys
if sys.version_info.major == 2:
    import commands as subprocess
else:
    import subprocess

def execute_command(command):
    # print '%% %s' % (command)
    (exit_status, output) = subprocess.getstatusoutput(command)
    return exit_status


class TestResilience(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    @skipUnlessDarwin
    @swiftTest
    def test_cross_module_extension_a_a(self):
        """Test that LLDB can debug across resilient boundaries"""
        self.build()
        self.doTestWithFlavor("a", "a")

    @skipUnlessDarwin
    @swiftTest
    def test_cross_module_extension_a_b(self):
        """Test that LLDB can debug across resilient boundaries"""
        self.build()
        self.doTestWithFlavor("a", "b")

    @skipUnlessDarwin
    @swiftTest
    def test_cross_module_extension_b_a(self):
        """Test that LLDB can debug across resilient boundaries"""
        self.build()
        self.doTestWithFlavor("b", "a")

    @skipUnlessDarwin
    @swiftTest
    def test_cross_module_extension_b_b(self):
        """Test that LLDB can debug across resilient boundaries"""
        self.build()
        self.doTestWithFlavor("b", "b")


    def setUp(self):
        TestBase.setUp(self)

    def createSymlinks(self, exe_flavor, mod_flavor):
        execute_command("cp " + self.getBuildArtifact("main." + exe_flavor) + " " + self.getBuildArtifact("main"))
        execute_command("ln -sf " + self.getBuildArtifact("main." + exe_flavor + ".dSYM") + " " + self.getBuildArtifact("main.dSYM"))

        execute_command("cp " + self.getBuildArtifact("libmod." + exe_flavor + ".dylib") + " " + self.getBuildArtifact("libmod.dylib"))
        execute_command("ln -sf " + self.getBuildArtifact("libmod." + exe_flavor + ".dylib.dSYM") + " " + self.getBuildArtifact("libmod.dylib.dSYM"))
        execute_command("ln -sf " + self.getBuildArtifact("mod." + exe_flavor + ".o") + " " + self.getBuildArtifact("mod.o"))

        execute_command("ln -sf " + self.getBuildArtifact("mod." + exe_flavor + ".swiftmodule") + " " + self.getBuildArtifact("mod.swiftmodule"))

    def cleanupSymlinks(self):
        execute_command(
            "rm " +
            self.getBuildArtifact("main") + " " +
            self.getBuildArtifact("main.dSYM") + " " +
            self.getBuildArtifact("libmod.dylib") + " " +
            self.getBuildArtifact("libmod.dylib.dSYM") + " " +
            self.getBuildArtifact("mod.swiftdoc") + " " +
            self.getBuildArtifact("mod.swiftmodule"))

    def check_global(self, symbol_name, substrs):
        self.expect("target var " + symbol_name,
                    DATA_TYPES_DISPLAYED_CORRECTLY,
                    substrs=substrs)
        self.expect("expr " + symbol_name,
                    DATA_TYPES_DISPLAYED_CORRECTLY,
                    substrs=substrs)

    def doTestWithFlavor(self, exe_flavor, mod_flavor):
        self.createSymlinks(exe_flavor, mod_flavor)

        exe_name = "main"
        exe_path = self.getBuildArtifact(exe_name)

        source_name = "main.swift"
        source_spec = lldb.SBFileSpec(source_name)

        target = self.dbg.CreateTarget(exe_path)
        self.assertTrue(target, VALID_TARGET)
        self.registerSharedLibrariesWithTarget(target, ['mod'])

        target, process, _, breakpoint = lldbutil.run_to_source_breakpoint(
            self, "break here", source_spec, exe_name=exe_path)
        dylib_breakpoint = target.BreakpointCreateByName("fA")

        # main.swift
        self.check_global("g_main_b", ["world"])
        self.check_global("g_main_s", ["a = 1"])
        self.check_global("g_main_t", ["a = 1", "a = 1"])
        self.check_global("g_main_nested_t", ["a = 1"])
        self.check_global("g_main_c", ["a = 1"])
        self.check_global("g_main_nested_c", ["a = 1"])

        # Test defining global variables in the expression evaluator.
        self.expect("expr -- var $g_main_b = g_main_b; $g_main_b",
                    substrs=["world"])
        self.expect("expr -- var $g_main_s = S(); $g_main_s", substrs=["a = 1"])
        self.expect("expr -- var $g_main_t = (S(), S()); $g_main_t",
                    substrs=["a = 1", "a = 1"])
        self.expect("expr -- var $g_main_c = g_main_c; $g_main_c",
                    substrs=["a = 1"])

        threads = lldbutil.continue_to_breakpoint(process, dylib_breakpoint)
        self.assertTrue(len(threads) == 1)
        
        # Test global variable inside the module defining S.
        self.check_global("g_b", ["hello"])
        self.check_global("g_s", ["a = 1"])
        self.check_global("g_t", ["a = 1", "a = 1"])
        self.check_global("g_c", ["a = 1"])
        # Test defining global variables in the expression evaluator
        # inside the module defining S.
        self.expect("expr -- var $g_b = g_b; $g_b", substrs=["hello"])
        self.expect("expr -- var $g_s = S(); $g_s", substrs=["a = 1"])
        self.expect("expr -- var $g_t = (S(), S()); $g_t",
                    substrs=["a = 1", "a = 1"])
        self.expect("expr -- var $g_c = g_c; $g_c", substrs=["a = 1"])
        threads = lldbutil.continue_to_breakpoint(process, breakpoint)

        # Back in main.swift
        self.assertTrue(len(threads) == 1)
        frame = threads[0].frames[0]
        
        # Try 'frame variable'
        var = frame.FindVariable("s")
        child = var.GetChildMemberWithName("a")
        lldbutil.check_variable(self, child, False, value="1")

        # Try the expression parser
        self.expect("expr s.a", DATA_TYPES_DISPLAYED_CORRECTLY, substrs=["1"])
        self.expect(
            "expr fA(s)",
            DATA_TYPES_DISPLAYED_CORRECTLY,
            substrs=["1"])

        process.Kill()

        self.cleanupSymlinks()

if __name__ == '__main__':
    import atexit
    lldb.SBDebugger.Initialize()
    atexit.register(lldb.SBDebugger.Terminate)
    unittest2.main()
