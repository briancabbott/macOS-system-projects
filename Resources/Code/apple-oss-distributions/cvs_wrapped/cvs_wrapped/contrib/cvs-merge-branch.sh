#!/bin/bash

##
# Merge diffs from a branch into the current working copy.
# First show you the diffs to apply, and ask for approval.
# The branch must have been created with cvs-make-branch
# in order for this to work.
##
# Copyright 1998-2002 Apple Computer, Inc.
##

##
# Set up PATH
##

MyPath=$(dirname $0):/usr/bin:/bin;

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
    echo "Usage: $(basename $0) [<options>] <branch_tag>";
    echo "	<branch_tag>: Tag for branch to merge.";
    echo "Options: -a automatically merge (don't ask); implies -f; does not commit";
    echo "         -f fast: skip diffs";
    echo "         -u unmerge";
    echo "         -v be verbose";
    exit 22;
}

# Ask yes or no question
boolean_ask ()
{
  local prompt;
  local reply;

  prompt=$1;

  while (true); do
      echo -n "${prompt} " ; read reply;

      case "${reply}" in
        y | yes | Y | YES | Yes )
          return 0;
          ;;
        n | no | N | NO | No )
          return 1;
          ;;
        *)
          echo -n "Huh? "
          ;;
      esac
  done
}

##
# Handle command line
##

   Action="MERGE";
Automatic="NO";
 SkipDiff="NO";
   q_Flag="-q";
   v_Flag="";

if ! args=$(getopt auvf $*); then usage; fi;
set -- $args;
for option; do
    case "$option" in
      -a)
	Automatic="YES";
	SkipDiff="YES";
	shift;
	;;
      -f)
	SkipDiff="YES";
	shift;
	;;
      -u)
	Action="UNMERGE";
	shift;
	;;
      -v)
	q_Flag="";
        v_Flag="-v";
	shift;
	;;
      --)
	shift;
	break;
	;;
    esac;
done;

Branch=$1; if [ $# != 0 ]; then shift; fi;

if [ $# != 0 ]; then usage; fi;

if [ -z "${Branch}" ]; then usage; fi;

if [ ! -d "CVS" ]; then
    echo "There is no version here. Exiting.";
    exit 2;
fi;

##
# Do The Right Thing
##

if [ "${SkipDiff}" = "NO" ]; then
    if [ "`basename $CVS_PROG`" = "ocvs" ]; then
	ocvs-diff-branch ${v_Flag} "${Branch}";
    else
	cvs-diff-branch ${v_Flag} "${Branch}";
    fi
fi;

echo -n "Prepared to ";

case $Action in
  MERGE)
    echo -n "merge";
    ;;
  UNMERGE)
    echo -n "unmerge";
    ;;
  *)
    echo "Internal error: assertion failed";
    exit 33;
esac;

echo " branch:";
echo "Branch tag: ${Branch}";

if [ "${Automatic}" = "NO" ] &&
   ! boolean_ask "Shall I continue? "; then
    exit 1;
fi;

case $Action in
  MERGE)
    echo -n "Merging";
    ;;
  UNMERGE)
    echo -n "Unmerging";
    ;;
esac;

echo -n " branch ${Branch} in working copy...";

case $Action in
  MERGE)
     Tag_left="${Branch}-base";
    Tag_right="${Branch}";
    ;;
  UNMERGE)
     Tag_left="${Branch}";
    Tag_right="${Branch}-base";
    ;;
esac;

${CVS_PROG} ${q_Flag} update -d -j "${Tag_left}" -j "${Tag_right}" | tee "#${Branch}.update#";

echo "Done.";

exit 0;
