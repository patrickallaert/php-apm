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
            <li><a href="request.php?id=<?php echo (int) $event->request->id ?>">Details for request #<?php echo (int) $event->request->id ?></a></li>
            <li class="active"><a href="#event">Details for event #<?php echo (int) $id ?></a></li>
          </ul>
          <?php
          if ($event !== false) {
          ?>
            <table class="table table-bordered table-striped">
              <tr>
                <td>ID:</td>
                <td><?php echo (int) $id ?></td>
              </tr>
              <tr>
                <td>Date:</td>
                <td><?php echo $event->timestamp->format("Y-m-d H:i:s") ?></td>
              </tr>
              <tr>
                <td>Error Type:</td>
                <td><?php echo APM\getErrorCodeFromID($event->type) ?></td>
              </tr>
              <tr>
                <td>URL:</td>
                <td>http://<?php echo htmlentities((empty($event->request->host) ? "[unknown]" : $event->request->host ) . $event->request->uri, ENT_COMPAT, "UTF-8") ?></td>
              </tr>
              <tr>
                <td>File:</td>
                <td><?php echo htmlentities($event->file, ENT_COMPAT, "UTF-8") ?></td>
              </tr>
              <tr>
                <td>Line:</td>
                <td><?php echo htmlentities($event->line, ENT_COMPAT, "UTF-8") ?></td>
              </tr>
              <tr>
                <td>Message:</td>
                <td><?php echo htmlentities($event->message, ENT_COMPAT, "UTF-8") ?></td>
              </tr>
              <tr>
                <td>IP:</td>
                <td><?php echo long2ip($event->request->ip) ?></td>
              </tr>
              <?php if (!empty($event->cookies)): ?>
                <tr>
                  <td>Cookies:</td>
                  <td><pre><?php echo htmlentities($event->cookies, ENT_COMPAT, "UTF-8") ?></pre></td>
                </tr>
              <?php endif; ?>
              <?php if (!empty($event->request->postVariables)): ?>
                <tr>
                  <td>POST data:</td>
                  <td><pre><?php echo htmlentities($event->request->postVariables, ENT_COMPAT, "UTF-8") ?></pre></td>
                </tr>
              <?php endif; ?>
              <?php if (!empty($event->request->referer)): ?>
                <tr>
                  <td>Referer:</td>
                  <td><pre><?php echo htmlentities($event->request->referer, ENT_COMPAT, "UTF-8") ?></pre></td>
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
              foreach ( explode( "\n", $event->backtrace ) as $line ) {
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
require "footer.html";
