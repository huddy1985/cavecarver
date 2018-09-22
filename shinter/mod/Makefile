obj-m += interpose.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

get:
	echo "// require nokasrl option"  >  interpose.h
	echo "#ifndef __inter__header_h_" >> interpose.h
	echo "#define __inter__header_h_" >> interpose.h
	echo "#define SYSCALL_TABLE 0x$(shell sudo cat /boot/System.map-$(shell uname -r) | grep -e '\ssys_call_table' | awk '{ print $$1}' )" >> interpose.h
	echo "#define sys_execve 0x$(shell sudo cat /boot/System.map-$(shell uname -r) | grep -e '\ssys_execve$$' | awk '{ print $$1}' )" >> interpose.h
	echo "#define sys_fork  0x$(shell sudo cat /boot/System.map-$(shell uname -r) | grep -e '\ssys_fork$$' | awk '{ print $$1}' )" >> interpose.h
	echo "#define sys_vfork 0x$(shell sudo cat /boot/System.map-$(shell uname -r) | grep -e '\ssys_vfork$$' | awk '{ print $$1}' )" >> interpose.h
	echo "#define SyS_clone 0x$(shell sudo cat /boot/System.map-$(shell uname -r) | grep -e '\sSyS_clone$$' | awk '{ print $$1}' )" >> interpose.h

	echo "#endif" >> interpose.h
