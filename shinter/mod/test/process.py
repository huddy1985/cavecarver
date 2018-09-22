import re, sys, argparse, os

entry_re = re.compile("([0-9]+)\.([0-9]+):([0-9]+\.[0-9]+):(.*)");
fork_re = re.compile("fork ([0-9]+)->([0-9]+)");
free_re = re.compile("free ([0-9]+))");

LOG_NAME = (1<<0)
LOG_ARG  = (1<<1)
LOG_ENV  = (1<<2)
LOG_PATH = (1<<3)
LOG_PID  = (1<<4)
LOG_FORK = (1<<5)
LOG_CONT = (1<<8)

class pidobj(object):
    def __init__(self,ppid,pid,idx):
        if not (ppid is None):
            self.childs.append(self)
        self.idx = idx
        self._ppid = ppid
        self._pid = pid
        self.args = []
        self.env = []
        self.execve = None
        self.cwd = None
        self.outputted = False
        self.childs = []

    def close(self):
        self.output();
        for c in self.childs:
            c.close()

    def output(self):
        if self.outputted:
            return
        p = self.getFilePath()
        try:
            os.makedirs(p)
        except:
            pass
        p = os.path.join(p,"cmd.sh")
        with open(p,"w") as f:
            self.writeenv(f);
            self.writecmd(f);
        self.outputted = True

    def getFileName(self):
        return self.execve.replace("/","_")
    def getFilePath(self):
        p = ("06%d_%d" %(self.idx, self.getFileName()))
        if not (self._ppid is None):
            p = os.path.join(self._ppid.getFilePath(),p)
        return p
    def setcwd(self,p):
        self.cwd = p
    def execve(self,n):
        self.execve = n
    def addarg(self,e):
        self.args.append(e)
    def addenv(self,e):
        self.env.append(e)

    def writeenv(self,f):
        f.write("#/bin/bash -e\n")
        env = []
        m_re = re.compile("([A-Za-z_0-9]+)=(.*)");
        for e in self.env:
            m = m_re.match(e)
            if (m):
                (n,v) = m.groups()
                f.write("export {}=\"{}\"\n".format(n,v))
            else:
                pass

    def writecmd(self,f):
        if not (self.cwd is None):
            f.write("cd "+ self.cwd+"\n")
        f.write(self.execve + " " +  " ".join(["'{}'".format(a) for a in self.args])+"\n")

class process(object):
    def __init__(self,i,opt):
        self.idx = 0
        self._i = i
        self._opt = opt
        self._prev = None
        self.pid = {}
        self.pids = []

    def write(self):
        for f in self.pids:
            f.output();

    def getpid(self, pid):
        if pid in self.pid:
            return self.pid[pid]
        return None

    def closepid(self,pid):
        if pid in self.pid:
            pobj = self.pid[pid]
            pobj.close();

    def openpid(self, ppid, pid):
        pobj = None
        if ppid in self.pid:
            pobj = self.pid[ppid]
        if pid in self.pid:
            self.pid[pid] = None
        o = pidobj(pobj, pid, self.idx)
        self.pids.appened(o)
        self.pid[pid] = o
        self.idx += 1#

    def run(self):
        prev = -1
        prevmsg = ""
        for l in self._i:
            m = entry_re.match(l)
            if not m:
                continue
            (typ,pid,stamp,rest) = m.groups();
            self.pushpart(int(typ),int(pid),float(stamp),rest)
        self.flush()

    def pushpart(self, typ, pid, stamp, rest):
        if self._prev is None:
            pass
        else:
            (prev_typ, prev_pid, prev_stamp,prev_rest) = self._prev
            if (typ & LOG_CONT):
                if (prev_typ == (typ & ~(LOG_CONT))):
                    self._prev = (prev_typ,prev_pid, prev_stamp, prev_rest+rest)
                    return
                else:
                    pass # error
            else:
                self.flush()
        self._prev = (typ, pid ,stamp, rest)

    def flush(self):
        if self._prev is None:
            return
        (typ,pid,stamp,rest) = self._prev
        pid = self.getpid(pid)
        self._prev = None
        if (typ == LOG_NAME and pid):
            pid.execve(rest)
        elif (typ == LOG_ARG and pid):
            pid.addarg(rest)
        elif (typ == LOG_ENV and pid):
            pid.addenv(rest)
        elif (typ == LOG_PATH and pid):
            pid.setcwd(rest)
        elif (typ == LOG_PID):
            pass
        elif (typ == LOG_FORK):
            m0 = fork_re.match(rest);
            m1 = free_re.match(rest);
            if (m0):
                (ppid, pid) = m0.groups()
                self.openpid(ppid, pid)
            elif(m1):
                (pid) = m1.groups()
                self.closepid(pid)



def main():

    parser = argparse.ArgumentParser(prog='repoto')
    parser.add_argument('--verbose', action='store_true', help='verbose')
    parser.add_argument('--log', type=str, default=None, help='logfile')
    parser.add_argument('--input', '-i', type=str, default=None, help='inputfile')
    opt = parser.parse_args()

    i = sys.stdin
    if not (opt.input is None):
        i = open(opt.input,"r")
    p = process(i,opt);
    p.run()
    p.write()



if __name__ == "__main__":
    main()
