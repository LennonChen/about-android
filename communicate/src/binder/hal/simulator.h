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
