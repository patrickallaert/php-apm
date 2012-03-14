CREATE TABLE IF NOT EXISTS event2 (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts INTEGER NOT NULL,
    type INTEGER NOT NULL,
    file TEXT NOT NULL,
    line INTEGER NOT NULL,
    message TEXT NOT NULL,
    backtrace TEXT NOT NULL,
    uri TEXT NOT NULL,
    ip INTEGER UNSIGNED NOT NULL
);

INSERT INTO event2 SELECT id, strftime('%s',ts), type, file, line, message, backtrace, '', 0 FROM event;

DROP TABLE event;
ALTER TABLE event2 RENAME TO event;
