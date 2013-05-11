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
            <li class="active"><a href="#faultyevents" data-toggle="tab">Faulty events</a></li>
            <li><a href="#slowrequests" data-toggle="tab">Slow requests</a></li>
          </ul>
          <div id="myTabContent" class="tab-content">
            <div class="tab-pane fade in active" id="faultyevents">
              <table id="events"><tr><td></td></tr></table>
              <div id="events-pager"></div>
            </div>
            <div class="tab-pane fade" id="slowrequests">
              <table id="slow-requests"><tr><td></td></tr></table>
              <div id="slow-requests-pager"></div>
            </div>
          </div>
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
          <script type="text/javascript">
          if (window.location.hash != "") {
              $('#myTab > li > a[href="'+window.location.hash+'"]').tab('show');
          }
          </script>
        </div>
      </div>
    </div>
  </body>
</html>
