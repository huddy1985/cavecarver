/home/eiselekd/bin/bin/qemu-system-x86_64 -hda /home/eiselekd/qemu/mlinux.img -m 1024 -net nic -net user -rtc clock=vm,base=1999-12-31 -enable-kvm -qmp unix:/tmp/socket,server,nowait & sleep 5; echo \
'{"execute":"qmp_capabilities"}'\
'{"execute":"qom-get","arguments":{"path":"/machine/unattached/device[0]","property":"feature-words"},"id":"feature-words"}'\
'{"execute":"qom-get","arguments":{"path":"/machine/unattached/device[0]","property":"family"},"id":"family"}' \
'{"execute":"qom-get","arguments":{"path":"/machine/unattached/device[0]","property":"model"},"id":"model"}' \
'{"execute":"qom-get","arguments":{"path":"/machine/unattached/device[0]","property":"stepping"},"id":"stepping"}'\
'{"execute":"qom-get","arguments":{"path":"/machine/unattached/device[0]","property":"model-id"},"id":"model-id"}'\
'{"execute":"qom-get","arguments":{"path":"/machine/unattached/device[0]","property":"tsc-frequency"},"id":"tsc-frequency"}'\
'{"execute":"quit"}' | nc -U /tmp/socket
