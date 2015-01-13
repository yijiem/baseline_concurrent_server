#!/bin/bash
echo "hostname="
echo "$1"
while :
do
    curl -v http://"$1":8000
    sleep 5
done