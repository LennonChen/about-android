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
在这里是给主线程分配Thread结构提，参数是gDvm.mainThreadStackSize,
默认主线程的堆栈大小为16KB(Thread.h中声明),已可以在AndroidRuntime.cpp
中通过"-XX:mainThreadStackSize=24K"来进行配置.

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

allocThread主要做了如下工作:

* 1.为thread分配内存.

* 2.检查thread->interpBreak.all是否8字节对齐; 检查InterpBreak大小是否合法;
  检查所要创建的线程的指定的堆栈大小是否合法

* 3.设置刚创建线程的状态为初始化状态.

* 4.为刚创建的线程创建interpStackSize大小的堆栈空间.stackBottom指向起始地址.
  初始化Thread堆栈相关变量.

```
    assert(((u4)stackBottom & 0x03) == 0); // looks like our malloc ensures this
    thread->interpStackSize = interpStackSize;
    thread->interpStackStart = stackBottom + interpStackSize;
    thread->interpStackEnd = stackBottom + STACK_OVERFLOW_RESERVE;
```

* 5.调用dvmInitInterpStack初始化新建线程的解释器堆栈.

path: dalvik/vm/Stack.cpp
```
/*
 * Initialize the interpreter stack in a new thread.
 *
 * Currently this doesn't do much, since we don't need to zero out the
 * stack (and we really don't want to if it was created with mmap).
 */
bool dvmInitInterpStack(Thread* thread, int stackSize)
{
    assert(thread->interpStackStart != NULL);

    assert(thread->interpSave.curFrame == NULL);

    return true;
}
```

* 6.调用dvmInitInterpreterState初始化新建线程的解释器状态.

从allocThread函数调用返回回到dvmThreadStartup函数中, 接下来将为gDvm的成员变量
threadIdMap创建一个大小为kMaxThreadId大小的BitVector, 之后再将thread状态设置为
THREAD_RUNNING

### 4. gDvm.threadIdMap

path: dalvik/vm/Thread.cpp
```
#define kMaxThreadId        ((1 << 16) - 1)   // 65535
#define kMainThreadId       1
...
gDvm.threadIdMap = dvmAllocBitVector(kMaxThreadId, false);
```

path: dalvik/vm/Globals.h
```
    /*
     * Thread ID bitmap.  We want threads to have small integer IDs so
     * we can use them in "thin locks".
     */
    BitVector*  threadIdMap;
```

BitVector的结构如下所示:

path: dalvik/vm/BitVector.h
```
/*
 * Expanding bitmap, used for tracking resources.  Bits are numbered starting
 * from zero.
 *
 * All operations on a BitVector are unsynchronized.
 */
struct BitVector {
    bool    expandable;     /* expand bitmap if we run out? */
    u4      storageSize;    /* current size, in 32-bit words */
    u4*     storage;
};
```

path: dalvik/vm/BitVector.cpp
```
/*
 * Allocate a bit vector with enough space to hold at least the specified
 * number of bits.
 */
BitVector* dvmAllocBitVector(unsigned int startBits, bool expandable)
{
    BitVector* bv;
    unsigned int count;

    assert(sizeof(bv->storage[0]) == 4);        /* assuming 32-bit units */

    bv = (BitVector*) malloc(sizeof(BitVector));

    count = (startBits + 31) >> 5; // (65535 + 31) >> 5 ==> 2048

    bv->storageSize = count;
    bv->expandable = expandable;
    bv->storage = (u4*) calloc(count, sizeof(u4));
    return bv;
}
```

接下来调用prepareThread函数来设置Thread的相关信息.

### 5.prepareThread

path: dalvik/vm/Thread.cpp
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

#### 设置线程ID

1. assignThreadId函数用于设置Thread的id标记来标记线程,其计数由1开始,
其计数是记录在进程内记录，用于标记进程内部的线程,如下所示:

path: dalvik/vm/Thread.cpp
```
/*
 * Assign the threadId.  This needs to be a small integer so that our
 * "thin" locks fit in a small number of bits.
 *
 * We reserve zero for use as an invalid ID.
 *
 * This must be called with threadListLock held.
 */
static void assignThreadId(Thread* thread)
{
    /*
     * Find a small unique integer.  threadIdMap is a vector of
     * kMaxThreadId bits;  dvmAllocBit() returns the index of a
     * bit, meaning that it will always be < kMaxThreadId.
     */
    int num = dvmAllocBit(gDvm.threadIdMap);
    if (num < 0) {
        ALOGE("Ran out of thread IDs");
        dvmAbort();     // TODO: make this a non-fatal error result
    }

    thread->threadId = num + 1; // 设置线程id

    assert(thread->threadId != 0);
}
```

##### dvmAllocBit

path: dalvik/vm/BitVector.cpp
```
/*
 * "Allocate" the first-available bit in the bitmap.
 *
 * This is not synchronized.  The caller is expected to hold some sort of
 * lock that prevents multiple threads from executing simultaneously in
 * dvmAllocBit/dvmFreeBit.
 */
int dvmAllocBit(BitVector* pBits)
{
    unsigned int word, bit;

retry:
    for (word = 0; word < pBits->storageSize; word++) {
        if (pBits->storage[word] != 0xffffffff) {
            /*
             * There are unallocated bits in this word.  Return the first.
             * ffs()函数用于查找一个整数中的第一个置位值(也就是bit为1的位)。
             */
            bit = ffs(~(pBits->storage[word])) -1;
            assert(bit < 32);
            pBits->storage[word] |= 1 << bit;
            return (word << 5) | bit;
        }
    }

    /*
     * Ran out of space, allocate more if we're allowed to.
     */
    if (!pBits->expandable)
        return -1;

    pBits->storage = (u4*)realloc(pBits->storage,
                    (pBits->storageSize + kBitVectorGrowth) * sizeof(u4));
    memset(&pBits->storage[pBits->storageSize], 0x00,
        kBitVectorGrowth * sizeof(u4));
    pBits->storageSize += kBitVectorGrowth;
    goto retry;
}
```

* 2.使用pthread_self()获取当前线程自身的ID并赋值给Thread的handle成员变量.

* 3.调用dvmGetSysThreadId()使用gettid()函数来获取线程ID赋值给Thread的成员
    变量systemTid.

#### setThreadSelf

setThreadSelf函数将新建的Thread保存到TSB中

path: dalvik/vm/Thread.cpp
```
/*
 * Explore our sense of self.  Stuffs the thread pointer into TLS.
 */
static void setThreadSelf(Thread* thread)
{
    int cc;

    cc = pthread_setspecific(gDvm.pthreadKeySelf, thread);
    if (cc != 0) {
        /*
         * Sometimes this fails under Bionic with EINVAL during shutdown.
         * This can happen if the timing is just right, e.g. a thread
         * fails to attach during shutdown, but the "fail" path calls
         * here to ensure we clean up after ourselves.
         */
        if (thread != NULL) {
            ALOGE("pthread_setspecific(%p) failed, err=%d", thread, cc);
            dvmAbort();     /* the world is fundamentally hosed */
        }
    }
}
```

在完成prepareThread之后，启动线程的绝大部分工作都已经完成.