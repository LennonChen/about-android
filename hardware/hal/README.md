Android HAL
========================================

硬件抽象层模块编写规范
----------------------------------------

Android系统的硬件抽象层以模块的形式来管理各个硬件访问接口，这与Linux内核系统一模块形式管理各个
硬件设备类似.每一个硬件模块都对应有一个动态链接库文件，这些动态链接库文件的命名需要符合一定的
规范。同时，在系统内部，每一个硬件抽象层模块都使用结构体hw_module_t来描述，而硬件设备则使用
hw_device_t来描述。

### 硬件抽象层模块文件命名规范

path: hardware/libhardware/hardware.c
```
/**
 * There are a set of variant filename for modules. The form of the filename
 * is "<MODULE_ID>.variant.so" so for the led module the Dream variants
 * of base "ro.product.board", "ro.board.platform" and "ro.arch" would be:
 *
 * led.trout.so
 * led.msm7k.so
 * led.ARMV6.so
 * led.default.so
 *
 * 硬件抽象层模块文件命名规范为：<MODULE_ID>.variant.so MODULE_ID表示模块的
 * ID, variant表示五个系统属性ro.hardware, ro.build.product, ro.product.board,
 * ro.board.platform, ro.arch之一.
 */
static const char *variant_keys[] = {
    "ro.hardware",  /* This goes first so that it can pick up a different
                       file on the emulator. */
    "ro.build.product",
    "ro.product.board",
    "ro.board.platform",
    "ro.arch"
};
```

系统在加载硬件抽象层模块时，依次按照ro.hardware, ro.product.board, ro.board.platform和ro.arch的
顺序来获取它们的属性值。如果其中的一个属性系统存在，那么就把它的值作为variant的值然后再检查对应
的库文件<MODULE_ID>.variant.so是否存在，如果存在那么就找到要加载的硬件抽象层模块库文件，否则就
继续查找下一个属性系统。如果这四个系统属性都不存在，或者对应于这四个系统属性的硬件抽象层模块文件
都不存在，那么就使用“<MODULE_ID>.default.so”来作为要加载的硬件抽象层模块文件的名称了.

### 硬件抽象层模块/硬件设备结构体定义规范

硬件抽象层模块hw_module_t结构体 struct hw_module_t

