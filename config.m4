dnl config.m4 for extension apm

PHP_ARG_ENABLE(apm, whether to enable apm support,
[  --enable-apm           Enable apm support])

if test "$PHP_APM" != "no"; then
  PHP_NEW_EXTENSION(apm, "apm.c", $ext_shared)
fi


