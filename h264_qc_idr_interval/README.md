## Introduction
This tools validates time interval between IDR frames in MPEG-TS files with H.264 video.

## Building
Run `build.sh` script on Linux systems.

## Usage
`./h264_qc_idr_interval <input filename> <video pid> <max IDR interval allowed in ms>`
  * `input filename`                 - Input MPEG-TS file with H.264 video.
  * `video pid`                      - Video PID to run QC on.
  * `max IDR interval allowed in ms` - Maximum time interval in ms allowed between two IDR frames.
  
## Logs
  * Error scenario - `IDR interval is too high in %s. Max limit:%d, Diff:%d, last_idr:%lu, cur_vid_pts:%lu`

## Exit code
  * On success - 0
  * On error   - 1
