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
 | Authors: Patrick Allaert <patrickallaert@php.net>                    |
 +----------------------------------------------------------------------+
*/

require "header.html";
?>
            <li><a href="apm.php#overview">General overview</a></li>
            <li><a href="apm.php#faultyevents">Requests with faulty events</a></li>
            <li><a href="apm.php#slowrequests">Slow requests</a></li>
            <li class="active"><a href="#request">Details for request #<?php echo (int) $id ?></a></li>
          </ul>
          <?php
          if ($request !== false) {
          ?>
            <table class="table table-bordered table-striped">
              <tr>
                <td>ID:</td>
                <td><?php echo (int) $id ?></td>
              </tr>
              <tr>
                <td>Date:</td>
                <td><?php echo $request->timestamp->format("Y-m-d H:i:s") ?></td>
              </tr>
              <tr>
                <td>URL:</td>
                <td>http://<?php echo htmlentities((empty($request->host) ? "[unknown]" : $request->host ) . $request->uri, ENT_COMPAT, "UTF-8") ?></td>
              </tr>
              <tr>
                <td>IP:</td>
                <td><?php echo long2ip($request->ip) ?></td>
              </tr>
              <?php if (!empty($request->cookies)): ?>
                <tr>
                  <td>Cookies:</td>
                  <td><pre><?php echo htmlentities($request->cookies, ENT_COMPAT, "UTF-8") ?></pre></td>
                </tr>
              <?php endif; ?>
              <?php if (!empty($request->postVariables)): ?>
                <tr>
                  <td>POST data:</td>
                  <td><pre><?php echo htmlentities($request->postVariables, ENT_COMPAT, "UTF-8") ?></pre></td>
                </tr>
              <?php endif; ?>
              <?php if (!empty($request->referer)): ?>
                <tr>
                  <td>Referer:</td>
                  <td><pre><?php echo htmlentities($request->referer, ENT_COMPAT, "UTF-8") ?></pre></td>
                </tr>
              <?php endif; ?>
              <?php if (!empty($request->referer)): ?>
                <tr>
                  <td>Duration:</td>
                  <td><pre><?php echo htmlentities($request->duration, ENT_COMPAT, "UTF-8") ?></pre></td>
                </tr>
                <?php endif; ?>
            </table>
          <?php
          }
          ?>
          <div id="myTabContent" class="tab-content">
            <div class="tab-pane fade" id="overview">
              <table id="requests"><tr><td></td></tr></table>
              <div id="requests-pager"></div>
            </div>
            <div class="tab-pane fade" id="faultyevents">
              <table id="events"><tr><td></td></tr></table>
              <div id="events-pager"></div>
            </div>
            <div class="tab-pane fade" id="slowrequests">
              <table id="slow-requests"><tr><td></td></tr></table>
              <div id="slow-requests-pager"></div>
            </div>
            <div class="tab-pane fade in active" id="request">
              <table id="request-details"><tr><td></td></tr></table>
              <div id="request-pager"></div>
            </div>
          </div>
<?php
require "footer.html";
