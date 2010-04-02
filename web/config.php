<?php
/*
 +----------------------------------------------------------------------+
 |  APM stands for Alternative PHP Monitor                              |
 +----------------------------------------------------------------------+
 | Copyright (c) 2008-2010  Davide Mendolia, Patrick Allaert            |
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

define("APM_DRIVER", "sqlite");
define("APM_LOADED", function_exists('apm_get_' . APM_DRIVER . '_events'));

function apm_get_events() {
    $args = func_get_args();
    return call_user_func_array("apm_get_" . APM_DRIVER . "_events", $args);
}
function apm_get_events_count() {
    $args = func_get_args();
    return call_user_func_array("apm_get_" . APM_DRIVER . "_events_count", $args);
}
function apm_get_slow_requests() {
    $args = func_get_args();
    return call_user_func_array("apm_get_" . APM_DRIVER . "_slow_requests", $args);
}
function apm_get_slow_requests_count() {
    $args = func_get_args();
    return call_user_func_array("apm_get_" . APM_DRIVER . "_slow_requests_count", $args);
}
function apm_get_event_info() {
    $args = func_get_args();
    return call_user_func_array("apm_get_" . APM_DRIVER . "_event_info", $args);
}
