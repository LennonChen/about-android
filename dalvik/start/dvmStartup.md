dvmStartup
========================================

path: dalvik/vm/Init.cpp
```
/*
 * VM initialization.  Pass in any options provided on the command line.
 * Do not pass in the class name or the options for the class.
 *
 * Returns 0 on success.
 */
std::string dvmStartup(int argc, const char* const argv[],
        bool ignoreUnrecognized, JNIEnv* pEnv)
{
    ScopedShutdown scopedShutdown;

    assert(gDvm.initializing);

    ALOGV("VM init args (%d):", argc);
    for (int i = 0; i < argc; i++) {
        ALOGV("  %d: '%s'", i, argv[i]);
    }
    setCommandLineDefaults();

    /*
     * Process the option flags (if any).
     */
    int cc = processOptions(argc, argv, ignoreUnrecognized);
    if (cc != 0) {
        if (cc < 0) {
            dvmFprintf(stderr, "\n");
            usage("dalvikvm");
        }
        return "syntax error";
    }

#if WITH_EXTRA_GC_CHECKS > 1
    /* only "portable" interp has the extra goodies */
    if (gDvm.executionMode != kExecutionModeInterpPortable) {
        ALOGI("Switching to 'portable' interpreter for GC checks");
        gDvm.executionMode = kExecutionModeInterpPortable;
    }
#endif

    /* Configure group scheduling capabilities */
    if (!access("/dev/cpuctl/tasks", F_OK)) {
        ALOGV("Using kernel group scheduling");
        gDvm.kernelGroupScheduling = 1;
    } else {
        ALOGV("Using kernel scheduler policies");
    }

    /* configure signal handling */
    if (!gDvm.reduceSignals)
        blockSignals();

    /* verify system page size */
    if (sysconf(_SC_PAGESIZE) != SYSTEM_PAGE_SIZE) {
        return StringPrintf("expected page size %d, got %d",
                SYSTEM_PAGE_SIZE, (int) sysconf(_SC_PAGESIZE));
    }

    /* mterp setup */
    ALOGV("Using executionMode %d", gDvm.executionMode);
    dvmCheckAsmConstants();

    /*
     * Initialize components.
     */
    dvmQuasiAtomicsStartup();
    if (!dvmAllocTrackerStartup()) {
        return "dvmAllocTrackerStartup failed";
    }
    if (!dvmGcStartup()) {
        return "dvmGcStartup failed";
    }
    if (!dvmThreadStartup()) {
        return "dvmThreadStartup failed";
    }
    if (!dvmInlineNativeStartup()) {
        return "dvmInlineNativeStartup";
    }
    if (!dvmRegisterMapStartup()) {
        return "dvmRegisterMapStartup failed";
    }
    if (!dvmInstanceofStartup()) {
        return "dvmInstanceofStartup failed";
    }
    if (!dvmClassStartup()) {
        return "dvmClassStartup failed";
    }

    /*
     * At this point, the system is guaranteed to be sufficiently
     * initialized that we can look up classes and class members. This
     * call populates the gDvm instance with all the class and member
     * references that the VM wants to use directly.
     */
    if (!dvmFindRequiredClassesAndMembers()) {
        return "dvmFindRequiredClassesAndMembers failed";
    }
    if (!dvmStringInternStartup()) {
        return "dvmStringInternStartup failed";
    }
    if (!dvmNativeStartup()) {
        return "dvmNativeStartup failed";
    }
    if (!dvmInternalNativeStartup()) {
        return "dvmInternalNativeStartup failed";
    }
    if (!dvmJniStartup()) {
        return "dvmJniStartup failed";
    }
    if (!dvmProfilingStartup()) {
        return "dvmProfilingStartup failed";
    }

    /*
     * Create a table of methods for which we will substitute an "inline"
     * version for performance.
     */
    if (!dvmCreateInlineSubsTable()) {
        return "dvmCreateInlineSubsTable failed";
    }

    /*
     * Miscellaneous class library validation.
     */
    if (!dvmValidateBoxClasses()) {
        return "dvmValidateBoxClasses failed";
    }

    /*
     * Do the last bits of Thread struct initialization we need to allow
     * JNI calls to work.
     */
    if (!dvmPrepMainForJni(pEnv)) {
        return "dvmPrepMainForJni failed";
    }

    /*
     * Explicitly initialize java.lang.Class.  This doesn't happen
     * automatically because it's allocated specially (it's an instance
     * of itself).  Must happen before registration of system natives,
     * which make some calls that throw assertions if the classes they
     * operate on aren't initialized.
     */
    if (!dvmInitClass(gDvm.classJavaLangClass)) {
        return "couldn't initialized java.lang.Class";
    }

    /*
     * Register the system native methods, which are registered through JNI.
     */
    if (!registerSystemNatives(pEnv)) {
        return "couldn't register system natives";
    }

    /*
     * Do some "late" initialization for the memory allocator.  This may
     * allocate storage and initialize classes.
     */
    if (!dvmCreateStockExceptions()) {
        return "dvmCreateStockExceptions failed";
    }

    /*
     * At this point, the VM is in a pretty good state.  Finish prep on
     * the main thread (specifically, create a java.lang.Thread object to go
     * along with our Thread struct).  Note we will probably be executing
     * some interpreted class initializer code in here.
     */
    if (!dvmPrepMainThread()) {
        return "dvmPrepMainThread failed";
    }

    /*
     * Make sure we haven't accumulated any tracked references.  The main
     * thread should be starting with a clean slate.
     */
    if (dvmReferenceTableEntries(&dvmThreadSelf()->internalLocalRefTable) != 0)
    {
        ALOGW("Warning: tracked references remain post-initialization");
        dvmDumpReferenceTable(&dvmThreadSelf()->internalLocalRefTable, "MAIN");
    }

    /* general debugging setup */
    if (!dvmDebuggerStartup()) {
        return "dvmDebuggerStartup failed";
    }

    if (!dvmGcStartupClasses()) {
        return "dvmGcStartupClasses failed";
    }

    /*
     * Init for either zygote mode or non-zygote mode.  The key difference
     * is that we don't start any additional threads in Zygote mode.
     */
    if (gDvm.zygote) {
        if (!initZygote()) {
            return "initZygote failed";
        }
        dvmPostInitZygote();
    } else {
        if (!dvmInitAfterZygote()) {
            return "dvmInitAfterZygote failed";
        }
    }


#ifndef NDEBUG
    if (!dvmTestHash())
        ALOGE("dvmTestHash FAILED");
    if (false /*noisy!*/ && !dvmTestIndirectRefTable())
        ALOGE("dvmTestIndirectRefTable FAILED");
#endif

    if (dvmCheckException(dvmThreadSelf())) {
        dvmLogExceptionStackTrace();
        return "Exception pending at end of VM initialization";
    }

    scopedShutdown.disarm();
    return "";
}
```

