#!/bin/bash
SLEEP_PID=SLEEP_PID
trap 'kill $SLEEP_PID; exit' USR1
echo $$ >$2
echo "saved signal_wait PID to $2; about to signal using PID from $1"
PID=`cat "$1"`
kill $PID
sleep 1d &
SLEEP_PID=$!
wait $SLEEP_PID
