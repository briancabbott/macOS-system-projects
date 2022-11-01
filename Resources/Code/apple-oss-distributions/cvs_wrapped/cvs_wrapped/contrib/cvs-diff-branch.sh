#!/bin/bash

##
# Show diffs for a branch.
# The branch must have been made with cvs-make-branch.
##
# Copyright 1998-2002 Apple Computer, Inc.
##

##
# Set up PATH
##

MyPath=/usr/bin:/bin;

if [ -z "${PATH}" ]; then
    export PATH=${MyPath};
else
    export PATH=${PATH}:${MyPath};
fi

CVS_PROG="$(basename $0)"
CVS_PROG="${CVS_PROG%%-*}"

##
# Usage
##

usage ()
{
    echo "Usage: $(basename $0) [-m <module>] [<options>] <branch_tag>";
    echo "	<module>: Name of CVS module. If not specified, use working copy in \".\".";
    echo "	<branch_tag>: Tag for branch to diff";
    echo "Options: -s summary only";
    echo "         -v be verbose";
    exit 22;
}

abort ()
{
    local Tee_File;

    echo -n "Abort: cleaning up...";

    if [ -n "${Tee_File}" ]; then
	rm -rf ${Tee_File};
    fi;

    echo "done.";

    exit 1;
}

##
# Catch signals
##

trap abort 1 2 15;

##
# Handle command line
##

     Project="";
Diff_Command="diff";
    Tee_File="";
      s_Flag="";
      q_Flag="-q";

if ! args=$(getopt svm:b: $*); then usage; fi;
set -- ${args};
for option; do
    case "${option}" in
      -m)
	     Project=$2;
	Diff_Command="rdiff";
	    Tee_File="/dev/null";
	shift;shift;
	;;
      -s)
	s_Flag="YES";
	shift;
	;;
      -v)
	q_Flag="";
	shift;
	;;
      --)
	shift;
	break;
	;;
    esac;
done;

if [ "${s_Flag}" = "YES" ]; then
  if [ "${Diff_Command}" = "rdiff" ]; then
    s_Flag="-s";
  else
    s_Flag="-q"
  fi;
fi;

Branch=$1; if [ $# != 0 ]; then shift; fi;

if [ $# != 0 ]; then usage; fi;

if [ -z "${Branch}" ]; then usage; fi;

if [ -z "${Project}" ] && [ ! -d "CVS" ]; then
    echo "There is no version here. Exiting.";
    exit 2;
fi;

if [ -z "${Tee_File}" ]; then Tee_File="#${Branch}.diffs#"; fi;

##
# Do The Right Thing
##

echo "Computing diffs for branch ${Branch}:";

${CVS_PROG} ${q_Flag} "${Diff_Command}" ${s_Flag} -r "${Branch}-base" -r "${Branch}" ${Project} | tee "${Tee_File}";
