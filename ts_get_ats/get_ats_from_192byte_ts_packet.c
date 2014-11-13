
/*****************************************************************************/
/* This file checks if the given input stream is in 192 byte TS packet format */
/* If so, then prints the datarate from 4 byte timecode embedded              */
/******************************************************************************/



/*****************************************************************************/
/*  System header files                                                      */
/*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
int sync_to_ts_packet_boundary(char *buf)
{
    int i;

    for (i=0; i<192; i++)
    {
        if (buf[i] == 0x47)
        {
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
    if (r != 192 || sync[0] != 0x47)
    {
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
	FILE *fp=NULL;
	FILE *fp_write=NULL;
	int r=0;
	int i=0;
	int ts_sync_offset=0;
	char local_buf[192];
    int ats_value = 0;
    int ats_value_old = 0;
    int ats_count = 0;
    float data_rate;
    int u4_byte;


	if(3 > argc) {
		printf ("Error: syntax - ./a.out <input_file> <ats count>\n");
        goto EXIT_FAIL;
	}
    
    ats_count = atoi(argv[2]);

    fp = fopen(argv[1],"rb");
    if (fp == NULL)
    {
        printf ("%s File open failed\n",argv[2]);
        goto EXIT_FAIL;
    }

    fp_write = fopen("output.ts","wb");
    if (fp == NULL)
    {
        printf ("output.ts File open failed\n");
        goto EXIT_FAIL;
    }

    while(1)
    {
        r = fread(local_buf, sizeof(char), 192, fp);
        if (r != 192)
        {
            printf ("File read : Read less %d/%d\n", r, 192);
            goto EXIT_FAIL;
        }
        ts_sync_offset = sync_to_ts_packet_boundary(local_buf);
        if (ts_sync_offset != -1)
        {
            ts_sync_offset = i+ts_sync_offset;
            break;
        }
        i = i+r;
    }

    printf ("offset = %d\n", ts_sync_offset);

    r = is_192_byte_packet(fp, ts_sync_offset);
    if (r == -1)
    {
        printf ("Input file doesn't have TS packets in 192 byte format.\n");
        goto EXIT_FAIL;
    }
    else
    {
        printf ("Input file has TS packets in 192 byte format.\n");
    }

    printf("Printing ATS values\n");
    fseek(fp, ts_sync_offset-4, SEEK_SET);
    for (i=0; i<ats_count; i++)
    {
        r = fread(local_buf, sizeof(char), 192, fp);
        if (r != 192)
        {
            printf ("File read : Read less %d/%d\n", r, 192);
            goto EXIT_FAIL;
        }

        fwrite (local_buf+4, 1, 188, fp_write);

        ats_value = 0;

        ats_value = ((unsigned char)local_buf[0] << 24) + ((unsigned char)local_buf[1] << 16) + 
                ((unsigned char)local_buf[2] << 8) + (unsigned char)local_buf[3];

        data_rate = (188*8*27)/(float)(ats_value-ats_value_old);
        printf("ATS %d = %d, data rate = %f\n", i+1, ats_value, data_rate);
        ats_value_old = ats_value;
    }

    if (fp != NULL)
        fclose(fp);
    return 1;

EXIT_FAIL:
    printf ("\nReturning with Failure :( \n");
    if (fp != NULL)
        fclose(fp);
    if (fp_write != NULL)
        fclose(fp_write);

    return 0;
}
