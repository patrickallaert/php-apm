<?php

namespace APM {

use DateTime;
use PDO;

const ORDER_ID = 1;
const ORDER_TIMESTAMP = 2;
const ORDER_HOST = 3;
const ORDER_URI = 4;
const ORDER_SCRIPT = 5;
const ORDER_FILE = 6;
const ORDER_TYPE = 7;
const ORDER_IP = 8;
const ORDER_MESSAGE = 9;
const ORDER_EVENTS_COUNT = 10;
const ORDER_DURATION = 11;

const ORDER_ASC = 1;
const ORDER_DESC = 2;

class Event {
    /**
     * @var \APM\Request
     */
    public $request;

    public function __set($name, $value) {
        if ($value === null) {
            $this->$name = null;
            return;
        }

        switch ($name) {
            case "id":
            case "line":
            case "type":
                $this->$name = (int)$value;
                break;
            case "timestamp":
                $this->timestamp = new DateTime("@$value");
                break;
            case "file":
            case "message":
            case "backtrace":
                $this->$name = $value;
                break;
            default:
                if ($this->request === null) {
                    $this->request = new Request;
                }

                switch ($name) {
                    case "request_id":
                        $name = "id";
                        break;
                    case "request_timestamp":
                        $name = "timestamp";
                }

                $this->request->$name = $value;
        }
    }
}

class RequestInfo {
    public function __set($name, $value) {
        if ($value === null) {
            $this->$name = null;
            return;
        }

        switch ($name) {
            case "id":
            case "eventsCount":
                $this->$name = (int)$value;
                break;
            case "duration":
                $this->$name = (float)$value;
                break;
            case "timestamp":
                $this->timestamp = new DateTime("@$value");
                break;
            case "script":
            case "uri":
            case "host":
                $this->$name = $value;
        }
    }
}

class Request extends RequestInfo {
    public function __set($name, $value) {
        if ($value === null) {
            $this->$name = null;
            return;
        }

        switch ($name) {
            case "ip":
            case "cookies":
            case "postVariables":
            case "referer":
                $this->$name = $value;
                break;
            default:
                parent::__set($name, $value);
        }
    }
}

class Repository {
    /**
     * @var \PDO
     */
    private $db;

    /**
     * @var \APM\Service\Request
     */
    private $requestService;

    /**
     * @var \APM\Service\Event
     */
    private $eventService;

    /**
     * @param $db \PDO
     */
    public function __construct($db) {
        $db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
        $this->db = $db;
    }

    /**
     * @return \APM\Service\Request
     */
    public function getRequestService() {
        if ($this->requestService === null) {
            switch ($this->db->getAttribute(PDO::ATTR_DRIVER_NAME)) {
                case "mysql":
                    $requestHandler = new Service\Handler\PDOMySQL\Request($this->db);
                    break;
                case "sqlite":
                    $requestHandler = new Service\Handler\PDO\Request($this->db);
            }
            $this->requestService = new Service\Request($requestHandler);
        }

        return $this->requestService;
    }

    /**
     * @return \APM\Service\Event
     */
    public function getEventService() {
        if ($this->eventService === null) {
            switch ($this->db->getAttribute(PDO::ATTR_DRIVER_NAME)) {
                case "mysql":
                    $eventHandler = new Service\Handler\PDOMySQL\Event($this->db);
                    break;
                case "sqlite":
                    $eventHandler = new Service\Handler\PDO\Event($this->db);
            }
            $this->eventService = new Service\Event($eventHandler);
        }

        return $this->eventService;
    }
}

/**
 * To convert an error ID to an error code
 *
 * @param int $errorId The Error ID
 * @return string The Error Code
 */
function getErrorCodeFromID($errorId) {
    switch ($errorId) {
        case 1:
            return 'E_ERROR';
        case 2:
            return 'E_WARNING';
        case 4:
            return 'E_PARSE';
        case 8:
            return 'E_NOTICE';
        case 16:
            return 'E_CORE_ERROR';
        case 32:
            return 'E_CORE_WARNING';
        case 64:
            return 'E_COMPILE_ERROR';
        case 128:
            return 'E_COMPILE_WARNING';
        case 256:
            return 'E_USER_ERROR';
        case 512:
            return 'E_USER_WARNING';
        case 1024:
            return 'E_USER_NOTICE';
        case 2048:
            return 'E_STRICT';
        case 4096:
            return 'E_RECOVERABLE_ERROR';
        case 8192:
            return 'E_DEPRECATED';
        case 16834:
            return 'E_USER_DEPRECATED';
    }
    return "";
}

}

