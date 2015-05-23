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


// Script to dump character names to database for use with chatload
<?php
if ($_SERVER['REQUEST_METHOD'] == 'POST' && isset($_POST['name'])) {
  $names = explode(",", $_POST['name']);
  try {
    $dbh = new PDO('mysql:host=localhost;dbname=chatloadDump;charset=utf8', 'chatloadDump', 'SQL_USER_PASSWORD');
  } catch (PDOException $e) {
    exit("Connection failed: ".$e->getMessage());
  }
  $dbh->beginTransaction();
  $stmt = $dbh->prepare("INSERT IGNORE INTO `characters` (`characterName`) VALUES (:charName);");
  foreach($names as $name) {
    $stmt->execute(array("charName" => $name));
    echo $name.": OK".PHP_EOL;
  }
  $dbh->commit();
} else {
  echo "Invalid request";
}
?>