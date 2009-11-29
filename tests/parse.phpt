--TEST--
Parse error test
--SKIPIF--
<?php 
require_once('skipif.inc'); 
?>
--FILE--
<?php parse error ?>
--EXPECTF--
Parse error: syntax error, unexpected T_STRING in %s/tests/parse.php on line 1