setCommandLineDefaults
----------------------------------------

1.调用函数setCommandLineDefaults来给Dalvik虚拟机设置默认参数，因为启动选项不一定会指定
Dalvik虚拟机的所有属性。

https://github.com/leeminghao/about-android/blob/master/dalvik/start/setCommandLineDefaults.md

processOptions
----------------------------------------

2.调用函数processOptions来处理参数argv和argc所描述的启动选项了，也就是根据这些选项值来
设置Dalvik虚拟机的属性，例如，设置Dalvik虚拟机的Java对象堆的最大值。

https://github.com/leeminghao/about-android/blob/master/dalvik/start/processOptions.md

blockSignals
----------------------------------------

3.如果我们没有在Dalvik虚拟机的启动选项中指定-Xrs，那么gDvm.reduceSignals的值就会被设置为
false，表示要在当前线程中屏蔽掉SIGQUIT信号。在这种情况下，会有一个线程专门用来处理SIGQUIT
信号。这个线程在接收到SIGQUIT信号的时候，就会将各个线程的调用堆栈打印出来，因此，这个线程
又称为dump-stack-trace线程。屏蔽当前线程的SIGQUIT信号是通过调用函数blockSignals来实现的

path: dalvik/vm/Init.cpp
```
/*
 * Configure signals.  We need to block SIGQUIT so that the signal only
 * reaches the dump-stack-trace thread.
 *
 * This can be disabled with the "-Xrs" flag.
 */
static void blockSignals()
{
    sigset_t mask;
    int cc;

    sigemptyset(&mask);
    sigaddset(&mask, SIGQUIT);
    sigaddset(&mask, SIGUSR1);      // used to initiate heap dump
#if defined(WITH_JIT) && defined(WITH_JIT_TUNING)
    sigaddset(&mask, SIGUSR2);      // used to investigate JIT internals
#endif
    sigaddset(&mask, SIGPIPE);
    cc = sigprocmask(SIG_BLOCK, &mask, NULL);
    assert(cc == 0);

    if (false) {
        /* TODO: save the old sigaction in a global */
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_sigaction = busCatcher;
        sa.sa_flags = SA_SIGINFO;
        cc = sigaction(SIGBUS, &sa, NULL);
        assert(cc == 0);
    }
}
```

