#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ts.h"
#include "pes.h"

#define USAGE "%s <input filename> <video pid> <max IDR interval allowed in ms>\n"
#define MAX_PES_STORE_SIZE (4000000)

uint8_t *pes_store_buf_;
int pes_store_buf_index_;

enum {
    VIDF_TYPE_UNDEFINED,
    VIDF_TYPE_IDR,
    VIDF_TYPE_P,
    VIDF_TYPE_B
};



int GetPesData(unsigned char *ts_buf, unsigned char *pes_buf, int pes_buf_index)
{
    unsigned char *ts_pld;
    unsigned char *pes_pld;
    int            payload_size;

    ts_pld  = ts_payload(ts_buf);
    // For unit start get pes_payload, otherwise ts_payload is pes_payload continuation.
    if(ts_get_unitstart(ts_buf))
        pes_pld = pes_payload(ts_pld);
    else
        pes_pld = ts_pld;

    payload_size = TS_SIZE - (unsigned int)(pes_pld - ts_buf);
    
    if ((pes_store_buf_index_+payload_size) > MAX_PES_STORE_SIZE)
    {
        fprintf (stderr, "Exceeding maximum PES size. Ideally this should never happen.\n");
        return -1;
    }

    if(payload_size > 0)
    {
        memcpy(pes_store_buf_ + pes_store_buf_index_, pes_pld, payload_size);
        pes_store_buf_index_ += payload_size;
    }
    return 0;
}

bool H264CheckAccessUnitDelim(unsigned char *es_data, int len) {
    unsigned int   val;
    int            offset = 0;
    bool           ret = false;

    val  = es_data[0] << 16;
    val |= es_data[1] << 8;
    val |= es_data[2];

    offset += 3;
    while(offset < len)
    {
        while(!((val & 0x00ffffff) == 0x000001))
        {
            val <<= 8;
            val |= es_data[offset];
            offset++;
            if (offset >= len)
                goto EXIT;
        }
        if(es_data[offset] == 0x9)
        {
            //printf("Found AUD: %x %x %x %x %x %x %x %x\n", val, es_data[0], es_data[1], es_data[2], es_data[3], es_data[4], es_data[5], es_data[6]);
            //printf("Found AUD: %x %x %x %x %x\n", val, es_data[offset-3], es_data[offset-2], es_data[offset-1], es_data[offset]);
            ret = true;
            break;
        }
        val <<= 8;
        val |= es_data[offset];
        offset++;
    }
EXIT:
    return ret;
}

int H264GetFrameType(unsigned char *es_data, int len)
{
    unsigned int   val;
    int            offset = 0;
    int            frame_type = VIDF_TYPE_UNDEFINED;
    int            tentative_frame_type = VIDF_TYPE_UNDEFINED;

    val  = es_data[0] << 16;
    val |= es_data[1] << 8;
    val |= es_data[2];

    offset += 3;
    while(offset < len && (frame_type == VIDF_TYPE_UNDEFINED))
    {
        while(!((val & 0x00ffffff) == 0x000001))
        {
            val <<= 8;
            val |= es_data[offset];
            offset++;
            // offset+1 as that byte is also getting accessed below.
            if ((offset+1) >= len)
                goto EXIT;
        }
        if(es_data[offset] == 0x9)
        {
            int primary_picture_type = ((es_data[offset+1] & 0xe0) >> 5);
            /* Check primary picture type */
            if(primary_picture_type == 0)
            {
                tentative_frame_type = VIDF_TYPE_IDR;
            }
            /*else if(primary_picture_type == 1)
            {
                // It was noticed that sometimes primary_picture_type of 1 is given for IDR frame also. So ignoring
                // primary_picture_type of 1
                frame_type = VIDF_TYPE_P;
            }*/
            else if(primary_picture_type == 2)
            {
                tentative_frame_type = VIDF_TYPE_B;
            }
        }
        if(es_data[offset] == 0x1)
        {
            frame_type = VIDF_TYPE_B;
        }
        else if(es_data[offset] == 0x21 || es_data[offset] == 0x41 ||
            es_data[offset] == 0x61)
        {
            frame_type = VIDF_TYPE_P;
        }
        else if(es_data[offset] == 0x25 || es_data[offset] == 0x45 ||
                es_data[offset] == 0x65)
        {
            frame_type = VIDF_TYPE_IDR;
        }
        else
        {
            /* Continue searching for next NAL */
            val <<= 8;
            val |= es_data[offset];
            offset++;
        }
    }
EXIT:
    /* frame_type will be initialized if a coded slice is seen in es_data buffer. We will use the same.
       In case no coded slice is seen  in es_data buffer, this can only happen in case of large SPS, PPS or
       SEI data, we will take what the access unit delimiter says. This modification is done because when
       primary_picture_type is 0 it is not guaranteed that the frame is IDR, it could be I frame. Also this is
       a shortcut to avoid accumulating multiple ts packets to greate es_data which ould guarantee availability
       of coded slice data*/
    if(frame_type == VIDF_TYPE_UNDEFINED)
    {
        if(tentative_frame_type != VIDF_TYPE_UNDEFINED)
        {
            frame_type = tentative_frame_type;
        }
    }
    return frame_type;
}


