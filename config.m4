dnl config.m4 for extension apm

dnl    APM stands for Alternative PHP Monitor
dnl    Copyright (C) 2008  Davide Mendolia, Patrick Allaert
dnl
dnl    This file is part of APM.
dnl
dnl    This source file is subject to version 3.01 of the PHP license,
dnl    that is bundled with this package in the file LICENSE, and is
dnl    available through the world-wide-web at the following url:
dnl    http://www.php.net/license/3_01.txt
dnl    If you did not receive a copy of the PHP license and are unable to
dnl    obtain it through the world-wide-web, please send a note to
dnl    license@php.net so we can mail you a copy immediately.

PHP_ARG_ENABLE(apm, whether to enable apm support,
[  --enable-apm           Enable apm support])

if test "$PHP_APM" != "no"; then

  AC_MSG_CHECKING([for sqlite3 files in default path])
  for i in /usr/local /usr; do
    if test -f $i/include/sqlite3.h; then
      SQLITE3_DIR=$i
      AC_MSG_RESULT(found in $i)
    fi
  done

  if test -z "$SQLITE3_DIR"; then
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please reinstall the sqlite])
  fi

  AC_MSG_CHECKING([for SQLite 3.*])
  PHP_CHECK_LIBRARY(sqlite3, sqlite3_open, [
    AC_MSG_RESULT(found)
    PHP_ADD_LIBRARY_WITH_PATH(sqlite3, $SQLITE3_DIR/lib, APM_SHARED_LIBADD)
    PHP_ADD_INCLUDE($SQLITE3_DIR/include)
  ],[
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please install SQLite 3.* first or check libsqlite3-dev is present])
  ],[
    APM_SHARED_LIBADD -lsqlite3
  ])

  AC_DEFINE(HAVE_SQLITE3,1,[sqlite3 found and included])

  PHP_NEW_EXTENSION(apm, apm.c backtrace.c, $ext_shared)
  PHP_SUBST(APM_SHARED_LIBADD)
fi