dvmAllocTrackerStartup
----------------------------------------

4.函数dvmAllocTrackerStartup用来初始化Davlik虚拟机的对象分配记录子模块，这样我们就可以通过
DDMS工具来查看Davlik虚拟机的对象分配情况。

path: dalvik/vm/AllocTracker.c
```
/*
 * Initialize a few things.  This gets called early, so keep activity to
 * a minimum.
 */
bool dvmAllocTrackerStartup()
{
    /* prep locks */
    dvmInitMutex(&gDvm.allocTrackerLock);

    /* initialized when enabled by DDMS */
    assert(gDvm.allocRecords == NULL);

    return true;
}
```

dvmGcStartUp
----------------------------------------

5.函数dvmGcStartup用来初始化Davlik虚拟机的垃圾收集(GC)子模块。

path: dalvik/vm/alloc/Alloc.cpp
```
/*
 * Initialize the GC universe.
 *
 * We're currently using a memory-mapped arena to keep things off of the
 * main heap.  This needs to be replaced with something real.
 */
bool dvmGcStartup()
{
    dvmInitMutex(&gDvm.gcHeapLock);
    pthread_cond_init(&gDvm.gcHeapCond, NULL);
    return dvmHeapStartup();
}
```

dvmThreadStartup
----------------------------------------

6.函数dvmThreadStartup用来初始化Davlik虚拟机的线程列表、为主线程创建一个Thread对象以及
为主线程初始化执行环境。Davlik虚拟机中的所有线程均是本地操作系统线程。
在Linux系统中，一般都是使用pthread库来创建和管理线程的，Android系统也不例外，也就是说，
Davlik虚拟机中的每一个线程均是一个pthread线程。注意，Davlik虚拟机中的每一个线程均用一个
Thread结构体来描述，这些Thread结构体组织在一个列表中，因此，这里要先对它进行初始化。

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmThreadStartup.md

dvmInlineNativeStartup
----------------------------------------

7.函数dvmInlineNativeStartup用来初始化Davlik虚拟机的内建Native函数表。这些内建Native函数主要是
针对java.Lang.String、java.Lang.Math、java.Lang.Float和java.Lang.Double类的，用来替换这些类的
某些成员函数原来的实现（包括Java实现和Native实现）。例如，当我们调用java.Lang.String类的成员函数
compareTo来比较两个字符串的大小时，实际执行的是由Davlik虚拟机提供的内建函数javaLangString_compareTo
(同样是定义在文件dalvik/vm/InlineNative.cpp中)。在提供有__memcmp16函数的系统中，函数
javaLangString_compareTo会利用它来直接比较两个字符串的大小。由于函数__memcmp16是用优化过的汇编语言
的来实现的，它的效率会更高。

