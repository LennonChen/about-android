AndroidRuntime.startVM
========================================

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

    ...

    result = 0;

bail:
    free(stackTraceFile);
    return result;
}
```

1.设置Dalvik虚拟机的启动选项.
----------------------------------------

```
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
```

2.JNI_CreateJavaVM
----------------------------------------

设置好Dalvik虚拟机的启动选项之后，AndroidRuntime的成员函数startVm就会调用另外一个函数
JNI_CreateJavaVM来创建以及初始化一个Dalvik虚拟机实例。

```
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
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/JNI_CreateJavaVM.md