<?php
/*
 +----------------------------------------------------------------------+
 |  APM stands for Alternative PHP Monitor                              |
 +----------------------------------------------------------------------+
 | Copyright (c) 2008-2010                                              |
 | Davide Mendolia, Patrick Allaert                                     |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt                                  |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 | Authors: Davide Mendolia <dmendolia@php.net>                         |
 |          Patrick Allaert <patrickallaert@php.net>                    |
 +----------------------------------------------------------------------+
*/
require 'setup.php';
?>
<!DOCTYPE html>
<html>
  <head>
    <title>APM</title>
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link href="css/bootstrap.min.css" rel="stylesheet">
    <link href="css/bootstrap-responsive.min.css" rel="stylesheet">
    <link rel="stylesheet" type="text/css" media="screen" href="css/smoothness/jquery-ui-1.9.1.custom.min.css" />
    <link rel="stylesheet" type="text/css" media="screen" href="css/ui.jqgrid.css" />
    <link rel="stylesheet" type="text/css" media="screen" href="css/apm.css" />
    <style type="text/css">
      body {
        padding-top: 60px;
        padding-bottom: 40px;
      }
      .sidebar-nav {
        padding: 9px 0;
      }
    </style>
  </head>
  <body>
    <div class="navbar navbar-inverse navbar-fixed-top">
      <div class="navbar-inner">
        <div class="container-fluid">
          <a class="brand" href="#">APM status</a>
        </div>
      </div>
    </div>
    <div class="container-fluid">
      <div class="row-fluid">
        <div class="span12">
          <?php
          if (APM_LOADED) {
          ?>
            <ul id="myTab" class="nav nav-tabs">
              <li><a href="apm.php#faultyevents">Faulty events</a></li>
              <li><a href="apm.php#slowrequests">Slow requests</a></li>
              <li class="active"><a href="#event">Details for event #<?php echo htmlentities($_GET['id'], null, 'UTF-8'); ?></a></li>
            </ul>
            <?php
            $event = apm_get_event_info($_GET['id']);
            if ($event !== false) {
            ?>
              <table class="table table-bordered table-striped">
                <tr>
                  <td>ID:</td>
                  <td><?php echo htmlentities($_GET['id'], null, 'UTF-8'); ?></td>
                </tr>
                <tr>
                  <td>Date:</td>
                  <td><?php echo strftime('%F %T', $event['timestamp']) ?></td>
                </tr>
                <tr>
                  <td>Error Type:</td>
                  <td><?php echo APMgetErrorCodeFromID($event['type']); ?></td>
                </tr>
                <tr>
                  <td>URL:</td>
                  <td>http://<?php echo htmlentities((empty($event['host']) ? '[unknown]' : $event['host'] ) . $event['uri'], ENT_COMPAT, 'UTF-8'); ?></td>
                </tr>
                <tr>
                  <td>File:</td>
                  <td><?php echo htmlentities($event['file'], ENT_COMPAT, 'UTF-8'); ?></td>
                </tr>
                <tr>
                  <td>Line:</td>
                  <td><?php echo htmlentities($event['line'], ENT_COMPAT, 'UTF-8'); ?></td>
                </tr>
                <tr>
                  <td>Message:</td>
                  <td><?php echo htmlentities($event['message'], ENT_COMPAT, 'UTF-8'); ?></td>
                </tr>
                <tr>
                  <td>IP:</td>
                  <td><?php echo long2ip($event['ip']); ?></td>
                </tr>
                <?php if (!empty($event['cookies'])): ?>
                  <tr>
                    <td>Cookies:</td>
                    <td><pre><?php echo htmlentities($event['cookies'], ENT_COMPAT, 'UTF-8'); ?></pre></td>
                  </tr>
                <?php endif; ?>
                <?php if (!empty($event['post_vars'])): ?>
                  <tr>
                    <td>POST data:</td>
                    <td><pre><?php echo htmlentities($event['post_vars'], ENT_COMPAT, 'UTF-8'); ?></pre></td>
                  </tr>
                <?php endif; ?>
                <?php if (!empty($event['referer'])): ?>
                  <tr>
                    <td>Referer:</td>
                    <td><pre><?php echo htmlentities($event['referer'], ENT_COMPAT, 'UTF-8'); ?></pre></td>
                  </tr>
                <?php endif; ?>
              </table>

            <!-- Stacktrace -->
            <div id="stacktrace">
                <label class="strong">Stacktrace</label>
                <br />
                <?php
                $lines = array();
                $filesSet = array();
                foreach ( explode( "\n", $event['stacktrace'] ) as $line ) {
                    if ( preg_match( "/#\d+ (.*) called at \[(.*):(\d+)\]/", $line, $matches ) ) {
                        $lines[] = $matches;
                        $filesSet[$matches[2]] = true;
                    }
                }
                // Searching for the common path
                $files = array_keys( $filesSet );
                $path = dirname( array_shift( $files ) );
                $previousPath = null;
                foreach ( $files as $file ) {
                    while ( substr_compare( $path, $file, 0, strlen( $path ) ) !== 0 && $path !== $previousPath ) {
                        $previousPath = $path;
                        $path = dirname( $path );
                    }
                }
                $odd = true;
                $pathLen = strlen( $path ) + 1;
                echo $path;
                ?>
                <table class="table">
                <?php
                foreach ( $lines  as $line ) {
                    echo '<tr class="', ($odd = !$odd) ? "even" : "odd", '"><td>', ( $fileExists = file_exists( $line[2] ) ) ? '<a href="file://' . $line[2] . '">' : "", substr( $line[2], $pathLen ), $fileExists ? "</a>" : "", ":$line[3]</td><td>", str_replace( "&lt;?php&nbsp;", "", highlight_string( "<?php $line[1]", true ) ), "</td></tr>";
                }
                ?>
                </table>
            </div>
            <?php
            }
            ?>
          <?php
          } else {
          ?>
          <strong>APM extention does not seem to be active or properly configured.</strong>
          <?php
          }
          ?>
          <script src="js/jquery-1.9.0.min.js" type="text/javascript"></script>
          <script src="js/i18n/grid.locale-en.js" type="text/javascript"></script>
          <script src="js/jquery.jqGrid.min.js" type="text/javascript"></script>
          <script src="js/jquery-ui-1.9.1.custom.min.js" type="text/javascript"></script>
          <script src="js/bootstrap.min.js"></script>
          <script src="js/apm.js" type="text/javascript"></script>
        </div>
      </div>
    </div>
  </body>
</html>
