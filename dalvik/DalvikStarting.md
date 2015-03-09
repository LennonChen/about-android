Dalvik启动过程
============================================================

  在Android系统中，应用程序进程都是由Zygote进程孵化出来的，而Zygote进程是由Init进程启动的。
Zygote进程在启动时会创建一个Dalvik虚拟机实例，每当它孵化一个新的应用程序进程时，
都会将这个Dalvik虚拟机实例复制到新的应用程序进程里面去，从而使得每一个应用程序进程都有一个
独立的Dalvik虚拟机实例。
  Zygote进程在启动的过程中，除了会创建一个Dalvik虚拟机实例之外，还会将Java运行时库加载到进程中来，
以及注册一些Android核心类的JNI方法来前面创建的Dalvik虚拟机实例中去。注意，一个应用程序进程被Zygote
进程孵化出来的时候，不仅会获得Zygote进程中的Dalvik虚拟机实例拷贝，还会与Zygote一起共享Java运行时库，
这完全得益于Linux内核的进程创建机制（fork）。这种Zygote孵化机制的优点是不仅可以快速地启动一个应用程序进程，
还可以节省整体的内存消耗，缺点是会影响开机速度，毕竟Zygote是在开机过程中启动的。不过，总体来说，是利大于弊的，
毕竟整个系统只有一个Zygote进程，而可能有无数个应用程序进程，而且我们不会经常去关闭手机，大多数情况下只是让它进入休眠状态。

代码分析
------------------------------------------------------------

### zygote

path: frameworks/base/cmds/app_process/app_main.cpp
```
int main(int argc, char* const argv[])
{
    ...
    // These are global variables in ProcessState.cpp
    mArgC = argc;
    mArgV = argv;

    mArgLen = 0;
    for (int i=0; i<argc; i++) {
        mArgLen += strlen(argv[i]) + 1;
    }
    mArgLen--;

    AppRuntime runtime;
    const char* argv0 = argv[0];

    // Process command line arguments
    // ignore argv[0]
    argc--;
    argv++;

    // Everything up to '--' or first non '-' arg goes to the vm

    int i = runtime.addVmArguments(argc, argv);

    // Parse runtime arguments.  Stop at first unrecognized option.
    bool zygote = false;
    bool startSystemServer = false;
    bool application = false;
    const char* parentDir = NULL;
    const char* niceName = NULL;
    const char* className = NULL;
    while (i < argc) {
        const char* arg = argv[i++];
        if (!parentDir) {
            parentDir = arg;
        } else if (strcmp(arg, "--zygote") == 0) {
            zygote = true;
            niceName = "zygote";
        } else if (strcmp(arg, "--start-system-server") == 0) {
            startSystemServer = true;
        } else if (strcmp(arg, "--application") == 0) {
            application = true;
        } else if (strncmp(arg, "--nice-name=", 12) == 0) {
            niceName = arg + 12;
        } else {
            className = arg;
            break;
        }
    }

    if (niceName && *niceName) {
        setArgv0(argv0, niceName);
        set_process_name(niceName);
    }

    runtime.mParentDir = parentDir;

    if (zygote) {
        runtime.start("com.android.internal.os.ZygoteInit",
                startSystemServer ? "start-system-server" : "");
    } else if (className) {
        // Remainder of args get passed to startup class main()
        runtime.mClassName = className;
        runtime.mArgC = argc - i;
        runtime.mArgV = argv + i;
        runtime.start("com.android.internal.os.RuntimeInit",
                application ? "application" : "tool");
    } else {
        fprintf(stderr, "Error: no class name or --zygote supplied.\n");
        app_usage();
        LOG_ALWAYS_FATAL("app_process: no class name or --zygote supplied.");
        return 10;
    }
}
```

### 1.AndroidRuntime::start

