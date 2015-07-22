dvmThreadStartup
========================================

gDvm VS gDvmJni
----------------------------------------

path: dalvik/vm/Init.cpp
```
/* global state */
struct DvmGlobals gDvm;
struct DvmJniGlobals gDvmJni;
```

struct DvmGlobals定义如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/Globals.h

dvmThreadStartup
----------------------------------------

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

    /* allocate a TLS slot */
    if (pthread_key_create(&gDvm.pthreadKeySelf, threadExitCheck) != 0) {
        ALOGE("ERROR: pthread_key_create failed");
        return false;
    }

    /* test our pthread lib */
    if (pthread_getspecific(gDvm.pthreadKeySelf) != NULL)
        ALOGW("WARNING: newly-created pthread TLS slot is not NULL");

    /* prep thread-related locks and conditions */
    dvmInitMutex(&gDvm.threadListLock);
    pthread_cond_init(&gDvm.threadStartCond, NULL);
    pthread_cond_init(&gDvm.vmExitCond, NULL);
    dvmInitMutex(&gDvm._threadSuspendLock);
    dvmInitMutex(&gDvm.threadSuspendCountLock);
    pthread_cond_init(&gDvm.threadSuspendCountCond, NULL);

    /*
     * Dedicated monitor for Thread.sleep().
     * TODO: change this to an Object* so we don't have to expose this
     * call, and we interact better with JDWP monitor calls.  Requires
     * deferring the object creation to much later (e.g. final "main"
     * thread prep) or until first use.
     */
    gDvm.threadSleepMon = dvmCreateMonitor(NULL);

    gDvm.threadIdMap = dvmAllocBitVector(kMaxThreadId, false);

    thread = allocThread(gDvm.mainThreadStackSize);
    if (thread == NULL)
        return false;

    /* switch mode for when we run initializers */
    thread->status = THREAD_RUNNING;

    /*
     * We need to assign the threadId early so we can lock/notify
     * object monitors.  We'll set the "threadObj" field later.
     */
    prepareThread(thread);
    gDvm.threadList = thread;

#ifdef COUNT_PRECISE_METHODS
    gDvm.preciseMethods = dvmPointerSetAlloc(200);
#endif

    return true;
}
```

### 1.pthread_key_create

创建线程私有数据, 该函数从TSD池中分配一项，将其地址值赋给gDvm.pthreadKeySelf供以后
所有线程访问使用。不论哪个线程调用了pthread_key_create()，所创建的 gDvm.pthreadKeySelf
都是所有线程可以访问的，但各个线程可以根据自己的需要往 gDvm.pthreadKeySelf 中填入不同
的值，相当于提供了一个同名而不同值的全局变量(这个全局变量相对于拥有这个变量的线程来说)。

### 2.初始化线程的互斥锁和条件变量

#### threadList

This always has at least one element in it (main),
and main is always the first entry.

#### dvmInitMutex(&gDvm.threadListLock);
#### pthread_cond_init(&gDvm.threadStartCond, NULL);

threadListLock同线程启动条件变量(threadStartCond)协同使用，用于如下几件事情的时候需要
拿到threadListLock:

* 向threadList中添加/删除一个线程item.
* 等待threadStartCond发送信号

#### pthread_cond_init(&gDvm.vmExitCond, NULL);

vmExitCond管理线程退出的条件变量,

#### dvmInitMutex(&gDvm._threadSuspendLock);

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

#### dvmInitMutex(&gDvm.threadSuspendCountLock);

Guards Thread->suspendCount for all threads, and
provides the lock for the condition variable that all suspended threads
sleep on (threadSuspendCountCond).
This has to be separate from threadListLock because of the way
threads put themselves to sleep.

#### pthread_cond_init(&gDvm.threadSuspendCountCond, NULL);

Suspended threads sleep on this.  They should sleep on the condition
variable until their "suspend count" is zero.
Paired with "threadSuspendCountLock".

### 3.allocThread

allocThread方法用于给一个Thread结构体指针分配内存并进行初始化操作.

Thread结构体的定义如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/Thread.md

```
/*
 * Alloc and initialize a Thread struct.
 *
 * Does not create any objects, just stuff on the system (malloc) heap.
 */
