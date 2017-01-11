obj-m += p2sb.o

all:
	unset ARCH; unset CROSS_COMPILE; make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	unset ARCH; unset CROSS_COMPILE; make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

