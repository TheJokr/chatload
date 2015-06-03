# chatload
chatload is a Windows console application written in C++ and created to collect EVE Online character names from chat logs and store them in a database.

# Installation
You can either download the [latest binary release](https://github.com/TheJokr/chatload/releases/latest) to contribute to any database or build it yourself (including your own database).

## Binary release
After downloading and unzipping the [latest release](https://github.com/TheJokr/chatload/releases/latest), run `chatload.exe` to upload all character names found in all your log files to the public database.  
The application is distributed as a standalone program, all required binaries are included in the release.  
In order to change the endpoint, see [Configuration](#configuration).  
No log data is uploaded, just the scraped character names.

### Configuration
`config.json` is used to determine which endpoint to use. If the file is not present upon execution, a new one will be generated using the default settings.  
In order to use a custom endpoint, change the default values.  
`host` refers to the base URI with protocol, hostname, and optionally a port number (eg. http://example.com).  
`resource` refers to the Path, Query, and Fragment of the POST endpoint (eg. /exampleScript.php).  
`parameter` refers to the POST parameter used to reference the character names on the server (eg. `$_POST['parameter']` in a PHP script).

## Building (Windows)
### Requirements
- [C++ REST SDK (Casablanca)](http://casablanca.codeplex.com/)
- [MySQL](http://www.mysql.com/)/[MariaDB](http://mariadb.org/) Database
- Python (2.6+)
  - [Requests](http://docs.python-requests.org/en/latest/)
  - [MySQLdb](http://sourceforge.net/projects/mysql-python/)
- Optional: Webserver with PHP
  - [PDO](http://php.net/manual/en/book.pdo.php)
    - [MySQL driver](http://php.net/manual/en/ref.pdo-mysql.php)

### Building
1. Replace `SQL_USER_PASSWORD` in `sql/chatload_db.sql`, `scripts/addDataToCharDump.py` and either `scripts/charDump.php` or `scripts/basicServer.py` with an actual password
2. Log into your MySQL server and execute `sql/chatload_db.sql` to create a database and a user for use with chatload scripts
3. Run `python scripts/basicServer.py [PORT] [HOST]` in the background or make sure `scripts/charDump.php` is available via POST request
4. Build chatload.exe
5. Distribute and execute the compiled binary to add data to your database
  - Make sure to include a `config.json` file with your custom `host` and `resource`
6. Execute `python scripts/addDataToCharDump.py` to add corporation, alliance and faction details to the character names

*Note*: You might have to run `3to2` when using `scripts/basicServer.py` or `scripts/addDataToCharDump.py` with Python &ge; 3.0

# Usage
`chatload.exe` to upload all character names to the public database (default) or the specified POST endpoint (custom configuration file).  
`chatload.exe --version` to show version and license information.  
`python scripts/basicServer.py [PORT] [HOST]` to run a basic web server (own Database).  
`python scripts/addDataToCharDump.py` to add all publicly available data to your own database (own Database).

# Credits
chatload is released under the GNU General Public License, version 3. The full license is available in the `LICENSE` file.  
Copyright (C) 2015  Leo Bl&ouml;cher

# Contact
[TheJokr@GitHub](https://github.com/TheJokr)