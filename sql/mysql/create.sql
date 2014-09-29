CREATE TABLE IF NOT EXISTS request (
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,
    ts TIMESTAMP NOT NULL,
    script TEXT NOT NULL,
    uri TEXT NOT NULL,
    host TEXT NOT NULL,
    ip INTEGER UNSIGNED NOT NULL,
    cookies TEXT NOT NULL,
    post_vars TEXT NOT NULL,
    referer TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS event (
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,
    request_id INTEGER UNSIGNED,
    ts TIMESTAMP NOT NULL,
    type INTEGER UNSIGNED NOT NULL,
    file TEXT NOT NULL,
    line MEDIUMINT UNSIGNED NOT NULL,
    message TEXT NOT NULL,
    backtrace BLOB NOT NULL,
    KEY request (request_id)
);

CREATE TABLE IF NOT EXISTS stats (
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,
    request_id INTEGER UNSIGNED,
    ts TIMESTAMP NOT NULL,
    duration FLOAT NOT NULL,
    KEY request (request_id)
);
