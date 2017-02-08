#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/sched.h>

#define GLOBALFIFO_MAJOR 0
#define GLOBALFIFO_SIZE  0x1000

static int globalfifo_major = GLOBALFIFO_MAJOR;

struct globalfifo_dev {
    struct cdev cdev;
    unsigned int current_len;
    unsigned char mem[GLOBALFIFO_SIZE];
    struct mutex mutex;
    wait_queue_head_t r_wait;
    wait_queue_head_t w_wait;
} *globalfifo_devp;

static int globalfifo_open(struct inode *inode, struct file *filp)
{
    filp->private_data = globalfifo_devp;
    return 0;
}

static int globalfifo_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t globalfifo_read(struct file *filp, char __user *buf,
    size_t count, loff_t *ppos)
{
    int ret;
    struct globalfifo_dev *dev = filp->private_data;
    DECLARE_WAITQUEUE(wait, current);

    add_wait_queue(&dev->r_wait, &wait);

    while (dev->current_len == 0) {
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }

        __set_current_state(TASK_INTERRUPTIBLE);

        schedule();
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            goto out2;
        }
    }

    if (count > dev->current_len)
        count = dev->current_len;

    if (copy_to_user(buf, dev->mem, count)) {
        ret = -EFAULT;
        goto out;
    } else {
        memcpy(dev->mem, dev->mem + count, dev->current_len - count);
        dev->current_len -= count;
        printk(KERN_INFO "read %zu bytes, current_len: %u\n", count, dev->current_len);
        
        wake_up_interruptible(&dev->w_wait);

        ret = count;
    }

out:
out2:
    remove_wait_queue(&dev->r_wait, &wait);
    set_current_state(TASK_RUNNING);
    return ret;
}

static ssize_t globalfifo_write(struct file *filp, const char __user *buf,
    size_t count, loff_t *ppos)
{
    int ret;
    struct globalfifo_dev *dev = filp->private_data;
    DECLARE_WAITQUEUE(wait, current);

    add_wait_queue(&dev->w_wait, &wait);

    while (dev->current_len == GLOBALFIFO_SIZE) {
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto out;
        }

        __set_current_state(TASK_INTERRUPTIBLE);

        schedule();
        if (signal_pending(current)) {
            ret = -ERESTARTSYS;
            goto out2;
        }
    }

    if (count > GLOBALFIFO_SIZE - dev->current_len)
        count = GLOBALFIFO_SIZE - dev->current_len;

    if (copy_from_user(dev->mem + dev->current_len, buf, count)) {
        ret = -EFAULT;
        goto out;
    } else {
        dev->current_len += count;
        printk(KERN_INFO "written %zu bytes, current_len: %u\n", count, dev->current_len);
    
        wake_up_interruptible(&dev->r_wait);

        ret = count;
    }

out:
out2:
    remove_wait_queue(&dev->w_wait, &wait);
    set_current_state(TASK_RUNNING);
    return ret;
}

static const struct file_operations globalfifo_fops = {
    .owner = THIS_MODULE,
    .open = globalfifo_open,
    .release = globalfifo_release,
    .read = globalfifo_read,
    .write = globalfifo_write,
};

static void globalfifo_setup_cdev(struct globalfifo_dev *dev, int index)
{
    int err, devno = MKDEV(globalfifo_major, index);

    cdev_init(&dev->cdev, &globalfifo_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &globalfifo_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_WARNING "Error %d adding globalfifo%d", err, index);
}

static int __init globalfifo_init(void)
{
    int ret;
    dev_t devno = MKDEV(globalfifo_major, 0);

    if (globalfifo_major)
        ret = register_chrdev_region(devno, 1, "globalfifo");
    else {
        ret = alloc_chrdev_region(&devno, 0, 1, "globalfifo");
        globalfifo_major = MAJOR(devno);
    }
    if (ret < 0)
        return ret;

    globalfifo_devp = kzalloc(sizeof(struct globalfifo_dev), GFP_KERNEL);
    if (!globalfifo_devp) {
        unregister_chrdev_region(devno, 1);
        return -ENOMEM;
    }

    globalfifo_setup_cdev(globalfifo_devp, 0);

    mutex_init(&globalfifo_devp->mutex);
    init_waitqueue_head(&globalfifo_devp->r_wait);
    init_waitqueue_head(&globalfifo_devp->w_wait);

    return 0;
}

static void __exit globalfifo_exit(void)
{
    cdev_del(&globalfifo_devp->cdev);
    kfree(globalfifo_devp);
    unregister_chrdev_region(MKDEV(globalfifo_major, 0), 1);
}

MODULE_AUTHOR("plding");
MODULE_LICENSE("Dual BSD/GPL");

module_init(globalfifo_init);
module_exit(globalfifo_exit);
