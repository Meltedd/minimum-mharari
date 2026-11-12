/*
 * scull.c -- the bare scull char module
 *
 * Copyright (C) 2001 Alessandro Rubini and Jonathan Corbet
 * Copyright (C) 2001 O'Reilly & Associates
 *
 * The source code in this file can be freely used, adapted,
 * and redistributed in source or binary form, so long as an
 * acknowledgment appears in derived source files.  The citation
 * should list that the code comes from the book "Linux Device
 * Drivers" by Alessandro Rubini and Jonathan Corbet, published
 * by O'Reilly & Associates.   No warranty is attached;
 * we cannot take responsibility for errors or fitness for use.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>

#include <linux/kernel.h>	/* printk() */
#include <linux/slab.h>		/* kmalloc() */
#include <linux/fs.h>		/* everything... */
#include <linux/errno.h>	/* error codes */
#include <linux/types.h>	/* size_t */
#include <linux/cdev.h>

#include <linux/uaccess.h>	/* copy_*_user */
#include <linux/sched.h>	/* current, task_struct */
#include <linux/list.h>		/* kernel linked lists */
#include <linux/mutex.h>	/* kernel mutex */

#include "scull.h"		/* local definitions */

/*
 * Our parameters which can be set at load time.
 */

static int scull_major =   SCULL_MAJOR;
static int scull_minor =   0;
static int scull_quantum = SCULL_QUANTUM;

module_param(scull_major, int, S_IRUGO);
module_param(scull_minor, int, S_IRUGO);
module_param(scull_quantum, int, S_IRUGO);

MODULE_AUTHOR("mharari");
MODULE_LICENSE("Dual BSD/GPL");

static struct cdev scull_cdev;		/* Char device structure */

/*
 * Log of every task (process or thread) that used SCULL_IOCIQUANTUM.
 * Each node records one unique (pid, tgid) pair; nodes are appended at
 * the tail, so the list is in first-use order.
 */
struct task_node {
	pid_t pid;
	pid_t tgid;
	struct list_head list;	/* links this node into task_list */
};

/* Head of the list, statically initialized to an empty list */
static LIST_HEAD(task_list);

/* Protects task_list: several tasks may issue the ioctl concurrently */
static DEFINE_MUTEX(task_list_lock);

/*
 * Add the current task to task_list unless its (pid, tgid) pair is
 * already recorded. If kmalloc fails, log an error and leave the list
 * unchanged.
 */
static void scull_log_current_task(void)
{
	struct task_node *node;

	mutex_lock(&task_list_lock);

	/* Look for an existing entry for this task (no duplicates) */
	list_for_each_entry(node, &task_list, list) {
		if (node->pid == current->pid && node->tgid == current->tgid)
			goto unlock;
	}

	/* Not found: allocate a new node and append it at the end */
	node = kmalloc(sizeof(*node), GFP_KERNEL);
	if (!node) {
		printk(KERN_ERR "scull: kmalloc failed, task %d not logged\n",
		       current->pid);
		goto unlock;
	}
	node->pid = current->pid;
	node->tgid = current->tgid;
	list_add_tail(&node->list, &task_list);

unlock:
	/* Single exit point, so the mutex is released on every path */
	mutex_unlock(&task_list_lock);
}

/*
 * Open and close
 */

static int scull_open(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "scull open\n");
	return 0;          /* success */
}

static int scull_release(struct inode *inode, struct file *filp)
{
	printk(KERN_INFO "scull close\n");
	return 0;
}

/*
 * The ioctl() implementation
 */

