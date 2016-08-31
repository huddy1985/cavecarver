#!/bin/bash
. ${HOME}/.o/p.sh
d=`dirname $(realpath $0)`
cat ${d}/standalone_client_config.ini          | \
    sed -e 's:--server--:${server}:g '         | \
    sed -e 's:--serverport--:${serverport}:g ' | \
    sed -e 's:--cert--:${cert}:g '  \
    > ${HOME}/.o/c.ini

killall obfs4proxy 2> /dev/null
(cd ${d}; python3 standalone_client.py -v ${HOME}/.o/c.ini) &

echo "Tunnel to ${server}:${serverport}, ssh gate at 127.0.0.1:8000"
