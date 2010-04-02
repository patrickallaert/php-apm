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
require 'config.php';
?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
<meta http-equiv="Content-Type" content="text/html;charset=utf-8" />
<title>APM status</title>
<style type="text/css">
#source {background-color: #ccc;}
</style>
</head>
<body id="event">
<?php
$event = apm_get_event_info($_GET['id']);
if ($event !== false) {
?>
<div class="%s">
<div>ID: <?php echo $_GET['id'] ?></div>
<div>Date: <?php echo strftime('%F %T', $event['timestamp']) ?></div>
<div>Type: <?php
switch ($event['type']) {
    case 1: echo 'E_ERROR'; break;
    case 2: echo 'E_WARNING'; break;
    case 4: echo 'E_PARSE'; break;
    case 8: echo 'E_NOTICE'; break;
    case 16: echo 'E_CORE_ERROR'; break;
    case 32: echo 'E_CORE_WARNING'; break;
    case 64: echo 'E_COMPILE_ERROR'; break;
    case 128: echo 'E_COMPILE_WARNING'; break;
    case 256: echo 'E_USER_ERROR'; break;
    case 512: echo 'E_USER_WARNING'; break;
    case 1024: echo 'E_USER_NOTICE'; break;
    case 2048: echo 'E_STRICT'; break;
    case 4096: echo 'E_RECOVERABLE_ERROR'; break;
    case 8192: echo 'E_DEPRECATED'; break;
    case 16384: echo 'E_USER_DEPRECATED'; break;
}
?></div>
<div>File: <?php echo $event['file'] ?></div>
<div>Line: <?php echo $event['line'] ?></div>
<div>Message: <?php echo $event['message'] ?></div>
<div>Stacktrace: <pre><?php echo $event['stacktrace'] ?></pre></div>
<div id="source">
<?php
if (is_readable($event['file'])) {
    highlight_file($event['file']);
}
?>
</div>
</div>
<?php
}
?>
</body>
</html>
