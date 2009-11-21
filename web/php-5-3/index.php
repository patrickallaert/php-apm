<?php
$db = new SQLite3(ini_get('apm.db_path').'/events');

$result = $db->query("SELECT id, ts, CASE type 
                          WHEN 1 THEN 'E_ERROR' 
                          WHEN 2 THEN 'E_WARNING' 
                          WHEN 4 THEN 'E_PARSE' 
                          WHEN 8 THEN 'E_NOTICE' 
                          WHEN 16 THEN 'E_CORE_ERROR' 
                          WHEN 32 THEN 'E_CORE_WARNING' 
                          WHEN 64 THEN 'E_COMPILE_ERROR' 
                          WHEN 128 THEN 'E_COMPILE_WARNING' 
                          WHEN 256 THEN 'E_USER_ERROR' 
                          WHEN 512 THEN 'E_USER_WARNING' 
                          WHEN 1024 THEN 'E_USER_NOTICE' 
                          WHEN 2048 THEN 'E_STRICT' 
                          WHEN 4096 THEN 'E_RECOVERABLE_ERROR' 
                          WHEN 8192 THEN 'E_DEPRECATED' 
                          WHEN 16384 THEN 'E_USER_DEPRECATED' 
                          END AS type, 
                          file, line, message, backtrace FROM event ORDER BY id DESC");
$events = array();
while ($row = $result->fetchArray()) {
    $events[] = $row;
}

$result = $db->query('SELECT * FROM slow_request');
$slowRequests = array();
while ($row = $result->fetchArray()) {
    $slowRequests[] = $row;
}
?>
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
  table#event-list td { border-bottom: 1px solid #999; }
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
<table id="event-list"><tr><th>#</th><th>Time</th><th>Type</th><th>Message</th></tr>
<?php foreach($events as $event) { ?>
<tr class="<?php echo $event['type'] ?>"><td><?php echo $event['id'] ?></td><td><?php echo $event['ts'] ?></td><td><?php echo $event['type'] ?></td><td><a href="event.php?id=<?php echo $event['id'] ?>"><?php echo $event['message'] ?></a></td></tr>
<?php } ?>
</table>

<h2>Slow requests</h2>
<table id="slow-request-list"><tr><th>#</th><th>Time</th><th>Duration</th><th>File</th></tr>
<?php 
$oddSlowRequest = true;
foreach($slowRequests as $slowRequest) { ?>
<tr class="<?php echo $oddSlowRequest ? "odd" : "even" ?>"><td><?php echo $slowRequest['id']?></td><td><?php echo $slowRequest['ts']?></td><td><?php echo $slowRequest['duration']?></td><td><?php echo $slowRequest['file']?></td></tr>
<?php 
$oddSlowRequest = !$oddSlowRequest;
} ?>
</table></body>
</html>