namespace APM\Service {

use APM;

class Event {
    /**
     * @var \APM\Service\Handler
     */
    protected $handler;

    /**
     * @param $handler \APM\Service\Handler
     */
    public function __construct($handler) {
        $this->handler = $handler;
    }

    /**
     * @param int $eventId
     *
     * @return \APM\Event
     */
    public function loadEvent($eventId) {
        return $this->handler->loadEvent($eventId);
    }

    /**
     * @param int $requestId
     * @param int $offset
     * @param int $limit
     * @param int $order
     * @param int $direction
     *
     * @return \APM\Event[]
     */
    public function loadEvents($requestId, $offset = 0, $limit = 30, $order = APM\ORDER_ID, $direction = APM\ORDER_ASC) {
        return $this->handler->loadEvents($requestId, $offset, $limit, $order, $direction);
    }

    /**
     * @param int $requestId
     *
     * @return int
     */
    public function getEventsCount($requestId) {
        return $this->handler->getEventsCount($requestId);
    }
}

class Request {
    protected $handler;

    public function __construct($handler) {
        $this->handler = $handler;
    }

    /**
     * @param int $requestId
     *
     * @return \APM\Request
     */
    public function loadRequest($requestId) {
        return $this->handler->loadRequest($requestId);
    }

    /**
     * @param int $offset
     * @param int $limit
     * @param int $order
     * @param int $direction
     *
     * @return mixed
     */
    public function loadRequests($offset = 0, $limit = 30, $order = APM\ORDER_ID, $direction = APM\ORDER_DESC) {
        return $this->handler->loadRequests($offset, $limit, $order, $direction);
    }

    /**
     * @param int $offset
     * @param int $limit
     * @param int $order
     * @param int $direction
     *
     * @return APM\Request[]
     */
    public function loadFaultyRequests($offset = 0, $limit = 30, $order = APM\ORDER_ID, $direction = APM\ORDER_DESC) {
        return $this->handler->loadFaultyRequests($offset, $limit, $order, $direction);
    }

    /**
     * @param int $offset
     * @param int $limit
     * @param int $order
     * @param int $direction
     *
     * @return APM\Request[]
     */
    public function loadSlowRequests($offset = 0, $limit = 30, $order = APM\ORDER_ID, $direction = APM\ORDER_DESC) {
        return $this->handler->loadSlowRequests($offset, $limit, $order, $direction);
    }

    /**
     * @return int
     */
    public function getRequestsCount() {
        return $this->handler->getRequestsCount();
    }

    /**
     * @return int
     */
    public function getFaultyRequestsCount() {
        return $this->handler->getFaultyRequestsCount();
    }

    /**
     * @return int
     */
    public function getSlowRequestsCount() {
        return $this->handler->getSlowRequestsCount();
    }
}

}

namespace APM\Service {

abstract class Handler {
    protected $db;

    public function __construct($db) {
        $this->db = $db;
    }
}

}

namespace APM\Service\Handler\PDO {

use APM;
use APM\Service\Handler;
use PDO;

class Event extends Handler {
    protected $loadEventQuery = <<<SQL
SELECT e.ts AS timestamp, r.ts AS request_timestamp, e.request_id, type, file, line, message, backtrace, script, uri, host, ip, cookies, post_vars AS postVariables, referer
FROM event e
JOIN request r ON r.id = e.request_id
WHERE e.id = %d
SQL;
    protected $loadEventsQuery = <<<SQL
SELECT id, ts AS timestamp, type, file, line, message
FROM event
WHERE request_id = %d
ORDER BY %s
LIMIT %d, %d
SQL;
    protected $getEventsCountQuery = <<<SQL
SELECT COUNT(*) AS cnt
FROM event
WHERE request_id = %d
SQL;
    protected $loadEventsOrderMap = array(
        APM\ORDER_ID => 'id %s',
        APM\ORDER_TIMESTAMP => 'ts %s',
        APM\ORDER_TYPE => 'type %1$s, id %1$s',
        APM\ORDER_FILE => 'file %1$s, id %1$s',
        APM\ORDER_MESSAGE => 'message %1$s, file %1$s, id %1$s',
    );

    /**
     * @param int $eventId
     *
     * @return \APM\Event
     */
    public function loadEvent($eventId) {
        $events = $this->db->query(sprintf($this->loadEventQuery, $eventId))->fetchAll(PDO::FETCH_CLASS, "APM\\Event");
        return $events[0];
    }

