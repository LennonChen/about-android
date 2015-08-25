simulator hal
========================================

为我们用内存模拟的硬件设备编写硬件抽象层接口每一个硬件抽象层模块在内核都对应一个驱动程序，
硬件抽象层模块就是通过驱动程序来访问硬件设备的。

硬件抽象层中的模块接口源文件一般保存在hardware/libhardware目录中.

Sources:
----------------------------------------

path: hardware/libhardware/simulator
```
simulator.cpp simulator.h Android.mk
```

path: simulator.h:

```
#ifndef ANDROID_INCLUDE_HARDWARE_SIMULATOR_H
#define ANDROID_INCLUDE_HARDWARE_SIMULATOR_H

#include <hardware/hardware.h>

__BEGIN_DECLS

/* 定义模块ID */
#define SIMULATOR_MODULE_ID "simulator"

/* 定义设备ID */
#define SIMULATOR_DEVICE_ID "simulator"

typedef struct simulator_module_t {
    struct hw_module_t common;
} simulator_module_t;

typedef struct simulator_device_t {
    struct hw_device_t common;
    int fd;
    int (*set_val)(struct simulator_device_t *dev, int val);
    int (*get_val)(struct simulator_device_t *dev, int *val);
} simulator_device_t;

__END_DECLS

#endif  /* ANDROID_INCLUDE_HARDWARE_SIMULATOR_H */
```

path: simulator.cpp:
```
#include <hardware/simulator.h>

#include <fcntl.h>
#include <errno.h>

#define LOG_TAG "SimulatorHAL"
#include <utils/Log.h>

#define DEVICE_NAME   "/dev/simulator_main"
#define MODULE_NAME   "Simulator"
#define MODULE_AUTHOR "liminghao@xiaomi.com"

static int simulator_close(struct hw_device_t *device);
static int simulator_set_val(struct simulator_device_t *dev, int val);
static int simulator_get_val(struct simulator_device_t *dev, int *val);

static int simulator_open(const struct hw_module_t *module, const char *id,
                          struct hw_device_t **device)
{
    if (!strcmp(id, SIMULATOR_DEVICE_ID)) {
        struct simulator_device_t *dev = NULL;

        dev = (struct simulator_device_t*)
            malloc(sizeof (struct simulator_device_t));
        if (!dev) {
            ALOGE("simulator_open: failed to alloc memory for simulator_device_t.\n" );
            return -EFAULT;
        }
        memset(dev, 0, sizeof (struct simulator_device_t));
        dev->common.tag     = HARDWARE_DEVICE_TAG;
        dev->common.version = 0;
        dev->common.module  = (struct hw_module_t*)module;
        dev->common.close   = simulator_close;
        dev->set_val = simulator_set_val;
        dev->get_val = simulator_get_val;

        if ((dev->fd = open(DEVICE_NAME, O_RDWR)) < 0) {
            ALOGE("simulator_open: failed to open device file %s(%s)\n",
                  DEVICE_NAME, strerror(errno));
            free(dev);
            return -EFAULT;
        }
        *device = &(dev->common);

        return 0;
    }

    return -EFAULT;
}

static struct hw_module_methods_t simulator_module_methods = {
 open: simulator_open,
};


/* 注意, 必须为HAL_MODULE_INFO_SYM,其为一个导出符号 */
struct simulator_module_t HAL_MODULE_INFO_SYM = {
 common: {
    tag: HARDWARE_MODULE_TAG,
    version_major: 1,
    version_minor: 0,
    id: SIMULATOR_MODULE_ID,
    name: MODULE_NAME,
    author: MODULE_AUTHOR,
    methods: &simulator_module_methods,
 },
};

static int simulator_close(struct hw_device_t *device)
{
    struct simulator_device_t *simulator_device = (struct simulator_device_t*)device;

    if (simulator_device) {
        close(simulator_device->fd);
        free(simulator_device);
    }

    return 0;
}

static int simulator_set_val(struct simulator_device_t *dev, int val)
{
    if (!dev) {
        ALOGE("simulator_set_val: Null dev pointer!\n");
        return -EFAULT;
    }

    if (write(dev->fd, &val, sizeof (val)) < 0) {
        ALOGE("simulator_set_val: failed to write '%d' to dev '%d'.\n",
              val, dev->fd);
        return -EFAULT;
    }
    return 0;
}

static int simulator_get_val(struct simulator_device_t *dev, int *val)
{
    if (!dev) {
        ALOGE("simulator_get_val: Null dev pointer!\n");
        return -EFAULT;
    }
    if (read(dev->fd, val, sizeof (*val)) < 0) {
        ALOGE("simulator_get_val: failed to read '%d' from dev '%d'.\n",
              *val, dev->fd);
        return -EFAULT;
    }
    return 0;
}
```

