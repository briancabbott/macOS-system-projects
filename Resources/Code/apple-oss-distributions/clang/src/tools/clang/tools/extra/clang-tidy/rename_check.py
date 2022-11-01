#!/usr/bin/env python
#
#===- rename_check.py - clang-tidy check renamer -------------*- python -*--===#
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
#===------------------------------------------------------------------------===#

import os
import re
import sys
import glob

def replaceInFile(fileName, sFrom, sTo):
  if sFrom == sTo:
    return
  txt = None
  with open(fileName, "r") as f:
    txt = f.read()

  if sFrom not in txt:
    return

  txt = txt.replace(sFrom, sTo)
  print("Replace '%s' -> '%s' in '%s'" % (sFrom, sTo, fileName))
  with open(fileName, "w") as f:
    f.write(txt)

def generateCommentLineHeader(filename):
  return ''.join(['//===--- ',
                  os.path.basename(filename),
                  ' - clang-tidy ',
                  '-' * max(0, 42 - len(os.path.basename(filename))),
                  '*- C++ -*-===//'])

def generateCommentLineSource(filename):
  return ''.join(['//===--- ',
                  os.path.basename(filename),
                  ' - clang-tidy',
                  '-' * max(0, 52 - len(os.path.basename(filename))),
                  '-===//'])

def fileRename(fileName, sFrom, sTo):
  if sFrom not in fileName:
    return fileName
  newFileName = fileName.replace(sFrom, sTo)
  print("Rename '%s' -> '%s'" % (fileName, newFileName))
  os.rename(fileName, newFileName)
  return newFileName

def getListOfFiles(clang_tidy_path):
  files =  glob.glob(os.path.join(clang_tidy_path,'*'))
  for dirname in files:
    if os.path.isdir(dirname):
      files += glob.glob(os.path.join(dirname,'*'))
  files += glob.glob(os.path.join(clang_tidy_path,'..', 'test', 'clang-tidy', '*'))
  files += glob.glob(os.path.join(clang_tidy_path,'..', 'docs', 'clang-tidy', 'checks', '*'))
  return [filename for filename in files if os.path.isfile(filename)]

def main():
  if len(sys.argv) != 4:
    print('Usage: rename_check.py <module> <old-check-name> <new-check-name>\n')
    print('       example: rename_check.py misc awesome-functions new-awesome-function')
    return

  module = sys.argv[1].lower()
  check_name = sys.argv[2]
  check_name_camel = ''.join(map(lambda elem: elem.capitalize(),
                                 check_name.split('-'))) + 'Check'
  check_name_new = sys.argv[3]
  check_name_new_camel = ''.join(map(lambda elem: elem.capitalize(),
                                 check_name_new.split('-'))) + 'Check'

  clang_tidy_path = os.path.dirname(sys.argv[0])

  header_guard_old = module.upper() + '_' + check_name.upper().replace('-', '_')
  header_guard_new = module.upper() + '_' + check_name_new.upper().replace('-', '_')

  for filename in getListOfFiles(clang_tidy_path):
    originalName = filename
    filename = fileRename(filename, check_name, check_name_new)
    filename = fileRename(filename, check_name_camel, check_name_new_camel)
    replaceInFile(filename, generateCommentLineHeader(originalName), generateCommentLineHeader(filename))
    replaceInFile(filename, generateCommentLineSource(originalName), generateCommentLineSource(filename))
    replaceInFile(filename, header_guard_old, header_guard_new)
    replaceInFile(filename, check_name, check_name_new)
    replaceInFile(filename, check_name_camel, check_name_new_camel)

if __name__ == '__main__':
  main()