path: dalvik/vm/InlineNative.cpp
```
/*
 * Allocate some tables.
 */
bool dvmInlineNativeStartup()
{
    gDvm.inlinedMethods =
        (Method**) calloc(NELEM(gDvmInlineOpsTable), sizeof(Method*));
    if (gDvm.inlinedMethods == NULL)
        return false;

    return true;
}
```

dvmRegisterMapStartup
----------------------------------------

8.函数dvmRegisterMapStartup用来初始化寄存器映射集(Register Map)子模块。Davlik虚拟机支持
精确垃圾收集(Exact GC或者Precise GC)，也就是说，在进行垃圾收集的时候，Davlik虚拟机可以准确
地判断当前正在使用的每一个寄存器里面保存的是对象使用还是非对象引用。对于对象引用，意味被引用的
对象现在还不可以回收，因此，就可以进行精确的垃圾收集。为了帮助垃圾收集器准备地判断寄存器保存的
是对象引用还是非对象引用，Davlik虚拟机在验证了一个类之后，还会为它的每一个成员函数生成一个寄存器
映射集。寄存器映射集记录了类成员函数在每一个GC安全点(Safe Point)中的寄存器使用情况，也就是记录
每一个寄存器里面保存的是对象引用还是非对象引用。由于垃圾收集器一定是在GC安全点进行垃圾收集的，
因此，根据每一个GC安全点的寄存器映射集，就可以准确地知道对象的引用情况，从而可以确定哪些可以回收，
哪些对象还不可以回收。

path: dalvik/vm/analysis/RegisterMap.cpp
```
/*
 * Prepare some things.
 */
bool dvmRegisterMapStartup()
{
#ifdef REGISTER_MAP_STATS
    MapStats* pStats = calloc(1, sizeof(MapStats));
    gDvm.registerMapStats = pStats;
#endif
    return true;
}
```

dvmInstanceofStartup
----------------------------------------

9.函数dvmInstanceofStartup用来初始化instanceof操作符子模块。在使用instanceof操作符来判断一个
对象A是否是一个类B的实例时，Davlik虚拟机需要检查类B是否是从对象A的声明类继承下来的。由于这个
检查的过程比较耗时，Davlik虚拟机在内部使用一个缓冲，用来记录第一次两个类之间的instanceof操作结果，
这样后面再碰到相同的instanceof操作时，就可以快速地得到结果。

path: dalvik/vm/oo/TypeCheck.cpp
```
/*
 * Allocate cache.
 */
bool dvmInstanceofStartup()
{
    gDvm.instanceofCache = dvmAllocAtomicCache(INSTANCEOF_CACHE_SIZE);
    if (gDvm.instanceofCache == NULL)
        return false;
    return true;
}
```

dvmClassStartup
----------------------------------------

10.函数dvmClassStartup用来初始化启动类加载器(Bootstrap Class Loader)，同时还会初始化
java.lang.Class类。启动类加载器是用来加载Java核心类的，用来保证安全性，即保证加载的
Java核心类是合法的。

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmClassStartup.md

dvmFindRequiredClassesAndMembers
----------------------------------------

11.函数dvmFindRequiredClassesAndMembers用来初始化一些必需的类.

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmFindRequiredClassesAndMembers.md

dvmStringInternStartup
----------------------------------------

12.函数dvmStringInternStartup用来初始化java.lang.String类内部私有一个字符串池，这样当Dalvik
虚拟机运行起来之后，我们就可以调用java.lang.String类的成员函数intern来访问这个字符串池里面的字符串。

