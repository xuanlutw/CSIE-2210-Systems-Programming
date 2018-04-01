#!/bin/bash
function start_server {
    ./read_server $1 1>/dev/null 2>/dev/null &
    read_server_pid=$!
    ./write_server $2 1>/dev/null 2>/dev/null &
    write_server_pid=$!
}
function end_server {
    kill -9 $read_server_pid 2>/dev/null
    kill -9 $write_server_pid 2>/dev/null
}

if [ $# -ne "2" ]; then
    echo "Usage: `basename $0` [port1] [port2]"
    exit 0
fi

for t in {1..6}
do
    start_server $1 $2
    ./test localhost $1 $2 $t
    if [ "$?" == "0" ]; then
        result[$t]="1"
    else
        result[$t]="0"
    fi
    end_server
    sleep 1
done
echo ${result[@]}