path: hardware/libhardware/include/hardware/hardware.h
```
/*
 * Value for the hw_module_t.tag field
 * 结构体hw_module_t.tag成员变量的值.
 */

#define MAKE_TAG_CONSTANT(A,B,C,D) (((A) << 24) | ((B) << 16) | ((C) << 8) | (D))
#define HARDWARE_MODULE_TAG MAKE_TAG_CONSTANT('H', 'W', 'M', 'T')
#define HARDWARE_DEVICE_TAG MAKE_TAG_CONSTANT('H', 'W', 'D', 'T')
#define HARDWARE_MAKE_API_VERSION(maj,min) \
            ((((maj) & 0xff) << 8) | ((min) & 0xff))

/*
 * The current HAL API version.
 *
 * All module implementations must set the hw_module_t.hal_api_version field
 * to this value when declaring the module with HAL_MODULE_INFO_SYM.
 *
 * Note that previous implementations have always set this field to 0.
 * Therefore, libhardware HAL API will always consider versions 0.0 and 1.0
 * to be 100% binary compatible.
 *
 */
#define HARDWARE_HAL_API_VERSION HARDWARE_MAKE_API_VERSION(1, 0)

/*
 * Helper macros for module implementors.
 *
 * The derived modules should provide convenience macros for supported
 * versions so that implementations can explicitly specify module/device
 * versions at definition time.
 *
 * Use this macro to set the hw_module_t.module_api_version field.
 */
#define HARDWARE_MODULE_API_VERSION(maj,min) HARDWARE_MAKE_API_VERSION(maj,min)

/*
 * Use this macro to set the hw_device_t.version field
 */
#define HARDWARE_DEVICE_API_VERSION(maj,min) HARDWARE_MAKE_API_VERSION(maj,min)

/**
 * Every hardware module must have a data structure named HAL_MODULE_INFO_SYM
 * and the fields of this data structure must begin with hw_module_t
 * followed by module specific information.
 *
 ** 硬件抽象层中的每一个硬件模块都必须有一个名叫HAL_MODULE_INFO_SYM的数据
 ** 结构，这个数据结构是自定义的，即HMI，这个自定义的描述某个硬件模块的结构体
 ** 第一个成员变量的类型必须为hw_module_t. 后面跟着描述某个特定硬件模块其特有的
 ** 信息.
*/
typedef struct hw_module_t {
/** tag must be initialized to HARDWARE_MODULE_TAG
** tag 必须初始化为HARDWARE_MODULE_TAG.
*/
    uint32_t tag;

    /**
     * The API version of the implemented module. The module owner is
     * responsible for updating the version when a module interface has
     * changed.
     *
     * The derived modules such as gralloc and audio own and manage this field.
     * The module user must interpret the version field to decide whether or
     * not to inter-operate with the supplied module implementation.
     * For example, SurfaceFlinger is responsible for making sure that
     * it knows how to manage different versions of the gralloc-module API,
     * and AudioFlinger must know how to do the same for audio-module API.
     *
     * The module API version should include a major and a minor component.
     * For example, version 1.0 could be represented as 0x0100. This format
     * implies that versions 0x0100-0x01ff are all API-compatible.
     *
     * In the future, libhardware will expose a hw_get_module_version()
     * (or equivalent) function that will take minimum/maximum supported
     * versions as arguments and would be able to reject modules with
     * versions outside of the supplied range.
     */

    uint16_t module_api_version;
#define version_major module_api_version
    /**
     * version_major/version_minor defines are supplied here for temporary
     * source code compatibility. They will be removed in the next version.
     * ALL clients must convert to the new version format.
     */

    /**
     * The API version of the HAL module interface. This is meant to
     * version the hw_module_t, hw_module_methods_t, and hw_device_t
     * structures and definitions.
     *
     * The HAL interface owns this field. Module users/implementations
     * must NOT rely on this value for version information.
     *
     * Presently, 0 is the only valid value.
     */
    uint16_t hal_api_version;

#define version_minor hal_api_version
    /** Identifier of module */
    const char *id;

    /** Name of this module */
    const char *name;

    /** Author/owner/implementor of the module */
    const char *author;

    /** Modules methods
     ** 定义了一个硬件抽象层模块的操作方法列表 */
    struct hw_module_methods_t* methods;

    /** module's dso
    ** dso用来保存加载硬件抽象层模块后得到的句柄值，每一个硬件抽象模块都对应有
    ** 一个动态链接库文件，加载硬件抽象层模块的加载过程实际上就是调用dlopen
    ** 函数来加载与其对应的动态链接库文件的过程，在调用dlclose函数来卸载这个
    ** 硬件抽象层模块时，要用到这个句柄值，因此，我们在加载时需要将它保存起来.
     */
    void* dso;

    /** padding to 128 bytes, reserved for future use */
    uint32_t reserved[32-7];
} hw_module_t;


/**
 * Name of the hal_module_info
 */
#define HAL_MODULE_INFO_SYM         HMI

/**
 * Name of the hal_module_info as a string
 */
#define HAL_MODULE_INFO_SYM_AS_STR  "HMI"

/* hw_module_methods_t描述了一个硬件抽象层模块的操作方法列表，现在其只有一个
** 成员变量，它是一个函数指针，用来打开硬件抽象层模块的硬件设备.
** open函数参数:
**   module - 表示要打开的硬件设备所在的模块;
**   id - 表示要打开的硬件设备id;
**   device - 用来描述一个已经打开了的硬件设备，用作返回值.
*/
typedef struct hw_module_methods_t {
    /** Open a specific device */
    int (*open)(const struct hw_module_t* module, const char* id,
            struct hw_device_t** device);
} hw_module_methods_t;

/**
 * Every device data structure must begin with hw_device_t
 * followed by module specific public methods and attributes.
 *
 ** 使用hw_device_t来描述硬件抽象层中的特定硬件设备，硬件抽象层模块中的每一个
 ** 硬件设备必须要有一个自定义的硬件设备描述结构体，而且它的第一个成员变量类型
 ** 必须为hw_device_t，后面跟着其特定的公共方法和属性.
 */

typedef struct hw_device_t {
    /** tag must be initialized to HARDWARE_DEVICE_TAG
     ** tag必须初始化为HARDWARE_DEVICE_TAG. */
    uint32_t tag;

    /**
     * Version of the module-specific device API. This value is used by
     * the derived-module user to manage different device implementations.
     *
     * The module user is responsible for checking the module_api_version
     * and device version fields to ensure that the user is capable of
     * communicating with the specific module implementation.
     *
     * One module can support multiple devices with different versions. This
     * can be useful when a device interface changes in an incompatible way
     * but it is still necessary to support older implementations at the same
     * time. One such example is the Camera 2.0 API.
     *
     * This field is interpreted by the module user and is ignored by the
     * HAL interface itself.
     */
    uint32_t version;

    /** reference to the module this device belongs to */
    struct hw_module_t* module;

    /** padding reserved for future use */
    uint32_t reserved[12];

    /** Close this device */
    int (*close)(struct hw_device_t* device);
} hw_device_t;
```

