qemu-system-x86_64  -hda linux-tinycore-3.4.img -m 1024 -net nic -net user -rtc clock=vm,base=1999-12-31 -enable-kvm -qmp unix:/tmp/socket,server,nowait \
		    -machine "kernel_irqchip=off"

#-monitor stdio 
#-machine "kernel_irqchip=on"
#qemu-kvm