path: dalvik/vm/Intern.cpp
```
/*
 * Prep string interning.
 */
bool dvmStringInternStartup()
{
    dvmInitMutex(&gDvm.internLock);
    gDvm.internedStrings = dvmHashTableCreate(256, NULL);
    if (gDvm.internedStrings == NULL)
        return false;
    gDvm.literalStrings = dvmHashTableCreate(256, NULL);
    if (gDvm.literalStrings == NULL)
        return false;
    return true;
}
```

dvmNativeStartup
----------------------------------------

13.函数dvmNativeStartup用来初始化Native Shared Object库加载表，也就是SO库加载表。
这个加载表是用来描述当前进程有哪些SO文件已经被加载过了。

path: dalvik/vm/Native.cpp
```
/*
 * Initialize the native code loader.
 */
bool dvmNativeStartup()
{
    gDvm.nativeLibs = dvmHashTableCreate(4, freeSharedLibEntry);
    if (gDvm.nativeLibs == NULL)
        return false;

    return true;
}
```

dvmInternalNativeStartup
----------------------------------------

14.函数dvmInternalNativeStartup用来初始化一个内部Native函数表。所有需要直接访问Dalvik虚拟机
内部函数或者数据结构的Native函数都定义在这张表中，因为它们如果定义在外部的其它SO文件中，
就无法直接访问Dalvik虚拟机的内部函数或者数据结构。例如，前面提到的java.lang.String类的
成员函数intent，由于它要访问Dalvik虚拟机内部的一个私有字符串池，因此，它所对应的Native函数
就要在Dalvik虚拟机内部实现。

path: dalvik/vm/InternalNative.cpp
```
/*
 * Set up hash values on the class names.
 */
bool dvmInternalNativeStartup()
{
    DalvikNativeClass* classPtr = gDvmNativeMethodSet;

    while (classPtr->classDescriptor != NULL) {
        classPtr->classDescriptorHash =
            dvmComputeUtf8Hash(classPtr->classDescriptor);
        classPtr++;
    }

    gDvm.userDexFiles = dvmHashTableCreate(2, dvmFreeDexOrJar);
    if (gDvm.userDexFiles == NULL)
        return false;

    return true;
}
```

dvmJniStartup
----------------------------------------

15.函数dvmJniStartup用来初始化全局引用表，以及加载一些与Direct Buffer相关的类，
如DirectBuffer、PhantomReference和ReferenceQueue等。我们在一个JNI方法中，
可能会需要访问一些Java对象，这样就需要通知GC，这些Java对象现在正在被Native Code引用，不能回收。
这些被Native Code引用的Java对象就会被记录在一个全局引用表中，具体的做法就是调用JNI环境对象
(JNIEnv)的成员函数NewLocalRef/DeleteLocalRef和NewGlobalRef/DeleteGlobalRef等来显式地引用或者
释放Java对象。有时候我们需要在Java代码中，直接在Native层分配内存，也就直接使用malloc来分配内存。
这些Native内存不同于在Java堆中分配的内存，区别在于前者需要不接受GC管理，而后者接受GC管理。
这些直接在Native层分配的内存有什么用呢？考虑一个场景，我们需要在Java代码中从一个IO设备中
读取数据。从IO设备读取数据意味着要调用由本地操作系统提供的read接口来实现。这样我们就有两种
做法。第一种做法在Native层临时分配一个缓冲区，用来保存从IO设备read回来的数据，然后再将这个
数据拷贝到Java层中去，也就是拷贝到Java堆去使用。第二种做法是在Java层创建一个对象，这个对象
在Native层直接关联有一块内存，从IO设备read回来的数据就直接保存这块内存中。第二种方法和
第一种方法相比，减少了一次内存拷贝，因而可以提高性能。
我们将这种能够直接在Native层中分配内存的Java对象就称为DirectBuffer。由于DirectBuffer使用的
内存是不接受GC管理的，因此，我们就需要通过其它的方式来管理它们。具体做法就是为每一个
DirectBuffer对象创建一个PhantomReference引用。注意，DirectBuffer对象本身是一个Java对象，
它是接受GC管理的。当GC准备回收一个DirectBuffer对象时，如果发现它还有PhantomReference引用，
那就会在回收它之前，把相应的PhantomReference引用加入到与之关联的一个ReferenceQueue队列中去。
这样我们就可以通过判断一个DirectBuffer对象的PhantomReference引用是否已经加入到一个相关的
ReferenceQueue队列中。如果已经加入了的话，那么就可以在该DirectBuffer对象被回收之前，
释放掉之前为它在Native层分配的内存.

