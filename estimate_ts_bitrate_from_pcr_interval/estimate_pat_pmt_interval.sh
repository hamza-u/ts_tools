#!/bin/bash

USAGE="$0 <Input TS file> <PCR PID> <PMT PID> <Packets to analyze>"
if [ $# -ne 4 ];then
    echo $USAGE
    exit 1
fi

TS_FILE=$1
PCR_PID=$2
PMT_PID=$3
PACKETS_TO_ANALYZE=$4

dvbsnoop -if "$TS_FILE" -s ts -n $PACKETS_TO_ANALYZE | python pat_pmt_interval.py $PCR_PID $PMT_PID