path: frameworks/base/core/jni/AndroidRuntime.cpp
```
/*
 * Start the Android runtime.  This involves starting the virtual machine
 * and calling the "static void main(String[] args)" method in the class
 * named by "className".
 *
 * Passes the main function two arguments, the class name and the specified
 * options string.
 */
void AndroidRuntime::start(const char* className, const char* options)
{
    ALOGD("\n>>>>>> AndroidRuntime START %s <<<<<<\n",
            className != NULL ? className : "(unknown)");

    /*
     * 'startSystemServer == true' means runtime is obsolete and not run from
     * init.rc anymore, so we print out the boot start event here.
     */
    if (strcmp(options, "start-system-server") == 0) {
        /* track our progress through the boot sequence */
        const int LOG_BOOT_PROGRESS_START = 3000;
        LOG_EVENT_LONG(LOG_BOOT_PROGRESS_START,
                       ns2ms(systemTime(SYSTEM_TIME_MONOTONIC)));
    }

    const char* rootDir = getenv("ANDROID_ROOT");
    if (rootDir == NULL) {
        rootDir = "/system";
        if (!hasDir("/system")) {
            LOG_FATAL("No root directory specified, and /android does not exist.");
            return;
        }
        setenv("ANDROID_ROOT", rootDir, 1);
    }

    //const char* kernelHack = getenv("LD_ASSUME_KERNEL");
    //ALOGD("Found LD_ASSUME_KERNEL='%s'\n", kernelHack);

    /* start the virtual machine */
    JniInvocation jni_invocation;
    jni_invocation.Init(NULL);
    JNIEnv* env;
    /* JNI JavaVM pointer */
    /* static JavaVM* mJavaVM; */
    /* 1.调用成员函数startVm来创建一个Dalvik虚拟机实例，并且保存在成员变量mJavaVM中。*/
    if (startVm(&mJavaVM, &env) != 0) {
        return;
    }
    onVmCreated(env);

    /*
     * Register android functions.
     */
    /* 2.调用成员函数startReg来注册一些Android核心类的JNI方法。*/
    if (startReg(env) < 0) {
        ALOGE("Unable to register all android natives\n");
        return;
    }

    /*
     * We want to call main() with a String array with arguments in it.
     * At present we have two arguments, the class name and an option string.
     * Create an array to hold them.
     */
    jclass stringClass;
    jobjectArray strArray;
    jstring classNameStr;
    jstring optionsStr;

    /* 3.调用参数className所描述的一个Java类的静态成员函数main，来作为Zygote进程的Java层入口。
     * 这个入口类就为com.android.internal.os.ZygoteInit。执行这一步的时候，Zygote进程中的Dalvik
     * 虚拟机实例就开始正式运作了。注意，在这一步中，也就是在com.android.internal.os.ZygoteInit
     * 类的静态成员函数main，会进行大量的Android核心类和系统资源文件预加载。其中，预加载的Android
     * 核心类可以参考frameworks/base/preloaded-classes这个文件，而预加载的系统资源就是包含在
     * /system/framework/framework-res.apk中。
     */
    stringClass = env->FindClass("java/lang/String");
    assert(stringClass != NULL);
    strArray = env->NewObjectArray(2, stringClass, NULL);
    assert(strArray != NULL);
    classNameStr = env->NewStringUTF(className);
    assert(classNameStr != NULL);
    env->SetObjectArrayElement(strArray, 0, classNameStr);
    optionsStr = env->NewStringUTF(options);
    env->SetObjectArrayElement(strArray, 1, optionsStr);

    /*
     * Start VM.  This thread becomes the main thread of the VM, and will
     * not return until the VM exits.
     */
    char* slashClassName = toSlashClassName(className);
    jclass startClass = env->FindClass(slashClassName);
    if (startClass == NULL) {
        ALOGE("JavaVM unable to locate class '%s'\n", slashClassName);
        /* keep going */
    } else {
        jmethodID startMeth = env->GetStaticMethodID(startClass, "main",
            "([Ljava/lang/String;)V");
        if (startMeth == NULL) {
            ALOGE("JavaVM unable to find main() in '%s'\n", className);
            /* keep going */
        } else {
            env->CallStaticVoidMethod(startClass, startMeth, strArray);

#if 0
            if (env->ExceptionCheck())
                threadExitUncaughtException(env);
#endif
        }
    }
    free(slashClassName);

    ALOGD("Shutting down VM\n");
    /* 4.从com.android.internal.os.ZygoteInit类的静态成员函数main返回来的时候，
     * 就说明Zygote进程准备要退出来了。在退出之前，会调用前面创建的Dalvik虚拟机
     * 实例的成员函数DetachCurrentThread和DestroyJavaVM。其中，前者用来将Zygote
     * 进程的主线程脱离前面创建的Dalvik虚拟机实例，后者是用来销毁前面创建的Dalvik虚拟机实例。
     */
    if (mJavaVM->DetachCurrentThread() != JNI_OK)
        ALOGW("Warning: unable to detach main thread\n");
    if (mJavaVM->DestroyJavaVM() != 0)
        ALOGW("Warning: VM did not shut down cleanly\n");
}
```

接下来，我们就主要关注Dalvik虚拟机实例的创建过程，以及Android核心类JNI方法的注册过程，
即AndroidRuntime类的成员函数startVm和startReg的实现。

