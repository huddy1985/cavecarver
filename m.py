#!/usr/bin/env python

import re
import sys, os

def print_memory_of_pid(pid, only_writable=False):
    """
    Run as root, take an integer PID and return the contents of memory to STDOUT
    """
    memory_permissions = 'rw' if only_writable else 'r-'
    sys.stderr.write("PID = %d" % pid)
    with open("/proc/%d/maps" % pid, 'r') as maps_file:
        with open("/proc/%d/mem" % pid, 'r', 0) as mem_file:
            for line in maps_file.readlines():  # for each mapped region
                m = re.match(r'([0-9A-Fa-f]+)-([0-9A-Fa-f]+) ([-r][-w])', line)
                if 1: #m.group(3) == memory_permissions:
                    
                    #sys.stderr.write("\nOK : \n" + line+"\n")
                    start = int(m.group(1), 16)
                    if start > 0xFFFFFFFFFFFF:
                        continue
                    f = os.open(('data0x%08x.data' %(start)), os.O_CREAT|os.O_TRUNC|os.O_WRONLY)
                    end = int(m.group(2), 16)
                    sys.stderr.write( "start = 0x%x - 0x%x" %(start,end) +  "\n")
                    mem_file.seek(start)  # seek to region start
                    try:
                        chunk = mem_file.read(end - start)  # read region contents
                        if (chunk.find("Diff_C") != -1):
                            sys.stderr.write(("Found Diff_C 0x%x\n" %(start)));
                        pass
                    except Exception as e:
                        sys.stderr.write(("Error 0x%x\n" %(start)) + str(e));
                    finally:
                        os.write(f, chunk);
                        
                    os.close(f);
                    #print chunk,  # dump contents to standard output
                else:
                    sys.stderr.write("\nPASS : \n" + line+"\n")

if __name__ == '__main__': # Execute this code when run from the commandline.
    try:
        assert len(sys.argv) == 2, "Provide exactly 1 PID (process ID)"
        pid = int(sys.argv[1])
        print_memory_of_pid(pid)
    except (AssertionError, ValueError) as e:
        print "Please provide 1 PID as a commandline argument."
        print "You entered: %s" % ' '.join(sys.argv)
        raise e
