#! /bin/sh

host_architecture=""
requested_architecture=""
architecture_to_use=""

# classic-inferior-support
translate_mode=0
translate_binary=""

PATH=$PATH:/sbin:/bin:/usr/sbin:/usr/bin

# gdb is setgid procmod and dyld will truncate any DYLD_FRAMEWORK_PATH etc
# settings on exec.  The user is really trying to set these things
# in their process, not gdb.  So we smuggle it over the setgid border in
# GDB_DYLD_* where it'll be laundered inside gdb before invoking the inferior.

unset GDB_DYLD_FRAMEWORK_PATH
unset GDB_DYLD_FALLBACK_FRAMEWORK_PATH
unset GDB_DYLD_LIBRARY_PATH
unset GDB_DYLD_FALLBACK_LIBRARY_PATH
unset GDB_DYLD_ROOT_PATH
unset GDB_DYLD_PATHS_ROOT
unset GDB_DYLD_IMAGE_SUFFIX
unset GDB_DYLD_INSERT_LIBRARIES
[ -n "$DYLD_FRAMEWORK_PATH" ] && GDB_DYLD_FRAMEWORK_PATH="$DYLD_FRAMEWORK_PATH"
[ -n "$DYLD_FALLBACK_FRAMEWORK_PATH" ] && GDB_DYLD_FALLBACK_FRAMEWORK_PATH="$DYLD_FALLBACK_FRAMEWORK_PATH"
[ -n "$DYLD_LIBRARY_PATH" ] && GDB_DYLD_LIBRARY_PATH="$DYLD_LIBRARY_PATH"
[ -n "$DYLD_FALLBACK_LIBRARY_PATH" ] && GDB_DYLD_FALLBACK_LIBRARY_PATH="$DYLD_FALLBACK_LIBRARY_PATH"
[ -n "$DYLD_ROOT_PATH" ] && GDB_DYLD_ROOT_PATH="$DYLD_ROOT_PATH"
[ -n "$DYLD_PATHS_ROOT" ] && GDB_DYLD_PATHS_ROOT="$DYLD_PATHS_ROOT"
[ -n "$DYLD_IMAGE_SUFFIX" ] && GDB_DYLD_IMAGE_SUFFIX="$DYLD_IMAGE_SUFFIX"
[ -n "$DYLD_INSERT_LIBRARIES" ] && GDB_DYLD_INSERT_LIBRARIES="$DYLD_INSERT_LIBRARIES"
export GDB_DYLD_FRAMEWORK_PATH
export GDB_DYLD_FALLBACK_FRAMEWORK_PATH
export GDB_DYLD_LIBRARY_PATH
export GDB_DYLD_FALLBACK_LIBRARY_PATH
export GDB_DYLD_ROOT_PATH
export GDB_DYLD_PATHS_ROOT
export GDB_DYLD_IMAGE_SUFFIX
export GDB_DYLD_INSERT_LIBRARIES

host_architecture=`(unset DYLD_PRINT_LIBRARIES; "arch") 2>/dev/null` || host_architecture=""

if [ -z "$host_architecture" ]; then
    echo "There was an error executing 'arch(1)'; assuming 'ppc'.";
    host_architecture="ppc";
fi


case "$1" in
  --help)
    echo "  --translate        Debug applications running under translate." >&2
    echo "  -arch i386|ppc     Specify a gdb targetting either ppc or i386" >&2
    ;;
  -arch=* | -a=* | --arch=*)
    requested_architecture=`echo "$1" | sed 's,^[^=]*=,,'`
    shift;;
  -arch | -a | --arch)
    shift
    requested_architecture="$1"
    shift;;
  -translate | --translate | -oah* | --oah*)
    translate_mode=1
    shift;;
esac

if [ $translate_mode -eq 1 ]
then
  if [ "$host_architecture" = i386 -a -x /usr/libexec/oah/translate ]
  then
    requested_architecture="ppc"
    translate_binary="/usr/libexec/oah/translate -execOAH"
  else
    echo ERROR: translate not available.  Running in normal debugger mode. >&2
  fi
fi

if [ -n "$requested_architecture" ]
then
  if [ "$requested_architecture" != ppc -a "$requested_architecture" != i386 ]
  then
    echo Unrecognized architecture \'$requested_architecture\', using host arch. >&2
    requested_architecture=""
  fi
fi

if [ -n "$requested_architecture" ]
then
  architecture_to_use="$requested_architecture"
else
  architecture_to_use="$host_architecture"
fi

case "$architecture_to_use" in
    ppc)
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-darwin"
        ;;
    i386)
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-i386-apple-darwin"
        ;;
    *)
        echo "Unknown architecture '$architecture_to_use'; using 'ppc' instead.";
        gdb="${GDB_ROOT}/usr/libexec/gdb/gdb-powerpc-apple-darwin"
        ;;
esac

if [ ! -x "$gdb" ]; then
    echo "Unable to start GDB: cannot find binary in '$gdb'"
    exit 1
fi

exec $translate_binary "$gdb" "$@"