static long scull_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0, tmp;
	int retval = 0;
    
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != SCULL_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > SCULL_IOC_MAXNR) return -ENOTTY;

	err = !access_ok((void __user *)arg, _IOC_SIZE(cmd));
	if (err) return -EFAULT;

	switch(cmd) {

	case SCULL_IOCRESET:
		scull_quantum = SCULL_QUANTUM;
		break;
        
	case SCULL_IOCSQUANTUM: /* Set: arg points to the value */
		retval = __get_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCTQUANTUM: /* Tell: arg is the value */
		scull_quantum = arg;
		break;

	case SCULL_IOCGQUANTUM: /* Get: arg is pointer to result */
		retval = __put_user(scull_quantum, (int __user *)arg);
		break;

	case SCULL_IOCQQUANTUM: /* Query: return it (it's positive) */
		return scull_quantum;

	case SCULL_IOCXQUANTUM: /* eXchange: use arg as pointer */
		tmp = scull_quantum;
		retval = __get_user(scull_quantum, (int __user *)arg);
		if (retval == 0)
			retval = __put_user(tmp, (int __user *)arg);
		break;

	case SCULL_IOCHQUANTUM: /* sHift: like Tell + Query */
		tmp = scull_quantum;
		scull_quantum = arg;
		return tmp;

	case SCULL_IOCIQUANTUM: /* Info: arg is pointer to a struct task_info */
	{
		/* Zero-initialized so no padding bytes leak to user space */
		struct task_info info = {0};

		/* Record this task in the log list (Part 2) */
		scull_log_current_task();

		/* Fill the struct from the caller's task_struct / thread_info */
		info.__state = READ_ONCE(current->__state);
		info.cpu = current->thread_info.cpu;	/* CPU from thread_info */
		info.prio = current->prio;
		info.pid = current->pid;
		info.tgid = current->tgid;
		info.nvcsw = current->nvcsw;
		info.nivcsw = current->nivcsw;

		/* Copy the whole struct at once; nonzero means bytes not copied */
		if (copy_to_user((struct task_info __user *)arg, &info, sizeof(info)))
			retval = -EFAULT;
		break;
	}

	default:  /* redundant, as cmd was checked against MAXNR */
		return -ENOTTY;
	}
	return retval;
}

struct file_operations scull_fops = {
	.owner =    THIS_MODULE,
	.unlocked_ioctl = scull_ioctl,
	.open =     scull_open,
	.release =  scull_release,
};

/*
 * Finally, the module stuff
 */

/*
 * The cleanup function is used to handle initialization failures as well.
 * Thefore, it must be careful to work correctly even if some of the items
 * have not been initialized
 */
static void scull_cleanup_module(void)
{
	dev_t devno = MKDEV(scull_major, scull_minor);
	struct task_node *node, *next;
	int i = 1;

	/*
	 * Print every logged task, then unlink and free its node. The _safe
	 * iterator keeps a pointer to the next node so we can free the
	 * current one while walking the list. (Empty list: loop does nothing.)
	 */
	mutex_lock(&task_list_lock);
	list_for_each_entry_safe(node, next, &task_list, list) {
		printk(KERN_INFO "Task %d: PID %d, TGID %d\n",
		       i++, node->pid, node->tgid);
		list_del(&node->list);
		kfree(node);
	}
	mutex_unlock(&task_list_lock);

	/* Get rid of the char dev entry */
	cdev_del(&scull_cdev);

	/* cleanup_module is never called if registering failed */
	unregister_chrdev_region(devno, 1);
}

static int scull_init_module(void)
{
	int result;
	dev_t dev = 0;

	/*
	 * Get a range of minor numbers to work with, asking for a dynamic
	 * major unless directed otherwise at load time.
	 */
	if (scull_major) {
		dev = MKDEV(scull_major, scull_minor);
		result = register_chrdev_region(dev, 1, "scull");
	} else {
		result = alloc_chrdev_region(&dev, scull_minor, 1, "scull");
		scull_major = MAJOR(dev);
	}
	if (result < 0) {
		printk(KERN_WARNING "scull: can't get major %d\n", scull_major);
		return result;
	}

	cdev_init(&scull_cdev, &scull_fops);
	scull_cdev.owner = THIS_MODULE;
	result = cdev_add (&scull_cdev, dev, 1);
	/* Fail gracefully if need be */
	if (result) {
		printk(KERN_NOTICE "Error %d adding scull character device", result);
		goto fail;
	}

	return 0; /* succeed */

  fail:
	scull_cleanup_module();
	return result;
}

module_init(scull_init_module);
module_exit(scull_cleanup_module);
