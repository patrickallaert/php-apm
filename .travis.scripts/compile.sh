#!/bin/bash
phpize
./configure
make all install
EXTENSIONDIR=`php -r 'echo ini_get("extension_dir");'`
INI=~/.phpenv/versions/$(phpenv version-name)/etc/conf.d/apm.ini
# in case we're having it already done in a non travis phpenv
grep apm.so $INI >/dev/null 2>&1 || echo "extension=${EXTENSIONDIR}/apm.so" > $INI
