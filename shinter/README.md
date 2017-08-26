shinterposer
============

Simple script to log and filter commands (i.e. gcc) and save the enviroment (env variables, current working directory and arguments) to be able to later restart the command.

shpreload.so
============

Use LD_PRELOAD to log execution env (env variables, cwd and arguments). Uses static embedded perl interpreter (using http://cvs.schmorp.de/App-Staticperl/bin/staticperl)  and can be therefore easily adjusted. At hte time of writing staticperl used perl-5.12.4.

Howto shinterposer
==================

Replace the command to log with "shinterpose.sh". Rename the original command to a known name, i.e. from "gcc" to "gcc.real" and save the command's new name in "shinterposer.sh"'s "$real" variable. Now when the command to log is called, first "shinterposer.sh" is called. It will save the enviroment and after that call "execve $real ..." to continue with the original command.

The command enviroment is saved to scripts that make it possible to restart the command.

 * SH_INTERPOSER_FILTER : comma seperated list of options to filter out
 * SH_INTERPOSER_LOG    : log command to file

Howto shpreload.so
==================

Download and install staticperl by running "make staticperl". This will create a staticperl dist in $(HOME)/.staticperl.
Then run "make shpreload.so". This compiles shpreload.so. shpreload.so will overlay functions execv, execve. Each time
one of these functions is called the respective logging function in shpreload.pm is called. shpreload.pm logs
execution similar to shinterposer.

addition: mod/interpose.ko
==========================

x86_64 kernel module that hooks into syscall execve and exposes [bin,cwd,argp,envp] at /sys/kernel/debug/interpose/execve.

simple strace version
=====================

strace -f -e trace=execve -v -s 100000 <cmd>

    -f : follow clone
    -e : filter execve
    -v : print env

to get execve printout with cwd path patch <strace>/execve.c:

        function decode_execve(struct tcb *tcp, const unsigned int index)
        {...
         (abbrev(tcp) ? printargc : printargv) (tcp, tcp->u_arg[index + 2]);
        +char b[PATH_MAX]; char fn[PATH_MAX];
        +sprintf(b, "/proc/%d/cwd", tcp->pid);
        +realpath(b,fn);
        +tprintf(", [cwd:%s]", fn);
         ...