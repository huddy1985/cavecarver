rm -rf linux-3.19.0

git://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/xenial
git://git.launchpad.net/~ubuntu-kernel/ubuntu/+source/linux/+git/yakkety

apt-get source linux-image-`uname -r`
cd linux-3.19.0/; \
fakeroot debian/rules clean; \
DEB_BUILD_OPTIONS=parallel=4 AUTOBUILD=1 NOEXTRAS=1 fakeroot debian/rules binary-generic binary

export DEB_BUILD_OPTIONS="debug nostrip noopt"
dpkg-buildpackage -rfakeroot -b

