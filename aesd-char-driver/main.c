/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major =   0;
int aesd_minor =   0;

MODULE_AUTHOR("zhya2609");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;
    PDEBUG("open");
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_buffer_entry *entry;
    size_t entry_offset;
    ssize_t retval = 0;
    size_t bytes_to_read;

    PDEBUG("read %zu bytes with offset %lld", count, *f_pos);

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer,
                *f_pos, &entry_offset);

    if (entry == NULL)
        goto out;

    bytes_to_read = entry->size - entry_offset;
    if (bytes_to_read > count)
        bytes_to_read = count;

    if (copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_read)) {
        retval = -EFAULT;
        goto out;
    }

    *f_pos += bytes_to_read;
    retval = bytes_to_read;

out:
    mutex_unlock(&dev->lock);
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    struct aesd_dev *dev = filp->private_data;
    ssize_t retval = -ENOMEM;
    char *new_buf;
    const char *newline;

    PDEBUG("write %zu bytes with offset %lld", count, *f_pos);

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    new_buf = krealloc(dev->working_entry, dev->working_entry_size + count, GFP_KERNEL);
    if (!new_buf)
        goto out;

    dev->working_entry = new_buf;

    if (copy_from_user(dev->working_entry + dev->working_entry_size, buf, count)) {
        retval = -EFAULT;
        goto out;
    }

    dev->working_entry_size += count;
    retval = count;

    newline = memchr(dev->working_entry, '\n', dev->working_entry_size);
    if (newline) {
        struct aesd_buffer_entry new_entry;
        const char *old_buffptr;

        new_entry.buffptr = dev->working_entry;
        new_entry.size = dev->working_entry_size;

        old_buffptr = dev->buffer.entry[dev->buffer.in_offs].buffptr;
        if (dev->buffer.full && old_buffptr)
            kfree(old_buffptr);

        aesd_circular_buffer_add_entry(&dev->buffer, &new_entry);

        dev->working_entry = NULL;
        dev->working_entry_size = 0;
    }

out:
    mutex_unlock(&dev->lock);
    return retval;
}

loff_t aesd_llseek(struct file *filp, loff_t offset, int whence)
{
    struct aesd_dev *dev = filp->private_data;
    loff_t total_size = 0;
    loff_t new_pos;
    uint8_t i;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (dev->buffer.entry[i].buffptr)
            total_size += dev->buffer.entry[i].size;
    }

    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = filp->f_pos + offset;
        break;
    case SEEK_END:
        new_pos = total_size + offset;
        break;
    default:
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    if (new_pos < 0 || new_pos > total_size) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    filp->f_pos = new_pos;
    mutex_unlock(&dev->lock);
    return new_pos;
}

long aesd_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct aesd_dev *dev = filp->private_data;
    struct aesd_seekto seekto;
    loff_t new_pos = 0;
    uint32_t i;
    uint8_t num_entries;
    uint8_t idx;

    if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC)
        return -ENOTTY;
    if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR)
        return -ENOTTY;

    if (cmd != AESDCHAR_IOCSEEKTO)
        return -ENOTTY;

    if (copy_from_user(&seekto, (struct aesd_seekto __user *)arg, sizeof(seekto)))
        return -EFAULT;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    num_entries = dev->buffer.full ? AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED
                                   : dev->buffer.in_offs;

    if (seekto.write_cmd >= num_entries) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    idx = (dev->buffer.out_offs + seekto.write_cmd) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    if (seekto.write_cmd_offset >= dev->buffer.entry[idx].size) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    for (i = 0; i < seekto.write_cmd; i++) {
        uint8_t cur_idx = (dev->buffer.out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        new_pos += dev->buffer.entry[cur_idx].size;
    }
    new_pos += seekto.write_cmd_offset;

    filp->f_pos = new_pos;
    mutex_unlock(&dev->lock);
    return 0;
}

struct file_operations aesd_fops = {
    .owner          = THIS_MODULE,
    .llseek         = aesd_llseek,
    .read           = aesd_read,
    .write          = aesd_write,
    .open           = aesd_open,
    .release        = aesd_release,
    .unlocked_ioctl = aesd_unlocked_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err)
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device, 0, sizeof(struct aesd_dev));

    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);

    result = aesd_setup_cdev(&aesd_device);

    if (result)
        unregister_chrdev_region(dev, 1);
    return result;
}

void aesd_cleanup_module(void)
{
    uint8_t index;
    struct aesd_buffer_entry *entry;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
        if (entry->buffptr)
            kfree(entry->buffptr);
    }

    kfree(aesd_device.working_entry);

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
