--TEST--
Backtrace test
--SKIPIF--
<?php 
require_once('skipif.inc'); 
?>
--FILE--
<?php

class trig {
	public static function ger2(){
		new self();
	}
	
	public function __construct(){
		trigger_error('error');
	}
}
function trigger1($a, $b, $c, $d, $e) {
	trig::ger2();
}

$z = 1; $y = ''; $x = array(0, 'key' => 'value'); $w = new stdClass(); $v = null;
trigger1($z, $y, $x, $w, $v);
?>
--EXPECTF--
Notice: error in %s/tests/backtrace.php on line 9
