<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en" lang="en">
  <head>
    <meta http-equiv="Content-Type" content="text/html;charset=utf-8" />
    <title>Events captured by APM</title>
  </head>
  <body>
    <h1>APM dashboard</h1>
    <h2>Events captured by APM</h2>
<?php
apm_get_events();
?>
    <h2>Slow requests captured by APM</h2>
<?php
apm_get_slow_requests();
?>
  </body>
</html>
