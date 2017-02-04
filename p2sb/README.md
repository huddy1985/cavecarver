*** Enable DCI on series 6 Skylake CPUs

   Reference : http://de.slideshare.net/phdays/tapping-into-the-core

   Maxim: For enabling DCI you need do following:

   1. Get PCI configuration space (from dev 0 reg 0x60) PCIBASE
      (generally 0xfe000000 for linux)
   2. Write to PCIBASE+0xf9000+0x10 (P2SB reg BAR0)
      any address (for example 0xfd000000) P2SBBASE
   3. Write to P2SBBASE+0xB80004 dword 0x10
   4. Read dword from P2SBBASE+0xB80004 and 4-bit must be set (DCI activated!)