### 2.AndroidRuntime::startVm

path: frameworks/base/core/jni/AndroidRuntime.cpp
```
/*
 * Start the Dalvik Virtual Machine.
 *
 * Various arguments, most determined by system properties, are passed in.
 * The "mOptions" vector is updated.
 *
 * Returns 0 on success.
 */
int AndroidRuntime::startVm(JavaVM** pJavaVM, JNIEnv** pEnv)
{
    int result = -1;
    /* path: libnativehelper/include/nativehelper/jni.h
     * typedef struct JavaVMInitArgs {
     *     jint        version;    /* use JNI_VERSION_1_2 or later */

     *     jint        nOptions;
     *     JavaVMOption* options;
     *     jboolean    ignoreUnrecognized;
     * } JavaVMInitArgs;
     */
    JavaVMInitArgs initArgs;
    /* path: libnativehelper/include/nativehelper/jni.h
     * typedef struct JavaVMOption {
           const char* optionString;
           void*       extraInfo;
       } JavaVMOption;
     * Vector<JavaVMOption> mOptions;
     */
    JavaVMOption opt;
    char propBuf[PROPERTY_VALUE_MAX];
    char stackTraceFileBuf[PROPERTY_VALUE_MAX];
    char dexoptFlagsBuf[PROPERTY_VALUE_MAX];
    char enableAssertBuf[sizeof("-ea:")-1 + PROPERTY_VALUE_MAX];
    char jniOptsBuf[sizeof("-Xjniopts:")-1 + PROPERTY_VALUE_MAX];
    char heapstartsizeOptsBuf[sizeof("-Xms")-1 + PROPERTY_VALUE_MAX];
    char heapsizeOptsBuf[sizeof("-Xmx")-1 + PROPERTY_VALUE_MAX];
    char heapgrowthlimitOptsBuf[sizeof("-XX:HeapGrowthLimit=")-1 + PROPERTY_VALUE_MAX];
    char heapminfreeOptsBuf[sizeof("-XX:HeapMinFree=")-1 + PROPERTY_VALUE_MAX];
    char heapmaxfreeOptsBuf[sizeof("-XX:HeapMaxFree=")-1 + PROPERTY_VALUE_MAX];
    char heaptargetutilizationOptsBuf[sizeof("-XX:HeapTargetUtilization=")-1 + PROPERTY_VALUE_MAX];
    char jitcodecachesizeOptsBuf[sizeof("-Xjitcodecachesize:")-1 + PROPERTY_VALUE_MAX];
    char extraOptsBuf[PROPERTY_VALUE_MAX];
    char* stackTraceFile = NULL;
    bool checkJni = false;
    bool checkDexSum = false;
    bool logStdio = false;
    enum {
      kEMDefault,
      kEMIntPortable,
      kEMIntFast,
      kEMJitCompiler,
    } executionMode = kEMDefault;

    /* 1. -Xcheck:jni: 用来启动JNI方法检查。我们在C/C++代码中，可以修改Java对象的成员变量或者调用Java对象的成员函数。
     * 加了-Xcheck:jni选项之后，就可以对要访问的Java对象的成员变量或者成员函数进行合法性检查，例如，检查类型是否匹配。
     * 我们可以通过dalvik.vm.checkjni或者ro.kernel.android.checkjni这两个系统属性来指定是否要启用-Xcheck:jni选项。
     * 注意，加了-Xcheck:jni选项之后，会使用得JNI方法执行变慢。
     */
    property_get("dalvik.vm.checkjni", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
        checkJni = true;
    } else if (strcmp(propBuf, "false") != 0) {
        /* property is neither true nor false; fall back on kernel parameter */
        property_get("ro.kernel.android.checkjni", propBuf, "");
        if (propBuf[0] == '1') {
            checkJni = true;
        }
    }

    /* 2.-Xint:portable，-Xint:fast，-Xint:jit：用来指定Dalvik虚拟机的执行模式。
     * Dalvik虚拟机支持三种运行模式，分别是Portable、Fast和Jit。
     * Portable是指Dalvik虚拟机以可移植的方式来进行编译，也就是说，编译出来的虚拟机可以在任意平台上运行。
     * Fast是针对当前平台对Dalvik虚拟机进行编译，这样编译出来的Dalvik虚拟机可以进行特殊的优化，从而使得它能更快地运行程序。
     * Jit不是解释执行代码，而是将代码动态编译成本地语言后再执行。
     * 我们可以通过dalvik.vm.execution-mode系统属笥来指定Dalvik虚拟机的解释模式。
     */
    property_get("dalvik.vm.execution-mode", propBuf, "");
    if (strcmp(propBuf, "int:portable") == 0) {
        executionMode = kEMIntPortable;
    } else if (strcmp(propBuf, "int:fast") == 0) {
        executionMode = kEMIntFast;
    } else if (strcmp(propBuf, "int:jit") == 0) {
        executionMode = kEMJitCompiler;
    }

    /* 3. -Xstacktracefile：用来指定调用堆栈输出文件。
     * Dalvik虚拟机接收到SIGQUIT（Ctrl-\或者kill -3）信号之后，会将所有线程的调用堆栈输出来，
     * 默认是输出到日志里面。指定了-Xstacktracefile选项之后，就可以将线程的调用堆栈输出到
     * 指定的文件中去。我们可以通过dalvik.vm.stack-trace-file系统属性来指定调用堆栈输出文件。
     */
    property_get("dalvik.vm.stack-trace-file", stackTraceFileBuf, "");

    property_get("dalvik.vm.check-dex-sum", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
        checkDexSum = true;
    }

    property_get("log.redirect-stdio", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
        logStdio = true;
    }

    strcpy(enableAssertBuf, "-ea:");
    property_get("dalvik.vm.enableassertions", enableAssertBuf+4, "");

    strcpy(jniOptsBuf, "-Xjniopts:");
    property_get("dalvik.vm.jniopts", jniOptsBuf+10, "");

    /* route exit() to our handler */
    opt.extraInfo = (void*) runtime_exit;
    opt.optionString = "exit";
    mOptions.add(opt);

    /* route fprintf() to our handler */
    opt.extraInfo = (void*) runtime_vfprintf;
    opt.optionString = "vfprintf";
    mOptions.add(opt);

    /* register the framework-specific "is sensitive thread" hook */
    opt.extraInfo = (void*) runtime_isSensitiveThread;
    opt.optionString = "sensitiveThread";
    mOptions.add(opt);

    opt.extraInfo = NULL;

    /* enable verbose; standard options are { jni, gc, class } */
    //options[curOpt++].optionString = "-verbose:jni";
    opt.optionString = "-verbose:gc";
    mOptions.add(opt);
    //options[curOpt++].optionString = "-verbose:class";

    /*
     * The default starting and maximum size of the heap.  Larger
     * values should be specified in a product property override.
     */
    strcpy(heapstartsizeOptsBuf, "-Xms");
    property_get("dalvik.vm.heapstartsize", heapstartsizeOptsBuf+4, "4m");
    opt.optionString = heapstartsizeOptsBuf;
    mOptions.add(opt);
    /* 4.-Xmx：用来指定Java对象堆的最大值。Dalvik虚拟机的Java对象堆的默认最大值是16M，
     * 不过我们可以通过dalvik.vm.heapsize系统属性来指定为其它值。
     */
    strcpy(heapsizeOptsBuf, "-Xmx");
    property_get("dalvik.vm.heapsize", heapsizeOptsBuf+4, "16m");
    opt.optionString = heapsizeOptsBuf;
    mOptions.add(opt);

    // Increase the main thread's interpreter stack size for bug 6315322.
    opt.optionString = "-XX:mainThreadStackSize=24K";
    mOptions.add(opt);

    // Set the max jit code cache size.  Note: size of 0 will disable the JIT.
    strcpy(jitcodecachesizeOptsBuf, "-Xjitcodecachesize:");
    property_get("dalvik.vm.jit.codecachesize", jitcodecachesizeOptsBuf+19,  NULL);
    if (jitcodecachesizeOptsBuf[19] != '\0') {
      opt.optionString = jitcodecachesizeOptsBuf;
      mOptions.add(opt);
    }

    strcpy(heapgrowthlimitOptsBuf, "-XX:HeapGrowthLimit=");
    property_get("dalvik.vm.heapgrowthlimit", heapgrowthlimitOptsBuf+20, "");
    if (heapgrowthlimitOptsBuf[20] != '\0') {
        opt.optionString = heapgrowthlimitOptsBuf;
        mOptions.add(opt);
    }

    strcpy(heapminfreeOptsBuf, "-XX:HeapMinFree=");
    property_get("dalvik.vm.heapminfree", heapminfreeOptsBuf+16, "");
    if (heapminfreeOptsBuf[16] != '\0') {
        opt.optionString = heapminfreeOptsBuf;
        mOptions.add(opt);
    }

    strcpy(heapmaxfreeOptsBuf, "-XX:HeapMaxFree=");
    property_get("dalvik.vm.heapmaxfree", heapmaxfreeOptsBuf+16, "");
    if (heapmaxfreeOptsBuf[16] != '\0') {
        opt.optionString = heapmaxfreeOptsBuf;
        mOptions.add(opt);
    }

    strcpy(heaptargetutilizationOptsBuf, "-XX:HeapTargetUtilization=");
    property_get("dalvik.vm.heaptargetutilization", heaptargetutilizationOptsBuf+26, "");
    if (heaptargetutilizationOptsBuf[26] != '\0') {
        opt.optionString = heaptargetutilizationOptsBuf;
        mOptions.add(opt);
    }

    property_get("ro.config.low_ram", propBuf, "");
    if (strcmp(propBuf, "true") == 0) {
      opt.optionString = "-XX:LowMemoryMode";
      mOptions.add(opt);
    }

    /*
     * Enable or disable dexopt features, such as bytecode verification and
     * calculation of register maps for precise GC.
     */
    property_get("dalvik.vm.dexopt-flags", dexoptFlagsBuf, "");
    if (dexoptFlagsBuf[0] != '\0') {
        const char* opc;
        const char* val;

        opc = strstr(dexoptFlagsBuf, "v=");     /* verification */
        if (opc != NULL) {
            switch (*(opc+2)) {
            case 'n':   val = "-Xverify:none";      break;
            case 'r':   val = "-Xverify:remote";    break;
            case 'a':   val = "-Xverify:all";       break;
            default:    val = NULL;                 break;
            }

            if (val != NULL) {
                opt.optionString = val;
                mOptions.add(opt);
            }
        }

        opc = strstr(dexoptFlagsBuf, "o=");     /* optimization */
        if (opc != NULL) {
            switch (*(opc+2)) {
            case 'n':   val = "-Xdexopt:none";      break;
            case 'v':   val = "-Xdexopt:verified";  break;
            case 'a':   val = "-Xdexopt:all";       break;
            case 'f':   val = "-Xdexopt:full";      break;
            default:    val = NULL;                 break;
            }

            if (val != NULL) {
                opt.optionString = val;
                mOptions.add(opt);
            }
        }

        opc = strstr(dexoptFlagsBuf, "m=y");    /* register map */
        if (opc != NULL) {
            opt.optionString = "-Xgenregmap";
            mOptions.add(opt);

            /* turn on precise GC while we're at it */
            opt.optionString = "-Xgc:precise";
            mOptions.add(opt);
        }
    }

    /* enable debugging; set suspend=y to pause during VM init */
    /* use android ADB transport */
    opt.optionString =
        "-agentlib:jdwp=transport=dt_android_adb,suspend=n,server=y";
    mOptions.add(opt);

    ALOGD("CheckJNI is %s\n", checkJni ? "ON" : "OFF");
    if (checkJni) {
        /* extended JNI checking */
        opt.optionString = "-Xcheck:jni";
        mOptions.add(opt);

        /* set a cap on JNI global references */
        opt.optionString = "-Xjnigreflimit:2000";
        mOptions.add(opt);

        /* with -Xcheck:jni, this provides a JNI function call trace */
        //opt.optionString = "-verbose:jni";
        //mOptions.add(opt);
    }

    char lockProfThresholdBuf[sizeof("-Xlockprofthreshold:") + sizeof(propBuf)];
    property_get("dalvik.vm.lockprof.threshold", propBuf, "");
    if (strlen(propBuf) > 0) {
      strcpy(lockProfThresholdBuf, "-Xlockprofthreshold:");
      strcat(lockProfThresholdBuf, propBuf);
      opt.optionString = lockProfThresholdBuf;
      mOptions.add(opt);
    }

    /* Force interpreter-only mode for selected opcodes. Eg "1-0a,3c,f1-ff" */
    char jitOpBuf[sizeof("-Xjitop:") + PROPERTY_VALUE_MAX];
    property_get("dalvik.vm.jit.op", propBuf, "");
    if (strlen(propBuf) > 0) {
        strcpy(jitOpBuf, "-Xjitop:");
        strcat(jitOpBuf, propBuf);
        opt.optionString = jitOpBuf;
        mOptions.add(opt);
    }

    /* Force interpreter-only mode for selected methods */
    char jitMethodBuf[sizeof("-Xjitmethod:") + PROPERTY_VALUE_MAX];
    property_get("dalvik.vm.jit.method", propBuf, "");
    if (strlen(propBuf) > 0) {
        strcpy(jitMethodBuf, "-Xjitmethod:");
        strcat(jitMethodBuf, propBuf);
        opt.optionString = jitMethodBuf;
        mOptions.add(opt);
    }

    if (executionMode == kEMIntPortable) {
        opt.optionString = "-Xint:portable";
        mOptions.add(opt);
    } else if (executionMode == kEMIntFast) {
        opt.optionString = "-Xint:fast";
        mOptions.add(opt);
    } else if (executionMode == kEMJitCompiler) {
        opt.optionString = "-Xint:jit";
        mOptions.add(opt);
    }

    if (checkDexSum) {
        /* perform additional DEX checksum tests */
        opt.optionString = "-Xcheckdexsum";
        mOptions.add(opt);
    }

    if (logStdio) {
        /* convert stdout/stderr to log messages */
        opt.optionString = "-Xlog-stdio";
        mOptions.add(opt);
    }

    if (enableAssertBuf[4] != '\0') {
        /* accept "all" to mean "all classes and packages" */
        if (strcmp(enableAssertBuf+4, "all") == 0)
            enableAssertBuf[3] = '\0';
        ALOGI("Assertions enabled: '%s'\n", enableAssertBuf);
        opt.optionString = enableAssertBuf;
        mOptions.add(opt);
    } else {
        ALOGV("Assertions disabled\n");
    }

    if (jniOptsBuf[10] != '\0') {
        ALOGI("JNI options: '%s'\n", jniOptsBuf);
        opt.optionString = jniOptsBuf;
        mOptions.add(opt);
    }

    if (stackTraceFileBuf[0] != '\0') {
        static const char* stfOptName = "-Xstacktracefile:";

        stackTraceFile = (char*) malloc(strlen(stfOptName) +
            strlen(stackTraceFileBuf) +1);
        strcpy(stackTraceFile, stfOptName);
        strcat(stackTraceFile, stackTraceFileBuf);
        opt.optionString = stackTraceFile;
        mOptions.add(opt);
    }

    /* extra options; parse this late so it overrides others */
    property_get("dalvik.vm.extra-opts", extraOptsBuf, "");
    parseExtraOpts(extraOptsBuf);

    /* Set the properties for locale */
    {
        char langOption[sizeof("-Duser.language=") + 3];
        char regionOption[sizeof("-Duser.region=") + 3];
        strcpy(langOption, "-Duser.language=");
        strcpy(regionOption, "-Duser.region=");
        readLocale(langOption, regionOption);
        opt.extraInfo = NULL;
        opt.optionString = langOption;
        mOptions.add(opt);
        opt.optionString = regionOption;
        mOptions.add(opt);
    }

    /*
     * We don't have /tmp on the device, but we often have an SD card.  Apps
     * shouldn't use this, but some test suites might want to exercise it.
     */
    opt.optionString = "-Djava.io.tmpdir=/sdcard";
    mOptions.add(opt);

    initArgs.version = JNI_VERSION_1_4;
    initArgs.options = mOptions.editArray();
    initArgs.nOptions = mOptions.size();
    initArgs.ignoreUnrecognized = JNI_FALSE;

    /*
     * Initialize the VM.
     *
     * The JavaVM* is essentially per-process, and the JNIEnv* is per-thread.
     * If this call succeeds, the VM is ready, and we can start issuing
     * JNI calls.
     */
     /* 设置好Dalvik虚拟机的启动选项之后，AndroidRuntime的成员函数startVm就会调用另外一个函数
      * JNI_CreateJavaVM来创建以及初始化一个Dalvik虚拟机实例。
      */
    if (JNI_CreateJavaVM(pJavaVM, pEnv, &initArgs) < 0) {
        ALOGE("JNI_CreateJavaVM failed\n");
        goto bail;
    }

    result = 0;

bail:
    free(stackTraceFile);
    return result;
}
```

