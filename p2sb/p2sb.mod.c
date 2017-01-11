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
	{ 0x551a9e15, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0xa2e59924, __VMLINUX_SYMBOL_STR(param_ops_int) },
	{ 0x5df1ef83, __VMLINUX_SYMBOL_STR(simple_attr_release) },
	{ 0xbdd620f3, __VMLINUX_SYMBOL_STR(simple_attr_write) },
	{ 0x9a894af4, __VMLINUX_SYMBOL_STR(simple_attr_read) },
	{ 0x33c06400, __VMLINUX_SYMBOL_STR(generic_file_llseek) },
	{ 0x3b416efd, __VMLINUX_SYMBOL_STR(debugfs_remove_recursive) },
	{ 0xace44780, __VMLINUX_SYMBOL_STR(debugfs_create_file) },
	{ 0x45c49cd8, __VMLINUX_SYMBOL_STR(debugfs_create_dir) },
	{ 0xe484e35f, __VMLINUX_SYMBOL_STR(ioread32) },
	{ 0xedc03953, __VMLINUX_SYMBOL_STR(iounmap) },
	{ 0x436c2179, __VMLINUX_SYMBOL_STR(iowrite32) },
	{ 0x27e1a049, __VMLINUX_SYMBOL_STR(printk) },
	{ 0x42c8de35, __VMLINUX_SYMBOL_STR(ioremap_nocache) },
	{ 0x6d895e99, __VMLINUX_SYMBOL_STR(simple_attr_open) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";


MODULE_INFO(srcversion, "E317EC666130AF541E51119");
