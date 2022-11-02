#!/usr/bin/env python

#===--- apply-fixit-edits.py - Tool for applying edits from .remap files ---===#
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2014 - 2015 Apple Inc. and the Swift project authors
# Licensed under Apache License v2.0 with Runtime Library Exception
#
# See http://swift.org/LICENSE.txt for license information
# See http://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
#
#===------------------------------------------------------------------------===#


import subprocess
import json
import argparse
import sys

def find_remap_files(path):
    out = None
    try:
        out = subprocess.check_output(["find", path, "-name", "*.remap"])
    except subprocess.CalledProcessError:
        return None
    lines = out.split('\n')
    lines.pop(-1)
    return lines

def apply_edits(path):
    remap_files = find_remap_files(path)
    if not remap_files:
        print "No remap files found"
        return 1;

    edits_set = set()
    for remap_file in remap_files:
        json_data = open(remap_file).read()
        json_data = json_data.replace(",\n }", "\n }")
        json_data = json_data.replace(",\n]", "\n]")
        curr_edits = json.loads(json_data)
        for ed in curr_edits:
            fname = ed["file"]
            offset = ed["offset"]
            length = ed.get("remove", 0)
            text = ed.get("text", "")
            edits_set.add((fname, offset, length, text))

    edits_per_file = {}
    for ed in edits_set:
        fname = ed[0]
        if not edits_per_file.has_key(fname):
            edits_per_file[fname] = []
        edits_per_file[fname].append((ed[1], ed[2], ed[3]))
    
    for fname, edits in edits_per_file.iteritems():
        print 'Updating', fname
        edits.sort(reverse=True)
        file_data = open(fname).read()
        for ed in edits:
            offset = ed[0]
            length = ed[1]
            text = ed[2]
            file_data = file_data[:offset] + str(text) + file_data[offset+length:]
        open(fname, 'w').write(file_data)
    return 0

def main():
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description="""Finds all .remap files in a directory and applies their
edits to the source files.""")
    parser.add_argument("build_dir_path",
        help="path to index info")

    args = parser.parse_args()
    return apply_edits(args.build_dir_path)

if __name__ == "__main__":
    sys.exit(main())
