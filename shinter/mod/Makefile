obj-m += interpose.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

get:
	echo "#ifndef __inter__header_h_" >  interpose.h
	echo "#define __inter__header_h_" >> interpose.h
	echo "#define SYSCALL_TABLE 0x$(shell sudo cat /boot/System.map-$(shell uname -r) | grep -e '\ssys_call_table' | awk '{ print $$1}' )" >> interpose.h
	echo "#define sys_execve 0x$(shell sudo cat /boot/System.map-$(shell uname -r) | grep -e '\ssys_execve$$' | awk '{ print $$1}' )" >> interpose.h
	echo "#endif" >> interpose.h
