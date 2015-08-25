simulator kernel
========================================

Sources
----------------------------------------

path: kernel/drivers/staging/simulator/

---simulator.h
---simulator.c
---Kconfig
---Makefile

simulator.h
```
#ifndef _LINUX_SIMULATOR_H
#define _LINUX_SIMULATOR_H

#define SIMULATOR_NAME "simulator_main"

#endif    /* _LINUX_SIMULATOR_H */
```

simulator.c
```
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

    /* 将用户提供的缓冲区的值写到四字节内存val中 */
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
```

Kconfig
```
config SIMULATOR_DEVICE
       tristate "Simulator Device Driver"
       default y
       help
       This is the simulator device driver for android system
```

Makefile
```
obj-$(CONFIG_SIMULATOR_DEVICE) += simulator.o
```

注意: $(CONFIG_SIMULATOR_DEVICE)是一个变量，其与驱动程序simulator的编译选项有关系
为了将这个模拟的硬件设备驱动程序编译进bootimage我们还需要做如下修改，并验证驱动程序是否正确:

1.修改内核驱动Kconfig文件, 添加如下信息:

path: kernel/drivers/Kconfig
```
menu “Device Drivers”
......
source "drivers/staging/simulator/Kconfig"
endmenu
```

将驱动程序simulator的Kconfig包含进来

2.修改内核驱动Makefile文件添加如下信息:

path: kernel/drivers/staging/Makefile
```
obj-$(CONFIG_SIMULATOR_DEVICE) += simulator/
```

3.修改完如上所示信息以后,运行命令:

```
$ make bootimage -j4
```

将驱动程序编译进bootimage中，然后将重新刷机器的bootimage，重启后就能在/dev/下看到名称为
simulator_main的设备了.

4.验证驱动程序是否正确的程序：

path: external/simulator/simulator.c
```
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#define SIMULATOR_DEVICE_NAME "/dev/simulator_main"

int main( int argc, char** argv )
{
    int fd  = -1;
    int val = 0;

    fd = open(SIMULATOR_DEVICE_NAME, 0666, O_RDWR );
    if ( fd == -1 ) {
        printf( "Failed to open device %s.\n", SIMULATOR_DEVICE_NAME );
        return -1;
    }

    printf( "Read original value:\n" );
    read( fd, &val, sizeof (val) );
    printf( "%d.\n\n", val );

    val = 7;
    printf( "Write value %d to %s.\n\n", val, SIMULATOR_DEVICE_NAME );
    write( fd, &val, sizeof (val) );

    printf( "Read the value again:\n" );
    read( fd, &val, sizeof (val) );
    printf( "%d.\n\n", val );

    close( fd );
    return 0;
}
```

path: external/simulator/Android.mk

```
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := simulator
LOCAL_SRC_FILES := $(call all-subdir-c-files)

include $(BUILD_EXECUTABLE)
```

将上述验证程序编译完生成的simulator可执行文件push到机器上的某个目录下执行，验证结果如下：

```
Read original value:
0.
Write value 7 to /dev/simulator_main.
Read the value again:
7.
```