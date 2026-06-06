#!/bin/bash


if [ -z "$1" ]; then
    echo "Usage: $0 path_to_file"
    exit 1
fi

if [ -z "$2" ]; then
    echo "Usage: $0 text_to_write"
    exit 1
fi

writefile="$1"
writestr="$2"

dir=$(dirname "$writefile")
file=$(basename "$writefile")

if [ -f "$writefile" ]; then
    echo "File $writefile already exists. Overwriting with content: $writestr"
    echo "$writestr" > "$writefile"
else
    echo "Creating file $writefile with content: $writestr"
    mkdir -p "$dir" && touch "$file"
    echo "$writestr" > "$writefile"
fi

if [ ! -f "$writefile" ]; then
    echo "File $writefile failed to create."
    exit 1
fi