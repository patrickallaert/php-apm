<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en" xml:lang="en">
<head>
<meta http-equiv="Content-Type" content="text/html;charset=utf-8" />
<title>APM status</title>
<style type="text/css">
<!--
/* Design by Jehan Bihin (http://www.daaboo.net/) */
  * {
    margin: 0;
    padding: 0;
    border: 0;
  }
  body {
    font-size: 62.5%;
    font-family: Arial, Helvetica, sans-serif;
  }
  h1 {
    background-color: #9999CC;
    font-size: 2em;
    padding: 0.2em 0.5em;
  }
  h2 {
    background-color: #eee;
    border-color: #999;
    border-style: solid;
    border-width: 1px 0;
    padding: 0.2em 0.6em;
    font-size: 1.4em;
  }
  table {
    margin: 1em 1em 1.6em;
    font-size: 1.2em;
    border-collapse: collapse;
  }
  table th {
    padding: 0 0.5em;
    text-align: left;
    border-bottom: 1px solid #999;
  }
  table td {
    padding: 0.2em 0.5em;
  }
  table#slow-request-list td { border-bottom: 1px solid #ccc; }
  table#slow-request-list tr.odd td { background-color: #eee; }
  table#event-list tr.E_ERROR td,
  table#event-list tr.E_CORE_ERROR td,
  table#event-list tr.E_PARSE td,
  table#event-list tr.E_COMPILE_ERROR td,
  table#event-list tr.E_USER_ERROR td,
  table#event-list tr.E_RECOVERABLE_ERROR td { background-color: #fbc; }
  table#event-list tr.E_WARNING td,
  table#event-list tr.E_CORE_WARNING td,
  table#event-list tr.E_COMPILE_WARNING td,
  table#event-list tr.E_USER_WARNING td { background-color: #fc8; }
  table#event-list tr.E_NOTICE td,
  table#event-list tr.E_USER_NOTICE td,
  table#event-list tr.E_STRICT td,
  table#event-list tr.E_DEPRECATED td,
  table#event-list tr.E_USER_DEPRECATED td { background-color: #ff8; }
-->
</style>
</head>
<body>
<h1>APM status</h1>
<h2>Faulty events</h2>
<?php apm_get_events() ?>
<h2>Slow requests</h2>
<?php apm_get_slow_requests() ?>
</body>
</html>
