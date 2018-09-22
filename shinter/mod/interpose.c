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
//extern void *sys_call_table[];
//void **syscall_table = (void**)&sys_call_table;
void **syscall_table = (void **)SYSCALL_TABLE;  /* use "make get" to retrive, require nokasrl option */

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
	void *orig;
};

struct save_cl save_SyS_clone;
struct save_cl save_sys_fork;
struct save_cl save_sys_vfork;
struct save_cl save_sys_execve;

/* 4.15: kernel/form.c: 2164

   SYSCALL_DEFINE5(clone, unsigned long, clone_flags, unsigned long, newsp,
   int __user *, parent_tidptr,
   int __user *, child_tidptr,
   unsigned long, tls)
*/
long (*original_SyS_clone)(unsigned long clone_flags, unsigned long newsp, int * parent_tidptr, int * child_tidptr, unsigned long tls) = (void*)SyS_clone;

/* 4.15: kernel/fork.c
   SYSCALL_DEFINE0(fork)
   {
   #ifdef CONFIG_MMU
   return _do_fork(SIGCHLD, 0, 0, NULL, NULL, 0);
   #else
   /\* can not support in nommu mode *\/
   return -EINVAL;
   #endif
   }
*/
long (*original_sys_fork)(void) = (void*)sys_fork;

/* 4.15: kernel/fork.c:
   #ifdef __ARCH_WANT_SYS_VFORK
   SYSCALL_DEFINE0(vfork)
   {
   return _do_fork(CLONE_VFORK | CLONE_VM | SIGCHLD, 0,
   0, NULL, NULL, 0);
   }
   #endif
*/


long (*original_sys_vfork)(void) = (void*)sys_vfork;
asmlinkage int (*original_execve)(const char *,
				  const char __user *const __user *,
				  const char __user *const __user *) = 0;
asmlinkage int (*original_getcwd)(char __user *buf, unsigned long size) = 0;


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

/*
  4.12:
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


  4.15:
  ffffffff8108b7c0 <SyS_clone>:
  ffffffff8108b7c0:	e8 ab 64 97 00       	callq  ffffffff81a01c70 <__fentry__>
  ffffffff8108b7c5:	55                   	push   %rbp
  ffffffff8108b7c6:	4d 89 c1             	mov    %r8,%r9
  ffffffff8108b7c9:	49 89 c8             	mov    %rcx,%r8
  ffffffff8108b7cc:	48 89 d1             	mov    %rdx,%rcx
  ffffffff8108b7cf:	31 d2                	xor    %edx,%edx
  ffffffff8108b7d1:	48 89 e5             	mov    %rsp,%rbp
  ffffffff8108b7d4:	e8 57 fb ff ff       	callq  ffffffff8108b330 <_do_fork>
  ffffffff8108b7d9:	5d                   	pop    %rbp
  ffffffff8108b7da:	c3                   	retq
*/
#define SYS_CLONE_OFF (0xffffffff81091172ul - 0xffffffff81091150ul)

/*
  ffffffff8108b790 <sys_vfork>:
  ffffffff8108b790:	e8 db 64 97 00       	callq  ffffffff81a01c70 <__fentry__>
  ffffffff8108b795:	55                   	push   %rbp
  ffffffff8108b796:	45 31 c9             	xor    %r9d,%r9d
  ffffffff8108b799:	45 31 c0             	xor    %r8d,%r8d
  ffffffff8108b79c:	31 c9                	xor    %ecx,%ecx
  ffffffff8108b79e:	31 d2                	xor    %edx,%edx
  ffffffff8108b7a0:	31 f6                	xor    %esi,%esi
  ffffffff8108b7a2:	48 89 e5             	mov    %rsp,%rbp
  ffffffff8108b7a5:	bf 11 41 00 00       	mov    $0x4111,%edi
  ffffffff8108b7aa:	e8 81 fb ff ff       	callq  ffffffff8108b330 <_do_fork>
  ffffffff8108b7af:	5d                   	pop    %rbp
  ffffffff8108b7b0:	c3                   	retq
*/

#define SYS_VFORK_OFF (0xffffffff8108b7aa - 0xffffffff8108b790ul)

