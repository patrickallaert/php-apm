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
$records = apm_get_slow_requests_count();
switch ($_GET['sidx']) {
    case 'time':
        $order = APM_ORDER_TIMESTAMP;
        break;
    case 'duration':
        $order = APM_ORDER_DURATION;
        break;
    case 'file':
        $order = APM_ORDER_FILE;
        break;
    case 'id':
    default:
        $order = APM_ORDER_ID;
}
?>
{
  total: "<?php echo ceil($records / $_GET['rows']) ?>",
  page: "<?php echo $_GET['page'] ?>",
  records: "<?php echo $records ?>",
  rows : [
    <?php
    apm_get_slow_requests($_GET['rows'], ($_GET['page'] - 1) * $_GET['rows'], $order, $_GET['sord'] === 'asc', true);
    ?>
  ]
}
