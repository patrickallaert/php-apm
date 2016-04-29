# APM (Alternative PHP Monitor)

[![Build Status](https://travis-ci.org/patrickallaert/php-apm.svg?branch=master)](https://travis-ci.org/patrickallaert/php-apm)

APM (Alternative PHP Monitor) is a monitoring extension enabling native [Application Performance Management (APM)](http://en.wikipedia.org/wiki/Application_performance_management) for PHP.
It doesn't require any modification to your application's code and let's you collect performance metrics as well as catching issues that may arise.

## Installing

### From PECL:

1. `$ sudo pecl install apm`

### From source:

1. `$ git clone https://github.com/patrickallaert/php-apm.git`
2. `$ cd php-apm`
3. `$ phpize`
4. Configure the extension, by default, **sqlite3**, **MariaDB/MySQL**, **[StatsD](https://github.com/etsy/statsd/)**, **HTTP**, and **Socket** support are enabled:

    ```
    $ ./configure [--with-sqlite3[=DIR]] [--with-mysql[=DIR]] [--enable-statsd] [--enable-socket] [--enable-http] [--with-debugfile[=FILE]]
    ```
    To disable the support of a `--with-*` switch, use: `--without-*`, example: `$ ./configure --without-sqlite3`
    To disable the support of a `--enable-*` switch, use: `--disable-*`, example: `$ ./configure --disable-socket`
5. Compile it:

    ```
    $ make [-j]
    ```
    Tip: use `-j` to activate parallel compilation.
6. Install it:

    ```
    $ sudo make install
    ```

## Upgrading

### From 2.0 to 2.1

#### MariaDB / MySQL

   ```sql
   ALTER TABLE request ADD COLUMN method TEXT NOT NULL;
   ```

#### SQLite

   ```sql
   ALTER TABLE request ADD COLUMN method TEXT NOT NULL DEFAULT '';
   ```

## Configuration

### Common

1. Activate the extension in the php configuration by adding:

    ```ini
    extension=apm.so
    ```
    Note: APM depends on JSON, so the apm.so extension must be loaded **after** JSON!

### MariaDB/MySQL driver

The MariaDB/MySQL driver lets you store errors and stats in a local, remote or shared database.

1. Create a user/database for storing APM events.
2. Set the following settings in the php configuration:

    ```ini
    apm.mysql_enabled=1
    ; Error reporting level specific to the MariaDB/MySQL driver, same level as for PHP's *error_reporting*
    apm.mysql_error_reporting=E_ALL|E_STRICT
    apm.mysql_host=<host>
    ;apm.mysql_port=<port>
    apm.mysql_user=<user>
    apm.mysql_pass=<password>
    apm.mysql_db=<dbname>
    ```

### SQLite driver

The SQLite driver lets you store errors and stats in a local SQLite database.

Note that if performance matters, you should better use the MariaDB/MySQL driver.

1. Create the directory for the SQLite database as it will be referenced in the setting: *apm.sqlite_db_path* (see next step):

    ```
    $ mkdir -p /var/php/apm/db/
    $ chmod a+rwx /var/php/apm/db/
    ```
2. Set the following settings in the php configuration:

    ```ini
    apm.sqlite_enabled=1
    ; The directory containing the "events" file
    apm.sqlite_db_path="/path/to/directory/configured/in/previous/step."
    ; Error reporting level specific to the SQLite3 driver, same level as for PHP's *error_reporting*
    apm.sqlite_error_reporting=E_ALL|E_STRICT
    ```

### StatsD driver

The [StatsD](https://github.com/etsy/statsd/) driver is handy to send performance metrics of your web sites/applications directly to StatsD, without interfering with your code. The most common backend for StatsD is [Graphite](http://graphite.wikidot.com/), see [Etsy's blog post on StatsD/Graphite], but [other backends](https://github.com/etsy/statsd/wiki/Backends) exist as well.

1. Install [StatsD](https://github.com/etsy/statsd/).
2. Configure it in the php configuration:

    ```ini
    ; Whether to enable the statsd driver
    apm.statsd_enabled=On
    ; Host of the statsd server
    apm.statsd_host=localhost
    ; Port of the statsd server
    apm.statsd_port=3306
    ```

### Socket driver

The socket driver lets you send errors and stats directly by Unix and/or TCP sockets. This is handy if you want to write you own piece of code to handle those.

1. Configure it in the php configuration:

    ```ini
    ; Whether to enable the socket driver
    apm.socket_enabled=On
    ; Socket path (accept multiple entries, separated by "|", prefixed with "file:" or "tcp:")
    ; Example: apm.socket_path=file:/var/tmp/apm.sock|tcp:localhost:1234
    apm.socket_path=file:/tmp/apm.sock
    ```

### Web frontend

The web frontend to visualize the data is now available at: https://github.com/patrickallaert/php-apm-web

### Advanced configuration

```ini
; Application identifier, helps identifying which application is being monitored
apm.application_id="My application"
; Enable the capture of events such as errors, notices, warnings,...
apm.event_enabled=On|Off
; Maximum duration in ms a request can take before it gets recorded
apm.stats_duration_threshold="100"
; Maximum User CPU time in ms a request can take before it gets recorded
apm.stats_user_cpu_threshold="100"
; Maximum System CPU time in ms a request can take before it gets recorded
apm.stats_sys_cpu_threshold="10"
; Boolean controlling whether the stacktrace should be stored or not
apm.store_stacktrace=On|Off
; Boolean controlling whether the ip should be stored or not
apm.store_ip=On|Off
; Boolean controlling whether the cookies should be stored or not
apm.store_cookies=On|Off
; Boolean controlling whether the POST variables should be stored or not
apm.store_post=On|Off
; Maximum recursion depth used when dumping a variable
; NOTE: With today's application complexity, the amount of data collected in stacktraces
; including arguments can lead to several thousands of MiB of memory consumed.
; We recommend increasing this value with care.
apm.dump_max_depth=1

; For the configuration below, <driver_name> can be any of: sqlite, mysql, statsd or socket

; Activate or deactivate a specific driver (note also that it must be compiled in to be activated).
apm.<driver_name>_enabled=On|Off
; Whether to collect stats for this driver (ATTENTION: Has a small performance overhead when used with a DB drivers!)
apm.<driver_name>_stats_enabled=On|Off
; Error reporting level specific to this driver
apm.<driver_name>_error_reporting=E_ALL|E_STRICT
; Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions)
apm.<driver_name>_exception_mode=1
; Stores silenced events (Errors, warnings, notices,... that are muted with the PHP '@' operator)
apm.<driver_name>_process_silenced_events=On|Off
```

For a list of all PHP ini directives, take a look at the [apm.ini file](apm.ini).

### Common issues

- SQLite database isn't created: this usually happens when your webserver or PHP process (fcgi, fpm) has no filesystem access:
    - Check your configuration: maybe SQLite isn't configured properly for APM
    - Check permissions on the directory and events file for the user that PHP runs as (sometimes it's your webserver user)
    - Check open_basedir restrictions

- Not all data on the specific PHP scripts in which errors occur is collected:
    - Usually an access issue, check permissions
    - Sometimes this is due to open_basedir restrictions

- APM is configured correctly, but nothing shows up, and phpinfo() doesn't show anyting related to APM:
    - APM wasn't actually loaded, check if the ini file you configured is actually used by PHP
    - the APM module was tried, but fatal errors prevented it from loading; check the PHP error log, usually a dependency like JSON wasn't loaded yet
