<?php
/*
 +----------------------------------------------------------------------+
 |  APM stands for Alternative PHP Monitor                              |
 +----------------------------------------------------------------------+
 | Copyright (c) 2008-2014  Davide Mendolia, Patrick Allaert            |
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
$repository = require "model/repository.php";
$requestService = $repository->getRequestService();
$rows = (int) $_GET["rows"];
$page = (int) $_GET["page"];

switch ($_GET["sidx"]) {
    case "time":
        $order = APM\ORDER_TIMESTAMP;
        break;
    case "host":
        $order = APM\ORDER_HOST;
        break;
    case "ip":
        $order = APM\ORDER_IP;
        break;
    case "uri":
        $order = APM\ORDER_URI;
        break;
    case "script":
        $order = APM\ORDER_SCRIPT;
        break;
    case "eventsCount":
        $order = APM\ORDER_EVENTS_COUNT;
        break;
    case "duration":
        $order = APM\ORDER_DURATION;
        break;
    case "user_cpu":
        $order = APM\ORDER_USER_CPU;
        break;
    case "sys_cpu":
        $order = APM\ORDER_SYS_CPU;
        break;
    case "mem_peak_usage":
        $order = APM\ORDER_MEM_PEAK_USAGE;
        break;
    case "id":
    default:
        $order = APM\ORDER_ID;
}

if (isset($_GET["stats"])) {
    $records = $requestService->getStatsCount();
    $data = $requestService->loadStats(
        ($page - 1) * $rows,
        $rows,
        $order,
        ($_GET["sord"] === "asc") ? APM\ORDER_ASC : APM\ORDER_DESC
    );
} else {
    $records = $requestService->getFaultyRequestsCount();
    $data = $requestService->loadFaultyRequests(
        ($page - 1) * $rows,
        $rows,
        $order,
        ($_GET["sord"] === "asc") ? APM\ORDER_ASC : APM\ORDER_DESC
    );
}
foreach ($data as &$request) {
    $request = (array) $request;
    $request["timestamp"] = $request["timestamp"]->format("Y-m-d H:i:s");
}

require "views/json/data.php";
