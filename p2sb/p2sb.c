/*
 * Primary to Sideband bridge (P2SB) driver
 *
 * Copyright (c) 2016, Intel Corporation.
 *
 * Authors: Andy Shevchenko <andriy.shevchenko@linux.intel.com>
 *			Jonathan Yong <jonathan.yong@intel.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/unistd.h>

#include "p2sb.h"

#define LOG_PATH (1<<3)

static int dbgflags = LOG_PATH;
module_param(dbgflags, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(dbgflags, "logging flags, 1:name logging");

#define SBREG_BAR	0x10
#define SBREG_HIDE	0xe1

//static DEFINE_SPINLOCK(p2sb_spinlock);

/*
dmesg  | grep MC
[    0.000000] ACPI: MCFG 0x00000000D7FF0000 00003C (v01 LENOVO TP-N1C   00001050 PTEC 00000002)
[    0.036148] mce: CPU supports 8 MCE banks
[    0.137842] PCI: MMCONFIG for domain 0000 [bus 00-3f] at [mem 0xf8000000-0xfbffffff] (base 0xf8000000)
[    0.137844] PCI: MMCONFIG at [mem 0xf8000000-0xfbffffff] reserved in E820


    For enablling DCI you need do following:
1. Get PCI configuration space (from dev 0 reg 0x60) PCIBASE
   (generally 0xfe000000 for linux)

2. Write to PCIBASE+0xf9000+0x10 (P2SB reg BAR0)
   any address (for example 0xfd000000) P2SBBASE

3. Write to P2SBBASE+0xB80004 dword 0x10

4. Read dword from P2SBBASE+0xB80004 and 4-bit must be set (DCI activated!)


*/



#define PCI_IDSEL_P2SB	0x0d

int *mmconfig = 0;

static int p2sb_init(void) {
    int i;

    mmconfig = ioremap_nocache(0xfe000000, 0xf9000 * 2);
    printk(KERN_INFO "[+] p2sb enable dci, 0x%p\n", mmconfig);

    for (i = 0; i < 0xf9000/4; i++) {
        unsigned int a = ioread32(&mmconfig[i]);
        if (a != 0xffffffff) {
            printk("0x%x: 0x%08x\n", i, a);
        }
    }

    return 0;
}

static void p2sb_exit(void) {

    printk(KERN_INFO "p2sb module exit\n");
    if (mmconfig)
        iounmap(mmconfig);
    return;
}

module_init(p2sb_init);
module_exit(p2sb_exit);


MODULE_LICENSE("GPL");

/*
  Local Variables:
  compile-command:"gcc mem.c -o mem"
  mode:c++
  c-basic-offset:4
  c-file-style:"bsd"
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
