#!/bin/bash
PORT=9999
HOST=localhost

while true; do
    read -r -p "horizon > " input
    [ "$input" = "exit" ] && break
    echo "$input" | nc -w 1 $HOST $PORT
done