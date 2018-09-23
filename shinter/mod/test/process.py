import re, sys, argparse, os

entry_re = re.compile("([0-9]+)\.([0-9]+):([0-9]+\.[0-9]+):(.*)");
fork_re = re.compile("fork ([0-9]+)->([0-9]+)");
free_re = re.compile("free ([0-9]+)");
execve_start_re = re.compile("start execve:([0-9]+)\.([0-9]+):(.*)");
execve_stop_re = re.compile("stop execve:([0-9]+)\.([0-9]+):(.*)");

LOG_NAME = (1<<0)
LOG_ARG  = (1<<1)
LOG_ENV  = (1<<2)
LOG_PATH = (1<<3)
LOG_PID  = (1<<4)
LOG_FORK = (1<<5)
LOG_CONT = (1<<8)

class pidobj(object):
    def __init__(self,up,ppid,pid,idx):
        self.up = up
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
    def stop(self):
        pass

    def tryoutput(self):
        pass

    def output(self):
        if self.outputted:
            return
        if (not self.isexecve()) and not self.haschildren():
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
        if self.execve is None:
            return "undef"
        p = os.path.basename(self.execve)
        return p.replace("/","_")
    def getFilePath(self):
        par = self.getparent()
        if (not self.isexecve()) and self.hasparent() and not (par is None): #jump over undef
            return par.getFilePath()
        p = ("%06d_%s" %(self.idx, self.getFileName()))
        par = self.getparent()
        if not (par is None):
            p = os.path.join(par.getFilePath(),p)
        return p
    def setcwd(self,p):
        self.cwd = p
    def setexecve(self,n):
        self.execve = n
    def setparent(self,p):
        self._ppid = p
            
    def isexecve(self):
        return not (self.execve is None)
    
    def getparent(self):
        if (self._ppid is None):
            return None
        return self.up.getpid(self._ppid)
    
    def hasparent(self):
        return not (self._ppid is None)
    def haschildren(self):
        return len(self.childs) > 0
    def checkpids(self,ppid,pid):
        p = self.getparent()
        if not (p is None):
            if not (p._pid == ppid):
                return False
        return (self._pid == pid)
    def addarg(self,e):
        self.args.insert(0, e)
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
        n = "true"
        if not (self.execve is None):
            n = self.execve
        f.write(n + " " +  " ".join(["'{}'".format(a) for a in self.args[1:]])+"\n")

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

    def getpid(self, pid, generate=False):
        if pid in self.pid:
            return self.pid[pid]
        if generate:
            return self.newpid(None, pid)
        return None

    def newpid(self, ppid, pid):
        o = pidobj(self,ppid, pid, self.idx)
        self.pid[pid] = o
        self.pids.append(o)
        self.idx += 1
        return o

    def closepid(self,pid):
        if pid in self.pid:
            pobj = self.pid[pid]
            pobj.close();

    # if the pid is reused via a execv (two in a row) then close the previouse
    def startexecv(self, ppid, pid, cmd):
        o = None
        if pid in self.pid:
            if self.pid[pid].isexecve():
                if (not self.pid[pid].checkpids(ppid,pid)):
                    pass #err
                self.pid[pid].stop()
            else:
                o = self.pid[pid] # reuse pid obj
        if o is None:
            o = self.newpid(ppid, pid)
        self.pid[pid] = o
        o.setexecve(cmd);
        o.setparent(ppid)

    def stopexecv(self, ppid, pid, cmd):
        if not (pid in self.pid):
            self.startexecv(ppid, pid, cmd)
        self.pid[pid].tryoutput()

    def openpid(self, ppid, pid):
        if pid in self.pid:
            pass

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
        pid = self.getpid(pid, generate=True)
        self._prev = None
        if (typ == LOG_NAME and pid):
            m0 = execve_start_re.match(rest)
            m1 = execve_stop_re.match(rest)
            if (m0):
                (eppid,epid,ecmd) = (int(m0.group(1)),int(m0.group(2)),m0.group(3))
                self.startexecv(eppid,epid,ecmd)
            elif (m1):
                (eppid,epid,ecmd) = (int(m1.group(1)),int(m1.group(2)),m1.group(3))
                self.stopexecv(eppid,epid,ecmd);
            else:
                pid.setexecve(rest)
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
                self.openpid(int(ppid), int(pid))
            elif(m1):
                (pid,) = m1.groups()
                self.closepid(int(pid))



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
