#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include "simulator.h"

struct simulator_device {
    __s32             val;    /* 使用这4个字节的内存来模拟硬件 */
    struct miscdevice misc;   /* 将这个设备定义为misc设备 */
    struct mutex      mutex;  /* 使用mutex保护缓冲区 */
};

static struct simulator_device *get_simulator_from_minor(int minor);

static ssize_t simulator_read(struct file *file, char __user *buf,
                              size_t count, loff_t *pos )
{
    struct simulator_device *sdev = file->private_data;
    ssize_t ret = 0;

    mutex_lock(&(sdev->mutex));
    if (count < sizeof (sdev->val)) {
        goto out;
    }
    /* 将四字节内存val的值拷贝到用户提供的缓冲区中 */
    if (copy_to_user(buf, &(sdev->val), sizeof (sdev->val))) {
        ret = -EFAULT;
        goto out;
    }
    ret = sizeof (sdev->val);

 out:
    mutex_unlock(&(sdev->mutex));
    return ret;
}

static ssize_t simulator_write(struct file *file, const char __user *buf,
                               size_t count, loff_t *pos )
{
    struct simulator_device *sdev = file->private_data;
    ssize_t ret = 0;

    mutex_lock(&(sdev->mutex));
    if (count > sizeof (sdev->val)) {
        goto out;
    }
    /* 将用户提供的缓冲区的值写到设备寄存器中 */
    if (copy_from_user(&(sdev->val), buf, count)) {
        ret = -EFAULT;
        goto out;
    }
    ret = sizeof (sdev->val);

 out:
    mutex_unlock(&(sdev->mutex));
    return ret;
}

static int simulator_open(struct inode *inode, struct file *file)
{
    struct simulator_device *sdev;

    sdev = get_simulator_from_minor(MINOR(inode->i_rdev));
    if (!sdev) {
        return -ENODEV;
    }
    file->private_data = sdev;

    return 0;
}

static int simulator_release(struct inode *ignored, struct file *file)
{
    return 0;
}

static const struct file_operations simulator_fops = {
    .owner   = THIS_MODULE,
    .read    = simulator_read,
    .write   = simulator_write,
    .open    = simulator_open,
    .release = simulator_release,
};

#define DEFINE_SIMULATOR_DEVICE(VAR, NAME, VAL)       \
    static struct simulator_device VAR = {            \
        .val  = VAL,                                  \
        .misc = {                                     \
            .minor  = MISC_DYNAMIC_MINOR,             \
            .name   = NAME,                           \
            .fops   = &simulator_fops,                \
            .parent = NULL,                           \
        },                                            \
        .mutex = __MUTEX_INITIALIZER(VAR.mutex),      \
    };

DEFINE_SIMULATOR_DEVICE(simulator_main, SIMULATOR_NAME, 0)

static struct simulator_device *get_simulator_from_minor(int minor)
{
    if (simulator_main.misc.minor == minor) {
        return &simulator_main;
    }

    return NULL;
}

static int __init simulator_init(void)
{
    int ret;

    ret = misc_register( &(simulator_main.misc) );
    if (unlikely(ret)) {
        printk(KERN_ERR "simulator: failed to register misc "
               "device for simulator '%s'!\n", simulator_main.misc.name );
        return ret;
    }

    return ret;
}

device_initcall(simulator_init);