static Thread* allocThread(int interpStackSize)
{
    Thread* thread;
    u1* stackBottom;

    thread = (Thread*) calloc(1, sizeof(Thread));
    if (thread == NULL)
        return NULL;

    /* Check sizes and alignment */
    assert((((uintptr_t)&thread->interpBreak.all) & 0x7) == 0);
    assert(sizeof(thread->interpBreak) == sizeof(thread->interpBreak.all));


#if defined(WITH_SELF_VERIFICATION)
    if (dvmSelfVerificationShadowSpaceAlloc(thread) == NULL)
        return NULL;
#endif

    assert(interpStackSize >= kMinStackSize && interpStackSize <=kMaxStackSize);

    thread->status = THREAD_INITIALIZING;

    /*
     * Allocate and initialize the interpreted code stack.  We essentially
     * "lose" the alloc pointer, which points at the bottom of the stack,
     * but we can get it back later because we know how big the stack is.
     *
     * The stack must be aligned on a 4-byte boundary.
     */
#ifdef MALLOC_INTERP_STACK
    stackBottom = (u1*) malloc(interpStackSize);
    if (stackBottom == NULL) {
#if defined(WITH_SELF_VERIFICATION)
        dvmSelfVerificationShadowSpaceFree(thread);
#endif
        free(thread);
        return NULL;
    }
    memset(stackBottom, 0xc5, interpStackSize);     // stop valgrind complaints
#else
    stackBottom = (u1*) mmap(NULL, interpStackSize, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANON, -1, 0);
    if (stackBottom == MAP_FAILED) {
#if defined(WITH_SELF_VERIFICATION)
        dvmSelfVerificationShadowSpaceFree(thread);
#endif
        free(thread);
        return NULL;
    }
#endif

    assert(((u4)stackBottom & 0x03) == 0); // looks like our malloc ensures this
    thread->interpStackSize = interpStackSize;
    thread->interpStackStart = stackBottom + interpStackSize;
    thread->interpStackEnd = stackBottom + STACK_OVERFLOW_RESERVE;

#ifndef DVM_NO_ASM_INTERP
    thread->mainHandlerTable = dvmAsmInstructionStart;
    thread->altHandlerTable = dvmAsmAltInstructionStart;
    thread->interpBreak.ctl.curHandlerTable = thread->mainHandlerTable;
#endif

    /* give the thread code a chance to set things up */
    dvmInitInterpStack(thread, interpStackSize);

    /* One-time setup for interpreter/JIT state */
    dvmInitInterpreterState(thread);

    return thread;
}
```

### 2.prepareThread

```
/*
 * Finish initialization of a Thread struct.
 *
 * This must be called while executing in the new thread, but before the
 * thread is added to the thread list.
 *
 * NOTE: The threadListLock must be held by the caller (needed for
 * assignThreadId()).
 */
static bool prepareThread(Thread* thread)
{
    assignThreadId(thread);
    thread->handle = pthread_self();
    thread->systemTid = dvmGetSysThreadId();

    //ALOGI("SYSTEM TID IS %d (pid is %d)", (int) thread->systemTid,
    //    (int) getpid());
    /*
     * If we were called by dvmAttachCurrentThread, the self value is
     * already correctly established as "thread".
     */
    setThreadSelf(thread);

    ALOGV("threadid=%d: interp stack at %p",
        thread->threadId, thread->interpStackStart - thread->interpStackSize);

    /*
     * Initialize invokeReq.
     */
    dvmInitMutex(&thread->invokeReq.lock);
    pthread_cond_init(&thread->invokeReq.cv, NULL);

    /*
     * Initialize our reference tracking tables.
     *
     * Most threads won't use jniMonitorRefTable, so we clear out the
     * structure but don't call the init function (which allocs storage).
     */
    if (!thread->jniLocalRefTable.init(kJniLocalRefMin,
            kJniLocalRefMax, kIndirectKindLocal)) {
        return false;
    }
    if (!dvmInitReferenceTable(&thread->internalLocalRefTable,
            kInternalRefDefault, kInternalRefMax))
        return false;

    memset(&thread->jniMonitorRefTable, 0, sizeof(thread->jniMonitorRefTable));

    pthread_cond_init(&thread->waitCond, NULL);
    dvmInitMutex(&thread->waitMutex);

    /* Initialize safepoint callback mechanism */
    dvmInitMutex(&thread->callbackMutex);

    return true;
}
```