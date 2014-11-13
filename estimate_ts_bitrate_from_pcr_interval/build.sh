#!/bin/bash

gcc estimate_ts_bitrate_from_pcr_interval.c ts.h  -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -o estimate_ts_bitrate
