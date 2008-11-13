CREATE TABLE event (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts TEXT,
    type INTEGER,
    file TEXT,
    line INTEGER,
    message TEXT);