    /**
     * @param int $requestId
     * @param int $offset
     * @param int $limit
     * @param int $order
     * @param int $direction
     *
     * @return \APM\Event[]
     */
    public function loadEvents($requestId, $offset, $limit, $order, $direction) {
        if (!isset($this->loadEventsOrderMap[$order])) {
            $order = APM\ORDER_ID;
        }

        return $this->db->query(
            sprintf(
                $this->loadEventsQuery,
                $requestId,
                sprintf($this->loadEventsOrderMap[$order], ($direction === APM\ORDER_ASC) ? "ASC" : "DESC"),
                $offset,
                $limit
            )
        )->fetchAll(PDO::FETCH_CLASS, "APM\\Event");
    }

    /**
     * @param int $requestId
     *
     * @return int
     */
    public function getEventsCount($requestId) {
        $row = $this->db->query(sprintf($this->getEventsCountQuery, $requestId))->fetch(PDO::FETCH_ASSOC);
        return $row["cnt"];
    }
}

class Request extends Handler {
    protected $loadRequestQuery = <<<SQL
SELECT r.id, r.ts AS timestamp, script, uri, host, ip, cookies, post_vars AS postVariables, referer, duration
FROM request r
LEFT JOIN slow_request s ON s.request_id = r.id
WHERE r.id = %d
SQL;
    protected $loadRequestsQuery = <<<SQL
SELECT r.id, r.ts AS timestamp, script, uri, host, ip, cookies, post_vars AS postVariables, referer, duration, COUNT(e.request_id) AS eventsCount
FROM request r
LEFT JOIN event e ON e.request_id = r.id
LEFT JOIN slow_request s ON s.request_id = r.id
GROUP BY r.id, r.ts, script, uri, host, ip, cookies, post_vars, referer, duration
ORDER BY %s
LIMIT %d, %d
SQL;
    protected $loadFaultyRequestsQuery = <<<SQL
SELECT r.id, r.ts AS timestamp, script, uri, host, COUNT(e.request_id) AS eventsCount
FROM request r
JOIN event e ON e.request_id = r.id
GROUP BY r.id, r.ts, script, uri, host
ORDER BY %s
LIMIT %d, %d
SQL;
    protected $loadSlowRequestsQuery = <<<SQL
SELECT r.id, r.ts AS timestamp, script, uri, host, duration
FROM request r
JOIN slow_request s ON s.request_id = r.id
GROUP BY r.id, r.ts, script, uri, host, duration
ORDER BY %s
LIMIT %d, %d
SQL;
    protected $getRequestsCountQuery = <<<SQL
SELECT COUNT(*) AS cnt
FROM request
SQL;
    protected $getFaultyRequestsCountQuery = <<<SQL
SELECT COUNT(DISTINCT r.id) AS cnt
FROM request r
JOIN event e ON e.request_id = r.id
SQL;
    protected $getSlowRequestsCountQuery = <<<SQL
SELECT COUNT(DISTINCT r.id) AS cnt
FROM request r
JOIN slow_request s ON s.request_id = r.id
SQL;
    protected $loadRequestsOrderMap = array(
        APM\ORDER_ID => 'r.id %s',
        APM\ORDER_TIMESTAMP => 'r.ts %s',
        APM\ORDER_SCRIPT => 'script %1$s, r.id %1$s',
        APM\ORDER_URI => 'uri %1$s, host %1$s, r.id %1$s',
        APM\ORDER_HOST => 'host %1$s, r.id %1$s',
        APM\ORDER_DURATION => 'duration %1$s, r.id %1$s',
        APM\ORDER_EVENTS_COUNT => 'eventsCount %1$s, r.id %1$s'
    );

    /**
     * @param int $requestId
     *
     * @return \APM\Request
     */
    public function loadRequest($requestId) {
        $requests = $this->db->query(sprintf($this->loadRequestQuery, $requestId))->fetchAll(PDO::FETCH_CLASS, "APM\\Request");
        return $requests[0];
    }

    /**
     * @param string $query
     * @param int $offset
     * @param int $limit
     * @param int $order
     * @param int $direction
     *
     * @return \APM\Request[]
     */
    private function loadRequestsHelper($query, $offset, $limit, $order, $direction) {
        if (!isset($this->loadRequestsOrderMap[$order])) {
            $order = APM\ORDER_ID;
        }

        return $this->db->query(
            sprintf(
                $query,
                sprintf($this->loadRequestsOrderMap[$order], ($direction === APM\ORDER_ASC) ? "ASC" : "DESC"),
                $offset,
                $limit
            )
        )->fetchAll(PDO::FETCH_CLASS, "APM\\Request");
    }

