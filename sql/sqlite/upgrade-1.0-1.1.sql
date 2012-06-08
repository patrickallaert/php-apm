CREATE TABLE IF NOT EXISTS event2 (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts INTEGER NOT NULL,
    type INTEGER NOT NULL,
    file TEXT NOT NULL,
    line INTEGER NOT NULL,
    message TEXT NOT NULL,
    backtrace TEXT NOT NULL,
    uri TEXT NOT NULL,
    host TEXT NOT NULL,
    ip INTEGER UNSIGNED NOT NULL,
    cookies TEXT NOT NULL,
    post_vars TEXT NOT NULL
);

INSERT INTO event2 SELECT id, strftime('%s',ts), type, file, line, message, backtrace, '', '', 0, '', '' FROM event;

DROP TABLE event;
ALTER TABLE event2 RENAME TO event;

CREATE TABLE IF NOT EXISTS slow_request2 (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts INTEGER NOT NULL,
    duration FLOAT NOT NULL,
    file TEXT NOT NULL
);

INSERT INTO slow_request2 SELECT id, strftime('%s',ts), duration, file FROM slow_request;

DROP TABLE slow_request;
ALTER TABLE slow_request2 RENAME TO slow_request;
