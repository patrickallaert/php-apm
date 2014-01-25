CREATE TABLE IF NOT EXISTS request (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts INTEGER NOT NULL,
    script TEXT NOT NULL,
    uri TEXT NOT NULL,
    host TEXT NOT NULL,
    ip INTEGER UNSIGNED NOT NULL,
    cookies TEXT NOT NULL,
    post_vars TEXT NOT NULL,
    referer TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS event (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    request_id INTEGER,
    ts INTEGER NOT NULL,
    type INTEGER NOT NULL,
    file TEXT NOT NULL,
    line INTEGER NOT NULL,
    message TEXT NOT NULL,
    backtrace BLOB NOT NULL
);

CREATE INDEX IF NOT EXISTS event_request ON event (request_id);

CREATE TABLE IF NOT EXISTS stats (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    request_id INTEGER,
    ts INTEGER NOT NULL,
    duration FLOAT NOT NULL
);

CREATE INDEX IF NOT EXISTS stats_request ON stats (request_id);
