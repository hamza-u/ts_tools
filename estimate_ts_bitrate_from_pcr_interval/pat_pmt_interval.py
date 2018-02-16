
import re
import sys

packets_since_last_pcr = 0
total_packet_count = 0
current_pcr = [0, 0]
prev_pcr = [0, 0]
pcr_pid = int(sys.argv[1])
pmt_pid = int(sys.argv[2])
PID = -1
last_pat_timestamp = 0
last_pmt_timestamp = 0

with sys.stdin as f:
    for line in f:
        # Get PID
        m = re.search("PID: ([0-9]+)", line)
        if m:
            PID = int(m.groups()[0])

        # Parse PCR
        m = re.search("program_clock_reference: (.*)", line)
        if m and PID == pcr_pid:
            # print "found pcr %s" %(m.groups()[0])
            m_pcr = re.search(
                "PCR-Timestamp: ([0-9]{1,2}):([0-9]{2}):([0-9]{2}).([0-9]{6})", line)
            if m_pcr:
                m_pcr = m_pcr.groups()
                prev_pcr = current_pcr
                current_pcr = [total_packet_count,
                               (int((float(m_pcr[0])*3600+float(m_pcr[1])*60+float(m_pcr[2]))*1000) + float(m_pcr[3])/1000)]

                # print prev_pcr, current_pcr
                if prev_pcr:
                    bitrate = (((current_pcr[0] - prev_pcr[0]) * 8 * 188) / (
                        (current_pcr[1] - prev_pcr[1]) / 1000)) / (1024*1024)
                    # print 'bitrate = %f Mbps, packets_diff = %d, pcr_diff = %f' %\
                    #    (bitrate, (current_pcr[0] - prev_pcr[0]), (current_pcr[1] - prev_pcr[1]) * 1000)
            PID = -1

        # Print PAT Interval
        if PID == 0:
            print "PAT interval = %f ms" % (current_pcr[1]-last_pat_timestamp)
            last_pat_timestamp = current_pcr[1]
            PID = -1
        # Print PMT Interval
        if PID == pmt_pid:
            print "PMT interval = %f ms" % (current_pcr[1]-last_pmt_timestamp)
            last_pmt_timestamp = current_pcr[1]
            PID = -1
