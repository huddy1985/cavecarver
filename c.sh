cryptsetup -c aes-xts-plain -s 512 luksFormat /dev/sdx keyfile

cryptsetup luksOpen /dev/sdx crypt-mapper --key-file  keyfile

cryptsetup luksClose crypt-mapper

cryptsetup luksUUID /dev/sdx

# automount:
# sudo cryptsetup luksAddKey /dev/sdx keyfile
# echo "crypt-mapper UUID=`cryptsetup luksUUID /dev/sdx` keyfile luks" >> /etc/crypttab


sudo mkfs.ext4 /dev/sda3
sudo cryptsetup -c aes-xts-plain -s 256 luksFormat /dev/sda3
sudo cryptsetup luksOpen /dev/sda3 luksroot
sudo mkfs.btrfs /dev/mapper/luksroot
sudo mount -o noatime,nodiratime,ssd,compress=lzo /dev/mapper/luksroot /mnt

sudo cryptsetup luksAddKey /dev/sdX /root/keyfile

