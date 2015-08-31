dvmGetCurrentJNIMethod
========================================

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

1.dvmThreadSelf
----------------------------------------

dvmThreadSelf通过调用pthread_getspecific函数来获取当前主线程的Thread结构.

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

当前主线程的Thread实例是在虚拟机启动函数dvmStartup中调用dvmThreadStartup来创建的,
具体如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmThreadStartup.md
