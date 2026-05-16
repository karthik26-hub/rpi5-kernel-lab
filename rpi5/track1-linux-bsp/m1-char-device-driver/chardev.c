// SPDX-License-Identifier: GPL-2.0
/*
 * chardev.c - RPi5 Track 1, M1: Character Device Driver
 *
 * Implements a full character device with:
 *   - Dynamic major number allocation (alloc_chrdev_region)
 *   - cdev registration (cdev_init, cdev_add)
 *   - Automatic /dev/chardev node via class_create + device_create
 *   - Kernel buffer (4KB) with read/write/llseek support
 *   - open/release with access tracking
 *   - ioctl: CHARDEV_CLEAR (clear buffer), CHARDEV_GET_LEN (get data length)
 *   - sysfs: /sys/kernel/chardev/ with stats (opens, reads, writes, bytes)
 *
 * Build:   make
 * Load:    sudo insmod chardev.ko
 * Test:    make test   OR   sudo ./test_chardev
 * Remove:  sudo rmmod chardev
 *
 * Usage from shell:
 *   echo "hello kernel" | sudo tee /dev/chardev
 *   sudo cat /dev/chardev
 *   cat /sys/kernel/chardev/stats
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/utsname.h>
#include <linux/ioctl.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Karthik Nambiar");
MODULE_DESCRIPTION("RPi5 Track1 M1 - Character device driver with ioctl + sysfs");
MODULE_VERSION("1.0");

/* ------------------------------------------------------------------ */
/* Defines                                                              */
/* ------------------------------------------------------------------ */

#define DEVICE_NAME   "chardev"
#define CLASS_NAME    "chardev_class"
#define BUF_SIZE      4096

/* ioctl magic + commands */
#define CHARDEV_IOC_MAGIC  'k'
#define CHARDEV_CLEAR      _IO(CHARDEV_IOC_MAGIC,  0)   /* clear buffer */
#define CHARDEV_GET_LEN    _IOR(CHARDEV_IOC_MAGIC, 1, int) /* get data len */

/* ------------------------------------------------------------------ */
/* Device state                                                         */
/* ------------------------------------------------------------------ */

static dev_t         dev_num;       /* major:minor */
static struct cdev   chardev_cdev;
static struct class  *chardev_class;
static struct device *chardev_device;

static char    *kbuf;               /* 4KB kernel buffer */
static size_t   kbuf_len = 0;       /* bytes currently in buffer */
static DEFINE_MUTEX(chardev_mutex);

/* Stats */
static atomic_t stat_opens  = ATOMIC_INIT(0);
static atomic_t stat_reads  = ATOMIC_INIT(0);
static atomic_t stat_writes = ATOMIC_INIT(0);
static atomic_t stat_bytes  = ATOMIC_INIT(0);

/* sysfs kobject */
static struct kobject *chardev_kobj;

/* ------------------------------------------------------------------ */
/* file_operations                                                      */
/* ------------------------------------------------------------------ */

static int chardev_open(struct inode *inode, struct file *filp)
{
    atomic_inc(&stat_opens);
    printk(KERN_INFO "chardev: open() — total opens: %d\n",
           atomic_read(&stat_opens));
    return 0;
}

static int chardev_release(struct inode *inode, struct file *filp)
{
    printk(KERN_INFO "chardev: release()\n");
    return 0;
}

static ssize_t chardev_read(struct file *filp, char __user *ubuf,
                             size_t count, loff_t *ppos)
{
    ssize_t ret;

    if (mutex_lock_interruptible(&chardev_mutex))
        return -ERESTARTSYS;

    if (*ppos >= kbuf_len) {
        ret = 0;   /* EOF */
        goto out;
    }

    if (count > kbuf_len - *ppos)
        count = kbuf_len - *ppos;

    if (copy_to_user(ubuf, kbuf + *ppos, count)) {
        ret = -EFAULT;
        goto out;
    }

    *ppos += count;
    atomic_inc(&stat_reads);
    printk(KERN_INFO "chardev: read() %zu bytes (pos=%lld)\n", count, *ppos);
    ret = count;

out:
    mutex_unlock(&chardev_mutex);
    return ret;
}

static ssize_t chardev_write(struct file *filp, const char __user *ubuf,
                              size_t count, loff_t *ppos)
{
    ssize_t ret;

    if (mutex_lock_interruptible(&chardev_mutex))
        return -ERESTARTSYS;

    if (count > BUF_SIZE) {
        printk(KERN_WARNING "chardev: write truncated to %d bytes\n", BUF_SIZE);
        count = BUF_SIZE;
    }

    if (copy_from_user(kbuf, ubuf, count)) {
        ret = -EFAULT;
        goto out;
    }

    kbuf_len = count;
    *ppos = count;
    atomic_inc(&stat_writes);
    atomic_add(count, &stat_bytes);
    printk(KERN_INFO "chardev: write() %zu bytes\n", count);
    ret = count;

out:
    mutex_unlock(&chardev_mutex);
    return ret;
}

static loff_t chardev_llseek(struct file *filp, loff_t offset, int whence)
{
    loff_t newpos;

    switch (whence) {
    case SEEK_SET: newpos = offset; break;
    case SEEK_CUR: newpos = filp->f_pos + offset; break;
    case SEEK_END: newpos = kbuf_len + offset; break;
    default: return -EINVAL;
    }

    if (newpos < 0 || newpos > BUF_SIZE)
        return -EINVAL;

    filp->f_pos = newpos;
    return newpos;
}

