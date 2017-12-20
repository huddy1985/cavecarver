#include <linux/module.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <asm/unistd.h>
#include <asm/pgtable_types.h>
#include <linux/highmem.h>
#include <linux/syscalls.h>
#include "interpose.h"
#include "log.h"
#include "log.c"
#include <linux/version.h>

MODULE_LICENSE("GPL");

struct interpose g_interpose;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,8,0)
// arch/x86/entry/syscall_64.c , arch/x86/include/asm/syscall.h
// -asmlinkage const sys_call_ptr_t sys_call_table[__NR_syscall_max+1] = {
// +asmlinkage sys_call_ptr_t sys_call_table[__NR_syscall_max+1] = {
// need to EXPORT_SYMBOL_GPL(sys_call_table); and recompile kernel
extern void *sys_call_table[];
void **syscall_table = (void**)&sys_call_table;
#else
void **syscall_table = (void **)SYSCALL_TABLE;  /* use "make get" to retrive */
#endif

#define LOG_NAME (1<<0)
#define LOG_ARG  (1<<1)
#define LOG_ENV  (1<<2)
#define LOG_PATH (1<<3)
#define LOG_PID  (1<<4)

static int dbgflags = LOG_NAME /*| LOG_PATH | LOG_ENV | LOG_ARG*/;
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

struct save_cl {
    void *addr;
    unsigned char b[5];
};

struct save_cl save_SyS_clone, save_sys_fork, save_sys_vfork;

long (*original_SyS_clone)(unsigned long clone_flags, unsigned long newsp, int * parent_tidptr, int * child_tidptr, unsigned long tls) = (void*)SyS_clone;
long (*original_sys_fork)(void) = (void*)sys_fork;
long (*original_sys_vfork)(void) = (void*)sys_vfork;

long new_SyS_clone(unsigned long clone_flags, unsigned long newsp, int * parent_tidptr, int * child_tidptr, unsigned long tls)
{
    int pid = current->pid;
    long npid = original_SyS_clone(clone_flags, newsp, parent_tidptr, child_tidptr, tls);
    sysfsdebug(&g_interpose, LOG_NAME, "+ %d->%d", pid, (int)npid);
    return npid;
}

long new_sys_fork(void)
{
    int pid = current->pid;
    long npid = original_sys_fork();
    sysfsdebug(&g_interpose, LOG_NAME, "+ %d->%d", pid, (int)npid);
    return npid;
}

long new_sys_vfork(void)
{
    int pid = current->pid;
    long npid = original_sys_vfork();
    sysfsdebug(&g_interpose, LOG_NAME, "+ %d->%d", pid, (int)npid);
    return npid;
}

/*
ffffffff81891440 <stub_fork>:
ffffffff81891440:	4c 89 7c 24 08       	mov    %r15,0x8(%rsp)
ffffffff81891445:	4c 89 74 24 10       	mov    %r14,0x10(%rsp)
ffffffff8189144a:	4c 89 6c 24 18       	mov    %r13,0x18(%rsp)
ffffffff8189144f:	4c 89 64 24 20       	mov    %r12,0x20(%rsp)
ffffffff81891454:	48 89 6c 24 28       	mov    %rbp,0x28(%rsp)
ffffffff81891459:	48 89 5c 24 30       	mov    %rbx,0x30(%rsp)
ffffffff8189145e:	e9 2d 27 7c ff       	jmpq   ffffffff81053b90 <sys_fork>
ffffffff81891463:	0f 1f 00             	nopl   (%rax)
ffffffff81891466:	66 2e 0f 1f 84 00 00 	nopw   %cs:0x0(%rax,%rax,1)
*/

void patch_clone(int nr, void *faddr, struct save_cl *cl) {

    void *syscalladdr = (void *)syscall_table[nr];
    long addr = ((long)syscalladdr) + (6 * 5); long diff;
    unsigned char b[5] = { 0xe9, 0, 0, 0, 0 };

    memcpy(cl->b, (void*)addr, 5);
    cl->addr = (void*)addr;

    diff = ((long)(void*)faddr)-(addr+5);
    b[1] = diff & 0xff;
    b[2] = (diff>>8) & 0xff;
    b[3] = (diff>>16) & 0xff;
    b[4] = (diff>>24) & 0xff;
    probe_kernel_write((void*)addr, b, 5);

}

