Dalvik启动过程
============================================================

  在Android系统中，应用程序进程都是由Zygote进程孵化出来的，而Zygote进程是由Init进程启动的。
Zygote进程在启动时会创建一个Dalvik虚拟机实例，每当它孵化一个新的应用程序进程时，
都会将这个Dalvik虚拟机实例复制到新的应用程序进程里面去，从而使得每一个应用程序进程都有一个
独立的Dalvik虚拟机实例。
  Zygote进程在启动的过程中，除了会创建一个Dalvik虚拟机实例之外，还会将Java运行时库加载到进程中来，
以及注册一些Android核心类的JNI方法来前面创建的Dalvik虚拟机实例中去。注意，一个应用程序进程被Zygote
进程孵化出来的时候，不仅会获得Zygote进程中的Dalvik虚拟机实例拷贝，还会与Zygote一起共享Java运行时库，
这完全得益于Linux内核的进程创建机制（fork）。这种Zygote孵化机制的优点是不仅可以快速地启动一个应用程序进程，
还可以节省整体的内存消耗，缺点是会影响开机速度，毕竟Zygote是在开机过程中启动的。不过，总体来说，是利大于弊的，
毕竟整个系统只有一个Zygote进程，而可能有无数个应用程序进程，而且我们不会经常去关闭手机，大多数情况下只是让它进入休眠状态。

代码分析
------------------------------------------------------------

### zygote

path: frameworks/base/cmds/app_process/app_main.cpp
```
int main(int argc, char* const argv[])
{
    ...
    // These are global variables in ProcessState.cpp
    mArgC = argc;
    mArgV = argv;

    mArgLen = 0;
    for (int i=0; i<argc; i++) {
        mArgLen += strlen(argv[i]) + 1;
    }
    mArgLen--;

    AppRuntime runtime;
    const char* argv0 = argv[0];

    // Process command line arguments
    // ignore argv[0]
    argc--;
    argv++;

    // Everything up to '--' or first non '-' arg goes to the vm

    int i = runtime.addVmArguments(argc, argv);

    // Parse runtime arguments.  Stop at first unrecognized option.
    bool zygote = false;
    bool startSystemServer = false;
    bool application = false;
    const char* parentDir = NULL;
    const char* niceName = NULL;
    const char* className = NULL;
    while (i < argc) {
        const char* arg = argv[i++];
        if (!parentDir) {
            parentDir = arg;
        } else if (strcmp(arg, "--zygote") == 0) {
            zygote = true;
            niceName = "zygote";
        } else if (strcmp(arg, "--start-system-server") == 0) {
            startSystemServer = true;
        } else if (strcmp(arg, "--application") == 0) {
            application = true;
        } else if (strncmp(arg, "--nice-name=", 12) == 0) {
            niceName = arg + 12;
        } else {
            className = arg;
            break;
        }
    }

    if (niceName && *niceName) {
        setArgv0(argv0, niceName);
        set_process_name(niceName);
    }

    runtime.mParentDir = parentDir;

    if (zygote) {
        runtime.start("com.android.internal.os.ZygoteInit",
                startSystemServer ? "start-system-server" : "");
    } else if (className) {
        // Remainder of args get passed to startup class main()
        runtime.mClassName = className;
        runtime.mArgC = argc - i;
        runtime.mArgV = argv + i;
        runtime.start("com.android.internal.os.RuntimeInit",
                application ? "application" : "tool");
    } else {
        fprintf(stderr, "Error: no class name or --zygote supplied.\n");
        app_usage();
        LOG_ALWAYS_FATAL("app_process: no class name or --zygote supplied.");
        return 10;
    }
}
```

### 1.AndroidRuntime::start

https://github.com/leeminghao/about-android/blob/master/dalvik/start/start.md

接下来，我们就主要关注Dalvik虚拟机实例的创建过程，以及Android核心类JNI方法的注册过程，
即AndroidRuntime类的成员函数startVm和startReg的实现。

##### JavaVM和JNIEnv

JavaVM和JNIEnv的类型定义如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/JavaVM-JNIEnv.md

### 2.AndroidRuntime::startVm

函数startVm来创建一个Dalvik虚拟机实例, 并且保存在成员变量mJavaVM中.

https://github.com/leeminghao/about-android/blob/master/dalvik/start/startVm.md