### 3.JNI_CreateJavaVM

path: dalvik/vm/Jni.cpp
```
/*
 * Create a new VM instance.
 *
 * The current thread becomes the main VM thread.  We return immediately,
 * which effectively means the caller is executing in a native method.
 */
jint JNI_CreateJavaVM(JavaVM** p_vm, JNIEnv** p_env, void* vm_args) {
    const JavaVMInitArgs* args = (JavaVMInitArgs*) vm_args;
    if (dvmIsBadJniVersion(args->version)) {
        ALOGE("Bad JNI version passed to CreateJavaVM: %d", args->version);
        return JNI_EVERSION;
    }

    // TODO: don't allow creation of multiple VMs -- one per customer for now

    /* zero globals; not strictly necessary the first time a VM is started */
    /* struct DvmGlobals gDvm; --dalvik/vm/Globals.h */
    memset(&gDvm, 0, sizeof(gDvm));

    /*
     * Set up structures for JNIEnv and VM.
     */
    /* 1.为当前进程创建一个Dalvik虚拟机实例，即一个JavaVMExt对象。*/
    JavaVMExt* pVM = (JavaVMExt*) calloc(1, sizeof(JavaVMExt));
    pVM->funcTable = &gInvokeInterface;
    pVM->envList = NULL;
    dvmInitMutex(&pVM->envListLock);

    UniquePtr<const char*[]> argv(new const char*[args->nOptions]);
    memset(argv.get(), 0, sizeof(char*) * (args->nOptions));

    /*
     * Convert JNI args to argv.
     *
     * We have to pull out vfprintf/exit/abort, because they use the
     * "extraInfo" field to pass function pointer "hooks" in.  We also
     * look for the -Xcheck:jni stuff here.
     */
    /* 2.将参数vm_args所描述的Dalvik虚拟机启动选项拷贝到变量argv所描述的一个字符串数组中去 */
    int argc = 0;
    for (int i = 0; i < args->nOptions; i++) {
        const char* optStr = args->options[i].optionString;
        if (optStr == NULL) {
            dvmFprintf(stderr, "ERROR: CreateJavaVM failed: argument %d was NULL\n", i);
            return JNI_ERR;
        } else if (strcmp(optStr, "vfprintf") == 0) {
            gDvm.vfprintfHook = (int (*)(FILE *, const char*, va_list))args->options[i].extraInfo;
        } else if (strcmp(optStr, "exit") == 0) {
            gDvm.exitHook = (void (*)(int)) args->options[i].extraInfo;
        } else if (strcmp(optStr, "abort") == 0) {
            gDvm.abortHook = (void (*)(void))args->options[i].extraInfo;
        } else if (strcmp(optStr, "sensitiveThread") == 0) {
            gDvm.isSensitiveThreadHook = (bool (*)(void))args->options[i].extraInfo;
        } else if (strcmp(optStr, "-Xcheck:jni") == 0) {
            gDvmJni.useCheckJni = true;
        } else if (strncmp(optStr, "-Xjniopts:", 10) == 0) {
            char* jniOpts = strdup(optStr + 10);
            size_t jniOptCount = 1;
            for (char* p = jniOpts; *p != 0; ++p) {
                if (*p == ',') {
                    ++jniOptCount;
                    *p = 0;
                }
            }
            char* jniOpt = jniOpts;
            for (size_t i = 0; i < jniOptCount; ++i) {
                if (strcmp(jniOpt, "warnonly") == 0) {
                    gDvmJni.warnOnly = true;
                } else if (strcmp(jniOpt, "forcecopy") == 0) {
                    gDvmJni.forceCopy = true;
                } else if (strcmp(jniOpt, "logThirdPartyJni") == 0) {
                    gDvmJni.logThirdPartyJni = true;
                } else {
                    dvmFprintf(stderr, "ERROR: CreateJavaVM failed: unknown -Xjniopts option '%s'\n",
                            jniOpt);
                    free(pVM);
                    free(jniOpts);
                    return JNI_ERR;
                }
                jniOpt += strlen(jniOpt) + 1;
            }
            free(jniOpts);
        } else {
            /* regular option */
            argv[argc++] = optStr;
        }
    }

    if (gDvmJni.useCheckJni) {
        dvmUseCheckedJniVm(pVM);
    }

    if (gDvmJni.jniVm != NULL) {
        dvmFprintf(stderr, "ERROR: Dalvik only supports one VM per process\n");
        free(pVM);
        return JNI_ERR;
    }
    gDvmJni.jniVm = (JavaVM*) pVM;

    /*
     * Create a JNIEnv for the main thread.  We need to have something set up
     * here because some of the class initialization we do when starting
     * up the VM will call into native code.
     */
    /* 3.为当前进程创建和初始化一个JNI环境，即一个JNIEnvExt对象，这是通过调用函数
     * dvmCreateJNIEnv来完成的。
     */
    JNIEnvExt* pEnv = (JNIEnvExt*) dvmCreateJNIEnv(NULL);

    /* Initialize VM. */
    /* 4.并且调用函数dvmStartup来初始化前面所创建的Dalvik虚拟机实例。*/
    gDvm.initializing = true;
    std::string status =
            dvmStartup(argc, argv.get(), args->ignoreUnrecognized, (JNIEnv*)pEnv);
    gDvm.initializing = false;

    if (!status.empty()) {
        free(pEnv);
        free(pVM);
        ALOGW("CreateJavaVM failed: %s", status.c_str());
        return JNI_ERR;
    }

    /*
     * Success!  Return stuff to caller.
     */
    /* 5.调用函数dvmChangeStatus将当前线程的状态设置为正在执行NATIVE代码，
     * 并且将面所创建和初始化好的JavaVMExt对象和JNIEnvExt对象通过输出参数p_vm和p_env返回给调用者。
     */
    dvmChangeStatus(NULL, THREAD_NATIVE);
    *p_env = (JNIEnv*) pEnv;
    *p_vm = (JavaVM*) pVM;
    ALOGV("CreateJavaVM succeeded");
    return JNI_OK;
}
```

