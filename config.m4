dnl config.m4 for extension apm

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


