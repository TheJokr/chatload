#!/usr/bin/python
# vim: set fileencoding=utf-8 :

# chatload: collect EVE Online character names from chat logs
# Copyright (C) 2015  Leo Blöcher

# This file is part of chatload.

# chatload is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# chatload is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with chatload.  If not, see <http://www.gnu.org/licenses/>.


# Script with a simple webserver to serve chatload uploads
from BaseHTTPServer import BaseHTTPRequestHandler, HTTPServer
from cgi import FieldStorage

from sys import argv

import MySQLdb


if (len(argv) > 2):
    HOST = argv[1]
    PORT = int(argv[2])
elif (len(argv) > 1):
    HOST = ""
    PORT = int(argv[1])
else:
    HOST = ""
    PORT = 8080


conn = MySQLdb.connect(host="localhost", user="chatloadDump", passwd="SQL_USER_PASSWORD", db="chatloadDump", charset="utf8")
cur = conn.cursor()


class SrvHandler(BaseHTTPRequestHandler):
    def do_POST(self):
        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.end_headers()
        form = FieldStorage(fp=self.rfile, headers=self.headers, environ={'REQUEST_METHOD':'POST', 'CONTENT_TYPE':self.headers['Content-Type']})
        names = form.getvalue('name').split(",")
        for char in names:
            cur.execute("INSERT IGNORE INTO `characters` (`characterName`) VALUES (%s);", [str(char)])
            self.wfile.write(str(char) + ": OK\n")
        conn.commit()


print("Server is starting up...")
httpd = HTTPServer((HOST, PORT), SrvHandler)
print("Serving at: http://{}:{}".format((HOST or "localhost"), PORT))

httpd.serve_forever()