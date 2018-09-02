all: re

o:
	gcc -I $(CURDIR)/../../bin/include -ggdb -Wl,--start-group -Wall -Wextra -o rsa rsa_open.c -L $(CURDIR)/../../bin/lib  -lssl -lcrypto  -lpthread -ldl -Wl,--end-group


re:
	g++ reassemble.cpp -o reassemble.exe -g

run:
	reassemble.exe --off 0xf7580 libtest.so

download:
	wget http://www.mirrorservice.org/sites/dl.sourceforge.net/pub/sourceforge/g/gn/gns-3/Qemu%20Appliances/linux-tinycore-3.4.img

clean:
	rm -f reassemble.exe


find:
	@id=`ps -A | grep memcheck-amd64- | awk '{print $$1}'`; \
	print "Found $$id"; \
	if [ -z "$${id}" ]; then exit 1; fi; \
	python m.py $$id;
	-for f in `ls *.data`; do \
		if xxd -p $$f | tr -d '\n' | grep -c 'a4063e881916d40eec1b83ada255623f'; then\
			echo $$f; \
		fi; \
	done


di:
	cd distorm; make

vb:
	cd valgrind-gen; bash  autogen.sh; ./configure --prefix=$(HOME)/bin-valgrind --enable-only64bit

vc:
	cd valgrind-gen; make; make install

vr:
	$(HOME)/bin-valgrind/bin/valgrind --tool=tracegrind ls

ve:
	export FLYCHECK_GENERIC_SRC=$(CURDIR)/valgrind-gen; \
	export FLYCHECK_GENERIC_BUILD=$(CURDIR)/valgrind-gen; \
	emacs -nw valgrind-gen/tracegrind

ve_:
	export FLYCHECK_GENERIC_SRC=$(CURDIR)/valgrind-gen; \
	export FLYCHECK_GENERIC_BUILD=$(CURDIR)/valgrind-gen; \
	emacs valgrind-gen/tracegrind

tags:
	-cd valgrind-gen; rm GPATH GRTAGS GTAGS
	cd valgrind-gen; find include VEX/pub VEX coregrind lackey taintgrind  -type f | grep -e '.c$$\|.h$$' | gtags -i -f -

vg-taint-prep:
	cd valgrind-gen/taintgrind; ../autogen.sh; ./configure --prefix=$(HOME)/bin-valgrind; make clean

vg-taint:
	cd valgrind-gen/taintgrind; make; make install





qemu-build-prepare:
	cd qemu-2.11+dfsg; dpkg-buildpackage -b -us -uc

qemu-build:
	cd qemu-2.11+dfsg/qemu-build; make V=1 -j8

qemu-config:
	mkdir -p qemu-2.11+dfsg/qemu-build;
	cd qemu-2.11+dfsg/qemu-build; ../configure '--with-pkgversion=Debian 1:2.11+dfsg-1ubuntu7.4' '--extra-cflags=-g -O2 -fdebug-prefix-map=$(CURDIR)/qemu-2.11+dfsg=. -fstack-protector-strong -Wformat -Werror=format-security -Wdate-time -D_FORTIFY_SOURCE=2 -DCONFIG_QEMU_DATAPATH=\\"/usr/share/qemu:/usr/share/seabios:/usr/lib/ipxe/qemu\\" -DVENDOR_UBUNTU' '--extra-ldflags=-Wl,-Bsymbolic-functions -Wl,-z,relro -Wl,--as-needed' '--prefix=/usr' '--sysconfdir=/etc' '--libdir=/usr/lib/x86_64-linux-gnu' '--libexecdir=/usr/lib/qemu' '--localstatedir=/var' '--disable-blobs' '--disable-strip' '--interp-prefix=/etc/qemu-binfmt/%M' '--localstatedir=/var' '--disable-user' '--enable-system' '--enable-linux-user' '--enable-modules' '--enable-linux-aio' '--audio-drv-list=pa,alsa,oss' '--enable-attr' '--enable-bluez' '--enable-brlapi' '--enable-virtfs' '--enable-cap-ng' '--enable-curl' '--enable-fdt' '--enable-gnutls' '--disable-gtk' '--disable-vte' '--enable-libiscsi' '--enable-curses' '--enable-smartcard' '--enable-rbd' '--enable-rdma' '--enable-vnc-sasl' '--enable-seccomp' '--enable-spice' '--enable-libusb' '--enable-usb-redir' '--enable-xen' '--enable-xfsctl' '--enable-vnc' '--enable-vnc-jpeg' '--enable-vnc-png' '--enable-kvm' '--enable-vhost-net' '--target-list=x86_64-softmmu,x86_64-linux-user' '--enable-debug'

#,nowait
qemu:
	qemu-2.11+dfsg/qemu-build/x86_64-softmmu/qemu-system-x86_64 -qmp tcp:localhost:4444,server -monitor stdio

#,nowait
qemu-qmp-shell-server:
	qemu-2.11+dfsg/qemu-build/x86_64-softmmu/qemu-system-x86_64 -qmp unix:./qmp-sock,server -monitor stdio

qemu-qmp-shell:
	qemu-2.11+dfsg/scripts/qmp/qmp-shell ./qmp-sock



pci:
	cd pcileech/pcileech; make
