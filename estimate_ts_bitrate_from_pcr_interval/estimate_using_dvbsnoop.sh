#!/bin/bash

USAGE="$0 <Input TS file> <PCR PID> <Packets to analyze>"
if [ $# -ne 3 ];then
    echo $USAGE
    exit 1
fi

TS_FILE=$1
PCR_PID=$2
PACKETS_TO_ANALYZE=$3

dvbsnoop -if "$TS_FILE" -s ts -n $PACKETS_TO_ANALYZE | python find_pcr.py $PCR_PID
