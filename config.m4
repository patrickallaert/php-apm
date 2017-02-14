dnl config.m4 for extension apm

dnl    APM stands for Alternative PHP Monitor
dnl    Copyright (C) 2008-2014  Davide Mendolia, Patrick Allaert
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
dnl
dnl    Authors: Patrick Allaert <patrickallaert@php.net>

AC_DEFUN([MYSQL_LIB_CHK], [
  str="$MYSQL_DIR/$1/lib$MYSQL_LIBNAME.*"
  for j in `echo $str`; do
    if test -r $j; then
      MYSQL_LIB_DIR=$MYSQL_DIR/$1
      break 2
    fi
  done
])

PHP_ARG_ENABLE(apm, whether to enable apm support,
[  --enable-apm            Enable apm support], yes)
PHP_ARG_WITH(sqlite3, enable support for sqlite3,
[  --with-sqlite3=DIR      Location of sqlite3 library], yes, no)
PHP_ARG_WITH(mysql, enable support for MySQL,
[  --with-mysql=FILE       FILE is path to mysql_config command], yes, no)
PHP_ARG_ENABLE(statsd, enable support for statsd,
[  --enable-statsd         Enable statsd support], yes, no)
PHP_ARG_ENABLE(socket, enable support for socket,
[  --enable-socket         Enable socket support], yes, no)
PHP_ARG_WITH(debugfile, enable the debug file,
[  --with-debugfile=[FILE]   Location of debugging file (/tmp/apm.debug by default)], no, no)
PHP_ARG_WITH(defaultdb, set default sqlite3 default DB path,
[  --with-defaultdb=DIR    Location of sqlite3 default DB], no, no)

if test -z "$PHP_ZLIB_DIR"; then
  PHP_ARG_WITH(zlib-dir, for the location of libz, 
  [  --with-zlib-dir[=DIR]     MySQL: Set the path to libz install prefix], no, no)
fi

if test "$PHP_APM" != "no"; then

  AC_CONFIG_HEADERS()

  if test "$PHP_DEBUGFILE" != "no"; then
    if test "$PHP_DEBUGFILE" = "yes"; then
      debugfile="/tmp/apm.debug"
    else
      debugfile="$PHP_DEBUGFILE"
    fi
    AC_DEFINE_UNQUOTED(APM_DEBUGFILE, "$debugfile", [file used for debugging APM])
  fi

  if test "$PHP_SQLITE3" != "no"; then
    sqlite3_driver="driver_sqlite3.c"
    AC_DEFINE(APM_DRIVER_SQLITE3, 1, [activate sqlite3 storage driver])
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
      PHP_ADD_LIBRARY_WITH_PATH(sqlite3, $SQLITE3_DIR/lib, APM_SHARED_LIBADD)
      PHP_ADD_INCLUDE($SQLITE3_DIR/include)
    ],[
      AC_MSG_RESULT([not found])
      AC_MSG_ERROR([Please install SQLite 3.* first or check libsqlite3-dev is present])
    ],[
      APM_SHARED_LIBADD -lsqlite3
    ])

    AC_DEFINE(HAVE_SQLITE3,1,[sqlite3 found and included])

    AC_MSG_CHECKING([location of default sqlite3 DB])
    if test "$PHP_DEFAULTDB" != "no"; then
       dnl Value defined at buildtime
       AC_MSG_RESULT($PHP_DEFAULTDB)
       AC_DEFINE_UNQUOTED(SQLITE3_DEFAULTDB,"$PHP_DEFAULTDB",[Default sqlite3 DB])
    elif test -d /var/lib/php; then
       dnl RPM distro use /var/lib/php
       AC_MSG_RESULT("/var/lib/php/apm/db")
       AC_DEFINE(SQLITE3_DEFAULTDB,"/var/lib/php/apm/db",[Default sqlite3 DB])
    else
       dnl Other distro use /var/php
       AC_MSG_RESULT("/var/php/apm/db")
       AC_DEFINE(SQLITE3_DEFAULTDB,"/var/php/apm/db",[Default sqlite3 DB])
    fi
  fi

  if test "$PHP_MYSQL" != "no"; then
    mysql_driver="driver_mysql.c"
    AC_DEFINE(APM_DRIVER_MYSQL, 1, [activate MySQL storage driver])

    if test "$PHP_MYSQL" != "yes"; then
      dnl mysql_config path given as option parameter
      MYSQL_CONFIG=$PHP_MYSQL
    else
      dnl search mysql_config in PATH
      AC_PATH_PROG(MYSQL_CONFIG, mysql_config, no)
    fi

    if test "$enable_maintainer_zts" = "yes"; then
      MYSQL_LIB_CFG='--libs_r'
      MYSQL_LIB_NAME='mysqlclient_r'
    else
      MYSQL_LIB_CFG='--libs'
      MYSQL_LIB_NAME='mysqlclient'
    fi

    AC_MSG_CHECKING([Configuration of MySQL ])
    if test -x "$MYSQL_CONFIG" && $MYSQL_CONFIG $MYSQL_LIB_CFG > /dev/null 2>&1; then
      MYSQL_INCLINE=`$MYSQL_CONFIG --cflags | $SED -e "s/'//g"`
      MYSQL_LIBLINE=`$MYSQL_CONFIG $MYSQL_LIB_CFG | $SED -e "s/'//g"`
      AC_MSG_RESULT([INC=$MYSQL_INCLINE  LIB=$MYSQL_LIBLINE])
    else
      AC_MSG_RESULT([mysql_config not found])
      AC_MSG_ERROR([Please reinstall the mysql distribution])
    fi

    PHP_CHECK_LIBRARY($MYSQL_LIB_NAME, mysql_close, [], [
      AC_MSG_ERROR([Missing symbols, Please reinstall the mysql distribution])
    ], $MYSQL_LIBLINE)
    PHP_CHECK_LIBRARY($MYSQL_LIB_NAME, mysql_error, [
      AC_DEFINE(HAVE_MYSQL,1,[MySQL found and included])
      PHP_EVAL_INCLINE($MYSQL_INCLINE)
      PHP_EVAL_LIBLINE($MYSQL_LIBLINE, APM_SHARED_LIBADD)
    ], [
      AC_MSG_ERROR([Missing symbols, Please reinstall the mysql distribution])
    ], $MYSQL_LIBLINE)
  fi

  if test "$PHP_STATSD" != "no"; then
    statsd_driver="driver_statsd.c"
    AC_DEFINE(APM_DRIVER_STATSD, 1, [activate statsd driver])
  fi

  if test "$PHP_SOCKET" != "no"; then
    socket_driver="driver_socket.c"
    AC_DEFINE(APM_DRIVER_SOCKET, 1, [activate socket driver])
  fi

  PHP_NEW_EXTENSION(apm, apm.c backtrace.c $sqlite3_driver $mysql_driver $statsd_driver $socket_driver, $ext_shared)
  PHP_SUBST(APM_SHARED_LIBADD)
fi
