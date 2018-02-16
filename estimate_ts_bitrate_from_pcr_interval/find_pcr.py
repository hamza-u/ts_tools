
import re
import sys

packets_since_last_pcr = 0
total_packet_count = 0
current_pcr = []
prev_pcr = []
pcr_pid = int(sys.argv[1])
PID = 0
with sys.stdin as f:
    for line in f:
        m = re.search("PID: ([0-9]+)", line)
        if m:
            PID = int(m.groups()[0])

        m = re.search("TS-Packet: ([0-9]+)", line)
        if m:
            total_packet_count = int(m.groups()[0])

        m = re.search("PCR_flag: ([0-1])", line)
        if m:
            flag = int(m.groups()[0])
            if flag == 1 and PID == pcr_pid:
                print "found PCR after packets ",  packets_since_last_pcr, " current packet num = ", total_packet_count
                packets_since_last_pcr = 0
            else:
                packets_since_last_pcr += 1

        m = re.search("program_clock_reference: (.*)", line)
        if m and PID == pcr_pid:
            print "found pcr %s" % (m.groups()[0])
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
                        (current_pcr[1] - prev_pcr[1]) / 1000)) / (1000*1000)
                    print 'bitrate = %f Mbps, packets_diff = %d, pcr_diff = %f pcr = %f' %\
                        (bitrate, (current_pcr[0] - prev_pcr[0]),
                         (current_pcr[1] - prev_pcr[1]), current_pcr[1]/1000)
