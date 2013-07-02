#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
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
	{ 0x437afd29, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x361f3b15, __VMLINUX_SYMBOL_STR(hid_unregister_driver) },
	{ 0x26a01ac4, __VMLINUX_SYMBOL_STR(__hid_register_driver) },
	{ 0xf0eb12ae, __VMLINUX_SYMBOL_STR(hid_disconnect) },
	{ 0xaf48f1a5, __VMLINUX_SYMBOL_STR(sysfs_create_group) },
	{ 0x5469df69, __VMLINUX_SYMBOL_STR(hid_connect) },
	{ 0x37a0cba, __VMLINUX_SYMBOL_STR(kfree) },
	{ 0x269e7779, __VMLINUX_SYMBOL_STR(dev_err) },
	{ 0xc46bc315, __VMLINUX_SYMBOL_STR(hid_open_report) },
	{ 0x49717b8f, __VMLINUX_SYMBOL_STR(dev_set_drvdata) },
	{ 0xf5502cb2, __VMLINUX_SYMBOL_STR(kmem_cache_alloc_trace) },
	{ 0x67e15406, __VMLINUX_SYMBOL_STR(kmalloc_caches) },
	{ 0x91715312, __VMLINUX_SYMBOL_STR(sprintf) },
	{ 0x53f6ffbc, __VMLINUX_SYMBOL_STR(wait_for_completion_timeout) },
	{ 0xf432dd3d, __VMLINUX_SYMBOL_STR(__init_waitqueue_head) },
	{ 0x9b14f11, __VMLINUX_SYMBOL_STR(input_event) },
	{ 0x40256835, __VMLINUX_SYMBOL_STR(complete_all) },
	{ 0xadaabe1b, __VMLINUX_SYMBOL_STR(pv_lock_ops) },
	{ 0xd52bf1ce, __VMLINUX_SYMBOL_STR(_raw_spin_lock) },
	{ 0x3c80c06c, __VMLINUX_SYMBOL_STR(kstrtoull) },
	{ 0xc3f8050b, __VMLINUX_SYMBOL_STR(dev_get_drvdata) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=hid";

MODULE_ALIAS("hid:b0003g*v0000046Dp0000C24D");

MODULE_INFO(srcversion, "48CD1348215E7C19F229A8D");
