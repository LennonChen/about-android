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
    /* 1.调用函数setCommandLineDefaults来给Dalvik虚拟机设置默认参数，因为启动选项不一定会指定
     * Dalvik虚拟机的所有属性。
     */
    setCommandLineDefaults();

    /*
     * Process the option flags (if any).
     */
    /* 2.调用函数dvmProcessOptions来处理参数argv和argc所描述的启动选项了，也就是根据这些选项值来
     * 设置Dalvik虚拟机的属性，例如，设置Dalvik虚拟机的Java对象堆的最大值。
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
    /* 3.如果我们没有在Dalvik虚拟机的启动选项中指定-Xrs，那么gDvm.reduceSignals的值就会被设置为
     * false，表示要在当前线程中屏蔽掉SIGQUIT信号。在这种情况下，会有一个线程专门用来处理SIGQUIT
     * 信号。这个线程在接收到SIGQUIT信号的时候，就会将各个线程的调用堆栈打印出来，因此，这个线程
     * 又称为dump-stack-trace线程。屏蔽当前线程的SIGQUIT信号是通过调用函数blockSignals来实现的
     */
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
    /* 3.这个函数定义在文件dalvik/vm/AllocTracker.c中，用来初始化Davlik虚拟机的对象分配记录子模块，
     * 这样我们就可以通过DDMS工具来查看Davlik虚拟机的对象分配情况。
     */
    if (!dvmAllocTrackerStartup()) {
        return "dvmAllocTrackerStartup failed";
    }
    /* 4.这个函数定义在文件dalvik/vm/alloc/Alloc.c中，用来初始化Davlik虚拟机的垃圾收集(GC)子模块。*/
    if (!dvmGcStartup()) {
        return "dvmGcStartup failed";
    }
    /* 5.这个函数定义在文件dalvik/vm/Thread.c中，用来初始化Davlik虚拟机的线程列表、为主线程创建
     * 一个Thread对象以及为主线程初始化执行环境。Davlik虚拟机中的所有线程均是本地操作系统线程。
     * 在Linux系统中，一般都是使用pthread库来创建和管理线程的，Android系统也不例外，也就是说，
     * Davlik虚拟机中的每一个线程均是一个pthread线程。注意，Davlik虚拟机中的每一个线程均用一个
     * Thread结构体来描述，这些Thread结构体组织在一个列表中，因此，这里要先对它进行初始化。
     */
    if (!dvmThreadStartup()) {
        return "dvmThreadStartup failed";
    }
    /* 6.这个函数定义在文件dalvik/vm/InlineNative.c中，用来初始化Davlik虚拟机的内建Native函数表。
     * 这些内建Native函数主要是针对java.Lang.String、java.Lang.Math、java.Lang.Float和
     * java.Lang.Double类的，用来替换这些类的某些成员函数原来的实现（包括Java实现和Native实现）。
     * 例如，当我们调用java.Lang.String类的成员函数compareTo来比较两个字符串的大小时，实际执行的
     * 是由Davlik虚拟机提供的内建函数javaLangString_compareTo（同样是定义在文件
     * dalvik/vm/InlineNative.c中）。在提供有__memcmp16函数的系统中，函数javaLangString_compareTo
     * 会利用它来直接比较两个字符串的大小。由于函数__memcmp16是用优化过的汇编语言的来实现的，
     * 它的效率会更高。
     */
    if (!dvmInlineNativeStartup()) {
        return "dvmInlineNativeStartup";
    }
    /* 7.这个函数定义在文件dalvik/vm/analysis/RegisterMap.c中，用来初始化寄存器映射集(Register Map)
     * 子模块。Davlik虚拟机支持精确垃圾收集（Exact GC或者Precise GC），也就是说，在进行垃圾收集的
     * 时候，Davlik虚拟机可以准确地判断当前正在使用的每一个寄存器里面保存的是对象使用还是非对象
     * 引用。对于对象引用，意味被引用的对象现在还不可以回收，因此，就可以进行精确的垃圾收集。
     * 为了帮助垃圾收集器准备地判断寄存器保存的是对象引用还是非对象引用，Davlik虚拟机在验证了一个
     * 类之后，还会为它的每一个成员函数生成一个寄存器映射集。寄存器映射集记录了类成员函数在每一个
     * GC安全点（Safe Point）中的寄存器使用情况，也就是记录每一个寄存器里面保存的是对象引用还是
     * 非对象引用。由于垃圾收集器一定是在GC安全点进行垃圾收集的，因此，根据每一个GC安全点的寄存器
     * 映射集，就可以准确地知道对象的引用情况，从而可以确定哪些可以回收，哪些对象还不可以回收。
     */
    if (!dvmRegisterMapStartup()) {
        return "dvmRegisterMapStartup failed";
    }
    /* 8.这个函数定义在文件dalvik/vm/oo/TypeCheck.c中，用来初始化instanceof操作符子模块。在使用
     * instanceof操作符来判断一个对象A是否是一个类B的实例时，Davlik虚拟机需要检查类B是否是从对象
     * A的声明类继承下来的。由于这个检查的过程比较耗时，Davlik虚拟机在内部使用一个缓冲，用来记录
     * 第一次两个类之间的instanceof操作结果，这样后面再碰到相同的instanceof操作时，就可以快速地
     * 得到结果。
     */
    if (!dvmInstanceofStartup()) {
        return "dvmInstanceofStartup failed";
    }
    /* 9.这个函数定义在文件dalvik/vm/oo/Class.c中，用来初始化启动类加载器(Bootstrap Class Loader)，
     * 同时还会初始化java.lang.Class类。启动类加载器是用来加载Java核心类的，用来保证安全性，即
     * 保证加载的Java核心类是合法的。
     */
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