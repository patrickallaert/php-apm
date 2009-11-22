--TEST--
Parse error test
--SKIPIF--
<?php 
require_once('skipif.inc'); 
?>
--FILE--
<?php parse error ?>
--EXPECT--
Parse error: syntax error, unexpected T_STRING in /home/davide/peclapm/tests/parse.php on line 1