/*
  ffffffff8108b760 <sys_fork>:
  ffffffff8108b760:	e8 0b 65 97 00       	callq  ffffffff81a01c70 <__fentry__>
  ffffffff8108b765:	55                   	push   %rbp
  ffffffff8108b766:	45 31 c9             	xor    %r9d,%r9d
  ffffffff8108b769:	45 31 c0             	xor    %r8d,%r8d
  ffffffff8108b76c:	31 c9                	xor    %ecx,%ecx
  ffffffff8108b76e:	31 d2                	xor    %edx,%edx
  ffffffff8108b770:	31 f6                	xor    %esi,%esi
  ffffffff8108b772:	48 89 e5             	mov    %rsp,%rbp
  ffffffff8108b775:	bf 11 00 00 00       	mov    $0x11,%edi
  ffffffff8108b77a:	e8 b1 fb ff ff       	callq  ffffffff8108b330 <_do_fork>
  ffffffff8108b77f:	5d                   	pop    %rbp
  ffffffff8108b780:	c3                   	retq
*/

#define SYS_FORK_OFF (0xffffffff8108b77a - 0xffffffff8108b760)

/*
  ffffffff8127eb60 <SyS_execve>:
  ffffffff8127eb60:	e8 0b 31 78 00       	callq  ffffffff81a01c70 <__fentry__>
  ffffffff8127eb65:	55                   	push   %rbp
  ffffffff8127eb66:	48 89 e5             	mov    %rsp,%rbp
  ffffffff8127eb69:	41 54                	push   %r12
  ffffffff8127eb6b:	53                   	push   %rbx
  ffffffff8127eb6c:	49 89 d4             	mov    %rdx,%r12
  ffffffff8127eb6f:	48 89 f3             	mov    %rsi,%rbx
  ffffffff8127eb72:	e8 39 88 00 00       	callq  ffffffff812873b0 <getname>
  ffffffff8127eb77:	6a 00                	pushq  $0x0
  ffffffff8127eb79:	4d 89 e1             	mov    %r12,%r9
  ffffffff8127eb7c:	48 89 d9             	mov    %rbx,%rcx
  ffffffff8127eb7f:	45 31 c0             	xor    %r8d,%r8d
  ffffffff8127eb82:	31 d2                	xor    %edx,%edx
  ffffffff8127eb84:	48 89 c6             	mov    %rax,%rsi
  ffffffff8127eb87:	bf 9c ff ff ff       	mov    $0xffffff9c,%edi
  ffffffff8127eb8c:	e8 9f f5 ff ff       	callq  ffffffff8127e130 <do_execveat_common.isra.34>
  ffffffff8127eb91:	48 8d 65 f0          	lea    -0x10(%rbp),%rsp
  ffffffff8127eb95:	48 98                	cltq
  ffffffff8127eb97:	5b                   	pop    %rbx
  ffffffff8127eb98:	41 5c                	pop    %r12
  ffffffff8127eb9a:	5d                   	pop    %rbp
  ffffffff8127eb9b:	c3                   	retq
  ffffffff8127eb9c:	0f 1f 40 00          	nopl   0x0(%rax)
*/

#define SYS_EXECVE_OFF (0xffffffff812abdf4ul - 0xffffffff812abdc0ul)

void patch_clone_4_15(void *base, u64 off, void *faddr, struct save_cl *cl)
{

	long addr = ((long)base) + off;
	long diff;
	unsigned char b[5] = { 0xe8, 0, 0, 0, 0 };

	memcpy(cl->b, (void*)addr, 5);
	cl->addr = (void*)addr;
	cl->orig = (void*)(addr + 5 + (int)(( (u32)cl->b[1]) |
					    (((u32)cl->b[2]) << 8) |
					    (((u32)cl->b[3]) << 16) |
					    (((u32)cl->b[4]) << 24)));

	diff = ((long)(void*)faddr)-(addr+5);
	b[1] = (diff & 0xff);
	b[2] = (diff>>8) & 0xff;
	b[3] = (diff>>16) & 0xff;
	b[4] = (diff>>24) & 0xff;
	probe_kernel_write((void*)addr, b, 5);
}

