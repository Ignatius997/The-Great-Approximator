#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Użycie: $0 <fragment_nazwy_procesu>"
    exit 1
fi

procname="$1"

# Wyświetl nagłówek
printf "%-8s %-10s %s\n" "PID" "USER" "COMMAND"

# Szukaj procesów zawierających fragment w nazwie (pomija grep i sam skrypt)
ps -eo pid,user,comm,args --no-headers | \
    grep -i "$procname" | \
    grep -v "grep" | \
    grep -v "$0" | \
    while read -r pid user comm args; do
        printf "%-8s %-10s %s\n" "$pid" "$user" "$args"
    done