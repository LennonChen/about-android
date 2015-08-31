AndroidRuntime.start
========================================

path: frameworks/base/core/jni/AndroidRuntime.cpp
```
/*
 * Start the Android runtime.  This involves starting the virtual machine
 * and calling the "static void main(String[] args)" method in the class
 * named by "className".
 *
 * Passes the main function two arguments, the class name and the specified
 * options string.
 */
void AndroidRuntime::start(const char* className, const char* options)
{
    ALOGD("\n>>>>>> AndroidRuntime START %s <<<<<<\n",
            className != NULL ? className : "(unknown)");

    ...
}
```

start方法中做了如下工作:

1.AndroidRuntime.startVm
----------------------------------------

```
    /*
     * 'startSystemServer == true' means runtime is obsolete and not run from
     * init.rc anymore, so we print out the boot start event here.
     */
    if (strcmp(options, "start-system-server") == 0) {
        /* track our progress through the boot sequence */
        const int LOG_BOOT_PROGRESS_START = 3000;
        LOG_EVENT_LONG(LOG_BOOT_PROGRESS_START,
                       ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));
    }

    const char* rootDir = getenv("ANDROID_ROOT");
    if (rootDir == NULL) {
        rootDir = "/system";
        if (!hasDir("/system")) {
            LOG_FATAL("No root directory specified, and /android does not exist.");
            return;
        }
        setenv("ANDROID_ROOT", rootDir, 1);
    }

    //const char* kernelHack = getenv("LD_ASSUME_KERNEL");
    //ALOGD("Found LD_ASSUME_KERNEL='%s'\n", kernelHack);

    /* start the virtual machine */
    JniInvocation jni_invocation;
    jni_invocation.Init(NULL);

    JNIEnv* env;
    /* JNI JavaVM pointer */
    /* static JavaVM* mJavaVM; */
    if (startVm(&mJavaVM, &env) != 0) {
        return;
    }
```

调用成员函数startVm来创建一个Dalvik虚拟机实例，并且保存在成员变量mJavaVM中.

https://github.com/leeminghao/about-android/blob/master/dalvik/start/startVm.md

2.onVmCreated
----------------------------------------

在调用startVm创建完成Dalvik虚拟机之后,接下来返回到start函数中继续执行,接下来执行onVmCreated
方法来进行一些早期的初始化操作.

```
    onVmCreated(env);
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/onVmCreated.md

3.startReg
----------------------------------------

调用成员函数startReg来注册一些Android核心类的JNI方法.

```
    /*
     * Register android functions.
     */
    if (startReg(env) < 0) {
        ALOGE("Unable to register all android natives\n");
        return;
    }
```

4.main
----------------------------------------

调用参数className所描述的一个Java类的静态成员函数main，来作为Zygote进程的Java层入口。
这个入口类就为com.android.internal.os.ZygoteInit(或者RuntimeInit)。执行这一步的时候，
Zygote进程(app_process进程)中的Dalvik虚拟机实例就开始正式运作了。注意，在这一步中，
如果是在com.android.internal.os.ZygoteInit类的静态成员函数main，会进行大量的Android
核心类和系统资源文件预加载。其中，预加载的Android核心类可以参考frameworks/base/preloaded-classes
这个文件，而预加载的系统资源就是包含在/system/framework/framework-res.apk中。

```
    /*
     * We want to call main() with a String array with arguments in it.
     * At present we have two arguments, the class name and an option string.
     * Create an array to hold them.
     */
    jclass stringClass;
    jobjectArray strArray;
    jstring classNameStr;
    jstring optionsStr;

    stringClass = env->FindClass("java/lang/String");
    assert(stringClass != NULL);
    strArray = env->NewObjectArray(2, stringClass, NULL);
    assert(strArray != NULL);
    classNameStr = env->NewStringUTF(className);
    assert(classNameStr != NULL);
    env->SetObjectArrayElement(strArray, 0, classNameStr);
    optionsStr = env->NewStringUTF(options);
    env->SetObjectArrayElement(strArray, 1, optionsStr);

    /*
     * Start VM.  This thread becomes the main thread of the VM, and will
     * not return until the VM exits.
     */
    char* slashClassName = toSlashClassName(className);
    jclass startClass = env->FindClass(slashClassName);
    if (startClass == NULL) {
        ALOGE("JavaVM unable to locate class '%s'\n", slashClassName);
        /* keep going */
    } else {
        jmethodID startMeth = env->GetStaticMethodID(startClass, "main",
            "([Ljava/lang/String;)V");
        if (startMeth == NULL) {
            ALOGE("JavaVM unable to find main() in '%s'\n", className);
            /* keep going */
        } else {
            env->CallStaticVoidMethod(startClass, startMeth, strArray);

#if 0
            if (env->ExceptionCheck())
                threadExitUncaughtException(env);
#endif
        }
    }
    free(slashClassName);
```

5.Shutting down VM
----------------------------------------

从com.android.internal.os.ZygoteInit(RuntimeInit)类的静态成员函数main返回来的时候，
就说明Zygote(app_process)进程准备要退出来了。在退出之前，会调用前面创建的Dalvik虚拟机
实例的成员函数DetachCurrentThread和DestroyJavaVM。其中，前者用来将Zygote(app_process)
进程的主线程脱离前面创建的Dalvik虚拟机实例，后者是用来销毁前面创建的Dalvik虚拟机实例。

```
    ALOGD("Shutting down VM\n");

    if (mJavaVM->DetachCurrentThread() != JNI_OK)
        ALOGW("Warning: unable to detach main thread\n");
    if (mJavaVM->DestroyJavaVM() != 0)
        ALOGW("Warning: VM did not shut down cleanly\n");
```