int GetNextIDRPts (uint8_t *buf, int buf_size, int vid_pid, uint64_t *video_pts, int *used_size) 
{

    int       curr_offset = 0;
    int       pid = 0;
    uint64_t  pts_val;
    int       frame_type = VIDF_TYPE_UNDEFINED;
    int       func_ret_val = 0;
    bool      ret_val = false;
    uint8_t   *ts_buf = buf;

    static bool found_f_type = false;
    static bool found_unit_start = false;
    

    while((buf_size-curr_offset) >= TS_SIZE)
    {
        pid = ts_get_pid(ts_buf);
        //printf ("pid=%d\n", pid);
        if(pid == vid_pid)
        {
            unsigned char *pes;

            pes = ts_payload(ts_buf);

            /* H264CheckAccessUnitDelim check was introduced since for some ts files it was noticed
             * that multiple pes payload were used to represent a single video frame of data. These
             * pes payloads corresponded to different slices within the same frame. This check
             * addition ensures that we always pick the ts packet having Access Unit Delimiter. This
             * works under the assumption that Access Unit Delimiter is present for H264 content
             * for any ts file. This assumption has been observed to be true in general.
             */
            if(ts_get_unitstart(ts_buf))// && H264CheckAccessUnitDelim(pes_payload(pes), (int)((ts_buf + TS_SIZE) - pes)))
            {
                pts_val = pes_get_pts(pes);

                pes_store_buf_index_ = 0;
                ret_val = GetPesData(ts_buf, pes_store_buf_, pes_store_buf_index_);
                if(ret_val != 0 )  
                    fprintf (stderr, "GetPesData failed returned %d.\n", ret_val);

                if(pes_store_buf_index_ > 0)
                {
                    frame_type = H264GetFrameType(pes_store_buf_, pes_store_buf_index_);
                }
                found_unit_start = true;
                found_f_type = false;
            }

            // Accumulate Video PES till we get frame_type
            else if (found_f_type == false)
            {
                pid = ts_get_pid(ts_buf);
                if(pid == vid_pid)
                {
                    ret_val = GetPesData(ts_buf, pes_store_buf_, pes_store_buf_index_);
                    if(ret_val != 0 )  
                        fprintf (stderr, "GetPesData failed returned %d.\n", ret_val);
                    if(pes_store_buf_index_ > 0)
                    {
                        frame_type = H264GetFrameType(pes_store_buf_, pes_store_buf_index_);
                    }
                    else
                    {
                        fprintf (stderr, "GetPesData could not get payload data.\n");
                        goto EXIT;
                    }
                }
            }
            if(frame_type != VIDF_TYPE_UNDEFINED)
            {
                *video_pts = pts_val/90;
                if (frame_type == VIDF_TYPE_IDR)
                    printf ("Got frame_type=%d, pts=%lu\n", frame_type, pts_val/90);
                ret_val = true;
                found_f_type = true;
                goto EXIT;
            }
        }
        ts_buf += 188;
        curr_offset += 188;
    }
    *used_size += curr_offset;
    return frame_type;
EXIT:
    *used_size += (curr_offset + TS_SIZE);
    return frame_type;
}

#define READ_BUF_SZ (TS_SIZE*100)

int main (int argc, char **argv)
{
    if (argc != 4)
    {
        printf (USAGE, argv[0]);
        return 1;
    }

    int ret_val = 0;
    int ret;
    FILE *inp_file;
    uint8_t *buf = (uint8_t*)malloc (READ_BUF_SZ * sizeof(uint8_t));
    int actual_read_size = 0;
    int vid_pid = atoi(argv[2]);
    int max_idr_interval = atoi(argv[3]);
    uint64_t video_pts = 0;
    uint64_t last_idr_pts = 0;
    int used_size = 0;

    int l=0;

    inp_file = fopen (argv[1], "rb");
    
    pes_store_buf_ = (uint8_t*)malloc (MAX_PES_STORE_SIZE * sizeof(uint8_t));

    while ((actual_read_size = fread (buf,1, READ_BUF_SZ, inp_file)) > 0)
    {
        if (actual_read_size> 0)
        {
            used_size = 0;
            while (used_size < actual_read_size)
            {
                ret = GetNextIDRPts (buf+used_size, actual_read_size-used_size, vid_pid, &video_pts, &used_size);

                if (ret != VIDF_TYPE_UNDEFINED)
                {
                    if (ret == VIDF_TYPE_IDR)
                    {
                        printf ("==Got idr:%d %lu\n", ret, video_pts);
                        last_idr_pts = video_pts;
                    }

                    if ((last_idr_pts != 0) && 
                            ((video_pts-last_idr_pts) > max_idr_interval))
                    {
                        fprintf (stderr, 
                                "IDR interval is too high in %s. Max limit:%d, Diff:%d, last_idr:%lu, cur_vid_pts:%lu\n",
                                argv[1], max_idr_interval, (video_pts-last_idr_pts), last_idr_pts, video_pts);
                        ret_val = 1;
                        goto EXIT;
                    }
                }
            }
        }
        l+=actual_read_size;
        if (feof(inp_file))
        {
            fprintf (stdout, "File read completed\n");
            break;
        }
    }
    fclose (inp_file);

EXIT:
    return ret_val;
}
