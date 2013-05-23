# APM (Alternative PHP Monitor)

## Installing

### From PECL:

1. `$ sudo pecl install apm`

### From source:

1. `$ git clone https://github.com/patrickallaert/php-apm.git`
2. `cd php-apm`
3. `$ phpize`
4. Configure the extension, by default, only **sqlite3** support is built:

    ```
    $ ./configure [--with-sqlite3[=DIR]] [--with-mysql[=DIR]] [--with-debugfile[=FILE]]
    ```
5. Compile it:

    ```
    $ make
    ```
6. Install it:

    ```
    $ sudo make install
    ```

## Configuration

1. Skip to 5. if you are not using the SQLite driver.
2. Create the directory for the SQLite database as referenced in the setting: *apm.sqlite_db_path*:

    ```
    $ mkdir -p /var/php/apm/db/
    $ chmod a+rwx /var/php/apm/db/
    ```
3. Create the sqlite3 schema:

    ```
    $ sqlite3 /var/php/apm/db/events < sql/sqlite/create.sql
    ```
4. Set the following settings:

    ```
    apm.sqlite_enabled=1
    ; The directory containing the "events" file
    apm.sqlite_db_path="/path/to/directory/configured/in/6."
    ; Error reporting level specific to the SQLite3 driver, same level as for PHP's *error_reporting*
    apm.sqlite_error_reporting=E_ALL|E_STRICT
    ```
5. Skip to 9. if you are not using the MySQL driver.
6. Create a user/database for storing APM events.
7. Create the tables:

    ```
    $ mysql -u <user> -p <password> <APMdatabase> < sql/mysql/create.sql
    ```
8. Set the following settings:

    ```
    apm.mysql_enabled=1
    ; Error reporting level specific to the MySQL driver, same level as for PHP's *error_reporting*
    apm.mysql_error_reporting=E_ALL|E_STRICT
    apm.mysql_host=<host>
    ;apm.mysql_port=<port>
    apm.mysql_user=<user>
    apm.mysql_pass=<password>
    apm.mysql_db=<dbname>
    ```
9. Copy the *web/* directory so that it can be accessed by your webserver.
10. Configure *web/setup.php* to use the correct backend:

    ```
    define("APM_DRIVER", "sqlite"); // correct values are "sqlite" or "mysql"
    ```
