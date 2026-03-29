#!/bin/sh


if [ -z "$1" ]; then
    echo "Usage: $0 path_to_filesdir"
    exit 1
fi

if [ -z "$2" ]; then
    echo "Usage: $0 path_to_searchstr"
    exit 1
fi

count_files=$(find "$1" -type f | wc -l)
count_lines=$(find "$1" -type f -exec grep -H "$2" {} \; | wc -l)

echo "The number of files are $count_files and the number of matching lines are $count_lines"
