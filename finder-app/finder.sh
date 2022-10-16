
#!/bin/bash

filesdir=$1
searchstr=$2

# Exits with return value 1 error and print statements if any of the parameters above were not specified
[ -z ${filesdir} ] && { echo "filesdir parameter is empty" ; exit 1; }
[ -z ${searchstr} ] && { echo "searchstr parameter is empty" ; exit 1; }

# Exits with return value 1 error and print statements if filesdir does not represent a directory on the filesystem
[ ! -d "${filesdir}" ] && { echo "${filesdir} is not exist" ; exit 1; }

# "The number of files are X and the number of matching lines are Y"
# where X is the number of files in the directory and all subdirectories and Y is the number of matching lines found in respective files.

files=$(find ${filesdir} -type f | wc -l)

strings=0
for file in $(ls ${filesdir})
do
    cnt=$(cat ${filesdir}/${file} | grep -o ${searchstr} | wc -l)
    if [ $cnt -gt 0 ]; then
        strings=$((strings+1))
    fi
done

echo "The number of files are ${files} and the number of matching lines are ${strings}"
