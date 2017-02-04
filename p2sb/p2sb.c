/* public domain src */
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <asm/unistd.h>
#include <linux/debugfs.h>

static struct dentry *dirret = 0;
/*
  usage:

   $insmod ./p2sb.ko PCI_BASE=0xe0000000 P2SB_BASE=0xfc000000
   $echo 1 > /sys/kernel/debug/dci/enable

   Gigabyte brix configuration space:

   $ dmesg  | grep MC
     [    0.000000] ACPI: MCFG 0x0000000087803BA8 00003C (v01 ALASKA A M I    01072009 MSFT 00000097)
     [    0.035393] mce: CPU supports 8 MCE banks
     [    0.132265] PCI: MMCONFIG for domain 0000 [bus 00-ff] at [mem 0xe0000000-0xefffffff] (base 0xe0000000)
     [    0.132267] PCI: MMCONFIG at [mem 0xe0000000-0xefffffff] reserved in E820

   Lenovo t460s configuration space:

   $dmesg  | grep MC
     [    0.000000] ACPI: MCFG 0x00000000D7FF0000 00003C (v01 LENOVO TP-N1C   00001050 PTEC 00000002)
     [    0.036148] mce: CPU supports 8 MCE banks
     [    0.137842] PCI: MMCONFIG for domain 0000 [bus 00-3f] at [mem 0xf8000000-0xfbffffff] (base 0xf8000000)
     [    0.137844] PCI: MMCONFIG at [mem 0xf8000000-0xfbffffff] reserved in E820

   For enabling DCI you need do following:

   1. Get PCI configuration space (from dev 0 reg 0x60) PCIBASE
      (generally 0xfe000000 for linux)
   2. Write to PCIBASE+0xf9000+0x10 (P2SB reg BAR0)
      any address (for example 0xfd000000) P2SBBASE
   3. Write to P2SBBASE+0xB80004 dword 0x10
   4. Read dword from P2SBBASE+0xB80004 and 4-bit must be set (DCI activated!)
*/

static unsigned int PCI_BASE  = 0xe0000000;
module_param(PCI_BASE, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(PCI_BASE, "Configuration space address, i.e. 0xf8000000 on t460s, 0xe0000000 on brix");
static unsigned int P2SB_BASE   = 0xfc000000;
module_param(P2SB_BASE, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(P2SB_BASE, "The address to assign to p2sb, free mmspace to use to access p2sb, i.e. 0xfc000000 or 0xfd000000, use /proc/iomem to determine");

#define PCI_IDSEL_P2SB	((0x1f<<15)+(1<<12)) /* 0xf9000 */
#define PCI_BAR0         0x10
#define RANGE            0x20000

static unsigned char *mmconfig = 0, *p2sbbase = 0;

static int p2sb_dci_onoff(int f) {
    unsigned int pre, post;
    int i; (void)i;

    /******************************************/
    /* initalize p2sb bar0 to P2SB_BASE param */
    mmconfig = ioremap_nocache(PCI_BASE, 0xf9000 * 2);
    printk(KERN_INFO "[+] p2sb bar init, 0x%p+0x%x <= 0x%x\n", mmconfig, PCI_IDSEL_P2SB+PCI_BAR0, P2SB_BASE);

    iowrite32(P2SB_BASE, mmconfig+PCI_IDSEL_P2SB+PCI_BAR0);

    if (mmconfig) {
        iounmap(mmconfig);
        mmconfig = 0;
    }

    /******************************************/
    /* Set dci enable bit                     */
    p2sbbase = ioremap_nocache(P2SB_BASE+0xB80000, 4096);

    pre = ioread32(p2sbbase+4);
    iowrite32(f ? 0x10 : 0, p2sbbase+4 );
    post = ioread32(p2sbbase+4);

    printk(KERN_INFO "[+] p2sb dci enable, 0x%x => 0x%x\n", pre, post);
    if (post & 0x10) {
        printk(KERN_INFO "[+] dci enabled\n");
    } else {
        printk(KERN_INFO "[-] dci disabled\n");
    }

    if (p2sbbase) {
        iounmap(p2sbbase);
        p2sbbase = 0;
    }
    return 0;
}

/******************************************************/
/*           /sys/kernel/debug/dci/enable             */

#ifdef CONFIG_DEBUG_FS
static int dci_status(void *data, u64 value)
{
    p2sb_dci_onoff(value ? 0x10 : 0);
    return 0;
}
DEFINE_SIMPLE_ATTRIBUTE(fops_debug, NULL, dci_status, "%llu\n");
#endif
/******************************************************/

static int p2sb_init(void) {

    printk(KERN_INFO "[+] p2sb module enter\n");

#ifdef CONFIG_DEBUG_FS
    dirret = debugfs_create_dir("dci", NULL);
    if (dirret)
        debugfs_create_file("enable", 0777, dirret, NULL, &fops_debug);
#else
    p2sb_dci_onoff(0x10);
#endif
    return 0;
}

static void p2sb_exit(void) {

    printk(KERN_INFO "[+] p2sb module exit\n");
    if (mmconfig)
        iounmap(mmconfig);
    if (p2sbbase)
        iounmap(p2sbbase);

    if (dirret)
        debugfs_remove_recursive(dirret);
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
