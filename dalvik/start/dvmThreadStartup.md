dvmThreadStartup
========================================

path: dalvik/vm/Thread.cpp
```
/*
 * Initialize thread list and main thread's environment.  We need to set
 * up some basic stuff so that dvmThreadSelf() will work when we start
 * loading classes (e.g. to check for exceptions).
 */
bool dvmThreadStartup()
{
    Thread* thread;

    ...

    return true;
}
```

1.pthread_key_create
----------------------------------------

创建线程私有数据, 该函数从TSD池中分配一项，将其地址值赋给gDvm.pthreadKeySelf供以后
所有线程访问使用。不论哪个线程调用了pthread_key_create()，所创建的 gDvm.pthreadKeySelf
都是所有线程可以访问的，但各个线程可以根据自己的需要往 gDvm.pthreadKeySelf 中填入不同
的值，相当于提供了一个同名而不同值的全局变量(这个全局变量相对于拥有这个变量的线程来说)。

```
    /* allocate a TLS slot */
    if (pthread_key_create(&gDvm.pthreadKeySelf, threadExitCheck) != 0) {
        ALOGE("ERROR: pthread_key_create failed");
        return false;
    }

    /* test our pthread lib */
    if (pthread_getspecific(gDvm.pthreadKeySelf) != NULL)
        ALOGW("WARNING: newly-created pthread TLS slot is not NULL");
```

2.初始化线程的互斥锁和条件变量
----------------------------------------

```
    /* prep thread-related locks and conditions */
    dvmInitMutex(&gDvm.threadListLock);
    pthread_cond_init(&gDvm.threadStartCond, NULL);
    pthread_cond_init(&gDvm.vmExitCond, NULL);
    dvmInitMutex(&gDvm._threadSuspendLock);
    dvmInitMutex(&gDvm.threadSuspendCountLock);
    pthread_cond_init(&gDvm.threadSuspendCountCond, NULL);
```

### threadList

This always has at least one element in it (main),
and main is always the first entry.

### dvmInitMutex(&gDvm.threadListLock);

### pthread_cond_init(&gDvm.threadStartCond, NULL);

threadListLock同线程启动条件变量(threadStartCond)协同使用，用于如下几件事情的时候需要
拿到threadListLock:

* 向threadList中添加/删除一个线程item.
* 等待threadStartCond发送信号

### pthread_cond_init(&gDvm.vmExitCond, NULL);

vmExitCond管理线程退出的条件变量,

### dvmInitMutex(&gDvm._threadSuspendLock);

The thread code grabs this before suspending all threads.  There
are a few things that can cause a "suspend all":
(1) the GC is starting;
(2) the debugger has sent a "suspend all" request;
(3) a thread has hit a breakpoint or exception that the debugger
    has marked as a "suspend all" event;
(4) the SignalCatcher caught a signal that requires suspension.
(5) (if implemented) the JIT needs to perform a heavyweight
    rearrangement of the translation cache or JitTable.

Because we use "safe point" self-suspension, it is never safe to
do a blocking "lock" call on this mutex -- if it has been acquired,
somebody is probably trying to put you to sleep.  The leading '_' is
intended as a reminder that this lock is special.

### dvmInitMutex(&gDvm.threadSuspendCountLock);

Guards Thread->suspendCount for all threads, and
provides the lock for the condition variable that all suspended threads
sleep on (threadSuspendCountCond).
This has to be separate from threadListLock because of the way
threads put themselves to sleep.

### pthread_cond_init(&gDvm.threadSuspendCountCond, NULL);

Suspended threads sleep on this.  They should sleep on the condition
variable until their "suspend count" is zero.
Paired with "threadSuspendCountLock".

3.gDvm.threadIdMap
----------------------------------------

```
    /*
     * Dedicated monitor for Thread.sleep().
     * TODO: change this to an Object* so we don't have to expose this
     * call, and we interact better with JDWP monitor calls.  Requires
     * deferring the object creation to much later (e.g. final "main"
     * thread prep) or until first use.
     */
    gDvm.threadSleepMon = dvmCreateMonitor(NULL);

    gDvm.threadIdMap = dvmAllocBitVector(kMaxThreadId, false);

```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmAllocBitVector.md

4.allocThread
----------------------------------------

```
    thread = allocThread(gDvm.mainThreadStackSize);
    if (thread == NULL)
        return false;

    /* switch mode for when we run initializers */
    thread->status = THREAD_RUNNING;
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/allocThread.md

接下来调用prepareThread函数来设置Thread的相关信息.

### 5.prepareThread

```
    /*
     * We need to assign the threadId early so we can lock/notify
     * object monitors.  We'll set the "threadObj" field later.
     */
    prepareThread(thread);
    gDvm.threadList = thread;

#ifdef COUNT_PRECISE_METHODS
    gDvm.preciseMethods = dvmPointerSetAlloc(200);
#endif
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/prepareThread.md
