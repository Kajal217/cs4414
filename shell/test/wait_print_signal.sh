#!/bin/bash
echo "saving PID to signal to $1; reading PID to signal afterwards from $3"
exec 3>&2
exec 2>/dev/null
bash -c 'echo $$; exec sleep 1d' >$1
exec 2>&3
echo "$2"
kill -USR1 `cat $3`
