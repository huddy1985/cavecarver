#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <asm/unistd.h>
#include <linux/highmem.h>
#include <linux/version.h>
#include <asm/cacheflush.h>
#include <asm/debug-monitors.h>
#include <asm/fixmap.h>
#include <asm/opcodes.h>
#include <asm/insn.h>

static void __kprobes *patch_map(void *addr, int fixmap)
{
	unsigned long uintaddr = (uintptr_t) addr;
	bool module = !core_kernel_text(uintaddr);
	struct page *page;

	if (module && IS_ENABLED(CONFIG_DEBUG_SET_MODULE_RONX))
		page = vmalloc_to_page(addr);
	else if (!module)
		page = pfn_to_page(PHYS_PFN(__pa(addr)));
	else
		return addr;

	BUG_ON(!page);
	return (void *)set_fixmap_offset(fixmap, page_to_phys(page) +
			(uintaddr & ~PAGE_MASK));
}

static void __kprobes patch_unmap(int fixmap)
{
	clear_fixmap(fixmap);
}

static DEFINE_RAW_SPINLOCK(patch_lock);

static int __kprobes __aarch64_ptr_write(void *addr, u64 ptr)
{
	void *waddr = addr;
	unsigned long flags = 0;
	int ret;

	raw_spin_lock_irqsave(&patch_lock, flags);
	waddr = patch_map(addr, FIX_TEXT_POKE0);

	ret = probe_kernel_write(waddr, &ptr, sizeof(void*));

	patch_unmap(FIX_TEXT_POKE0);
	raw_spin_unlock_irqrestore(&patch_lock, flags);

	return ret;
}
