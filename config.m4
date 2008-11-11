dnl config.m4 for extension apm

PHP_ARG_ENABLE(apm, whether to enable apm support,
[  --enable-apm           Enable apm support])

if test "$PHP_APM" != "no"; then

  PHP_CHECK_LIBRARY(sqlite, sqlite_open, [
    PHP_ADD_LIBRARY_WITH_PATH(sqlite, $SQLITE_DIR/$PHP_LIBDIR, SQLITE_SHARED_LIBADD)
    PHP_ADD_INCLUDE($SQLITE_DIR/include)
  ],[
    AC_MSG_ERROR([wrong sqlite lib version or lib not found])
  ],[
    -L$SQLITE_DIR/$PHP_LIBDIR -lm
  ])

  PHP_NEW_EXTENSION(apm, "apm.c", $ext_shared)

fi


