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

    ...

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

1.setCommandLineDefaults
----------------------------------------

调用函数setCommandLineDefaults来给Dalvik虚拟机设置默认参数，因为启动选项不一定会指定
Dalvik虚拟机的所有属性。

```
    ALOGV("VM init args (%d):", argc);
    for (int i = 0; i < argc; i++) {
        ALOGV("  %d: '%s'", i, argv[i]);
    }
    setCommandLineDefaults();
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/setCommandLineDefaults.md

2.processOptions
----------------------------------------

调用函数processOptions来处理参数argv和argc所描述的启动选项了，也就是根据这些选项值来
设置Dalvik虚拟机的属性，例如，设置Dalvik虚拟机的Java对象堆的最大值。

```
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
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/processOptions.md

3.Configure group scheduling capabilities
----------------------------------------

```
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
```

4.blockSignals
----------------------------------------

如果我们没有在Dalvik虚拟机的启动选项中指定-Xrs，那么gDvm.reduceSignals的值就会被设置为
false，表示要在当前线程中屏蔽掉SIGQUIT信号。在这种情况下，会有一个线程专门用来处理SIGQUIT
信号。这个线程在接收到SIGQUIT信号的时候，就会将各个线程的调用堆栈打印出来，因此，这个线程
又称为dump-stack-trace线程。屏蔽当前线程的SIGQUIT信号是通过调用函数blockSignals来实现的

```
    /* configure signal handling */
    if (!gDvm.reduceSignals)
        blockSignals();
```

5.dvmCheckAsmConstants
----------------------------------------

```
    /* verify system page size */
    if (sysconf(_SC_PAGESIZE) != SYSTEM_PAGE_SIZE) {
        return StringPrintf("expected page size %d, got %d",
                SYSTEM_PAGE_SIZE, (int) sysconf(_SC_PAGESIZE));
    }

    /* mterp setup */
    ALOGV("Using executionMode %d", gDvm.executionMode);
    dvmCheckAsmConstants();
```

6.dvmQuasiAtomicsStartup
----------------------------------------

```
    /*
     * Initialize components.
     */
    dvmQuasiAtomicsStartup();
```

7.dvmAllocTrackerStartup
----------------------------------------

函数dvmAllocTrackerStartup用来初始化Davlik虚拟机的对象分配记录子模块，这样我们就可以通过
DDMS工具来查看Davlik虚拟机的对象分配情况。

```
    if (!dvmAllocTrackerStartup()) {
        return "dvmAllocTrackerStartup failed";
    }
```

8.dvmGcStartUp
----------------------------------------

函数dvmGcStartup用来初始化Davlik虚拟机的垃圾收集(GC)子模块。

```
    if (!dvmGcStartup()) {
        return "dvmGcStartup failed";
    }
```

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

9.dvmThreadStartup
----------------------------------------

函数dvmThreadStartup用来初始化Davlik虚拟机的线程列表、为主线程创建一个Thread对象以及为主线程
初始化执行环境。Davlik虚拟机中的所有线程均是本地操作系统线程。在Linux系统中，一般都是使用
pthread库来创建和管理线程的，Android系统也不例外，也就是说，Davlik虚拟机中的每一个线程均是
一个pthread线程。注意，Davlik虚拟机中的每一个线程均用一个Thread结构体来描述，这些Thread结构体
组织在一个列表中，因此，这里要先对它进行初始化。

```
    if (!dvmThreadStartup()) {
        return "dvmThreadStartup failed";
    }
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmThreadStartup.md

10.dvmInlineNativeStartup
----------------------------------------

函数dvmInlineNativeStartup用来初始化Davlik虚拟机的内建Native函数表。这些内建Native函数主要是
针对java.Lang.String、java.Lang.Math、java.Lang.Float和java.Lang.Double类的，用来替换这些类的
某些成员函数原来的实现（包括Java实现和Native实现）。例如，当我们调用java.Lang.String类的成员函数
compareTo来比较两个字符串的大小时，实际执行的是由Davlik虚拟机提供的内建函数javaLangString_compareTo
(同样是定义在文件dalvik/vm/InlineNative.cpp中)。在提供有__memcmp16函数的系统中，函数
javaLangString_compareTo会利用它来直接比较两个字符串的大小。由于函数__memcmp16是用优化过的汇编语言
的来实现的，它的效率会更高。

```
    if (!dvmInlineNativeStartup()) {
        return "dvmInlineNativeStartup";
    }
```

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

11.dvmRegisterMapStartup
----------------------------------------

函数dvmRegisterMapStartup用来初始化寄存器映射集(Register Map)子模块。Davlik虚拟机支持
精确垃圾收集(Exact GC或者Precise GC)，也就是说，在进行垃圾收集的时候，Davlik虚拟机可以准确
地判断当前正在使用的每一个寄存器里面保存的是对象使用还是非对象引用。对于对象引用，意味被引用的
对象现在还不可以回收，因此，就可以进行精确的垃圾收集。为了帮助垃圾收集器准备地判断寄存器保存的
是对象引用还是非对象引用，Davlik虚拟机在验证了一个类之后，还会为它的每一个成员函数生成一个寄存器
映射集。寄存器映射集记录了类成员函数在每一个GC安全点(Safe Point)中的寄存器使用情况，也就是记录
每一个寄存器里面保存的是对象引用还是非对象引用。由于垃圾收集器一定是在GC安全点进行垃圾收集的，
因此，根据每一个GC安全点的寄存器映射集，就可以准确地知道对象的引用情况，从而可以确定哪些可以回收，
哪些对象还不可以回收。

```
    if (!dvmRegisterMapStartup()) {
        return "dvmRegisterMapStartup failed";
    }
