#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <asm/unistd.h>
//#include <asm/pgtable_types.h>
#include <linux/highmem.h>
#include "interpose.h"
#include "log.h"
#include "log.c"
#include "arm.c"
#include <linux/version.h>

MODULE_LICENSE("GPL");

struct interpose g_interpose;

extern void *sys_call_table[];
void **syscall_table = (void**)&sys_call_table;

#define LOG_NAME (1<<0)
#define LOG_ARG  (1<<1)
#define LOG_ENV  (1<<2)
#define LOG_PATH (1<<3)

static int dbgflags = LOG_PATH | LOG_NAME | LOG_ENV | LOG_ARG;
module_param(dbgflags, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(dbgflags, "logging flags, 1:name logging");

#define SYSFS_NAME 1

#  define sysfsdebug(sim,flag, ...) do {				\
		if (dbgflags & flag) {					\
			char b[128+1]; int cnt = snprintf(b, 128, ##__VA_ARGS__); \
			b[128] = 0;					\
			interpose_push(sim, flag, b, cnt+1);		\
		}							\
	} while(0);

#define MAX_PATH 4096
#define MAX_ARG_STRINGS (PAGE_SIZE * 32)
#define MAX_ARG_STRLEN 0x7FFFFFFF

static const char __user *get_user_arg_ptr(const char __user *const __user *argv, int nr)
{
	const char __user *native;
	if (get_user(native, argv + nr))
		return ERR_PTR(-EFAULT);
	return native;
}

static int count(const char __user *const __user *argv, int max)
{
	int i = 0;
	if (argv != NULL) {
		for (;;) {
			const char __user *p = get_user_arg_ptr(argv, i);
			if (!p)
				break;
			if (IS_ERR(p))
				return -EFAULT;
			if (i >= max)
				return -E2BIG;
			++i;
		}
	}
	return i;
}

static int log_strings(int typ, int argc, const char __user *const __user *argv)
{
	int ret;
	while (argc-- > 0) {
		const char __user *str; char *ptr;
		int len;

		ret = -EFAULT;
		str = get_user_arg_ptr(argv, argc);
		if (IS_ERR(str))
			goto out;

		len = strnlen_user(str, MAX_ARG_STRLEN);
		if (!len)
			goto out;

		ptr = kmalloc(len+1, GFP_KERNEL);
		if (!ptr)
			goto out;

		if (copy_from_user(ptr, str, len)) {
			ret = -EFAULT;
			goto out;
		}
		ptr[len] = '\n';
		interpose_push(&g_interpose, typ, ptr, len+1);

		kfree(ptr);
	}
	ret = 0;
out:
	return ret;
}

asmlinkage int (*original_execve)(const char *,
				  const char __user *const __user *,
				  const char __user *const __user *);
asmlinkage int (*original_getcwd)(char __user *buf, unsigned long size);

asmlinkage int new_execve(const char *p,
			  const char __user *const __user *argp,
			  const char __user *const __user *envp) {

	mm_segment_t fs;
	int argc, envc; char *ptr;

	/* executable to load */
	sysfsdebug(&g_interpose, LOG_NAME, "+ execve:%s", p);

	/* current working dir */
	ptr = kmalloc(MAX_PATH+1, GFP_KERNEL);
	if (ptr) {

	  fs = get_fs();
	  set_fs (get_ds());
	  original_getcwd(ptr, PATH_MAX);
	  set_fs(fs);
	  sysfsdebug(&g_interpose, LOG_PATH, "pwd:%s", ptr);
	  kfree(ptr);
	}

	/* arg and env */
	argc = count(argp, MAX_ARG_STRINGS);
	envc = count(envp, MAX_ARG_STRINGS);

	if (dbgflags & LOG_ARG)
	  log_strings(LOG_ARG, argc, argp);

	if (dbgflags & LOG_ENV)
	  log_strings(LOG_ENV, envc, envp);

	return (*original_execve)(p, argp, envp);
}

static int interpose_init(void) {

	spin_lock_init(&g_interpose.lock);
	interpose_text_init(&g_interpose);

	printk(KERN_ALERT "[+] module init, sys_call_table 0x%p\n", (void *)&syscall_table);
	//write_cr0 (read_cr0 () & (~ 0x10000));

	printk(KERN_ALERT "[+] save old execve\n");
	original_execve = (void *)syscall_table[__NR_execve];
	printk(KERN_ALERT "[+] save getcwd\n");
	original_getcwd = (void *)syscall_table[__NR_getcwd];

	printk(KERN_ALERT "[+] install new execve\n");
	__aarch64_ptr_write(&(syscall_table[__NR_execve]),(u64)(void*)&new_execve);

	printk(KERN_ALERT "[+] protect\n");
	//write_cr0 (read_cr0 () | 0x10000);

	return 0;
}

static void interpose_exit(void) {

  //write_cr0 (read_cr0 () & (~ 0x10000));

	printk(KERN_ALERT "[+] restore execve\n");
	syscall_table[__NR_execve] = (void *) original_execve;

	//write_cr0 (read_cr0 () | 0x10000);

	printk(KERN_ALERT "module exit\n");

	interpose_text_exit(&g_interpose);

	return;
}

module_init(interpose_init);
module_exit(interpose_exit);
