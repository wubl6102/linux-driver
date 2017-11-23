#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define GLOBALMEM_SIZE  10
#define MEM_CLEAR       0x01
#define GLOBALMEM_MAJOR 230
#define DEVICE_NUM      10

static int globalmem_major = GLOBALMEM_MAJOR;

struct globalmem_dev {
    struct cdev cdev;
    unsigned char mem[GLOBALMEM_SIZE];
};

struct globalmem_dev *globalmem_devp;

static int globalmem_open(struct inode *inode, struct file *filp)
{
    filp->private_data = container_of(inode->i_cdev, struct globalmem_dev, cdev);
    return 0;
}

static int globalmem_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static ssize_t globalmem_read(struct file *filp, char __user *buf, size_t count,
    loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int size = count;
    int ret = 0;
    struct globalmem_dev *dev = filp->private_data;

    if (p > GLOBALMEM_SIZE)
        return 0;
    if (size > GLOBALMEM_SIZE - p)
        size = GLOBALMEM_SIZE - p;

    if (copy_to_user(buf, dev->mem + p, size)) {
        ret = -EFAULT;
    } else {
        *ppos += size;
        ret = size;

        printk(KERN_INFO "read %u byte(s) from %lu\n", size, p);
    }

    return ret;
}

static ssize_t globalmem_write(struct file *filp, const char __user *buf,
    size_t count, loff_t *ppos)
{
    unsigned long p = *ppos;
    unsigned int size = count;
    int ret = 0;
    struct globalmem_dev *dev = filp->private_data;

    if (p > GLOBALMEM_SIZE)
        return 0;
    if (size > GLOBALMEM_SIZE - p)
        size = GLOBALMEM_SIZE - p;

    if (copy_from_user(dev->mem + p, buf, size)) {
        ret = -EFAULT;
    } else {
        *ppos += size;
        ret = size;

        printk(KERN_INFO "written %u byte(s) from %lu\n", size, p);
    }

    return ret;
}

static loff_t globalmem_llseek(struct file *filp, loff_t offset, int orig)
{
    loff_t ret = 0;

    switch (orig) {

    case 0:
        if (offset < 0 || (unsigned int) offset > GLOBALMEM_SIZE) {
            ret = -EINVAL;
            break;
        }
        filp->f_pos = (unsigned int) offset;
        ret = filp->f_pos;
        break;

    case 1:
        if (filp->f_pos + offset < 0 || filp->f_pos + offset > GLOBALMEM_SIZE) {
            ret = -EINVAL;
            break;
        }
        filp->f_pos += offset;
        ret = filp->f_pos;
        break;

    case 2:
        if (offset > 0 || -offset >= GLOBALMEM_SIZE) {
            ret = -EINVAL;
            break;
        }
        filp->f_pos = GLOBALMEM_SIZE + offset;
        ret = filp->f_pos;
        break;

    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

static long globalmem_ioctl(struct file *filp, unsigned int cmd,
    unsigned long arg)
{
    struct globalmem_dev *dev = filp->private_data;

    switch (cmd) {

    case MEM_CLEAR:
        memset(dev->mem, 0, GLOBALMEM_SIZE);
        printk(KERN_INFO "globalmem is set to zero\n");
        break;

    default:
        return -EINVAL;
    }

    return 0;
}

static const struct file_operations globalmem_fops = {
    .owner = THIS_MODULE,
    .open = globalmem_open,
    .release = globalmem_release,
    .read = globalmem_read,
    .write = globalmem_write,
    .llseek = globalmem_llseek,
    .unlocked_ioctl = globalmem_ioctl,
};

static void globalmem_setup_dev(struct globalmem_dev *dev, int index)
{
    int err, devno = MKDEV(globalmem_major, index);

    cdev_init(&dev->cdev, &globalmem_fops);
    dev->cdev.owner = THIS_MODULE;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_NOTICE "Error %d adding globalmem%d\n", err, index);
}

static int __init globalmem_init(void)
{
    int i, ret;
    dev_t devno;

    if (globalmem_major) {
        devno = MKDEV(globalmem_major, 0);
        ret = register_chrdev_region(devno, DEVICE_NUM, "globalmem2");
    } else {
        ret = alloc_chrdev_region(&devno, 0, DEVICE_NUM, "globalmem2");
        globalmem_major = MAJOR(devno);
    }
    if (ret < 0)
        return ret;

    globalmem_devp = kzalloc(sizeof(struct globalmem_dev) * DEVICE_NUM, GFP_KERNEL);
    if (!globalmem_devp) {
        ret = -ENOMEM;
        goto fail_malloc;
    }

    for (i = 0; i < DEVICE_NUM; i++) {
        globalmem_setup_dev(globalmem_devp + i, i);
    }

    return 0;

fail_malloc:
    unregister_chrdev_region(devno, DEVICE_NUM);
    return ret;
}

static void __exit globalmem_exit(void)
{
    int i;

    for (i = 0; i < DEVICE_NUM; i++)
        cdev_del(&globalmem_devp[i].cdev);
    kfree(globalmem_devp);
    unregister_chrdev_region(MKDEV(globalmem_major, 0), DEVICE_NUM);
}

MODULE_AUTHOR("Ding Peilong");
MODULE_LICENSE("Dual BSD/GPL");

module_init(globalmem_init);
module_exit(globalmem_exit);
