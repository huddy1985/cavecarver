#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <asm/uaccess.h>
#include "log.h"

static struct dentry *interpose_root = 0;
DEFINE_MUTEX(interpose_lock);

static int interpose_open(struct inode *inode, struct file *file);
static int interpose_release(struct inode *inode, struct file *file);
static ssize_t interpose_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos);

#define LOG_CONT (1<<8)

static const struct file_operations interpose_fops = {
	.owner =	THIS_MODULE,
	.open =	interpose_open,
	.llseek =	no_llseek,
	.read =	interpose_read,
	.release =	interpose_release,
};

struct interpose_reader {
	struct list_head reader;
	struct kmem_cache *e_slab;
	int nmsgs, msgs_lost;
	struct list_head msgs;
	wait_queue_head_t wait;
	struct interpose *inter;
};

#define DATA_SIZE 63
struct interpose_msg {
	struct list_head msg;
	unsigned int stamp;
	unsigned int type, cnt;
	char data[DATA_SIZE+1];
};
#define MAX_MSGS  (4*PAGE_SIZE / sizeof(struct interpose_msg))

static void interpose_ctor(void *mem) {
	memset(mem, 0, sizeof(struct interpose_msg));
}

static int interpose_open(struct inode *inode, struct file *file) {
	struct interpose *inter; int ret = 0;
	struct interpose_reader *rp;
	unsigned long flags;

	inter = inode->i_private;
	printk("interpose_open: %p\n",inter);

	mutex_lock(&interpose_lock);

	rp = kzalloc(sizeof(struct interpose_reader), GFP_KERNEL);
	if (rp == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	INIT_LIST_HEAD(&rp->msgs);
	INIT_LIST_HEAD(&rp->reader);
	init_waitqueue_head(&rp->wait);
	rp->inter = inter;

	/* create fixed size allocator */
	rp->e_slab = kmem_cache_create("interposeslab",
				       sizeof(struct interpose_msg), sizeof(long), 0,
				       interpose_ctor);
	if (rp->e_slab == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	/* protect reader list */
	spin_lock_irqsave(&inter->lock, flags);
	if (inter->rcnt == 0) {
		inter->monitor = 1;
	}
	inter->rcnt++;
	list_add_tail(&rp->reader, &inter->readers);
	spin_unlock_irqrestore(&inter->lock, flags);

	file->private_data = rp;
	ret = 0;
out:
	mutex_unlock(&interpose_lock);
	return ret;
}

static int interpose_release(struct inode *inode, struct file *file) {
	struct interpose *inter;
	struct interpose_reader *rp;
	struct list_head *p;
	struct interpose_msg *ep;
	unsigned long flags;

	inter = inode->i_private;
	rp = file->private_data;

	spin_lock_irqsave(&inter->lock, flags);
	list_del(&rp->reader);
	if (--inter->rcnt <= 0)
		inter->monitor = 0;;
	spin_unlock_irqrestore(&inter->lock, flags);

	while (!list_empty(&rp->msgs)) {
		p = rp->msgs.next;
		ep = list_entry(p, struct interpose_msg, msg);
		list_del(p);
		--rp->nmsgs;
		kmem_cache_free(rp->e_slab, ep);
	}

	kmem_cache_destroy(rp->e_slab);
	return 0;
}

/* call with buffer big enough to fit DATA_SIZE+64, i.e.
 *  dd if=/sys/debug/interpose/io bs=256
 */
static ssize_t interpose_read(struct file *file, char __user *buf, size_t nbytes, loff_t *ppos) {

	struct interpose *inter;
	struct interpose_reader *rp = file->private_data;
	struct list_head *p;
	struct interpose_msg *ep;
	unsigned long flags; int cnt = 0;
	char prepare[128];
	DECLARE_WAITQUEUE(waita, current);

	inter = rp->inter;
  
	add_wait_queue(&rp->wait, &waita);
	set_current_state(TASK_INTERRUPTIBLE);
	while (1) {

		/* protect msgs */
		spin_lock_irqsave(&inter->lock, flags);
		if (list_empty(&rp->msgs)) {
			ep = 0;
		} else {
			p = rp->msgs.next;
			list_del(p);
			--rp->nmsgs;
			ep = list_entry(p, struct interpose_msg, msg);
		}
		spin_unlock_irqrestore(&inter->lock, flags);

		/* found one */
		if (ep)
			break;

		/* continue waiting */
		if (file->f_flags & O_NONBLOCK) {
			set_current_state(TASK_RUNNING);
			remove_wait_queue(&rp->wait, &waita);
			return -EWOULDBLOCK;
		}
		schedule();
		if (signal_pending(current)) {
			remove_wait_queue(&rp->wait, &waita);
			return -EINTR;
		}
		set_current_state(TASK_INTERRUPTIBLE);
	}

	set_current_state(TASK_RUNNING);
	remove_wait_queue(&rp->wait, &waita);

	cnt = snprintf(prepare, sizeof(prepare), "%03d:%u.%06u:%s\n", (int)ep->type, (unsigned int)ep->stamp / (unsigned int)1000000, (unsigned int)ep->stamp % (unsigned int)1000000, ep->data );
	cnt = min_t(int, nbytes, cnt);

	if (cnt) {
		if (copy_to_user(buf, prepare, cnt))
			cnt = -EFAULT;
	}

	kmem_cache_free(rp->e_slab, ep);
	return cnt;

}

void interpose_push(struct interpose *inter, int type, char *data, int len) {
	unsigned long flags;
	struct list_head *pos;
	struct interpose_reader *rp;
	struct interpose_msg *ep;
	struct timeval tval;
	unsigned int stamp;
	int left, curpos, typadd;

	do_gettimeofday(&tval);
	stamp = tval.tv_sec & 0xFFF;
	stamp = stamp * 1000000 + tval.tv_usec;

	spin_lock_irqsave(&inter->lock, flags);
	list_for_each (pos, &inter->readers) {
		rp = list_entry(pos, struct interpose_reader, reader);

		left = len; curpos = 0;
		typadd = type;
    
		while (left > 0) {
    
			if (rp->nmsgs >= MAX_MSGS ||
			    (ep = kmem_cache_alloc(rp->e_slab, GFP_ATOMIC)) == NULL) {
				rp->msgs_lost++;
				goto skip;
			}
    
			/* assemble msg and append to list*/
			ep->stamp = stamp;
			ep->type = typadd;
			ep->cnt = len = min_t(int, len, DATA_SIZE);

			memcpy(ep->data, data + curpos , len);
			ep->data[len] = 0;
	    
			list_add_tail(&ep->msg, &rp->msgs);
			rp->nmsgs++;

			curpos += len;
			left -= len;
			typadd |= LOG_CONT;
		}

	skip:    
		/* wake up processes blocked in interpose_read */
		wake_up(&rp->wait);
	}
	spin_unlock_irqrestore(&inter->lock, flags);
}

int  interpose_text_init(struct interpose *inter)
{
	struct dentry *mondir;
	struct dentry *d;

	printk(KERN_DEBUG "interpose-: try mount /sys/kernel/debug/interpose/execve\n");

	spin_lock_init(&inter->lock);
	INIT_LIST_HEAD(&inter->readers);

	mondir = debugfs_create_dir("interpose", NULL);
	if (IS_ERR(mondir))
		return 0;
	if (mondir == NULL) {
		printk(KERN_NOTICE "interpose-: unable to create interpose debug directory\n");
		return -ENOMEM;
	}
	interpose_root = mondir;
	d = debugfs_create_file("execve", 0600, interpose_root, inter, &interpose_fops);
	inter->dent_u = d;

	return 0;
}

void interpose_text_exit(struct interpose *inter)
{
	printk(KERN_DEBUG "interpose-: unmounting /sys/kernel/debug/interpose/execve\n");
	if (inter->dent_u)
		debugfs_remove(inter->dent_u);
	if (interpose_root)
		debugfs_remove(interpose_root);
}
