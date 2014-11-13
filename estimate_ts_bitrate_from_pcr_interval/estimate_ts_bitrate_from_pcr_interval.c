#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>   /* uint8_t, uint16_t, etc... */
#include "ts.h"

#define TS_PID_TO_IGNORE 8191

typedef enum __ts_packet_type_t {
    BYTE_FORMAT_188 = 188,
    BYTE_FORMAT_192 = 192
}ts_packet_type_t;

int sync_to_ts_packet_boundary(uint8_t *buf)
{
    int i;

    for (i=0; i<192; i++) {
        if (buf[i] == 0x47) {
            return i;
        }
     }

    return -1;
}

int is_192_byte_packet(FILE *fp, int ts_sync_offset)
{
    int r=0;
    char sync[192];
    
    if (ts_sync_offset<4)
        return -1;

    r = fseek(fp, ts_sync_offset, SEEK_SET);
    if (r == -1)
        return -1;

    r = fread(sync, sizeof(char), 192, fp);
    if (r != 192 || sync[0] != 0x47) {
        return -1;
    }

    r = fread(sync, sizeof(char), 192, fp);
    if (r != 192 || sync[0] != 0x47) {
        return -1;
    }

    return 0;
}

static void compute_bitrate(double pcr_diff, int packets_since_last_pcr, 
                                            ts_packet_type_t ts_packet_type) {
    double bitrate;

    bitrate = (packets_since_last_pcr * 8 * 188) / (pcr_diff);

    printf ("Bitrate = %lf Mbps, packets_diff=%d, pcr_diff=%lf\n", bitrate/(1000*1000),
                    packets_since_last_pcr, pcr_diff, ts_packet_type);
}

int main(int argc, char *argv[]) {
	uint8_t local_buf[192];
	FILE *fp=NULL;
	int r=0;
	int i=0;
	int ts_sync_offset=0;
    ts_packet_type_t ts_packet_type;
    int packets_to_analyze;
    int read_bytes;
    int ts_type_offset;
    int packets_since_last_pcr=0;
    uint64_t    ts_pcr;
    uint64_t    ts_pcrext;
    uint64_t    ts_pid;
    uint64_t    pcr_pid;
    double       pcr_in_sec=0;
    double       old_pcr=0;
    int pid_interval=0;

	if(4 > argc) {
		printf ("Error: syntax - ./a.out <input_file> <PCR pid> <TS packets to analyze>\n");
        goto EXIT_FAIL;
	}

    pcr_pid            = strtoull(argv[2], NULL, 10);
    packets_to_analyze = atoi(argv[3]);
    
    // Open TS file for reading.
    fp = fopen(argv[1],"rb");
    if (fp == NULL) {
        printf ("%s File open failed\n",argv[1]);
        goto EXIT_FAIL;
    }

    // Sync to TS packet boundary.
    while(1) {
        r = fread(local_buf, sizeof(uint8_t), 192, fp);
        if (r != 192) {
            printf ("File read : Read less %d/%d\n", r, 192);
            goto EXIT_FAIL;
        }
        ts_sync_offset = sync_to_ts_packet_boundary(local_buf);
        if (ts_sync_offset != -1) {
            ts_sync_offset = i+ts_sync_offset;
            break;
        }
        i = i+r;
    }

    printf ("offset = %d\n", ts_sync_offset);

    // Find TS packet format. 192 bytes or 188 bytes??
    r = is_192_byte_packet(fp, ts_sync_offset);
    if (r == -1) {
        printf ("Input file doesn't have TS packets in 192 byte format.\n");
        ts_packet_type = BYTE_FORMAT_188;
        read_bytes     = 188;
        ts_type_offset = 0;
    } else {
        printf ("Input file has TS packets in 192 byte format.\n");
        ts_packet_type = BYTE_FORMAT_192;
        read_bytes     = 192;
        ts_type_offset = 4;
    }

    /* Seek to TS sync.
       In case of 192 byte format as first four bits are for timecode,
       ts_type_offset is subtracted frmm ts_sync_offset. */
    r = fseek(fp, ts_sync_offset-ts_type_offset, SEEK_SET);
    if (r==-1) {
        printf("SEEK to file start failed.\n");
        goto EXIT_FAIL;
    }

    // Check if a given PID has PCR and find bitrate using it.
    for(i=0;i<packets_to_analyze;i++) {
        r = fread(local_buf, sizeof(uint8_t), read_bytes, fp);
        if (r == 0) {
            printf ("Completed reading file. \n");
            break;
        } else if (r != read_bytes) {
            printf ("File read : Read less %d/%d\n", r, read_bytes);
            goto EXIT_FAIL;
        }

        ts_pid = ts_get_pid(local_buf+ts_type_offset);
/*
        //Follwing blocks monitors how frequent a PID appears in TS.
        {
            pid_interval++;

            if(ts_pid==480) {
                printf ("\t\t\t\t\t\t\t\t PID: % "PRIu16, ts_pid);
                printf(" interval: %d\n", pid_interval);
                pid_interval = 0;
        }
*/
        packets_since_last_pcr++;

        // Check if this packet has PCR.
        if(ts_pid == pcr_pid && 
          ts_has_adaptation(local_buf+ts_type_offset) && 
          tsaf_has_pcr(local_buf+ts_type_offset)) {

            ts_pcr      = tsaf_get_pcr(local_buf+ts_type_offset);
            ts_pcrext   = tsaf_get_pcrext(local_buf+ts_type_offset);

            pcr_in_sec  = (ts_pcr*300 + ts_pcrext) / (double)27000000;

            //printf("PCR: %"PRIu16,  ts_pcr);
            //printf("::%"PRIu16, ts_pcrext);
            //printf(" = %lf secs",  pcr_in_sec);
            //printf("\tPID=%"PRIu16, ts_pid);
            //printf("\t\t\tReceived PCR after %d packets and %f seconds.\n", 
            //            packets_since_last_pcr-1, pcr_in_sec-old_pcr);

            if (old_pcr != 0) {
                compute_bitrate(pcr_in_sec-old_pcr, packets_since_last_pcr, ts_packet_type);
            }
            printf ("PCR=%lf\n", pcr_in_sec);
            old_pcr = pcr_in_sec;
            packets_since_last_pcr = 0;
        }
    }


    if (fp != NULL) {
        fclose(fp);
    }
    return 0;

EXIT_FAIL:
    printf ("\nReturning with Failure :( \n");
    if (fp != NULL) {
        fclose(fp);
    }
    return 1;
}
