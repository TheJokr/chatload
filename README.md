# chatload [![Build Status](https://travis-ci.org/TheJokr/chatload.svg?branch=master)](https://travis-ci.org/TheJokr/chatload)
chatload is a Windows console application created to collect EVE Online character names from chat logs and store them in a database.

# Installation
You can either download the [latest release](https://github.com/TheJokr/chatload/releases/latest) to contribute to any existing database or build it yourself from source. You can also create your own database using the included schema and scripts.

## Binary release
After downloading and unzipping the [latest release](https://github.com/TheJokr/chatload/releases/latest), run `chatload.exe` to upload all character names found in all local log files to the public database. In order to change the default behaviour, see [Configuration](#configuration).  
The application is distributed as a standalone program, all required binaries are included in the release.  
No log data is uploaded, just the scraped character names.

## Configuration
`config.json` is used to determine which endpoint(s) to use.
If the file is not present upon execution, a new one will be generated using the default settings.
In order to use (a) custom endpoint(s), change the default values.

`POST` is an array of objects, each containing:
- `host`: the base URI with protocol, hostname, and optionally a port number (eg. http://example.com)
- `resource`: the path, query, and fragment of the POST endpoint (eg. /exampleScript.php)
- `parameter`: the POST parameter used to reference the character names on the server (eg. `$_POST['parameter']` in a PHP script)

`regex` is a string containing a regular expression used to filter the files which are read by the application. Use `.*` to match all log files.

#### Example
```json
{
    "POST": [
        {
            "host": "http://server1.example.com",
            "parameter": "names",
            "resource": "/uploadCharacterNames.php"
        },
        {
            "host": "http://server2.example.com:8080",
            "parameter": "names",
            "resource": "/scripts/statistics.php"
        }
    ],
    "regex": ".*"
}
```

## Building (Windows)
### Requirements
- [C++ REST SDK (Casablanca)](http://casablanca.codeplex.com/)
- [MySQL](http://www.mysql.com/)/[MariaDB](http://mariadb.org/) Database
- Python (&ge; 2.6)
  - [Requests](http://docs.python-requests.org/en/latest/)
  - [MySQL Connector/Python](http://dev.mysql.com/doc/connector-python/en/index.html)
- Optional: Webserver with PHP
  - [PDO](http://php.net/manual/en/book.pdo.php)
    - [MySQL driver](http://php.net/manual/en/ref.pdo-mysql.php)

### Building
1. Replace `SQL_USER_PASSWORD` in `sql/schema.sql`, `scripts/addDataToDB.py`,  `scripts/charDump.php` and `scripts/basicServer.py` with a password
2. Execute `sql/schema.sql` on your database to create the required tables
3. Run `python scripts/basicServer.py [PORT] [HOST]` in the background or make sure `scripts/charDump.php` is available via POST request
4. Build chatload.exe
5. Distribute and execute the compiled binary to add data to your database
  - Make sure to include a `config.json` file with custom settings
6. Execute `python scripts/addDataToDB.py` to add corporation, alliance and faction details to the character names

*Note*: You have to run `2to3` when using `scripts/basicServer.py` or `scripts/addDataToDB.py` with Python &ge; 3.0.

# Usage
`chatload.exe` to upload all character names to the public database (default) or the specified POST endpoint (custom `config.json` file).  
`chatload.exe --version` to show version and license information.  
`python scripts/basicServer.py [PORT] [HOST]` to run a basic web server which will add character names to your database.  
`python scripts/addDataToDB.py` to add all publicly available data to your database.

# Credits
chatload is released under the GNU General Public License, version 3. The full license is available in the `LICENSE.md` file.  
Copyright (C) 2015  Leo Bl&ouml;cher

# Contact
[TheJokr@GitHub](https://github.com/TheJokr)
