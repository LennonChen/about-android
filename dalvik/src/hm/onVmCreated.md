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
接下来调用env指向的FindClass方法来生成对应Hm类jclass类型. 在调用startVM来创建Dalvik虚拟的实例
的时候在JNI_CreateJavaVM方法中调用了dvmCreateJNIEnv函数来创建和初始化一个JNI环境，即一个
JNIEnvExt对象, 在dvmCreateJNIEnv方法中，对应的funcTable被设置为了gNativeInterface全局变量,
如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/gNativeInterface.md

其中FindClass函数的实现如下所示:

FindClass
----------------------------------------

path: dalvik/vm/Jni.cpp
```
/*
 * Find a class by name.
 *
 * We have to use the "no init" version of FindClass here, because we might
 * be getting the class prior to registering native methods that will be
 * used in <clinit>.
 *
 * We need to get the class loader associated with the current native
 * method.  If there is no native method, e.g. we're calling this from native
 * code right after creating the VM, the spec says we need to use the class
 * loader returned by "ClassLoader.getBaseClassLoader".  There is no such
 * method, but it's likely they meant ClassLoader.getSystemClassLoader.
 * We can't get that until after the VM has initialized though.
 */
static jclass FindClass(JNIEnv* env, const char* name) {
    ScopedJniThreadState ts(env);

    const Method* thisMethod = dvmGetCurrentJNIMethod();
    assert(thisMethod != NULL);

    Object* loader;
    Object* trackedLoader = NULL;
    if (ts.self()->classLoaderOverride != NULL) {
        /* hack for JNI_OnLoad */
        assert(strcmp(thisMethod->name, "nativeLoad") == 0);
        loader = ts.self()->classLoaderOverride;
    } else if (thisMethod == gDvm.methDalvikSystemNativeStart_main ||
               thisMethod == gDvm.methDalvikSystemNativeStart_run) {
        /* start point of invocation interface */
        if (!gDvm.initializing) {
            loader = trackedLoader = dvmGetSystemClassLoader();
        } else {
            loader = NULL;
        }
    } else {
        loader = thisMethod->clazz->classLoader;
    }

    char* descriptor = dvmNameToDescriptor(name);
    if (descriptor == NULL) {
        return NULL;
    }
    ClassObject* clazz = dvmFindClassNoInit(descriptor, loader);
    free(descriptor);

    jclass jclazz = (jclass) addLocalReference(ts.self(), (Object*) clazz);
    dvmReleaseTrackedAlloc(trackedLoader, ts.self());
    return jclazz;
}
```

### dvmGetCurrentJNIMethod

path: dalvik/vm/Jni.cpp
```
/*
 * Get the method currently being executed by examining the interp stack.
 */
const Method* dvmGetCurrentJNIMethod() {
    assert(dvmThreadSelf() != NULL);

    void* fp = dvmThreadSelf()->interpSave.curFrame;
    const Method* meth = SAVEAREA_FROM_FP(fp)->method;

    assert(meth != NULL);
    assert(dvmIsNativeMethod(meth));
    return meth;
}
```

### dvmThreadSelf

path: dalvik/vm/Thread.cpp
```
/*
 * Like pthread_self(), but on a Thread*.
 */
Thread* dvmThreadSelf()
{
    return (Thread*) pthread_getspecific(gDvm.pthreadKeySelf);
}
```