Android.mk：

```
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_SHARED_LIBRARIES := liblog
LOCAL_SRC_FILES := simulator.cpp
LOCAL_MODULE := simulator.default

include $(BUILD_SHARED_LIBRARY)
```

运行命令：mmm hardware/libhardware/simulator/ 将编译生成的simulator.default.so文件
push到机器上/system/lib/hw/simulator.default.so重启。

硬件抽象层模块的加载过程
----------------------------------------

Android系统中的硬件抽象层模块是由系统统一加载的，当调用者需要加载这些模块时，只需要指定它们的ID
值即可.

path: hardware/libhardware/include/hardware/hardware.h

```
/**
 * Get the module info associated with a module by id.
 *
 * id - 输入参数，表示要加载的硬件抽象层模块ID;
 * module - 是输出参数，如果加载成功, 那么它指向一个自定义硬件抽象层模块结构体
 * @return: 0 == success, <0 == error and *module == NULL
 */
int hw_get_module(const char *id, const struct hw_module_t **module);
```

path：hardware/libhardware/hardware.c

```
/** Base path of the hal modules，
** 以下两个宏用来定义要加载的硬件抽象层模块文件所在目录 */
#define HAL_LIBRARY_PATH1 "/system/lib/hw"
#define HAL_LIBRARY_PATH2 "/vendor/lib/hw"    /* 设备厂商提供的 */

/**
 * There are a set of variant filename for modules. The form of the filename
 * is "<MODULE_ID>.variant.so" so for the led module the Dream variants
 * of base "ro.product.board", "ro.board.platform" and "ro.arch" would be:
 *
 * led.trout.so
 * led.msm7k.so
 * led.ARMV6.so
 * led.default.so
 */

static const char *variant_keys[] = {
    "ro.hardware",  /* This goes first so that it can pick up a different
                       file on the emulator. */
    "ro.build.product",
    "ro.product.board",
    "ro.board.platform",
    "ro.arch"
};

static const int HAL_VARIANT_KEYS_COUNT =
    (sizeof(variant_keys)/sizeof(variant_keys[0]));

/**
 * Load the file defined by the variant and if successful
 * return the dlopen handle and the hmi.
 * @return 0 = success, !0 = failure.
 */
static int load(const char *id,
        const char *path,
        const struct hw_module_t **pHmi)
{

    int status;
    void *handle;
    struct hw_module_t *hmi;

    /*
     * load the symbols resolving undefined symbols before
     * dlopen returns. Since RTLD_GLOBAL is not or'd in with
     * RTLD_NOW the external symbols will not be global
     * 硬件抽象层模块文件实际上是一个动态链接库文件，即so文件.
     * 调用dlopen函数将它加载到内存中. */
    handle = dlopen(path, RTLD_NOW);
    if (handle == NULL) {
        char const *err_str = dlerror();
        ALOGE("load: module=%s\n%s", path, err_str?err_str:"unknown");
        status = -EINVAL;
        goto done;
    }

    /* Get the address of the struct hal_module_info.
     * 利用dlsym函数来获得库文件里面名称为HAL_MODULE_INFO_SYM_AS_STR
     * 的符号，这个符号指向的是一个自定义的硬件抽象层模块结构体，它包含了对应的
     * 硬件抽象层模块的所有信息，因为每一个自定义的硬件抽象模块必须包含一个名称
     * 为”HMI”的符号，而且这个符号的第一个成员变量的类型必须定义为
     * hw_module_t，因此这下面可以将其安全地转换为hw_module_t结构体指针.
     */
    const char *sym = HAL_MODULE_INFO_SYM_AS_STR;
    hmi = (struct hw_module_t *)dlsym(handle, sym);
    if (hmi == NULL) {
        ALOGE("load: couldn't find symbol %s", sym);
        status = -EINVAL;
        goto done;
    }

    /* Check that the id matches
     * 验证所加载的库文件是否正确. */
    if (strcmp(id, hmi->id) != 0) {
        ALOGE("load: id=%s != hmi->id=%s", id, hmi->id);
        status = -EINVAL;
        goto done;
    }
    hmi->dso = handle;

    /* success */
    status = 0;

done:
    if (status != 0) {
        hmi = NULL;
        if (handle != NULL) {
            dlclose(handle);
            handle = NULL;
        }
    } else {
        ALOGV("loaded HAL id=%s path=%s hmi=%p handle=%p",
                id, path, *pHmi, handle);
    }

    *pHmi = hmi;

    return status;
}

int hw_get_module_by_class(const char *class_id, const char *inst,
                           const struct hw_module_t **module)
{
    int status;
    int i;
    const struct hw_module_t *hmi = NULL;
    char prop[PATH_MAX];
    char path[PATH_MAX];
    char name[PATH_MAX];

    if (inst)
        snprintf(name, PATH_MAX, "%s.%s", class_id, inst);
    else
        strlcpy(name, class_id, PATH_MAX);

    /*
     * Here we rely on the fact that calling dlopen multiple times on
     * the same .so will simply increment a refcount (and not load
     * a new copy of the library).
     * We also assume that dlopen() is thread-safe.
     */
     /* Loop through the configuration variants looking for a module
     ** 以下for循环根据数组variant_keys在HAL_LIBRARY_PATH1和
     ** HAL_LIBRARY_PATH2目录中检查对应的硬件抽象层模块库文件是否存在,
     ** 如果存在则结束for循环.
     */
    for (i=0 ; i<HAL_VARIANT_KEYS_COUNT+1 ; i++) {
        if (i < HAL_VARIANT_KEYS_COUNT) {
            if (property_get(variant_keys[i], prop, NULL) == 0) {
                continue;
            }
            snprintf(path, sizeof(path), "%s/%s.%s.so",
                     HAL_LIBRARY_PATH2, name, prop);
            if (access(path, R_OK) == 0) break;
            snprintf(path, sizeof(path), "%s/%s.%s.so",
                     HAL_LIBRARY_PATH1, name, prop);
            if (access(path, R_OK) == 0) break;
        } else {
            snprintf(path, sizeof(path), "%s/%s.default.so",
                     HAL_LIBRARY_PATH1, name);
            if (access(path, R_OK) == 0) break;
        }
    }

    status = -ENOENT;
    if (i < HAL_VARIANT_KEYS_COUNT+1) {
        /* load the module, if this fails, we're doomed, and we should not try
         * to load a different variant.
         ** 调用load函数来执行加载硬件抽象层模块的操作.
         */
        status = load(class_id, path, module);
    }

    return status;
}

int hw_get_module(const char *id, const struct hw_module_t **module)
{
    return hw_get_module_by_class(id, NULL, module);
}
```


为了能够使硬件抽象模块中的函数能够访问/dev/simulator_main设备，我们在/system/core/rootdir
目录下中一个名为ueventd.rc的配置见增加以下一行内容：

```
/dev/simulator_main     0666 root root
```

重新编译bootimage，将boot刷到手机上重启,命令如下：

```
$ make bootimage -j4
$ fastboot flash boot boot.img
```