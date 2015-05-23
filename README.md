# chatload
chatload is a Windows console application written in C++ and created to collect EVE Online character names from chat logs and store them in a database.

# Installation
You can either download the [latest binary release](https://github.com/TheJokr/chatload/releases/latest) to contribute to the public database or build it yourself to create your own database.

## Binary release
After downloading and unzipping the [latest release](https://github.com/TheJokr/chatload/releases/latest), run `chatload.exe` to upload all character names of your log files to the public database.  
The application is distributed as a standalone program, all required binaries are included in the release.  
No log data is uploaded, just the scraped character names.

## Building (Windows)
### Requirements
- [C++ REST SDK (Casablanca)](http://casablanca.codeplex.com/)
- [MySQL](http://www.mysql.com/) Database
- Python (2.6+)
  - [Requests](http://docs.python-requests.org/en/latest/)
  - [MySQLdb](http://sourceforge.net/projects/mysql-python/)
- Optional: Webserver with PHP
  - [PDO](http://php.net/manual/en/book.pdo.php)
    - [MySQL driver](http://php.net/manual/en/ref.pdo-mysql.php)

### Building
1. Replace all placeholders in all files with actual data
  - `YOUR_HOST` and `YOUR_RESOURCE` in `src/chatload.cpp`
  - `SQL_USER_PASSWORD` in `sql/chatload_db.sql`, `scripts/addDataToCharDump.py` and either `scripts/charDump.php` or `scripts/basicServer.py`
2. Log into your MySQL server and execute `sql/chatload_db.sql` to create a database and a user for use with chatload
3. Run `python scripts/basicServer.py` in the background or make sure `scripts/charDump.php` is available via POST request
4. Build chatload.cpp
5. Distribute and execute the compiled binary to add data to your database
6. Execute `python scripts/addDataToCharDump.py` to add corporation, alliance and faction details to the character names

# Usage
`chatload.exe` to upload all character names to the public database (binary release) or the specified POST endpoint (self-built).  
`chatload.exe --version` to show version and license information.  
`python scripts/basicServer.py [PORT] [HOST]` to run a basic web server (self-built).  
`python scripts/addDataToCharDump.py` to add all publicly available data to your database (self-built).

# Credits
chatload is released under the GNU General Public License, version 3. The full license is available in the `LICENSE` file.  
Copyright (C) 2015  Leo Bl&ouml;cher

# Contact
[TheJokr@GitHub](https://github.com/TheJokr)