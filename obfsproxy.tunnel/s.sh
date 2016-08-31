#!/bin/bash
. ${HOME}/.o/p.sh
d=`dirname $(realpath $0)`
cat ${d}/standalone_client_config.ini          | \
    sed -e 's:--server--:${server}:g '         | \
    sed -e 's:--serverport--:${serverport}:g ' | \
    sed -e 's:--serverlocal--:${serverlocal}:g ' | \
    sed -e 's:--node-id--:${node-id}:g ' | \
    sed -e 's:--private-key--:${private-key}:g ' | \
    sed -e 's:--public-key--:${public-key}:g ' | \
    sed -e 's:--drbg-seed--:${drbg-seed}:g ' | \
    sed -e 's:--cert--:${cert}:g '  \
    > ${HOME}/.o/s.ini

killall obfs4proxy 2> /dev/null
(cd ${d}; python3 standalone_server.py -v ${HOME}/.o/s.ini) &
echo "Tunnel at ${serverlocal}:${serverport}, external ${server}::${serverport}"
