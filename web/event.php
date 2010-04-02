<?php
/*
 +----------------------------------------------------------------------+
 |  APM stands for Alternative PHP Monitor                              |
 +----------------------------------------------------------------------+
 | Copyright (c) 2008-2010                                              |
 | Davide Mendolia, Patrick Allaert, Paul Dragoonis                     |	
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
	<link rel="stylesheet" type="text/css" media="screen" href="css/apm.css" />
	<script src="js/jquery-1.3.2.min.js" type="text/javascript"></script>	
	<script src="js/apm.js" type="text/javascript"></script>		
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
<div id="error_info">
	<dl>
		<dt>ID:</dt>
		<dd><?php echo htmlentities($_GET['id'], null, 'UTF-8'); ?></dd>
		
		<dt>Date:</dt>
		<dd><?php echo strftime('%F %T', $event['timestamp']) ?></dd>
		
		<dt>Error Type:</dt>
		<dd><?php echo getErrorCodeFromID($event['type']); ?></dd>
		
		<dt>File:</dt>
		<dd><?php echo htmlentities($event['file'], ENT_COMPAT, 'UTF-8'); ?></dd>
		
		<dt>Line:</dt>
		<dd><?php echo htmlentities($event['line'], ENT_COMPAT, 'UTF-8'); ?></dd>
		
		<dt>Message:</dt>
		<dd><?php echo htmlentities($event['message'], ENT_COMPAT, 'UTF-8'); ?></dd>
</dl>

<!-- Stacktrace -->
<div id="stacktrace">
	<label class="strong">Stacktrace</label>
	<br />
	<p><?php echo nl2br($event['stacktrace']); ?></p>
</div>

<!-- Source -->		
<div style="margin: 5px;"><a id="source_toggle" href="javascript:void(0);">Show Sourcecode</a></div>
<div id="source" style="display: none; font-size: 10px; padding-top: 5px; padding-left: 5px;">
	<?php
	if (is_readable($event['file'])) {
	    highlight_file($event['file']);
	} else {
		echo '<p>Unable to read file: ' . htmlentities($event['file'], ENT_COMPAT, 'UTF-8') . '</p>';
	}
	?>
	</div>
</div>
<?php
}
?>
</body>
</html>
