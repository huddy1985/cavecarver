/home/eiselekd/bin/bin/qemu-system-x86_64 -hda /home/eiselekd/qemu/mlinux.img -m 1024 -net nic -net user -rtc clock=vm,base=1999-12-31 -enable-kvm -qmp unix:/tmp/socket,server,nowait