```

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

12.dvmInstanceofStartup
----------------------------------------

函数dvmInstanceofStartup用来初始化instanceof操作符子模块。在使用instanceof操作符来判断一个
对象A是否是一个类B的实例时，Davlik虚拟机需要检查类B是否是从对象A的声明类继承下来的。由于这个
检查的过程比较耗时，Davlik虚拟机在内部使用一个缓冲，用来记录第一次两个类之间的instanceof操作结果，
这样后面再碰到相同的instanceof操作时，就可以快速地得到结果。

```
    if (!dvmInstanceofStartup()) {
        return "dvmInstanceofStartup failed";
    }
```

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

13.dvmClassStartup
----------------------------------------

函数dvmClassStartup用来初始化启动类加载器(Bootstrap Class Loader)，同时还会初始化
java.lang.Class类。启动类加载器是用来加载Java核心类的，用来保证安全性，即保证加载的
Java核心类是合法的。

```
    if (!dvmClassStartup()) {
        return "dvmClassStartup failed";
    }
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmClassStartup.md

14.dvmFindRequiredClassesAndMembers
----------------------------------------

函数dvmFindRequiredClassesAndMembers用来初始化一些必需的类.

```
    /*
     * At this point, the system is guaranteed to be sufficiently
     * initialized that we can look up classes and class members. This
     * call populates the gDvm instance with all the class and member
     * references that the VM wants to use directly.
     */
    if (!dvmFindRequiredClassesAndMembers()) {
        return "dvmFindRequiredClassesAndMembers failed";
    }
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmFindRequiredClassesAndMembers.md

15.dvmStringInternStartup
----------------------------------------

函数dvmStringInternStartup用来初始化java.lang.String类内部私有一个字符串池，这样当Dalvik
虚拟机运行起来之后，我们就可以调用java.lang.String类的成员函数intern来访问这个字符串池里面的字符串。

```
    if (!dvmStringInternStartup()) {
        return "dvmStringInternStartup failed";
    }
```

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

16.dvmNativeStartup
----------------------------------------

函数dvmNativeStartup用来初始化Native Shared Object库加载表，也就是SO库加载表。
这个加载表是用来描述当前进程有哪些SO文件已经被加载过了。

```
    if (!dvmNativeStartup()) {
        return "dvmNativeStartup failed";
    }
```

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

17.dvmInternalNativeStartup
----------------------------------------

函数dvmInternalNativeStartup用来初始化一个内部Native函数表。所有需要直接访问Dalvik虚拟机
内部函数或者数据结构的Native函数都定义在这张表中，因为它们如果定义在外部的其它SO文件中，
就无法直接访问Dalvik虚拟机的内部函数或者数据结构。例如，前面提到的java.lang.String类的
成员函数intent，由于它要访问Dalvik虚拟机内部的一个私有字符串池，因此，它所对应的Native函数
就要在Dalvik虚拟机内部实现。

```
    if (!dvmInternalNativeStartup()) {
        return "dvmInternalNativeStartup failed";
    }
```

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

18.dvmJniStartup
----------------------------------------

```
    if (!dvmJniStartup()) {
        return "dvmJniStartup failed";
    }
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmJniStartup.md

19.dvmProfilingStartup
----------------------------------------

dvmProfilingStartup这个函数定义在文件dvm/vm/Profile.cpp用来初始化Dalvik虚拟机的性能分析子模块，
以及加载dalvik.system.VMDebug类等。

```
    if (!dvmProfilingStartup()) {
        return "dvmProfilingStartup failed";
    }
```

20.dvmCreateInlineSubsTable
----------------------------------------

```
    /*
     * Create a table of methods for which we will substitute an "inline"
     * version for performance.
     */
    if (!dvmCreateInlineSubsTable()) {
        return "dvmCreateInlineSubsTable failed";
    }
```

21.dvmValidateBoxClasses
----------------------------------------

dvmValidateBoxClasses这个函数定义在文件dalvik/vm/reflect/Reflect.cpp中，用来验证Dalvik虚拟机
中存在相应的装箱类，并且这些装箱类有且仅有一个成员变量，这个成员变量是用来描述对应的数字值的。
这些装箱类包括java.lang.Boolean、java.lang.Character、java.lang.Float、java.lang.Double、
java.lang.Byte、java.lang.Short、java.lang.Integer和java.lang.Long。所谓装箱，就是可以自动将
一个数值转换一个对象，例如，将数字1自动转换为一个java.lang.Integer对象。相应地，也要求能将一个
装箱类对象转换成一个数字，例如，将一个值等于1的java.lang.Integer对象转换为数字1。