static long chardev_ioctl(struct file *filp, unsigned int cmd,
                           unsigned long arg)
{
    int len;

    switch (cmd) {
    case CHARDEV_CLEAR:
        mutex_lock(&chardev_mutex);
        memset(kbuf, 0, BUF_SIZE);
        kbuf_len = 0;
        mutex_unlock(&chardev_mutex);
        printk(KERN_INFO "chardev: ioctl CLEAR — buffer wiped\n");
        return 0;

    case CHARDEV_GET_LEN:
        len = (int)kbuf_len;
        if (copy_to_user((int __user *)arg, &len, sizeof(len)))
            return -EFAULT;
        printk(KERN_INFO "chardev: ioctl GET_LEN — %d bytes\n", len);
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations chardev_fops = {
    .owner          = THIS_MODULE,
    .open           = chardev_open,
    .release        = chardev_release,
    .read           = chardev_read,
    .write          = chardev_write,
    .llseek         = chardev_llseek,
    .unlocked_ioctl = chardev_ioctl,
};

/* ------------------------------------------------------------------ */
/* sysfs — /sys/kernel/chardev/stats                                   */
/* ------------------------------------------------------------------ */

static ssize_t stats_show(struct kobject *kobj, struct kobj_attribute *attr,
                           char *buf)
{
    return sysfs_emit(buf,
        "opens:       %d\n"
        "reads:       %d\n"
        "writes:      %d\n"
        "bytes_total: %d\n"
        "buf_used:    %zu\n"
        "buf_size:    %d\n",
        atomic_read(&stat_opens),
        atomic_read(&stat_reads),
        atomic_read(&stat_writes),
        atomic_read(&stat_bytes),
        kbuf_len,
        BUF_SIZE);
}

static struct kobj_attribute attr_stats = __ATTR_RO(stats);
static struct attribute *chardev_attrs[] = { &attr_stats.attr, NULL };
static struct attribute_group chardev_attr_group = { .attrs = chardev_attrs };

/* ------------------------------------------------------------------ */
/* Module init / exit                                                   */
/* ------------------------------------------------------------------ */

static int __init chardev_init(void)
{
    int ret;

    printk(KERN_INFO "chardev: loading on %s\n", utsname()->release);

    /* 1. Allocate kernel buffer */
    kbuf = kzalloc(BUF_SIZE, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    /* 2. Allocate major:minor dynamically */
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "chardev: alloc_chrdev_region failed: %d\n", ret);
        goto err_free_buf;
    }
    printk(KERN_INFO "chardev: allocated major=%d minor=%d\n",
           MAJOR(dev_num), MINOR(dev_num));

    /* 3. Init and add cdev */
    cdev_init(&chardev_cdev, &chardev_fops);
    chardev_cdev.owner = THIS_MODULE;
    ret = cdev_add(&chardev_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "chardev: cdev_add failed: %d\n", ret);
        goto err_unreg_chrdev;
    }

    /* 4. Create device class */
    chardev_class = class_create(CLASS_NAME);
    if (IS_ERR(chardev_class)) {
        ret = PTR_ERR(chardev_class);
        printk(KERN_ERR "chardev: class_create failed: %d\n", ret);
        goto err_cdev_del;
    }

    /* 5. Create /dev/chardev automatically */
    chardev_device = device_create(chardev_class, NULL, dev_num,
                                   NULL, DEVICE_NAME);
    if (IS_ERR(chardev_device)) {
        ret = PTR_ERR(chardev_device);
        printk(KERN_ERR "chardev: device_create failed: %d\n", ret);
        goto err_class_destroy;
    }

    /* 6. Create sysfs /sys/kernel/chardev/ */
    chardev_kobj = kobject_create_and_add(DEVICE_NAME, kernel_kobj);
    if (!chardev_kobj) {
        ret = -ENOMEM;
        goto err_device_destroy;
    }
    ret = sysfs_create_group(chardev_kobj, &chardev_attr_group);
    if (ret)
        goto err_kobj_put;

    printk(KERN_INFO "chardev: /dev/chardev created (major=%d)\n",
           MAJOR(dev_num));
    printk(KERN_INFO "chardev: echo 'hello' | sudo tee /dev/chardev\n");
    printk(KERN_INFO "chardev: sudo cat /dev/chardev\n");
    printk(KERN_INFO "chardev: cat /sys/kernel/chardev/stats\n");
    return 0;

err_kobj_put:
    kobject_put(chardev_kobj);
err_device_destroy:
    device_destroy(chardev_class, dev_num);
err_class_destroy:
    class_destroy(chardev_class);
err_cdev_del:
    cdev_del(&chardev_cdev);
err_unreg_chrdev:
    unregister_chrdev_region(dev_num, 1);
err_free_buf:
    kfree(kbuf);
    return ret;
}

static void __exit chardev_exit(void)
{
    sysfs_remove_group(chardev_kobj, &chardev_attr_group);
    kobject_put(chardev_kobj);
    device_destroy(chardev_class, dev_num);
    class_destroy(chardev_class);
    cdev_del(&chardev_cdev);
    unregister_chrdev_region(dev_num, 1);
    kfree(kbuf);
    printk(KERN_INFO "chardev: unloaded\n");
}

module_init(chardev_init);
module_exit(chardev_exit);
