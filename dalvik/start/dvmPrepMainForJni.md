dvmPrepMainForJni
========================================

path: dalvik/vm/Thread.cpp
```
/*
 * Finish preparing the parts of the Thread struct required to support
 * JNI registration.
 */
bool dvmPrepMainForJni(JNIEnv* pEnv)
{
    Thread* self;

    /* main thread is always first in list at this point */
    self = gDvm.threadList;
    assert(self->threadId == kMainThreadId);

    /* create a "fake" JNI frame at the top of the main thread interp stack */
    if (!createFakeEntryFrame(self))
        return false;

    /* fill these in, since they weren't ready at dvmCreateJNIEnv time */
    dvmSetJniEnvThreadId(pEnv, self);
    dvmSetThreadJNIEnv(self, (JNIEnv*) pEnv);

    return true;
}
```

dvmPreMainForJni方法主要用来在调用函数dvmThreadStartup创建完主线程之后准备主线程JNI运行环境，
主要完成如下工作:

* 1.获取主线程结构体指针保存在self中.

* 2.调用createFakeEntryFrame函数创建一个Fake的入口Frame.具体实现如下所示:

path: dalvik/vm/Thread.cpp
```
/*
 * Add a stack frame that makes it look like the native code in the main
 * thread was originally invoked from interpreted code.  This gives us a
 * place to hang JNI local references.  The VM spec says (v2 5.2) that the
 * VM begins by executing "main" in a class, so in a way this brings us
 * closer to the spec.
 */
static bool createFakeEntryFrame(Thread* thread)
{
    /*
     * Because we are creating a frame that represents application code, we
     * want to stuff the application class loader into the method's class
     * loader field, even though we're using the system class loader to
     * load it.  This makes life easier over in JNI FindClass (though it
     * could bite us in other ways).
     *
     * Unfortunately this is occurring too early in the initialization,
     * of necessity coming before JNI is initialized, and we're not quite
     * ready to set up the application class loader.  Also, overwriting
     * the class' defining classloader pointer seems unwise.
     *
     * Instead, we save a pointer to the method and explicitly check for
     * it in FindClass.  The method is private so nobody else can call it.
     */

    assert(thread->threadId == kMainThreadId);      /* main thread only */

    if (!dvmPushJNIFrame(thread, gDvm.methDalvikSystemNativeStart_main))
        return false;

    /*
     * Null out the "String[] args" argument.
     */
    assert(gDvm.methDalvikSystemNativeStart_main->registersSize == 1);
    u4* framePtr = (u4*) thread->interpSave.curFrame;
    framePtr[0] = 0;

    return true;
}
```
