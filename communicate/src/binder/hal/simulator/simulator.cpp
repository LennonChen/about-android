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
