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
$eventService = $repository->getEventService();
$records = $eventService->getEventsCount($_GET["id"]);
$rows = (int) $_GET["rows"];
$page = (int) $_GET["page"];

switch ($_GET["sidx"]) {
    case "time":
        $order = APM\ORDER_TIMESTAMP;
        break;
    case "type":
        $order = APM\ORDER_TYPE;
        break;
    case "file":
        $order = APM\ORDER_FILE;
        break;
    case "message":
        $order = APM\ORDER_MESSAGE;
        break;
    case "line":
    case "id":
    default:
        $order = APM\ORDER_ID;
}

$data = $eventService->loadEvents(
    $_GET["id"],
    ($page - 1) * $rows,
    $rows,
    $order,
    ($_GET["sord"] === "desc") ? APM\ORDER_DESC : APM\ORDER_ASC
);

foreach ($data as &$event) {
    $event = (array) $event;
    $event["timestamp"] = $event["timestamp"]->format("Y-m-d H:i:s");
    $event["type"] = APM\getErrorCodeFromID($event["type"]);
}

require "views/json/data.php";