```
    /*
     * Miscellaneous class library validation.
     */
    if (!dvmValidateBoxClasses()) {
        return "dvmValidateBoxClasses failed";
    }
```

22.dvmPrepMainForJni
----------------------------------------

dvmPrepMainForJni这个函数定义在文件dalvik/vm/Thread.cpp中，用来准备主线程的JNI环境，即将
在前面为主线程创建的Thread对象与在前面中创建的JNI环境关联起来。在前面虽然我们已经为当前线程
创建好一个JNI环境了，但是还没有将该JNI环境与主线程关联，也就是还没有将主线程的ID设置到该JNI
环境中去。

```
    /*
     * Do the last bits of Thread struct initialization we need to allow
     * JNI calls to work.
     */
    if (!dvmPrepMainForJni(pEnv)) {
        return "dvmPrepMainForJni failed";
    }
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmPrepMainThread.md

23.dvmInitClass
----------------------------------------

```
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
```

24.registerSystemNatives
----------------------------------------

registerSystemNatives这个函数定义在文件dalvik/vm/Init.cpp中，它调用另外一个函数
jniRegisterSystemMethods，后者接着又调用了函数registerCoreLibrariesJni来为Java核心类注册JNI方法。
函数registerCoreLibrariesJni定义在文件libcore/luni/src/main/native/Register.cpp中。

```
    /*
     * Register the system native methods, which are registered through JNI.
     */
    if (!registerSystemNatives(pEnv)) {
        return "couldn't register system natives";
    }
```

25.dvmCreateStockExceptions
----------------------------------------

dvmCreateStockExceptions这个函数定义在文件dalvik/vm/alloc/Alloc.cpp中，用来预创建一些与
内存分配有关的异常对象，并且将它们缓存起来，以便以后可以快速使用。这些异常对象包括
java.lang.OutOfMemoryError、java.lang.InternalError和java.lang.NoClassDefFoundError。

```
    /*
     * Do some "late" initialization for the memory allocator.  This may
     * allocate storage and initialize classes.
     */
    if (!dvmCreateStockExceptions()) {
        return "dvmCreateStockExceptions failed";
    }
```

26.dvmPrepMainThread
----------------------------------------

dvmPrepMainThread这个函数定义在文件dalvik/vm/Thread.cpp中，用来为主线程创建一个
java.lang.ThreadGroup对象、一个java.lang.Thread对角和java.lang.VMThread对象。
这些Java对象和在前面创建的C++层Thread对象关联一起，共同用来描述Dalvik虚拟机的主线程。

```
    /*
     * At this point, the VM is in a pretty good state.  Finish prep on
     * the main thread (specifically, create a java.lang.Thread object to go
     * along with our Thread struct).  Note we will probably be executing
     * some interpreted class initializer code in here.
     */
    if (!dvmPrepMainThread()) {
        return "dvmPrepMainThread failed";
    }
```

27.dvmReferenceTableEntries
----------------------------------------

dvmReferenceTableEntries这个函数定义在文件dalvik/vm/ReferenceTable.h中，用来确保
主线程当前不引用有任何Java对象，这是为了保证主线程接下来以干净的方式来执行程序入口。

```
    /*
     * Make sure we haven't accumulated any tracked references.  The main
     * thread should be starting with a clean slate.
     */
    if (dvmReferenceTableEntries(&dvmThreadSelf()->internalLocalRefTable) != 0)
    {
        ALOGW("Warning: tracked references remain post-initialization");
        dvmDumpReferenceTable(&dvmThreadSelf()->internalLocalRefTable, "MAIN");
    }
```

28.dvmDebuggerStartup
----------------------------------------

dvmDebuggerStartup这个函数定义在文件dalvik/vm/Debugger.cpp中，用来初始化Dalvik
虚拟机的调试环境。注意，Dalvik虚拟机与Java虚拟机一样，都是通过JDWP协议来支持远程调试的。

```
    /* general debugging setup */
    if (!dvmDebuggerStartup()) {
        return "dvmDebuggerStartup failed";
    }
```

29.dvmGcStartupClasses
----------------------------------------

```
    if (!dvmGcStartupClasses()) {
        return "dvmGcStartupClasses failed";
    }
```

30.Finishing
----------------------------------------

```
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
```

### initZygote

这段代码完成Dalvik虚拟机的最后一步初始化工作。它检查Dalvik虚拟机是否指定了-Xzygote启动选项。
如果指定了的话，那么就说明当前是在Zyogte进程中启动Dalvik虚拟机，因此，接下来就会调用函数
initZygote来执行最后一步初始化工作。否则的话，就会调用另外一个函数dvmInitAfterZygote来
执行最后一步初始化工作。由于当前是在Zyogte进程中启动Dalvik虚拟机的，因此，接下来我们就
继续分析函数initZygote的实现。

https://github.com/leeminghao/about-android/blob/master/dalvik/start/initZygote.md

这一步执行完成之后，Dalvik虚拟机的创建和初始化工作就完成了.