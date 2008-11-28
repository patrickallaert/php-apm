dnl config.m4 for extension apm

dnl    APM stands for Alternative PHP Monitor
dnl    Copyright (C) 2008  Davide Mendolia, Patrick Allaert
dnl
dnl    This file is part of APM.
dnl
dnl    APM is free software: you can redistribute it and/or modify
dnl    it under the terms of the GNU General Public License as published by
dnl    the Free Software Foundation, either version 3 of the License, or
dnl    (at your option) any later version.
dnl
dnl    APM is distributed in the hope that it will be useful,
dnl    but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl    GNU General Public License for more details.
dnl
dnl    You should have received a copy of the GNU General Public License
dnl    along with APM.  If not, see <http://www.gnu.org/licenses/>.

PHP_ARG_ENABLE(apm, whether to enable apm support,
[  --enable-apm           Enable apm support])

if test "$PHP_APM" != "no"; then

  AC_MSG_CHECKING([for SQLite 3.*])
  PHP_CHECK_LIBRARY(sqlite3, sqlite3_open, [
    AC_MSG_RESULT(found)
    PHP_ADD_LIBRARY_WITH_PATH(sqlite3, $SQLITE3_DIR/$PHP_LIBDIR, SQLITE3_SHARED_LIBADD)
    PHP_ADD_INCLUDE($SQLITE3_DIR/include)
  ],[
    AC_MSG_RESULT([not found])
    AC_MSG_ERROR([Please install SQLite 3.* first or check libsqlite3-dev is present])
  ],[
    -L$SQLITE3_DIR/$PHP_LIBDIR -lm
  ])

  PHP_NEW_EXTENSION(apm, "apm.c", $ext_shared)

fi

