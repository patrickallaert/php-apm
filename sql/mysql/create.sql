CREATE TABLE IF NOT EXISTS event (
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,
    ts TIMESTAMP NOT NULL,
    type TINYINT UNSIGNED NOT NULL,
    file TEXT NOT NULL,
    line MEDIUMINT UNSIGNED NOT NULL,
    message TEXT NOT NULL,
    backtrace TEXT NOT NULL,
    uri TEXT NOT NULL,
    host TEXT NOT NULL,
    ip INTEGER UNSIGNED NOT NULL,
    cookies TEXT NOT NULL,
    post_vars TEXT NOT NULL,
    referer TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS slow_request (
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,
    ts TIMESTAMP NOT NULL,
    duration FLOAT NOT NULL,
    file TEXT NOT NULL
);