asmlinkage int (*original_execve)(const char *,
				  const char __user *const __user *,
				  const char __user *const __user *);
asmlinkage int (*original_getcwd)(char __user *buf, unsigned long size);

asmlinkage int new_execve(const char *p,
			  const char __user *const __user *argp,
			  const char __user *const __user *envp) {
    //printk("test\n");
    //return original_execve(p, argp, envp);

	mm_segment_t fs;
	int argc, envc; char *ptr;

	/* executable to load */
	sysfsdebug(&g_interpose, LOG_NAME, "+ %d.%d:execve:%s", current->real_parent? current->real_parent->pid : -1, current->pid, p);

	/* current working dir */
	if (dbgflags & LOG_PATH) {
	    ptr = kmalloc(MAX_PATH+1, GFP_KERNEL);
	    if (ptr) {

		fs = get_fs();
		set_fs (get_ds());
		original_getcwd(ptr, PATH_MAX);
		set_fs(fs);
		sysfsdebug(&g_interpose, LOG_PATH, "pwd:%s", ptr);
		kfree(ptr);
	    }
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

unsigned char oldcall[5];
void *oldcodep;

static int interpose_init(void) {

    long addr, diff;
    unsigned char b[5] = { 0xe8, 0, 0, 0, 0 };

	spin_lock_init(&g_interpose.lock);
	interpose_text_init(&g_interpose);

	printk(KERN_ALERT "[+] module init, sys_call_table 0x%p\n", (void *)&syscall_table);
	write_cr0 (read_cr0 () & (~ 0x10000));

	printk(KERN_ALERT "[+] save old execve\n");
	original_execve = oldcodep = (void *)syscall_table[__NR_execve];
	printk(KERN_ALERT "[+] save getcwd\n");
	original_getcwd = (void *)syscall_table[__NR_getcwd];

	printk(KERN_ALERT "[+] install new execve %p\n", (void*)&new_execve);

	addr = (long)original_execve;

	original_execve = (void*)sys_execve;

	memcpy(&oldcall, (void*)addr, 5);

	/* patch the stub_execve call */
	diff = ((long)(void*)&new_execve)-(addr+5);
	b[1] = diff & 0xff;
	b[2] = (diff>>8) & 0xff;
	b[3] = (diff>>16) & 0xff;
	b[4] = (diff>>24) & 0xff;
	probe_kernel_write((void*)addr, b, 5);

/*
#define __NR_clone 56
#define __NR_fork 57
#define __NR_vfork 58
*/
	patch_clone(__NR_clone, (void*)&new_SyS_clone, &save_SyS_clone);
	patch_clone(__NR_fork, (void*)&new_sys_fork, &save_sys_fork);
	patch_clone(__NR_vfork, (void*)&new_sys_vfork, &save_sys_vfork);

	//syscall_table[__NR_execve] = (void*)&new_execve;
	printk(KERN_ALERT "[+] protect\n");
	write_cr0 (read_cr0 () | 0x10000);

	return 0;
}

static void interpose_exit(void) {

	write_cr0 (read_cr0 () & (~ 0x10000));

	printk(KERN_ALERT "[+] restore execve\n");
	probe_kernel_write(oldcodep, (void*)oldcall, 5);

	probe_kernel_write(save_SyS_clone.addr, save_SyS_clone.b, 5);
	probe_kernel_write(save_sys_fork.addr, save_sys_fork.b, 5);
	probe_kernel_write(save_sys_vfork.addr, save_sys_vfork.b, 5);

	write_cr0 (read_cr0 () | 0x10000);

	printk(KERN_ALERT "module exit\n");

	interpose_text_exit(&g_interpose);

	return;
}

module_init(interpose_init);
module_exit(interpose_exit);