path: dalvik/vm/Jni.cpp
```
bool dvmJniStartup() {
    if (!gDvm.jniGlobalRefTable.init(kGlobalRefsTableInitialSize,
                                 kGlobalRefsTableMaxSize,
                                 kIndirectKindGlobal)) {
        return false;
    }
    if (!gDvm.jniWeakGlobalRefTable.init(kWeakGlobalRefsTableInitialSize,
                                 kGlobalRefsTableMaxSize,
                                 kIndirectKindWeakGlobal)) {
        return false;
    }

    dvmInitMutex(&gDvm.jniGlobalRefLock);
    dvmInitMutex(&gDvm.jniWeakGlobalRefLock);

    if (!dvmInitReferenceTable(&gDvm.jniPinRefTable, kPinTableInitialSize, kPinTableMaxSize)) {
        return false;
    }

    dvmInitMutex(&gDvm.jniPinRefLock);

    return true;
}
```

dvmProfilingStartup
----------------------------------------

16.dvmProfilingStartup这个函数定义在文件dvm/vm/Profile.cpp用来初始化Dalvik虚拟机的性能分析子模块，
以及加载dalvik.system.VMDebug类等。

dvmValidateBoxClasses
----------------------------------------

17.dvmValidateBoxClasses这个函数定义在文件dalvik/vm/reflect/Reflect.cpp中，用来验证Dalvik虚拟机
中存在相应的装箱类，并且这些装箱类有且仅有一个成员变量，这个成员变量是用来描述对应的数字值的。
这些装箱类包括java.lang.Boolean、java.lang.Character、java.lang.Float、java.lang.Double、
java.lang.Byte、java.lang.Short、java.lang.Integer和java.lang.Long。
所谓装箱，就是可以自动将一个数值转换一个对象，例如，将数字1自动转换为一个java.lang.Integer对象。
相应地，也要求能将一个装箱类对象转换成一个数字，例如，将一个值等于1的java.lang.Integer对象转换
为数字1。

dvmPrepMainForJni
----------------------------------------

18.dvmPrepMainForJni这个函数定义在文件dalvik/vm/Thread.cpp中，用来准备主线程的JNI环境，即将
在前面为主线程创建的Thread对象与在前面中创建的JNI环境关联起来。在前面虽然我们已经为当前线程
创建好一个JNI环境了，但是还没有将该JNI环境与主线程关联，也就是还没有将主线程的ID设置到该JNI
环境中去。

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmPrepMainThread.md

registerSystemNatives
----------------------------------------

19.registerSystemNatives这个函数定义在文件dalvik/vm/Init.cpp中，它调用另外一个函数
jniRegisterSystemMethods，后者接着又调用了函数registerCoreLibrariesJni来为Java核心类注册JNI方法。
函数registerCoreLibrariesJni定义在文件libcore/luni/src/main/native/Register.cpp中。

dvmCreateStockExceptions
----------------------------------------

20.dvmCreateStockExceptions这个函数定义在文件dalvik/vm/alloc/Alloc.cpp中，用来预创建一些与
内存分配有关的异常对象，并且将它们缓存起来，以便以后可以快速使用。这些异常对象包括
java.lang.OutOfMemoryError、java.lang.InternalError和java.lang.NoClassDefFoundError。

dvmPrepMainThread
----------------------------------------

