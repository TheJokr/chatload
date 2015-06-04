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


# Script to add publicly available data to character names for use with chatload
import time
import sys
import xml.etree.ElementTree as ET
import requests

import MySQLdb


def split_list(inputList, wanted_parts=1):
    length = len(inputList)
    return [inputList[i*length // wanted_parts: (i+1)*length // wanted_parts]
            for i in range(wanted_parts)]


# Request characters (existing ones (>1 month since last update) and new ones)
conn = MySQLdb.connect(
    host="localhost",
    user="chatloadDump",
    passwd="SQL_USER_PASSWORD",
    db="chatloadDump",
    charset="utf8")
cur = conn.cursor()

cur.execute(
    '''SELECT *
    FROM `characters`
    WHERE `lastModified` < DATE_SUB(CURDATE(), INTERVAL 1 MONTH)
      OR `characterID` IS NULL;''')
res = cur.fetchall()

if (not res):
    print("Everything is up-to-date!")
    sys.exit(0)


# Convert tuples to lists (can't edit data in tuples)
allData = list()
for row in res:
    allData.append(list(row))

# Divide output into lists of 200 chars (higher number breaks some APIs)
parts = split_list(allData, (len(allData)//200)+1)


# Add API data
removeCharacters = list()
for charList in parts:
    r = requests.post(
        "https://api.eveonline.com/eve/CharacterID.xml.aspx",
        data={"names": ",".join([char[2] for char in charList])},
        headers={"User-Agent": "CharacterDB Scraper"},
        timeout=5)
    if (r.status_code != 200):
        print("Execution of POST request failed!")
    # Replace Unicode replacement character with a ? to stop exceptions
    xml = ET.fromstring(r.text.replace(u"\uFFFD", u"?"))
    for char in charList:
        row = xml[1][0].find(".//*[@name='"+char[2]+"']")
        if (row is not None and row.attrib['characterID'] != '0'):
            char[1] = int(row.attrib["characterID"])
        else:
            # These character names are invalid
            removeCharacters.append(char)
            print("INVALID CHARACTER NAME: "+str(char[2])+" - Removed!")
    charList = [char for char in charList if char[1] != None]
    r = requests.post(
        "https://api.eveonline.com"
        "/eve/CharacterAffiliation.xml.aspx",
        data={"ids": ",".join([str(char[1]) for char in charList])},
        headers={"User-Agent": "CharacterDB Scraper"},
        timeout=10)
    if (r.status_code != 200):
        print("Execution of POST request failed!")
    # Replace Unicode replacement character with a ? to stop exceptions
    xml = ET.fromstring(r.text.replace(u"\uFFFD", "?"))
    for char in charList:
        row = xml[1][0].find(".//*[@characterID='"+str(char[1])+"']")
        char[3] = int(row.attrib['corporationID'])
        char[4] = str(row.attrib['corporationName'])
        char[5] = int(row.attrib['allianceID'])
        char[6] = str(row.attrib['allianceName'])
        char[7] = int(row.attrib['factionID'])
        char[8] = str(row.attrib['factionName'])
    # Sleep for 1 second to lower API load
    print("SecSleep")
    time.sleep(1)


# Update DB
print("Updating database")

if (removeCharacters):
    print("Removing characters...")
    cur.executemany(
        '''DELETE FROM `characters`
        WHERE `ID` = %s;''',
        [char[0] for char in removeCharacters])

allData = list()
for part in parts:
    allData += part

if (allData):
    print("Updating characters...")
    for char in allData:
        cur.execute(
            '''UPDATE `characters`
            SET `characterID` = %(charID)s,
            `corporationID` = %(corpID)s, `corporationName` = %(corpName)s,
            `allianceID` = %(allyID)s, `allianceName` = %(allyName)s,
            `factionID` = %(facID)s, `factionName` = %(facName)s
            WHERE `ID` = %(id)s;''',
            {"id": char[0],
             "charID": char[1],
             "corpID": char[3],
             "corpName": char[4],
             "allyID": (char[5] if char[5] != 0 else None),
             "allyName": (char[6] if char[6] != '' else None),
             "facID": (char[7] if char[7] != 0 else None),
             "facName": (char[8] if char[8] != '' else None)})

conn.commit()
