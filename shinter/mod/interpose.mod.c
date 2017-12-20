#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x3d6976bf, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x8c6f16c7, __VMLINUX_SYMBOL_STR(no_llseek) },
	{ 0xec1b975b, __VMLINUX_SYMBOL_STR(param_ops_int) },
	{ 0xa30e18f8, __VMLINUX_SYMBOL_STR(pv_cpu_ops) },
	{ 0x7ea1a2bc, __VMLINUX_SYMBOL_STR(probe_kernel_write) },
	{ 0x74e493b7, __VMLINUX_SYMBOL_STR(debugfs_remove) },
	{ 0x87d7c353, __VMLINUX_SYMBOL_STR(debugfs_create_file) },
	{ 0xff7799f8, __VMLINUX_SYMBOL_STR(debugfs_create_dir) },
	{ 0x3356b90b, __VMLINUX_SYMBOL_STR(cpu_tss) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0xd2b09ce5, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0xbcc308bb, __VMLINUX_SYMBOL_STR(strnlen_user) },
	{ 0x6d334118, __VMLINUX_SYMBOL_STR(__get_user_8) },
	{ 0xa6bbd805, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0xa1958cbf, __VMLINUX_SYMBOL_STR(kmem_cache_alloc) },
	{ 0x4f68e5c9, __VMLINUX_SYMBOL_STR(do_gettimeofday) },
	{ 0x27cfea5a, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x7c7bce8, __VMLINUX_SYMBOL_STR(kmem_cache_create) },
	{ 0x9e88526, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0x837701c9, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x18f427aa, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0xb4328ac4, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x4f8b5ddb, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0x8526c35a, __VMLINUX_SYMBOL_STR(remove_wait_queue) },
	{ 0x1000e51, __VMLINUX_SYMBOL_STR(schedule) },
	{ 0xc9fef317, __VMLINUX_SYMBOL_STR(add_wait_queue) },
	{ 0xffd5a395, __VMLINUX_SYMBOL_STR(default_wake_function) },
	{ 0x65c544a3, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0xc7b8fd46, __VMLINUX_SYMBOL_STR(kmem_cache_destroy) },
	{ 0x72c4f890, __VMLINUX_SYMBOL_STR(kmem_cache_free) },
	{ 0x1916e38c, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_irqrestore) },
	{ 0x680ec266, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irqsave) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "235134AE1B048F39B90CABA");
