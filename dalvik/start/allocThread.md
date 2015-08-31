allocThread
========================================

allocThread方法用于给一个Thread结构体指针分配内存并进行初始化操作.在这里是给主线程分配Thread结构，
参数是gDvm.mainThreadStackSize,默认主线程的堆栈大小为16KB(Thread.h中声明),已可以在
AndroidRuntime.cpp中通过"-XX:mainThreadStackSize=24K"来进行配置.

path: dalvik/vm/Thread.cpp
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

    ...

    return thread;
}
```

Thread
----------------------------------------

Thread结构体的定义如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/Thread.md

allocThread主要做了如下工作:

1.为thread分配内存.
----------------------------------------

```
    thread = (Thread*) calloc(1, sizeof(Thread));
    if (thread == NULL)
        return NULL;
```

2.检查并初始化
----------------------------------------

检查thread->interpBreak.all是否8字节对齐; 检查InterpBreak大小是否合法;
设置刚创建线程的状态为初始化状态.检查所要创建的线程的指定的堆栈大小是否合法

```
#if defined(WITH_SELF_VERIFICATION)
    if (dvmSelfVerificationShadowSpaceAlloc(thread) == NULL)
        return NULL;
#endif

    /* Check sizes and alignment */
    assert((((uintptr_t)&thread->interpBreak.all) & 0x7) == 0);
    assert(sizeof(thread->interpBreak) == sizeof(thread->interpBreak.all));
    assert(interpStackSize >= kMinStackSize && interpStackSize <=kMaxStackSize);

    thread->status = THREAD_INITIALIZING;
```

3.创建堆栈空间
----------------------------------------

为刚创建的线程创建interpStackSize大小的堆栈空间.stackBottom指向起始地址.
初始化Thread堆栈相关变量.

```
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
```

4.初始化堆栈
----------------------------------------

```
#ifndef DVM_NO_ASM_INTERP
    thread->mainHandlerTable = dvmAsmInstructionStart;
    thread->altHandlerTable = dvmAsmAltInstructionStart;
    thread->interpBreak.ctl.curHandlerTable = thread->mainHandlerTable;
#endif

    /* give the thread code a chance to set things up */
    dvmInitInterpStack(thread, interpStackSize);


```

调用dvmInitInterpStack初始化新建线程的解释器堆栈.

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

5.初始化解释器状态
----------------------------------------

调用dvmInitInterpreterState初始化新建线程的解释器状态.

```
    /* One-time setup for interpreter/JIT state */
    dvmInitInterpreterState(thread);
```
