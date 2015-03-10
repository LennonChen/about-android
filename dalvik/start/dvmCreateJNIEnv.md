path: dalvik/vm/Jni.cpp
```
/*
 * Create a new JNIEnv struct and add it to the VM's list.
 *
 * "self" will be NULL for the main thread, since the VM hasn't started
 * yet; the value will be filled in later.
 */
JNIEnv* dvmCreateJNIEnv(Thread* self) {
    JavaVMExt* vm = (JavaVMExt*) gDvmJni.jniVm;

    //if (self != NULL)
    //    ALOGI("Ent CreateJNIEnv: threadid=%d %p", self->threadId, self);

    assert(vm != NULL);

    /* 1.创建一个JNIEnvExt对象，用来描述一个JNI环境，并且设置这个JNIEnvExt对象的宿主Dalvik虚拟机，
     * 以及所使用的本地接口表，即设置这个JNIEnvExt对象的成员变量funcTable。本地接口表由全局变量
     * gNativeInterface来描述。
     */
    JNIEnvExt* newEnv = (JNIEnvExt*) calloc(1, sizeof(JNIEnvExt));
    /* 2.参数self描述的是前面创建的JNIEnvExt对象要关联的线程，可以通过调用函数dvmSetJniEnvThreadId
     * 来将它们关联起来。注意，当参数self的值等于NULL的时候，就表示前面的JNIEnvExt对象是要与主线程
     * 关联的，但是要等到后面再关联，因为现在用来描述主线程的Thread对象还没有准备好。通过将一个
     * JNIEnvExt对象的成员变量envThreadId和self的值都设置为0x77777779来表示它还没有与线程关联。
     */
    newEnv->funcTable = &gNativeInterface;
    if (self != NULL) {
        dvmSetJniEnvThreadId((JNIEnv*) newEnv, self);
        assert(newEnv->envThreadId != 0);
    } else {
        /* make it obvious if we fail to initialize these later */
        newEnv->envThreadId = 0x77777775;
        newEnv->self = (Thread*) 0x77777779;
    }
    if (gDvmJni.useCheckJni) {
        dvmUseCheckedJniEnv(newEnv);
    }

    ScopedPthreadMutexLock lock(&vm->envListLock);

    /* 3.在一个Dalvik虚拟机里面，可以运行多个线程。所有关联有JNI环境的线程都有一个对应的
     * JNIEnvExt对象，这些JNIEnvExt对象相互连接在一起保存在用来描述其宿主Dalvik虚拟机的一个
     * JavaVMExt对象的成员变量envList中。因此，前面创建的JNIEnvExt对象需要连接到其宿主
     * Dalvik虚拟机的JavaVMExt链表中去。
     */
    /* insert at head of list */
    newEnv->next = vm->envList;
    assert(newEnv->prev == NULL);
    if (vm->envList == NULL) {
        // rare, but possible
        vm->envList = newEnv;
    } else {
        vm->envList->prev = newEnv;
    }
    vm->envList = newEnv;

    //if (self != NULL)
    //    ALOGI("Xit CreateJNIEnv: threadid=%d %p", self->threadId, self);
    return (JNIEnv*) newEnv;
}
```