21.dvmPrepMainThread这个函数定义在文件dalvik/vm/Thread.cpp中，用来为主线程创建一个
java.lang.ThreadGroup对象、一个java.lang.Thread对角和java.lang.VMThread对象。
这些Java对象和在前面创建的C++层Thread对象关联一起，共同用来描述Dalvik虚拟机的主线程。

dvmReferenceTableEntries
----------------------------------------

22.dvmReferenceTableEntries这个函数定义在文件dalvik/vm/ReferenceTable.h中，用来确保
主线程当前不引用有任何Java对象，这是为了保证主线程接下来以干净的方式来执行程序入口。

dvmDebuggerStartup
----------------------------------------

23.dvmDebuggerStartup这个函数定义在文件dalvik/vm/Debugger.cpp中，用来初始化Dalvik
虚拟机的调试环境。注意，Dalvik虚拟机与Java虚拟机一样，都是通过JDWP协议来支持远程调试的。


这段代码完成Dalvik虚拟机的最后一步初始化工作。它检查Dalvik虚拟机是否指定了-Xzygote启动选项。
如果指定了的话，那么就说明当前是在Zyogte进程中启动Dalvik虚拟机，因此，接下来就会调用函数
initZygote来执行最后一步初始化工作。否则的话，就会调用另外一个函数dvmInitAfterZygote来
执行最后一步初始化工作。由于当前是在Zyogte进程中启动Dalvik虚拟机的，因此，接下来我们就
继续分析函数initZygote的实现。

path: dalvik/vm/Init.cpp
```
/*
 * Do zygote-mode-only initialization.
 */
static bool initZygote()
{
    /* zygote goes into its own process group */
    setpgid(0,0);

    // See storage config details at http://source.android.com/tech/storage/
    // Create private mount namespace shared by all children
    if (unshare(CLONE_NEWNS) == -1) {
        SLOGE("Failed to unshare(): %s", strerror(errno));
        return -1;
    }

    // Mark rootfs as being a slave so that changes from default
    // namespace only flow into our children.
    if (mount("rootfs", "/", NULL, (MS_SLAVE | MS_REC), NULL) == -1) {
        SLOGE("Failed to mount() rootfs as MS_SLAVE: %s", strerror(errno));
        return -1;
    }

    // Create a staging tmpfs that is shared by our children; they will
    // bind mount storage into their respective private namespaces, which
    // are isolated from each other.
    const char* target_base = getenv("EMULATED_STORAGE_TARGET");
    if (target_base != NULL) {
        if (mount("tmpfs", target_base, "tmpfs", MS_NOSUID | MS_NODEV,
                "uid=0,gid=1028,mode=0751") == -1) {
            SLOGE("Failed to mount tmpfs to %s: %s", target_base, strerror(errno));
            return -1;
        }
    }

    // Mark /system as NOSUID | NODEV
    const char* android_root = getenv("ANDROID_ROOT");

    if (android_root == NULL) {
        SLOGE("environment variable ANDROID_ROOT does not exist?!?!");
        return -1;
    }

    std::string mountDev(getMountsDevDir(android_root));
    if (mountDev.empty()) {
        SLOGE("Unable to find mount point for %s", android_root);
        return -1;
    }

    if (mount(mountDev.c_str(), android_root, "none",
            MS_REMOUNT | MS_NOSUID | MS_NODEV | MS_RDONLY | MS_BIND, NULL) == -1) {
        SLOGE("Remount of %s failed: %s", android_root, strerror(errno));
        return -1;
    }

#ifdef HAVE_ANDROID_OS
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        // Older kernels don't understand PR_SET_NO_NEW_PRIVS and return
        // EINVAL. Don't die on such kernels.
        if (errno != EINVAL) {
            SLOGE("PR_SET_NO_NEW_PRIVS failed: %s", strerror(errno));
            return -1;
        }
    }
#endif

    return true;
}
```

这一步执行完成之后，Dalvik虚拟机的创建和初始化工作就完成了.