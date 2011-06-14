dnl config.m4 for extension apm

dnl    APM stands for Alternative PHP Monitor
dnl    Copyright (C) 2008-2010  Davide Mendolia, Patrick Allaert
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
dnl    Authors: Davide Mendolia <dmendolia@php.net>
dnl             Patrick Allaert <patrickallaert@php.net>

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
[  --with-mysql=DIR        Location of MySQL base directory], no, no)

if test -z "$PHP_ZLIB_DIR"; then
  PHP_ARG_WITH(zlib-dir, for the location of libz, 
  [  --with-zlib-dir[=DIR]     MySQL: Set the path to libz install prefix], no, no)
fi

if test "$PHP_APM" != "no"; then

  AC_CONFIG_HEADERS()

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
  fi

  if test "$PHP_MYSQL" != "no"; then
    mysql_driver="driver_mysql.c"
    AC_DEFINE(APM_DRIVER_MYSQL, 1, [activate MySQL storage driver])

    MYSQL_DIR=
    MYSQL_INC_DIR=

    for i in $PHP_MYSQL /usr/local /usr; do
      if test -r $i/include/mysql/mysql.h; then
        MYSQL_DIR=$i
        MYSQL_INC_DIR=$i/include/mysql
        break
      elif test -r $i/include/mysql.h; then
        MYSQL_DIR=$i
        MYSQL_INC_DIR=$i/include
        break
      fi
    done

    if test -z "$MYSQL_DIR"; then
      AC_MSG_ERROR([Cannot find MySQL header files])
    fi

    if test "$enable_maintainer_zts" = "yes"; then
      MYSQL_LIBNAME=mysqlclient_r
    else
      MYSQL_LIBNAME=mysqlclient
    fi
    case $host_alias in
      *netware*[)]
        MYSQL_LIBNAME=mysql
        ;;
    esac

    for i in $PHP_LIBDIR $PHP_LIBDIR/mysql; do
      MYSQL_LIB_CHK($i)
    done

    if test -z "$MYSQL_LIB_DIR"; then
      AC_MSG_ERROR([Cannot find lib$MYSQL_LIBNAME under $MYSQL_DIR.])
    fi

    PHP_CHECK_LIBRARY($MYSQL_LIBNAME, mysql_close, [ ],
    [
      if test "$PHP_ZLIB_DIR" != "no"; then
        PHP_ADD_LIBRARY_WITH_PATH(z, $PHP_ZLIB_DIR, MYSQL_SHARED_LIBADD)
        PHP_CHECK_LIBRARY($MYSQL_LIBNAME, mysql_error, [], [
          AC_MSG_ERROR([mysql configure failed. Please check config.log for more information.])
        ], [
          -L$PHP_ZLIB_DIR/$PHP_LIBDIR -L$MYSQL_LIB_DIR 
        ])  
        MYSQL_LIBS="-L$PHP_ZLIB_DIR/$PHP_LIBDIR -lz"
      else
        PHP_ADD_LIBRARY(z,, MYSQL_SHARED_LIBADD)
        PHP_CHECK_LIBRARY($MYSQL_LIBNAME, mysql_errno, [], [
          AC_MSG_ERROR([Try adding --with-zlib-dir=<DIR>. Please check config.log for more information.])
        ], [
          -L$MYSQL_LIB_DIR
        ])   
        MYSQL_LIBS="-lz"
      fi
    ], [
      -L$MYSQL_LIB_DIR 
    ])

    PHP_ADD_LIBRARY_WITH_PATH($MYSQL_LIBNAME, $MYSQL_LIB_DIR, MYSQL_SHARED_LIBADD)
    PHP_ADD_INCLUDE($MYSQL_INC_DIR)

    MYSQL_MODULE_TYPE=external
    MYSQL_LIBS="-L$MYSQL_LIB_DIR -l$MYSQL_LIBNAME $MYSQL_LIBS"
    MYSQL_INCLUDE=-I$MYSQL_INC_DIR

    PHP_SUBST_OLD(MYSQL_MODULE_TYPE)
    PHP_SUBST_OLD(MYSQL_LIBS)
    PHP_SUBST_OLD(MYSQL_INCLUDE)

    AC_DEFINE(HAVE_MYSQL,1,[MySQL found and included])
  fi

  PHP_NEW_EXTENSION(apm, apm.c backtrace.c $sqlite3_driver $mysql_driver, $ext_shared)
  PHP_SUBST(APM_SHARED_LIBADD)
fi
