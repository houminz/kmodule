#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

struct scull_dev {
    int size;
    void *data;
    struct cdev cdev;
};

#define SCULL_SIZE 1024

static int major = 0;
static int minor = 0;
static int count = 6;
static dev_t dev_num;
static char *scull_name = "scull";
static struct scull_dev *scull_device;
static struct class *cls = NULL;

static inline void clean_up_scull_data(struct scull_dev *sdev)
{
    kfree(sdev->data);
    sdev->data = NULL;
}

static int scull_open(struct inode *inode, struct file *fp)
{
    struct scull_dev *sdev = container_of(inode->i_cdev, struct scull_dev, cdev);

    if ((fp->f_flags &O_ACCMODE) == O_WRONLY) {
        clean_up_scull_data(sdev);
    }

    fp->private_data = sdev;

    printk(KERN_INFO "scull open!\n");
    return 0;
}

static ssize_t scull_write(struct file *fp, const char __user *buffer, size_t st, loff_t * pos)
{
    struct scull_dev *sdev = fp->private_data;
    int result = -ENOMEM;

    if (!sdev->data) {
        sdev->data = kmalloc(SCULL_SIZE, GFP_KERNEL);
        if (!sdev->data) {
            goto nomem;
        }
    }

    result = (sdev->size - *pos) > st ? st : (sdev->size - *pos);
    printk(KERN_INFO "scull_write : result = %d st = %ld pos = %lld\n", result, st, *pos);
    copy_from_user((sdev->data + *pos), buffer, result);

    *pos += result;
nomem:
    printk(KERN_INFO "scull_write char: %d!\n", result);
    return result;
}

static ssize_t scull_read(struct file *fp, char __user *buffer, size_t st, loff_t *pos)
{
    struct scull_dev *sdev = fp->private_data;
    int result;

    if (!sdev->data) {
        result = 0;
        goto nodata;
    }


    result = (sdev->size - *pos) > st ? st : (sdev->size - *pos);
    printk(KERN_INFO "scull_read : result = %d st = %ld pos = %lld\n", result, st, *pos);
    copy_to_user(buffer, (sdev->data + *pos), result);

    *pos += result;

nodata:
    printk(KERN_INFO "scull_read char: %d!\n", result);
    return result;
}

static int scull_release(struct inode *inode, struct file *fp)
{
    printk(KERN_INFO "scull release!\n");
    return 0;
}

static loff_t scull_llseek(struct file *fp, loff_t off, int whence)
{
    loff_t result;
    struct scull_dev *sdev = fp->private_data;

    switch(whence) {
    case 0:
        result = off;
        break;
    case 1:
        result = fp->f_pos + off;
        break;
    case 2:
        result = sdev->size + off;
        break;
    default:
        return -EINVAL;
    }

    if (result > SCULL_SIZE) {
        return -EINVAL;
    }

    fp->f_pos = result;

    printk(KERN_INFO "scull llseek!\n");
    return result;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = scull_open,
    .write = scull_write,
    .read = scull_read,
    .release = scull_release,
    .llseek = scull_llseek,
};

static int scull_init(void)
{
    int i;
    int result = 0;
    struct device *devp = NULL;

    // 1. alloc chrdev region
    result = alloc_chrdev_region(&dev_num, minor, count, scull_name);
    if (result < 0) {
        printk(KERN_ALERT "register dev error!\n");
        goto ERR_FINAL;
    }
    major = MAJOR(dev_num);

    // 2. create scull_dev instance
    scull_device = kmalloc(sizeof(struct scull_dev), GFP_KERNEL);
    if (!scull_device) {
        printk(KERN_ALERT "no memory!\n");
        result = -ENOMEM;
        goto ERR_KMALLOC;
    }

    memset(scull_device, 0, sizeof(struct scull_dev));
    scull_device->size = SCULL_SIZE;

    // 3. init cdev object
    cdev_init(&scull_device->cdev, &fops);
    scull_device->cdev.owner = THIS_MODULE;
    scull_device->cdev.ops = &fops;

    // 4. register cdev object
    if (cdev_add(&scull_device->cdev, dev_num, count)) {
        printk(KERN_ALERT "ERROR: cdev_add %d\n", count);
        goto ERR_REGISTER_CDEV;
    }

    // 5. create class
    cls = class_create(THIS_MODULE, scull_name);
    if (IS_ERR(cls)) {
        result = PTR_ERR(cls);
        goto ERR_CREATE_CLASS;
    }

    // 6. create devices
    for (i = minor; i < (count + minor); i++) {
        devp = device_create(cls, NULL, MKDEV(major, i), NULL, "%s%d", scull_name, i);
        if (IS_ERR(devp)) {
            result = PTR_ERR(devp);
            goto ERR_CREATE_DEVICE;
        }
    }

    printk(KERN_INFO "%s init, major = %d\n", scull_name, major);

    return 0;

ERR_CREATE_DEVICE:
    for (--i; i >= minor; i--) {
        device_destroy(cls, MKDEV(major, i));
    }
    class_destroy(cls);

ERR_CREATE_CLASS:
ERR_REGISTER_CDEV:
    cdev_del(&scull_device->cdev);

    kfree(scull_device);
    scull_device = NULL;
ERR_KMALLOC:
    unregister_chrdev_region(dev_num, count);

ERR_FINAL:
    printk(KERN_INFO "%s : %s : %d - fail.\n", __FILE__, __func__, __LINE__);

    return result;

}

static void scull_exit(void)
{
    int i;

    for (i = minor; i < (minor + count); i++) {
        device_destroy(cls, MKDEV(major, i));
    }
    class_destroy(cls);

    cdev_del(&scull_device->cdev);
    clean_up_scull_data(scull_device);
    kfree(scull_device);
    scull_device = NULL;
    unregister_chrdev_region(dev_num, count);

    printk(KERN_INFO "%s exit!\n", scull_name);
}


module_init(scull_init);
module_exit(scull_exit);
MODULE_AUTHOR("houmin.wei@outlook.com");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("A Char Device Driver");
