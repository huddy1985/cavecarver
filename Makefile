all:
	gcc -I $(CURDIR)/../../bin/include -ggdb -Wl,--start-group -Wall -Wextra -o rsa rsa_open.c -L $(CURDIR)/../../bin/lib  -lssl -lcrypto  -lpthread -ldl -Wl,--end-group


re:
	g++ reassemble.cpp -o reassemble.exe -g

download:
	wget http://www.mirrorservice.org/sites/dl.sourceforge.net/pub/sourceforge/g/gn/gns-3/Qemu%20Appliances/linux-tinycore-3.4.img
