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


# Script to add publicly available data to database entries
from __future__ import division

import math
import time
import sys
import xml.etree.ElementTree as ET

import requests
import mysql.connector


# Request characters
conn = mysql.connector.connect(
    host="localhost",
    user="chatloadDump",
    password="SQL_USER_PASSWORD",
    database="chatloadDump",
    charset="utf8")
cur = conn.cursor()

cur.execute(
    '''SELECT *
       FROM `characters`
       WHERE `lastModified` < DATE_SUB(CURDATE(), INTERVAL 1 MONTH)
         OR `characterID` IS NULL;''')
res = cur.fetchall()

if not res:
    print("Everything is up-to-date!")
    sys.exit(0)


# Convert tuples to lists
allData = [list(row) for row in res]

# Reset timestamps of to-be-processed characters
cur.executemany(
    '''UPDATE `characters`
       SET `lastModified` = NOW()
       WHERE `ID` = %s''',
    [(char[0], ) for char in res])

# Divide output into lists of 200 characters each
parts = [allData[i*200: (i+1)*200] for i
         in range(int(math.ceil(len(allData) / 200)))]


# Add API data
delCharacters = []
for charList in parts:
    r = requests.post(
        "https://api.eveonline.com/eve/CharacterID.xml.aspx",
        data={'names': ','.join([char[2] for char in charList])},
        headers={'User-Agent': "chatload Scraper"},
        timeout=5)
    if r.status_code != 200:
        print("Execution of POST request failed!")
        continue
    xml = ET.fromstring(r.content)

    for char in charList:
        row = xml[1][0].find(".//*[@name='{}']".format(char[2]))
        if row is not None and row.attrib['characterID'] != '0':
            char[1] = int(row.attrib["characterID"])
        else:
            # Invalid character name
            delCharacters.append(char)
            print("INVALID CHARACTER NAME: {} - Pending removal!".format(
                char[2]))
    charList = [char for char in charList if char[1] is not None]

    r = requests.post(
        "https://api.eveonline.com/eve/CharacterAffiliation.xml.aspx",
        data={'ids': ','.join(map(str, [char[1] for char in charList]))},
        headers={'User-Agent': "chatload Scraper"},
        timeout=10)
    if r.status_code != 200:
        print("Execution of POST request failed!")
        continue
    xml = ET.fromstring(r.content)

    for char in charList:
        row = xml[1][0].find(".//*[@characterID='{}']".format(char[1]))
        char[3] = int(row.attrib['corporationID'])
        char[4] = str(row.attrib['corporationName'])
        char[5] = int(row.attrib['allianceID'])
        char[6] = str(row.attrib['allianceName'])
        char[7] = int(row.attrib['factionID'])
        char[8] = str(row.attrib['factionName'])

    # Sleep for .25 seconds to lower API load
    time.sleep(0.25)


# Update DB
print("Updating database")

if delCharacters:
    print("Removing marked characters...")
    cur.executemany(
        '''DELETE FROM `characters`
           WHERE `ID` = %s;''',
        [(char[0], ) for char in delCharacters])

allData = []
for part in parts:
    allData += part

if allData:
    print("Updating characters...")
    for char in allData:
        cur.execute(
            '''UPDATE `characters`
               SET `characterID` = %(charID)s,
                 `corporationID` = %(corpID)s,
                 `corporationName` = %(corpName)s,
                 `allianceID` = %(allyID)s,
                 `allianceName` = %(allyName)s,
                 `factionID` = %(facID)s,
                 `factionName` = %(facName)s
               WHERE `ID` = %(id)s;''',
            {'id': char[0],
             'charID': char[1],
             'corpID': char[3],
             'corpName': char[4],
             'allyID': char[5] if char[5] != 0 else None,
             'allyName': char[6] if char[6] != '' else None,
             'facID': char[7] if char[7] != 0 else None,
             'facName': char[8] if char[8] != '' else None})

conn.commit()
conn.close()
