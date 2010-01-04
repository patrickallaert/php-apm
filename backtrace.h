/*
  +----------------------------------------------------------------------+
  |  APM stands for Alternative PHP Monitor                              |
  +----------------------------------------------------------------------+
  | Copyright (c) 2008-2009  Davide Mendolia, Patrick Allaert            |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
*/

#ifndef BACKTRACE_H
#define BACKTRACE_H

#include "ext/standard/php_smart_str.h"

void append_backtrace(smart_str *trace_str TSRMLS_DC);

#endif