    /**
     * @param int $offset
     * @param int $limit
     * @param int $order
     * @param int $direction
     *
     * @return APM\Request[]
     */
    public function loadRequests($offset, $limit, $order, $direction) {
        return $this->loadRequestsHelper($this->loadRequestsQuery, $offset, $limit, $order, $direction);
    }

    /**
     * @param int $offset
     * @param int $limit
     * @param int $order
     * @param int $direction
     *
     * @return APM\Request[]
     */
    public function loadFaultyRequests($offset, $limit, $order, $direction) {
        return $this->loadRequestsHelper($this->loadFaultyRequestsQuery, $offset, $limit, $order, $direction);
    }

    /**
     * @param int $offset
     * @param int $limit
     * @param int $order
     * @param int $direction
     *
     * @return APM\Request[]
     */
    public function loadSlowRequests($offset, $limit, $order, $direction) {
        return $this->loadRequestsHelper($this->loadSlowRequestsQuery, $offset, $limit, $order, $direction);
    }

    /**
     * @return int
     */
    public function getRequestsCount() {
        $row = $this->db->query($this->getRequestsCountQuery)->fetch(PDO::FETCH_ASSOC);
        return $row["cnt"];
    }

    /**
     * @return int
     */
    public function getFaultyRequestsCount() {
        $row = $this->db->query($this->getFaultyRequestsCountQuery)->fetch(PDO::FETCH_ASSOC);
        return $row["cnt"];
    }

    /**
     * @return int
     */
    public function getSlowRequestsCount() {
        $row = $this->db->query($this->getSlowRequestsCountQuery)->fetch(PDO::FETCH_ASSOC);
        return $row["cnt"];
    }
}

}

namespace APM\Service\Handler\PDOMySQL {

use APM\Service\Handler;

class Event extends Handler\PDO\Event {
    protected $loadEventQuery = <<<SQL
SELECT UNIX_TIMESTAMP(e.ts) AS timestamp, UNIX_TIMESTAMP(r.ts) AS request_timestamp, e.request_id, type, file, line, message, backtrace, script, uri, host, ip, cookies, post_vars AS postVariables, referer
FROM event e
JOIN request r ON r.id = e.request_id
WHERE e.id = %d
SQL;
    protected $loadEventsQuery = <<<SQL
SELECT id, UNIX_TIMESTAMP(ts) AS timestamp, type, file, line, message
FROM event
WHERE request_id = %d
ORDER BY %s %s
LIMIT %d, %d
SQL;
}

class Request extends Handler\PDO\Request {
    protected $loadRequestQuery = <<<SQL
SELECT r.id, UNIX_TIMESTAMP(r.ts) AS timestamp, script, uri, host, ip, cookies, post_vars AS postVariables, referer, duration
FROM request r
LEFT JOIN slow_request s ON s.request_id = r.id
WHERE r.id = %d
SQL;
    protected $loadRequestsQuery = <<<SQL
SELECT r.id, UNIX_TIMESTAMP(r.ts) AS timestamp, script, uri, host, ip, cookies, post_vars AS postVariables, referer, duration, COUNT(e.request_id) AS eventsCount
FROM request r
LEFT JOIN event e ON e.request_id = r.id
LEFT JOIN slow_request s ON s.request_id = r.id
GROUP BY r.id, r.ts, script, uri, host, ip, cookies, post_vars, referer, duration
ORDER BY %s
LIMIT %d, %d
SQL;
    protected $loadFaultyRequestsQuery = <<<SQL
SELECT r.id, UNIX_TIMESTAMP(r.ts) AS timestamp, script, uri, host, COUNT(e.request_id) AS eventsCount
FROM request r
JOIN event e ON e.request_id = r.id
GROUP BY r.id, r.ts, script, uri, host
ORDER BY %s
LIMIT %d, %d
SQL;
    protected $loadSlowRequestsQuery = <<<SQL
SELECT r.id, UNIX_TIMESTAMP(r.ts) AS timestamp, script, uri, host, duration
FROM request r
JOIN slow_request s ON s.request_id = r.id
GROUP BY r.id, r.ts, script, uri, host, duration
ORDER BY %s
LIMIT %d, %d
SQL;
}

}