/* 4.15 */
int testpatch_4_15(void)
{
  if (!( 0xe8 == *(unsigned char *)(SyS_clone + SYS_CLONE_OFF))) {
    printk("Cannot find clone call @ %lx got %02x\n", SyS_clone + SYS_CLONE_OFF, *(unsigned char *)(SyS_clone + SYS_CLONE_OFF));
  }
  if (!( 0xe8 == *(unsigned char *)(sys_execve + SYS_EXECVE_OFF))) {
    printk("Cannot find execve call @ %lx got 0x02%x\n", sys_execve + SYS_EXECVE_OFF, *(unsigned char *)(sys_execve + SYS_EXECVE_OFF));
  }

    if (( 0xe8 == *(unsigned char *)(SyS_clone + SYS_CLONE_OFF)) &&
	/*( 0xe8 == *(char *)(sys_vfork + SYS_VFORK_OFF)) &&
	  ( 0xe8 == *(char *)(sys_fork + SYS_FORK_OFF)) &&*/
	( 0xe8 == *(unsigned char *)(sys_execve + SYS_EXECVE_OFF))) {
	return 1;
    }
    return 0;
}


void patch_4_15(void)
{
	patch_clone_4_15((void*)SyS_clone,  SYS_CLONE_OFF,  new_SyS_clone, &save_SyS_clone );
	original_SyS_clone = save_SyS_clone.orig;
	//patch_clone_4_15((void*)sys_vfork,  SYS_VFORK_OFF,  new_sys_vfork, &save_sys_vfork );
	//patch_clone_4_15((void*)sys_fork,   SYS_FORK_OFF,   new_sys_fork,  &save_sys_fork  );
	patch_clone_4_15((void*)sys_execve, SYS_EXECVE_OFF, new_execve,    &save_sys_execve);
	original_execve = save_sys_execve.orig;
}

/* 4.12 */
void patch_clone(int nr, void *faddr, struct save_cl *cl)
{

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


unsigned char oldcall[5];
void *oldcodep;

static int interpose_init(void) {

	//long addr/*, diff*/;
	//unsigned char b[5] = { 0xe8, 0, 0, 0, 0 };

	spin_lock_init(&g_interpose.lock);
	interpose_text_init(&g_interpose);

	printk(KERN_ALERT "[+] module init, sys_call_table 0x%p\n", (void *)&syscall_table);
	write_cr0 (read_cr0 () & (~ 0x10000));

	printk(KERN_ALERT "[+] save getcwd\n");
	original_getcwd = (void *)syscall_table[__NR_getcwd];

	/*
	addr = (long)original_execve;

	original_execve = (void*)sys_execve;

	memcpy(&oldcall, (void*)addr, 5);
	*/

	/* patch the stub_execve call */
	/* 4.12
	   diff = ((long)(void*)&new_execve)-(addr+5);
	   b[1] = diff & 0xff;
	   b[2] = (diff>>8) & 0xff;
	   b[3] = (diff>>16) & 0xff;
	   b[4] = (diff>>24) & 0xff;
	   probe_kernel_write((void*)addr, b, 5);
	*/
	/*
	  patch_clone(__NR_clone, (void*)&new_SyS_clone, &save_SyS_clone);
	  patch_clone(__NR_fork, (void*)&new_sys_fork, &save_sys_fork);
	  patch_clone(__NR_vfork, (void*)&new_sys_vfork, &save_sys_vfork);
	*/
/*
  #define __NR_clone 56
  #define __NR_fork 57
  #define __NR_vfork 58
*/

	if (testpatch_4_15()) {
	    printk(KERN_ALERT "[+] install fork, vfork, clone and execve\n");
	    patch_4_15();
	} else {
	    printk(KERN_ALERT "[-] wrong offsets\n");

	}

	//syscall_table[__NR_execve] = (void*)&new_execve;
	printk(KERN_ALERT "[+] protect\n");
	write_cr0 (read_cr0 () | 0x10000);

	return 0;
}

static void interpose_exit(void) {

	write_cr0 (read_cr0 () & (~ 0x10000));

	printk(KERN_ALERT "[+] restore execve\n");
	//probe_kernel_write(oldcodep, (void*)oldcall, 5);

	probe_kernel_write(save_SyS_clone.addr, save_SyS_clone.b, 5);
	probe_kernel_write(save_sys_fork.addr, save_sys_fork.b, 5);
	probe_kernel_write(save_sys_vfork.addr, save_sys_vfork.b, 5);
	probe_kernel_write(save_sys_execve.addr, save_sys_execve.b, 5);

	write_cr0 (read_cr0 () | 0x10000);

	printk(KERN_ALERT "module exit\n");

	interpose_text_exit(&g_interpose);

	return;
}

module_init(interpose_init);
module_exit(interpose_exit);
