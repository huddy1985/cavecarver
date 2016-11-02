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
	{ 0x75999ebb, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xe7868d27, __VMLINUX_SYMBOL_STR(no_llseek) },
	{ 0xd1b4605b, __VMLINUX_SYMBOL_STR(param_ops_int) },
	{ 0xbe5960a0, __VMLINUX_SYMBOL_STR(pv_cpu_ops) },
	{ 0x17fa2d05, __VMLINUX_SYMBOL_STR(sys_call_table) },
	{ 0x216b2c5f, __VMLINUX_SYMBOL_STR(debugfs_remove) },
	{ 0xf8321c83, __VMLINUX_SYMBOL_STR(debugfs_create_file) },
	{ 0xa6693296, __VMLINUX_SYMBOL_STR(debugfs_create_dir) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x4f6b400b, __VMLINUX_SYMBOL_STR(_copy_from_user) },
	{ 0xd2b09ce5, __VMLINUX_SYMBOL_STR(__kmalloc) },
	{ 0xbcc308bb, __VMLINUX_SYMBOL_STR(strnlen_user) },
	{ 0x6d334118, __VMLINUX_SYMBOL_STR(__get_user_8) },
	{ 0xa6bbd805, __VMLINUX_SYMBOL_STR(__wake_up) },
	{ 0xe97595f5, __VMLINUX_SYMBOL_STR(kmem_cache_alloc) },
	{ 0x4f68e5c9, __VMLINUX_SYMBOL_STR(do_gettimeofday) },
	{ 0xdb7305a1, __VMLINUX_SYMBOL_STR(__stack_chk_fail) },
	{ 0x1e047854, __VMLINUX_SYMBOL_STR(warn_slowpath_fmt) },
	{ 0x4f8b5ddb, __VMLINUX_SYMBOL_STR(_copy_to_user) },
	{ 0x88db9f48, __VMLINUX_SYMBOL_STR(__check_object_size) },
	{ 0x28318305, __VMLINUX_SYMBOL_STR(snprintf) },
	{ 0x8526c35a, __VMLINUX_SYMBOL_STR(remove_wait_queue) },
	{ 0x1000e51, __VMLINUX_SYMBOL_STR(schedule) },
	{ 0xc9fef317, __VMLINUX_SYMBOL_STR(add_wait_queue) },
	{ 0xffd5a395, __VMLINUX_SYMBOL_STR(default_wake_function) },
	{ 0xfd0260d8, __VMLINUX_SYMBOL_STR(current_task) },
	{ 0x3576993c, __VMLINUX_SYMBOL_STR(mutex_unlock) },
	{ 0x5774824b, __VMLINUX_SYMBOL_STR(kmem_cache_create) },
	{ 0x9e88526, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0xeaf89948, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0xc67c2e62, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x87af1744, __VMLINUX_SYMBOL_STR(mutex_lock) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x63daa57c, __VMLINUX_SYMBOL_STR(kmem_cache_destroy) },
	{ 0x368779d4, __VMLINUX_SYMBOL_STR(kmem_cache_free) },
	{ 0x1916e38c, __VMLINUX_SYMBOL_STR(_raw_spin_unlock_irqrestore) },
	{ 0x680ec266, __VMLINUX_SYMBOL_STR(_raw_spin_lock_irqsave) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "71C33B38714C6A3AD966AB1");