gDvm是一个类型为DvmGlobals的全局变量，用来收集当前进程所有虚拟机相关的信息,
gDvmJni是一个类型为DvmJniGlobals的全局变量，用来收集当前进程Jni环境的信息:

https://github.com/leeminghao/about-android/blob/master/dalvik/Globals.h

每一个Dalvik虚拟机实例都有一个函数表，保存在对应的JavaVMExt对象的成员变量funcTable中,
JavaVMExt的类型定义如下所示:

path: dalvik/vm/JniInternal.h
```
struct JavaVMExt {
    const struct JNIInvokeInterface* funcTable;     /* must be first */

    const struct JNIInvokeInterface* baseFuncTable;

    /* head of list of JNIEnvs associated with this VM */
    JNIEnvExt*      envList;
    pthread_mutex_t envListLock;
};
```

##### funTable

这个函数表又被指定为gInvokeInterface。gInvokeInterface是一个类型为JNIInvokeInterface
的结构体，类型定义如下所示:

path: libnativehelper/include/nativehelper/jni.h
```
/*
 * JNI invocation interface.
 */
struct JNIInvokeInterface {
    void*       reserved0;
    void*       reserved1;
    void*       reserved2;

    jint        (*DestroyJavaVM)(JavaVM*);
    jint        (*AttachCurrentThread)(JavaVM*, JNIEnv**, void*);
    jint        (*DetachCurrentThread)(JavaVM*);
    jint        (*GetEnv)(JavaVM*, void**, jint);
    jint        (*AttachCurrentThreadAsDaemon)(JavaVM*, JNIEnv**, void*);
};
```

