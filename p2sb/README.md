*** Enable DCI on series 6 Skylake CPUs

   Reference : http://de.slideshare.net/phdays/tapping-into-the-core

   Maxim: For enabling DCI you need do following:

   1. Get PCI configuration space (from dev 0 reg 0x60) PCIBASE
      (generally 0xfe000000 for linux)
   2. Write to PCIBASE+0xf9000+0x10 (P2SB reg BAR0)
      any address (for example 0xfd000000) P2SBBASE
   3. Write to P2SBBASE+0xB80004 dword 0x10
   4. Read dword from P2SBBASE+0xB80004 and 4-bit must be set (DCI activated!)

       You also need OTG USB 3.0 Super-Speed A/A Debugging Cable:
       Gen 1 DbC cable is a standard USB3 Type A to Type A cable but with VBUS and USB2 signals (D+/D-) removed
       i.e.: http://www.datapro.net/products/usb-3-0-super-speed-a-a-debugging-cable.html

To debug the target system you need to also enable "Debug Interface" option in the BIOS via i.e. AMIBCP or
https://twitter.com/h0t_max/status/819237533775175681?s=03 . DCI can be enabled via AMIBCP as well.
