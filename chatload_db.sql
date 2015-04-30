/*
 * chatload: collect EVE Online character names from chat logs
 * Copyright (C) 2015  Leo Blöcher
 *
 * This file is part of chatload.
 *
 * chatload is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * chatload is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with chatload.  If not, see <http://www.gnu.org/licenses/>.
 */


-- MySQL config file for use with chatload
CREATE DATABASE IF NOT EXISTS `chatloadDump` DEFAULT CHARSET = utf8;

CREATE USER 'chatloadDump'@'localhost' IDENTIFIED BY 'SQL_USER_PASSWORD';
GRANT ALL ON `chatloadDump`.* TO 'chatloadDump'@'localhost';
FLUSH PRIVILEGES;


CREATE TABLE IF NOT EXISTS `chatloadDump`.`characters` (
  `ID` int NOT NULL PRIMARY KEY AUTO_INCREMENT,
  `characterID` INT DEFAULT NULL UNIQUE,
  `characterName` VARCHAR(255) NOT NULL UNIQUE,
  `corporationID` INT DEFAULT NULL,
  `corporationName` VARCHAR(255) DEFAULT NULL,
  `allianceID` INT DEFAULT NULL,
  `allianceName` VARCHAR(255) DEFAULT NULL,
  `factionID` INT DEFAULT NULL,
  `factionName` VARCHAR(255) DEFAULT NULL,
  `lastModified` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
) DEFAULT CHARSET=utf8 COMMENT='DB to save chatload.exe uploads';