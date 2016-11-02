mod/interpose.ko
================

x86_64 kernel module that hooks into syscall execve and exposes [bin,cwd,argp,envp] at /sys/kernel/debug/interpose/execve.

Symbol sys_call_table in interpose.h has to be extracted from /boot/System.map using "make get".


