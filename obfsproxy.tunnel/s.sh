#!/bin/bash
. ${HOME}/.o/p.sh
d=`dirname $(realpath $0)`
cat ${d}/standalone_server_config.ini          | \
    sed -e "s:--server--:${server}:g "         | \
    sed -e "s:--serverport--:${serverport}:g " | \
    sed -e "s:--serverlocal--:${serverlocal}:g " | \
    sed -e "s:--node-id--:${nodeid}:g " | \
    sed -e "s:--private-key--:${privatekey}:g " | \
    sed -e "s:--public-key--:${publickey}:g " | \
    sed -e "s:--drbg-seed--:${drbgseed}:g " | \
    sed -e "s:--cert--:${cert}:g " | \
    sed -e "s:--state--:${HOME}/.o:g " | \
    sed -e "s:--obfs--:${obfs}:g " \
    > ${HOME}/.o/s.ini

killall obfs4proxy 2> /dev/null
(cd ${d}; python3 standalone_server.py -v ${HOME}/.o/s.ini) &
sleep 1
echo "Tunnel at ${serverlocal}:${serverport}, external ${server}::${serverport}"
