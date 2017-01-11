#rm -rf linux-3.19.0

#git://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/xenial
#git://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/yakkety
#apt-get source linux-image-4.8.0-22-generic
#fakeroot debian/rules editconfigs

#amd64/config.flavour.generic
#amd64/config.flavour.lowlatency
#chmod a+x linux-4.8.0/debian/scripts/misc/splitconfig.pl

apt-get source linux-image-`uname -r`
cd linux-4.8.0/; \
fakeroot debian/rules clean; \
DEB_BUILD_OPTIONS=parallel=4 AUTOBUILD=1 NOEXTRAS=1 fakeroot debian/rules binary-generic binary

#export DEB_BUILD_OPTIONS="debug nostrip noopt"
#dpkg-buildpackage -rfakeroot -b

# errata:
# sudo apt-get build-dep linux-image-$(uname -r)
# sudo apt-get install  libcurses5-dev
# fakeroot debian/rules editconfigs
# chmod a+rwx /home/eiselekd/git/cavecarver/ubuntu/linux-4.4.0/debian/scripts/misc/splitconfig.pl
# /home/eiselekd/git/cavecarver/ubuntu/linux-4.4.0/debian.master/config/annotations
# remove strictmem annotatioon

