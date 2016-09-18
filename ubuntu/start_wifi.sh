#!/bin/bash
# Workaround
sudo nmcli radio wifi off
sudo rfkill unblock wlan
sudo ifconfig wlxb0b2dcd9e50c 10.0.0.1
sleep 1
sudo service hostapd restart
sudo service dnsmasq restart
# Routing
sudo sysctl net.ipv4.ip_forward=1
# NAT
sudo iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
sudo hostapd /etc/default/hostapd
# Wait for STRG+C
# Restore NAT
sudo iptables -D POSTROUTING -t nat -o eth0 -j MASQUERADE
# Restore Routing
sudo sysctl net.ipv4.ip_forward=0
# Stop Services
sudo service dnsmasq stop
sudo service hostapd stop
