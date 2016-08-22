
linux:
	git clone git://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/xenial ubuntu-linux
	cd ubuntu-linux; fakeroot debian/rules clean
	cd ubuntu-linux; fakeroot debian/rules binary-headers binary-generic skipdbg=false

linux-ver:
	apt-get source linux-headers-`uname -r`
