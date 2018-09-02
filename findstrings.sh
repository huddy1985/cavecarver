#!/bin/bash
#set -x
n=$1
s=$2
id=`ps -A | grep $n | awk '{print $1}'`
echo "Found '$n': pid $id, search for '$s'";
if [ -z "${id}" ]; then exit 1; fi;
rm -rf tmp_data
mkdir tmp_data;
(cd tmp_data; python ../m.py $id 2> /dev/null; )
chmod a+rwx -R tmp_data
for f in `ls tmp_data/*.data`; do
    echo "test $f"
    strings $f >> tmp_data/strings.txt
done
