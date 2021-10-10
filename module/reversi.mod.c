#include <linux/build-salt.h>
#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(.gnu.linkonce.this_module) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section(__versions) = {
	{ 0xfc69738b, "module_layout" },
	{ 0xf96bbaf, "misc_deregister" },
	{ 0xa692696b, "misc_register" },
	{ 0x8c8569cb, "kstrtoint" },
	{ 0xce807a25, "up_write" },
	{ 0x57bc19d2, "down_write" },
	{ 0x53b954a2, "up_read" },
	{ 0x21271fd0, "copy_user_enhanced_fast_string" },
	{ 0x1f199d24, "copy_user_generic_string" },
	{ 0xecdcabd2, "copy_user_generic_unrolled" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x668b19a1, "down_read" },
	{ 0xc5850110, "printk" },
	{ 0xdecd0b29, "__stack_chk_fail" },
	{ 0xbdfb6dbb, "__fentry__" },
};

MODULE_INFO(depends, "");

