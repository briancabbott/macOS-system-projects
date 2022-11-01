dnl
dnl $Id: config.m4,v 1.17.4.3.4.1 2005/11/18 15:10:33 sniper Exp $
dnl

PHP_ARG_WITH(libedit,for libedit readline replacement, 
[  --with-libedit[=DIR]    Include libedit readline replacement (CLI/CGI only)])

if test "$PHP_LIBEDIT" = "no"; then
  PHP_ARG_WITH(readline,for readline support,
  [  --with-readline[=DIR]   Include readline support (CLI/CGI only)])
fi

if test "$PHP_READLINE" != "no"; then
  for i in $PHP_READLINE /usr/local /usr; do
    test -f $i/include/readline/readline.h && READLINE_DIR=$i && break
  done

  if test -z "$READLINE_DIR"; then
    AC_MSG_ERROR(Please reinstall readline - I cannot find readline.h)
  fi

  PHP_ADD_INCLUDE($READLINE_DIR/include)

  PHP_READLINE_LIBS=""
  AC_CHECK_LIB(ncurses, tgetent,
  [
    PHP_ADD_LIBRARY(ncurses,,READLINE_SHARED_LIBADD)
    PHP_READLINE_LIBS="$PHP_READLINE_LIBS -lncurses"
  ],[
    AC_CHECK_LIB(termcap, tgetent,
    [
      PHP_ADD_LIBRARY(termcap,,READLINE_SHARED_LIBADD)
      PHP_READLINE_LIBS="$PHP_READLINE_LIBS -ltermcap"
    ])
  ])

  PHP_CHECK_LIBRARY(readline, readline,
  [
    PHP_ADD_LIBRARY_WITH_PATH(readline, $READLINE_DIR/lib, READLINE_SHARED_LIBADD)
  ], [
    AC_MSG_ERROR(readline library not found)
  ], [
    -L$READLINE_DIR/lib $PHP_READLINE_LIBS
  ])

  PHP_CHECK_LIBRARY(readline, rl_callback_read_char,
  [
    AC_DEFINE(HAVE_RL_CALLBACK_READ_CHAR, 1, [ ])
  ],[],[
    -L$READLINE_DIR/lib $PHP_READLINE_LIBS
  ])

  PHP_CHECK_LIBRARY(history, add_history,
  [
    PHP_ADD_LIBRARY_WITH_PATH(history, $READLINE_DIR/lib, READLINE_SHARED_LIBADD)
  ], [
    AC_MSG_ERROR(history library required by readline not found)
  ], [
    -L$READLINE_DIR/lib $PHP_READLINE_LIBS
  ])

  PHP_NEW_EXTENSION(readline, readline.c, $ext_shared, cli)
  PHP_SUBST(READLINE_SHARED_LIBADD)
  AC_DEFINE(HAVE_LIBREADLINE, 1, [ ])

elif test "$PHP_LIBEDIT" != "no"; then

  for i in $PHP_LIBEDIT /usr/local /usr; do
    test -f $i/include/readline/readline.h && LIBEDIT_DIR=$i && break
  done

  if test -z "$LIBEDIT_DIR"; then
    AC_MSG_ERROR(Please reinstall libedit - I cannot find readline.h)
  fi

  PHP_ADD_INCLUDE($LIBEDIT_DIR/include)

  AC_CHECK_LIB(ncurses, tgetent,
  [
    PHP_ADD_LIBRARY(ncurses,,READLINE_SHARED_LIBADD)
  ],[
    AC_CHECK_LIB(termcap, tgetent,
    [
      PHP_ADD_LIBRARY(termcap,,READLINE_SHARED_LIBADD)
    ])
  ])

  PHP_CHECK_LIBRARY(edit, readline,
  [
    PHP_ADD_LIBRARY_WITH_PATH(edit, $LIBEDIT_DIR/lib, READLINE_SHARED_LIBADD)  
  ], [
    AC_MSG_ERROR(edit library required by readline not found)
  ], [
    -L$READLINE_DIR/lib 
  ])

  PHP_NEW_EXTENSION(readline, readline.c, $ext_shared, cli)
  PHP_SUBST(READLINE_SHARED_LIBADD)
  AC_DEFINE(HAVE_LIBEDIT, 1, [ ])
fi