而gInvokeInterface的定义如下所示：

path: dalvik/vm/Jni.cpp
```
static const struct JNIInvokeInterface gInvokeInterface = {
    NULL,
    NULL,
    NULL,

    DestroyJavaVM,
    AttachCurrentThread,
    DetachCurrentThread,

    GetEnv,

    AttachCurrentThreadAsDaemon,
};
```

有了这个Dalvik虚拟机函数表之后，我们就可以将当前线程Attach或者Detach到Dalvik虚拟机中去，
或者销毁当前进程的Dalvik虚拟机等。

##### envList

每一个Dalvik虚拟机实例还有一个JNI环境列表，保存在对应的JavaVMExt对象的成员变量envList中。
注意，JavaVMExt对象的成员变量envList描述的是一个JNIEnvExt列表，其中，每一个Attach到Dalvik
虚拟机中去的线程都有一个对应的JNIEnvExt，用来描述它的JNI环境。有了这个JNI环境之后，我们才可以
在Java函数和C/C++函数之间互相调用。

path: dalvik/vm/JniInternal.h
```
struct JNIEnvExt {
    const struct JNINativeInterface* funcTable;     /* must be first */

    const struct JNINativeInterface* baseFuncTable;

    u4      envThreadId;
    Thread* self;

    /* if nonzero, we are in a "critical" JNI call */
    int     critical;

    struct JNIEnvExt* prev;
    struct JNIEnvExt* next;
};
```

每一个JNIEnvExt对象都有两个成员变量prev和next，它们均是一个JNIEnvExt指针，分别指向前一个
JNIEnvExt对象和后一个JNIEnvExt对象，也就是说，每一个Dalvik虚拟机实例的成员变量envList
描述的是一个双向JNIEnvExt列表，其中，列表中的第一个JNIEnvExt对象描述的是主线程的JNI环境。

### 4.dvmCreateJNIEnv

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
     * 以及所使用的本地接口表，即设置这个JNIEnvExt对象的成员变量funcTable和vm。这里的宿主Dalvik
     * 虚拟机即为当前进程的Dalvik虚拟机，它保存在全局变量gDvm的成员变量vmList中。本地接口表由
     * 全局变量gNativeInterface来描述。
     */
    JNIEnvExt* newEnv = (JNIEnvExt*) calloc(1, sizeof(JNIEnvExt));
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
