all:
	gcc -I $(CURDIR)/../../bin/include -ggdb -Wl,--start-group -Wall -Wextra -o rsa rsa_open.c -L $(CURDIR)/../../bin/lib  -lssl -lcrypto  -lpthread -ldl -Wl,--end-group
