dnl config.m4 for extension apm

PHP_ARG_ENABLE(apm, whether to enable apm support,
[  --enable-apm           Enable apm support])

if test "$PHP_APM" != "no"; then
  PHP_EXTENSION(apm, $ext_shared)
fi
