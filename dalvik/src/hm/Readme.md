hm sample
========================================

下面我们使用hm这个简单的例子来学习Android虚拟机的工作原理.

usage
----------------------------------------

```
$ adb root && adb remount
$ adb push hm.jar /system/frameworks/
$ adb push hm /system/bin/
$ adb shell
# hm
```

hm脚本如下
----------------------------------------

```
# Script to start "hm" on the device, which has a very rudimentary
# shell.
#
base=/system
export CLASSPATH=$base/framework/hm.jar
exec app_process $base/bin com.android.commands.hm.Hm "$@"
```

hm脚本在运行过程需要依赖于hm.jar包,hm.jar包生成过程如下所示:

hm.jar的生成过程
----------------------------------------

https://github.com/leeminghao/about-android/blob/master/dalvik/src/hm/build.md

app_process执行Hm类的过程
----------------------------------------

从hm脚本可以看出android是直接调用app_process来加载Hm类来执行的，
app_process的具体实现过程如下所示：

https://github.com/leeminghao/about-android/blob/master/dalvik/src/hm/app_process.md

通过分析app_process进程源码我们知道，因为没有指定--zygote参数，所以是调用runtime的start
方法来加载RuntimeInit类来进行后续工作的.

### AndroidRuntime.start

https://github.com/leeminghao/about-android/blob/master/dalvik/start/start.md

start方法中做了如下工作:

#### AndroidRuntime.startVm

调用成员函数startVm来创建一个Dalvik虚拟机实例，并且保存在成员变量mJavaVM中.

https://github.com/leeminghao/about-android/blob/master/dalvik/start/startVm.md

startVm函数主要完成如下工作:

* 1. 设置Dalvik虚拟机的启动选项.
* 2. 设置好Dalvik虚拟机的启动选项之后，AndroidRuntime的成员函数startVm就会调用另外一个函数
     JNI_CreateJavaVM来创建以及初始化一个Dalvik虚拟机实例。

##### JNI_CreateJavaVM

https://github.com/leeminghao/about-android/blob/master/dalvik/start/JNI_CreateJavaVM.md

JNI_CreateJavaVM主要完成如下工作:

* 1.为当前进程创建一个Dalvik虚拟机实例，即一个JavaVMExt对象.

###### JavaVMExt

path: dalvik/vm/JniInternal.h
```
struct JavaVMExt {
    const struct JNIInvokeInterface* funcTable;     /* must be first */

    const struct JNIInvokeInterface* baseFuncTable;

    /* head of list of JNIEnvs associated with this VM */
    JNIEnvExt*      envList;
    pthread_mutex_t envListLock;
};
```

JNIInvokeInterface的定义如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/JNIInvokeInterface.md

* 2.处理Dalvik虚拟机的启动选项.

* 3.为当前线程创建和初始化一个JNI环境，即一个JNIEnvExt对象,这是通过调用函数dvmCreateJNIEnv来完成的

###### JNIEnvExt

path: dalvik/vm/JniInternal.h
```
struct JNIEnvExt {
    const struct JNINativeInterface* funcTable;     /* must be first */

    const struct JNINativeInterface* baseFuncTable;

    u4      envThreadId;
    Thread* self;

    /* if nonzero, we are in a "critical" JNI call */
    int     critical;

    struct JNIEnvExt* prev;
    struct JNIEnvExt* next;
};
```

JNINativeInterface的定义如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/JNINativeInterface.md

* 4.调用函数dvmStartup来初始化前面所创建的Dalvik虚拟机实例.

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmStartup.md

* 5.调用函数dvmChangeStatus将当前线程的状态设置为正在执行NATIVE代码.

* 6.将面所创建和初始化好的JavaVMExt对象和JNIEnvExt对象通过输出参数p_vm和p_env返回给调用者.

在调用startVm创建完成Dalvik虚拟机之后,接下来返回到start函数中继续执行, 接下来执行
onVmCreated方法来进行一些早期的初始化操作:

#### onVmCreated(env)
