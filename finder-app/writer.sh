
#!/bin/bash

writefile=$1
writestr=$2

[ -z ${writefile} ] && { echo "writefile parameter is empty" ; exit 1; }
[ -z ${writestr} ] && { echo "writestr parameter is empty" ; exit 1; }

mkdir -p "${writefile%/*}" && touch "${writefile}"

test $? -eq 0 || { echo "can't create $writefile"; exit 1; }

echo ${writestr} > ${writefile}