### 3.JNI_CreateJavaVM

在startVM函数设置好Dalvik虚拟机的启动选项之后，AndroidRuntime的成员函数startVm就会调用另外一个
函数JNI_CreateJavaVM来创建以及初始化一个Dalvik虚拟机实例。

https://github.com/leeminghao/about-android/blob/master/dalvik/start/JNI_CreateJavaVM.md

gDvm是一个类型为DvmGlobals的全局变量，用来收集当前进程所有虚拟机相关的信息,
gDvmJni是一个类型为DvmJniGlobals的全局变量，用来收集当前进程Jni环境的信息:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/Globals.h

每一个Dalvik虚拟机实例都有一个函数表，保存在对应的JavaVMExt对象的成员变量funcTable中,
JavaVMExt的类型定义如下所示:

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

##### JavaVMExt.funTable

这个函数表又被指定为gInvokeInterface。gInvokeInterface是一个类型为JNIInvokeInterface
的结构体，类型定义如下所示:

path: libnativehelper/include/nativehelper/jni.h
```
/*
 * JNI invocation interface.
 */
struct JNIInvokeInterface {
    void*       reserved0;
    void*       reserved1;
    void*       reserved2;

    jint        (*DestroyJavaVM)(JavaVM*);
    jint        (*AttachCurrentThread)(JavaVM*, JNIEnv**, void*);
    jint        (*DetachCurrentThread)(JavaVM*);
    jint        (*GetEnv)(JavaVM*, void**, jint);
    jint        (*AttachCurrentThreadAsDaemon)(JavaVM*, JNIEnv**, void*);
};
```

而gInvokeInterface的定义如下所示：

path: dalvik/vm/Jni.cpp
```
static const struct JNIInvokeInterface gInvokeInterface = {
    NULL,
    NULL,
    NULL,

    DestroyJavaVM,
    AttachCurrentThread,
    DetachCurrentThread,

    GetEnv,

    AttachCurrentThreadAsDaemon,
};
```

有了这个Dalvik虚拟机函数表之后，我们就可以将当前线程Attach或者Detach到Dalvik虚拟机中去，
或者销毁当前进程的Dalvik虚拟机等。

##### JavaVmExt.envList

每一个Dalvik虚拟机实例还有一个JNI环境列表，保存在对应的JavaVMExt对象的成员变量envList中。
注意，JavaVMExt对象的成员变量envList描述的是一个JNIEnvExt列表，其中，每一个Attach到Dalvik
虚拟机中去的线程都有一个对应的JNIEnvExt，用来描述它的JNI环境。有了这个JNI环境之后，我们才可以
在Java函数和C/C++函数之间互相调用。

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

每一个JNIEnvExt对象都有两个成员变量prev和next，它们均是一个JNIEnvExt指针，分别指向前一个
JNIEnvExt对象和后一个JNIEnvExt对象，也就是说，每一个Dalvik虚拟机实例的成员变量envList
描述的是一个双向JNIEnvExt列表，其中，列表中的第一个JNIEnvExt对象描述的是主线程的JNI环境。

##### JNIEnvExt.funcTable

funcTable是一个JNINativeInterface的类型的指针，该类型定义在
libnativehelper/include/nativehelper/jni.h中，如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/JNINativeInterface.md

### 4.dvmCreateJNIEnv

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmCreateJNIEnv.md

gNativeInterface是一个类型为JNINativeInterface的结构体，它定义如下所示：

https://github.com/leeminghao/about-android/blob/master/dalvik/start/gNativeInterface.md

从gNativeInterface的定义就可以看出，结构体JNINativeInterface用来描述一个本地接口表。
当我们需要在C/C++代码在中调用Java函数，就要用到这个本地接口表，例如：
* 调用函数FindClass可以找到指定的Java类；
* 调用函数GetMethodID可以获得一个Java类的成员函数，并且可以通过类似CallObjectMethod函数来
  间接调用它；
* 调用函数GetFieldID可以获得一个Java类的成员变量，并且可以通过类似SetIntField的函数来设置它的值；
* 调用函数RegisterNatives和UnregisterNatives可以注册和反注册JNI方法到一个Java类中去，
  以便可以在Java函数中调用；
* 调用函数GetJavaVM可以获得当前进程中的Dalvik虚拟机实例。

### 5.dvmStartup

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmStartup.md
