onVmCreated
========================================

onVmCreated定义
----------------------------------------

path: frameworks/base/cmds/app_process/app_main.cpp
```
class AppRuntime : public AndroidRuntime
{
public:
    AppRuntime()
        : mParentDir(NULL)
        , mClassName(NULL)
        , mClass(NULL)
        , mArgC(0)
        , mArgV(NULL)
    {
    }

    ...

    virtual void onVmCreated(JNIEnv* env)
    {
        if (mClassName == NULL) {
            return; // Zygote. Nothing to do here.
        }

        /*
         * This is a little awkward because the JNI FindClass call uses the
         * class loader associated with the native method we're executing in.
         * If called in onStarted (from RuntimeInit.finishInit because we're
         * launching "am", for example), FindClass would see that we're calling
         * from a boot class' native method, and so wouldn't look for the class
         * we're trying to look up in CLASSPATH. Unfortunately it needs to,
         * because the "am" classes are not boot classes.
         *
         * The easiest fix is to call FindClass here, early on before we start
         * executing boot class Java code and thereby deny ourselves access to
         * non-boot classes.
         */
        // 在这里mClassName="com.android.commands.hm.Hm"
        char* slashClassName = toSlashClassName(mClassName);
        mClass = env->FindClass(slashClassName);
        if (mClass == NULL) {
            ALOGE("ERROR: could not find class '%s'\n", mClassName);
        }
        free(slashClassName);

        mClass = reinterpret_cast<jclass>(env->NewGlobalRef(mClass));
    }

    ...

    const char* mParentDir;
    const char* mClassName;
    jclass mClass;
    int mArgC;
    const char* const* mArgV;
};
```

在onVmCreated函数中首先将"com.android.commands.hm.Hm"类名称装换为"com/android/commands/hm/Hm"
接下来调用env指向的FindClass方法来生成对应Hm类jclass类型.
在调用startVM来创建Dalvik虚拟的实例的时候在JNI_CreateJavaVM方法中调用了dvmCreateJNIEnv函数

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmCreateJNIEnv.md

来创建和初始化一个JNI环境，即一个JNIEnvExt对象, 在dvmCreateJNIEnv方法中，对应的funcTable
被设置为了gNativeInterface全局变量,如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/gNativeInterface.md

1.FindClass
----------------------------------------

其中FindClass函数的实现如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/FindClass.md
