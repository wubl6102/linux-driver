#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define GLOBALMEM2_MAJOR        201
#define GLOBALMEM2_DEVICE_NUM   10
#define GLOBALMEM2_SIZE         0x1000

static int globalmem2_major = GLOBALMEM2_MAJOR;

struct globalmem2_dev {
    struct cdev cdev;
    unsigned char mem[GLOBALMEM2_SIZE];
} *globalmem2_devp;

static int globalmem2_open(struct inode *inode, struct file *filp)
{
    filp->private_data = container_of(inode->i_cdev, struct globalmem2_dev, cdev);
    return 0;
}

static int globalmem2_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t globalmem2_read(struct file *filp, char __user *buf,
    size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int count = size;
    struct globalmem2_dev *dev = filp->private_data;

    if (p >= GLOBALMEM2_SIZE)
        return 0;

    if (count > GLOBALMEM2_SIZE - p)
        count = GLOBALMEM2_SIZE - p;

    printk(KERN_INFO "read %u bytes from %lu\n", count, p);
    if (copy_to_user(buf, dev->mem + p, count) != 0)
        return -EFAULT;

    *ppos += count;
    return count;
}

static ssize_t globalmem2_write(struct file *filp, const char __user *buf,
    size_t size, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int count = size;
    struct globalmem2_dev *dev = filp->private_data;

    if (p >= GLOBALMEM2_SIZE)
        return 0;

    if (count > GLOBALMEM2_SIZE - p)
        count = GLOBALMEM2_SIZE - p;

    printk(KERN_INFO "written %u bytes from %lu\n", count, p);
    if (copy_from_user(dev->mem + p, buf, count) != 0)
        return -EFAULT;

    *ppos += count;
    return count;
}

static loff_t globalmem2_llseek(struct file *filp, loff_t offset, int whence)
{
    switch (whence) {
    case 0:
        if (offset < 0 || offset > GLOBALMEM2_SIZE)
            return -EINVAL;
        return filp->f_pos = offset;

    case 1:
        if (filp->f_pos + offset < 0 || filp->f_pos + offset > GLOBALMEM2_SIZE)
            return -EINVAL;
        return (filp->f_pos += offset);

    case 2:
        if (offset > 0 || GLOBALMEM2_SIZE + offset < 0)
            return -EINVAL;
        return (filp->f_pos = GLOBALMEM2_SIZE + offset);

    default:
        return -EINVAL;
    }
}

static long globalmem2_ioctl(struct file *filp,
    unsigned int cmd, unsigned long arg)
{
    struct globalmem2_dev *dev = filp->private_data;

    switch (cmd) {
    case 0x01:
        memset(dev->mem, 0, GLOBALMEM2_SIZE);
        printk(KERN_INFO "globalmem2 is set to zero\n");
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

static const struct file_operations globalmem2_fops = {
    .owner = THIS_MODULE,
    .open = globalmem2_open,
    .release = globalmem2_release,
    .read = globalmem2_read,
    .write = globalmem2_write,
    .llseek = globalmem2_llseek,
    .unlocked_ioctl = globalmem2_ioctl,
};

static int __init globalmem2_init(void)
{
    int result, i;
    dev_t devno = MKDEV(globalmem2_major, 0);

    if (globalmem2_major)
        result = register_chrdev_region(devno, GLOBALMEM2_DEVICE_NUM, "globalmem2");
    else {
        result = alloc_chrdev_region(&devno, 0, GLOBALMEM2_DEVICE_NUM, "globalmem2");
        globalmem2_major = MAJOR(devno);
    }
    if (result < 0)
        return result;

    globalmem2_devp = kmalloc(GLOBALMEM2_DEVICE_NUM * sizeof(struct globalmem2_dev), GFP_KERNEL);
    if (!globalmem2_devp) {
        unregister_chrdev_region(devno, GLOBALMEM2_DEVICE_NUM);
        return -ENOMEM;
    }
    memset(globalmem2_devp, 0, GLOBALMEM2_DEVICE_NUM * sizeof(struct globalmem2_dev));

    for (i = 0; i < GLOBALMEM2_DEVICE_NUM; ++i) {
        dev_t devno = MKDEV(globalmem2_major, i);

        cdev_init(&(globalmem2_devp + i)->cdev, &globalmem2_fops);
        cdev_add(&(globalmem2_devp + i)->cdev, devno, 1);
    }

    return 0;
}

static void __exit globalmem2_exit(void)
{
    int i;

    for (i = 0; i < GLOBALMEM2_DEVICE_NUM; ++i)
        cdev_del(&(globalmem2_devp + i)->cdev);

    kfree(globalmem2_devp);
    unregister_chrdev_region(MKDEV(globalmem2_major, 0), GLOBALMEM2_DEVICE_NUM);
}

MODULE_AUTHOR("plding");
MODULE_LICENSE("Dual BSD/GPL");

module_init(globalmem2_init);
module_exit(globalmem2_exit);
