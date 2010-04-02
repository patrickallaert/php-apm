<?php

/**
 * To convert an error ID to an error code
 * @param integer $p_iErrorID The Error ID
 * @return string The Error Code
 */
function getErrorCodeFromID($p_iErrorID) {
	$codes = array(
		1      => 'E_ERROR',
		2      => 'E_WARNING',
		4      => 'E_PARSE',
		8      => 'E_NOTICE',
		16     => 'E_CORE_ERROR',
		32     => 'E_CORE_WARNING',
		64     => 'E_COMPILE_ERROR',
		128    => 'E_COMPILE_WARNING',
		256    => 'E_USER_ERROR', 
		512    => 'E_USER_WARNING',
		1024   => 'E_USER_NOTICE',
		2048   => 'E_STRICT',
		4096   => 'E_RECOVERABLE_ERROR',
		8192   => 'E_DEPRECATED',
		16834  => 'E_USER_DEPRECATED'
	);
	return (isset($p_iErrorID) ? $codes[$p_iErrorID] : '');
}


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