注意：硬件抽象层中的硬件设备是由其所在的模块提供的接口来打开的，而关闭则是由硬件设备自身来完成的.

为了描述Android硬件抽象层，我们使用一个实例simulator来描述整个过程,我们用4个字节的内存模拟
一个虚拟的字符硬件设备，并为这个设备开发驱动程序来讲述硬件抽象层的工作原理.

1.simulator driver
----------------------------------------

https://github.com/leeminghao/about-android/tree/master/hardware/hal/simulator_driver.md

至此，我们用内存模拟的虚拟硬件设备已经能够正常工作了，传统的Linux系统把对硬件的支持完全实现在
内核空间中，即把对硬件的支持完全实现在硬件驱动模块中，但是Android系统不但在内核中实现了硬件
驱动程序对硬件支持，而且还在用户空间中增加了一层叫做硬件抽象层(HAL)，在我理解看来，增加的这层
硬件抽象层有两个好处：

* 1.向下屏蔽了硬件驱动模块中的实现细节：这是出于商业目的，这样做保护硬件厂商，Linux内核源代码
版权遵循GNU License，而Android源代码版权遵循Apache License，前者在发布产品时，必须公布源代码，
而后者无须发布源代码。如果把对硬件支持的所有代码都放在Linux驱动层，那就意味着发布时要公开驱动
程序的源代码，而公开源代码就意味着把硬件的相关参数和实现都公开了，在手机市场竞争激烈的今天，
这对厂家来说，损害是非常大的。因此，Android才会想到把对硬件的支持分成硬件抽象层和内核驱动层，
内核驱动层只提供简单的访问硬件逻辑，例如读写硬件寄存器的通道，至于从硬件中读到了什么值或者
写了什么值到硬件中的逻辑，都放在硬件抽象层中去了，这样就可以把商业秘密隐藏起来了。也正是由于
这个分层的原因，Android被踢出了Linux内核主线代码树中。大家想想，Android放在内核空间的驱动程序
对硬件的支持是不完整的，把Linux内核移植到别的机器上去时，由于缺乏硬件抽象层的支持，硬件就完全
不能用了，这也是为什么说Android是开放系统而不是开源系统的原因;

* 2.向上提供了硬件的抽象访问服务: 这是出于技术目的，对于软件工程师来说，硬件和操作系统都是透明的，
其无需关心内核中驱动程序是如何将一个值读或者写到硬件中去，而是在硬件抽象层中把对硬件的细节访问
都封装起来以接口的程序提供给上层开发者使用，上层开发者无需关心具体的硬件访问细节，大大降低了开发
难度.

Android系统为硬件抽象层中的模块接口定义了编写规范，我们必须按照这个规范来编写自己的硬件模块接口，
否则就会导致无法正常访问硬件，下面我们先介绍Android硬件抽象层模块接口的编写规范，然后再按照这个
规范为我们用内存模拟的硬件设备simulator开发硬件抽象层模块接口，并且分析，硬件抽象层模块的加载过
程和硬件设备的访问权限问题.

2.simulator hal
----------------------------------------

https://github.com/leeminghao/about-android/tree/master/hardware/hal/simulator_hal.md