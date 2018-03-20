import sys, re, os
import shutil
import subprocess
import shlex
import errno
import exceptions

USAGE = "python %s <input filename> <video pid> <max IDR interval allowed in ms> <TS bitrate in bps>"

def run_shell_command(command_str):
    #args=shlex.split(command_str)
    p = subprocess.Popen(command_str, stdout = subprocess.PIPE, stderr= subprocess.PIPE, shell=True)
    out, err = p.communicate()
    exit_code = p.returncode
    #print (exit_code, out.strip(), err.strip())
    return (exit_code, out.strip(), err.strip())



if len (sys.argv) != 5:
    print USAGE %sys.argv[0]
    sys.exit(50)

INP_FILE = sys.argv[1]
VID_PID = int(sys.argv[2])
MAX_IDR_INTERVAL = int(sys.argv[3])
MAX_IDR_BYTE_POSITION = float(sys.argv[4]) * 2 / 8 # 2 seconds allowed.

exit_code,stdout,stderr = run_shell_command("./h264_qc_idr_interval '%s' %d %d" %(INP_FILE, VID_PID, MAX_IDR_INTERVAL))
if exit_code != 0:
    print "h264_qc_idr_interval failed with exit code %d" %exit_code
    sys.stderr.write(stderr+"\n")
    sys.exit(exit_code)
## IDR interbal QC ends. 

## IDR offset QC starts.
offset=[]
pts=[]
deviation=[]

for line in iter(stdout.splitlines()):
    m= re.search (".*idr:1 ([0-9]*) ([0-9]*).*", line)
    if m:
        pts.append (int(m.groups()[0]))
        offset.append (int(m.groups()[1]))

x1=pts[0]
x2=pts[-1]
y1=offset[0]
y2=offset[-1]

#print x1,x2,y1,y2

# y=mx+c
m = float (y2-y1) / float(x2-x1)
c = float (y2) - float (m*x2)

#print m,c
i=0
for x in pts:
    expected_off = float(m)*x + c
    curr_off = offset[i]
    deviation.append (abs((curr_off - expected_off)))
    i=i+1

#deviation.sort (reverse=True)
#print deviation[:10]
max_devition = max (deviation)

if max_devition >= MAX_IDR_BYTE_POSITION:
    sys.stderr.write("Allowed IDR byte position difference = %d, current diff = %d\n" %(MAX_IDR_BYTE_POSITION, max_devition))
    sys.exit(11)

print "Success :: Allowed IDR byte position difference = %d, current diff = %d" %(MAX_IDR_BYTE_POSITION, max_devition)
sys.exit (0)


'''import matplotlib.pyplot as plt
x=np.array(pts)
y=np.array(offset)
plt.plot(x, y, 'o', label='Original data', markersize=10)

plt.plot(x, m*x + c, 'r', label='Fitted line')
plt.legend()
plt.show()
'''
