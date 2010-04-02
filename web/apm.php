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
<link rel="stylesheet" type="text/css" media="screen" href="css/redmond/jquery-ui-1.7.1.custom.css" />
<link rel="stylesheet" type="text/css" media="screen" href="css/ui.jqgrid.css" />
<link rel="stylesheet" type="text/css" media="screen" href="css/greybox.css" />
<link rel="stylesheet" type="text/css" media="screen" href="css/apm.css" />
</head>
<body>
<h1>APM status</h1>
<?php
if (APM_LOADED) {
?>
<h2>Faulty events</h2>
<table id="events"></table>
<div id="events-pager"></div>
<h2>Slow requests</h2>
<table id="slow-requests"></table>
<div id="slow-requests-pager"></div>
<?php
} else {
?>
<strong>APM extention does not seem to be active or properly configured.</strong>
<?php
}
?>
<script src="js/jquery-1.3.2.min.js" type="text/javascript"></script>
<script src="js/i18n/grid.locale-en.js" type="text/javascript"></script>
<script src="js/jquery.jqGrid.min.js" type="text/javascript"></script>
<script src="js/greybox.js" type="text/javascript"></script>
<script src="js/apm.js" type="text/javascript"></script>
</body>
</html>
