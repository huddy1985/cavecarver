cryptsetup -c aes-xts-plain -s 512 luksFormat /dev/sdx keyfile

cryptsetup luksOpen /dev/sdx crypt-mapper --key-file  keyfile

cryptsetup luksClose crypt-mapper

cryptsetup luksUUID /dev/sdx

# automount:
# sudo cryptsetup luksAddKey /dev/sdx keyfile
# echo "crypt-mapper UUID=`cryptsetup luksUUID /dev/sdx` keyfile luks" >> /etc/crypttab
