prepareThread
========================================

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
    ...

    return true;
}
```

1.设置线程ID
----------------------------------------

```
    assignThreadId(thread);
    thread->handle = pthread_self();
    thread->systemTid = dvmGetSysThreadId();
    //ALOGI("SYSTEM TID IS %d (pid is %d)", (int) thread->systemTid,
    //    (int) getpid());
```

### assignThreadId

assignThreadId函数用于设置Thread的id标记来标记线程,其计数由1开始,其计数是记录在进程内记录，
用于标记进程内部的线程,如下所示:

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

#### dvmAllocBit

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

### handle

使用pthread_self()获取当前线程自身的ID并赋值给Thread的handle成员变量.

### systemTid

调用dvmGetSysThreadId()使用gettid()函数来获取线程ID赋值给Thread的成员变量systemTid.

2.setThreadSelf
----------------------------------------

setThreadSelf函数将新建的Thread保存到TSB中

```
    /*
     * If we were called by dvmAttachCurrentThread, the self value is
     * already correctly established as "thread".
     */
    setThreadSelf(thread);

    ALOGV("threadid=%d: interp stack at %p",
        thread->threadId, thread->interpStackStart - thread->interpStackSize);

```

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

3.初始化线程同步机制
----------------------------------------

```
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
```
