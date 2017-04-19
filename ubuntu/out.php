<?php

// must rename zones after download to country.zone
$files = glob("*.zone");
$of = fopen('out.sh','w');

fwrite($of,'#!/bin/sh' . "\n");
fwrite($of,"# Get zone files from http://www.ipdeny.com/ipblocks/\n");
fwrite($of,"# Script generator written by Anthony Maro\n");
fwrite($of,"# https://www.ossramblings.com/whitelisting-ipaddress-with-iptables-ipset\n");
fwrite($of,'IPSET="/sbin/ipset"' . "\n");
foreach ($files as $file) {
  echo "Working on $file\n";
  $i = 0;
  $if = file($file);
  $info = pathinfo($file);
  $country = $info['filename'];
  $zone = "zone-" . $country;
  fwrite($of,"echo \"Allowing $country\"\n");
  fwrite($of,'$IPSET create ' . $zone . " hash:net\n");
  fwrite($of,'$IPSET flush ' . $zone . "\n");
  foreach($if as $line) {
    $line = trim($line);
    fwrite($of, '$IPSET ' . "add $zone $line\n");
    $i++;
    if ($i > 30000) {
      $i = 0;
      echo "splitting $country\n";
      $zone = "zone-" . $country . "-2";
      fwrite($of,"echo \"Splitting $zone\"\n");
      fwrite($of,'$IPSET ' . "create $zone hash:net\n");
      fwrite($of,'$IPSET ' . "flush $zone\n");
    }
  }
}
fwrite($of,"echo \"Countries allowed.\"\n");
fclose($of);
        
?>