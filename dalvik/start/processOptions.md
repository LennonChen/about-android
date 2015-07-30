processOptions
========================================

path: dalvik/vm/Init.cpp
```
/*
 * Process an argument vector full of options.  Unlike standard C programs,
 * argv[0] does not contain the name of the program.
 *
 * If "ignoreUnrecognized" is set, we ignore options starting with "-X" or "_"
 * that we don't recognize.  Otherwise, we return with an error as soon as
 * we see anything we can't identify.
 *
 * Returns 0 on success, -1 on failure, and 1 for the special case of
 * "-version" where we want to stop without showing an error message.
 */
static int processOptions(int argc, const char* const argv[],
    bool ignoreUnrecognized)
{
    int i;

    ALOGV("VM options (%d):", argc);
    for (i = 0; i < argc; i++)
        ALOGV("  %d: '%s'", i, argv[i]);

    /*
     * Over-allocate AssertionControl array for convenience.  If allocated,
     * the array must be able to hold at least one entry, so that the
     * zygote-time activation can do its business.
     */
    assert(gDvm.assertionCtrl == NULL);
    if (argc > 0) {
        gDvm.assertionCtrl =
            (AssertionControl*) malloc(sizeof(AssertionControl) * argc);
        if (gDvm.assertionCtrl == NULL)
            return -1;
        assert(gDvm.assertionCtrlCount == 0);
    }

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-help") == 0) {
            /* show usage and stop */
            return -1;

        } else if (strcmp(argv[i], "-version") == 0) {
            /* show version and stop */
            showVersion();
            return 1;
        } else if (strcmp(argv[i], "-showversion") == 0) {
            /* show version and continue */
            showVersion();

        } else if (strcmp(argv[i], "-classpath") == 0 ||
                   strcmp(argv[i], "-cp") == 0)
        {
            /* set classpath */
            if (i == argc-1) {
                dvmFprintf(stderr, "Missing classpath path list\n");
                return -1;
            }
            free(gDvm.classPathStr); /* in case we have compiled-in default */
            gDvm.classPathStr = strdup(argv[++i]);

        } else if (strncmp(argv[i], "-Xbootclasspath:",
                sizeof("-Xbootclasspath:")-1) == 0)
        {
            /* set bootclasspath */
            const char* path = argv[i] + sizeof("-Xbootclasspath:")-1;

            if (*path == '\0') {
                dvmFprintf(stderr, "Missing bootclasspath path list\n");
                return -1;
            }
            free(gDvm.bootClassPathStr);
            gDvm.bootClassPathStr = strdup(path);

        } else if (strncmp(argv[i], "-Xbootclasspath/a:",
                sizeof("-Xbootclasspath/a:")-1) == 0) {
            const char* appPath = argv[i] + sizeof("-Xbootclasspath/a:")-1;

            if (*(appPath) == '\0') {
                dvmFprintf(stderr, "Missing appending bootclasspath path list\n");
                return -1;
            }
            char* allPath;

            if (asprintf(&allPath, "%s:%s", gDvm.bootClassPathStr, appPath) < 0) {
                dvmFprintf(stderr, "Can't append to bootclasspath path list\n");
                return -1;
            }
            free(gDvm.bootClassPathStr);
            gDvm.bootClassPathStr = allPath;

        } else if (strncmp(argv[i], "-Xbootclasspath/p:",
                sizeof("-Xbootclasspath/p:")-1) == 0) {
            const char* prePath = argv[i] + sizeof("-Xbootclasspath/p:")-1;

            if (*(prePath) == '\0') {
                dvmFprintf(stderr, "Missing prepending bootclasspath path list\n");
                return -1;
            }
            char* allPath;

            if (asprintf(&allPath, "%s:%s", prePath, gDvm.bootClassPathStr) < 0) {
                dvmFprintf(stderr, "Can't prepend to bootclasspath path list\n");
                return -1;
            }
            free(gDvm.bootClassPathStr);
            gDvm.bootClassPathStr = allPath;

        } else if (strncmp(argv[i], "-D", 2) == 0) {
            /* Properties are handled in managed code. We just check syntax. */
            if (strchr(argv[i], '=') == NULL) {
                dvmFprintf(stderr, "Bad system property setting: \"%s\"\n",
                    argv[i]);
                return -1;
            }
            gDvm.properties->push_back(argv[i] + 2);

        } else if (strcmp(argv[i], "-jar") == 0) {
            // TODO: handle this; name of jar should be in argv[i+1]
            dvmFprintf(stderr, "-jar not yet handled\n");
            assert(false);

        } else if (strncmp(argv[i], "-Xms", 4) == 0) {
            size_t val = parseMemOption(argv[i]+4, 1024);
            if (val != 0) {
                if (val >= kMinHeapStartSize && val <= kMaxHeapSize) {
                    gDvm.heapStartingSize = val;
                } else {
                    dvmFprintf(stderr,
                        "Invalid -Xms '%s', range is %dKB to %dKB\n",
                        argv[i], kMinHeapStartSize/1024, kMaxHeapSize/1024);
                    return -1;
                }
            } else {
                dvmFprintf(stderr, "Invalid -Xms option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-Xmx", 4) == 0) {
            size_t val = parseMemOption(argv[i]+4, 1024);
            if (val != 0) {
                if (val >= kMinHeapSize && val <= kMaxHeapSize) {
                    gDvm.heapMaximumSize = val;
                } else {
                    dvmFprintf(stderr,
                        "Invalid -Xmx '%s', range is %dKB to %dKB\n",
                        argv[i], kMinHeapSize/1024, kMaxHeapSize/1024);
                    return -1;
                }
            } else {
                dvmFprintf(stderr, "Invalid -Xmx option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-XX:HeapGrowthLimit=", 20) == 0) {
            size_t val = parseMemOption(argv[i] + 20, 1024);
            if (val != 0) {
                gDvm.heapGrowthLimit = val;
            } else {
                dvmFprintf(stderr, "Invalid -XX:HeapGrowthLimit option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-XX:HeapMinFree=", 16) == 0) {
            size_t val = parseMemOption(argv[i] + 16, 1024);
            if (val != 0) {
                gDvm.heapMinFree = val;
            } else {
                dvmFprintf(stderr, "Invalid -XX:HeapMinFree option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-XX:HeapMaxFree=", 16) == 0) {
            size_t val = parseMemOption(argv[i] + 16, 1024);
            if (val != 0) {
                gDvm.heapMaxFree = val;
            } else {
                dvmFprintf(stderr, "Invalid -XX:HeapMaxFree option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strcmp(argv[i], "-XX:LowMemoryMode") == 0) {
          gDvm.lowMemoryMode = true;
        } else if (strncmp(argv[i], "-XX:HeapTargetUtilization=", 26) == 0) {
            const char* start = argv[i] + 26;
            const char* end = start;
            double val = strtod(start, const_cast<char**>(&end));
            // Ensure that we have a value, there was no cruft after it and it
            // satisfies a sensible range.
            bool sane_val = (start != end) && (end[0] == '\0') &&
                (val >= 0.1) && (val <= 0.9);
            if (sane_val) {
                gDvm.heapTargetUtilization = val;
            } else {
                dvmFprintf(stderr, "Invalid -XX:HeapTargetUtilization option '%s'\n", argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-Xss", 4) == 0) {
            size_t val = parseMemOption(argv[i]+4, 1);
            if (val != 0) {
                if (val >= kMinStackSize && val <= kMaxStackSize) {
                    gDvm.stackSize = val;
                    if (val > gDvm.mainThreadStackSize) {
                        gDvm.mainThreadStackSize = val;
                    }
                } else {
                    dvmFprintf(stderr, "Invalid -Xss '%s', range is %d to %d\n",
                        argv[i], kMinStackSize, kMaxStackSize);
                    return -1;
                }
            } else {
                dvmFprintf(stderr, "Invalid -Xss option '%s'\n", argv[i]);
                return -1;
            }

        } else if (strncmp(argv[i], "-XX:mainThreadStackSize=", strlen("-XX:mainThreadStackSize=")) == 0) {
            size_t val = parseMemOption(argv[i] + strlen("-XX:mainThreadStackSize="), 1);
            if (val != 0) {
                if (val >= kMinStackSize && val <= kMaxStackSize) {
                    gDvm.mainThreadStackSize = val;
                } else {
                    dvmFprintf(stderr, "Invalid -XX:mainThreadStackSize '%s', range is %d to %d\n",
                               argv[i], kMinStackSize, kMaxStackSize);
                    return -1;
                }
            } else {
                dvmFprintf(stderr, "Invalid -XX:mainThreadStackSize option '%s'\n", argv[i]);
                return -1;
            }

        } else if (strncmp(argv[i], "-XX:+DisableExplicitGC", 22) == 0) {
            gDvm.disableExplicitGc = true;
        } else if (strcmp(argv[i], "-verbose") == 0 ||
            strcmp(argv[i], "-verbose:class") == 0)
        {
            // JNI spec says "-verbose:gc,class" is valid, but cmd line
            // doesn't work that way; may want to support.
            gDvm.verboseClass = true;
        } else if (strcmp(argv[i], "-verbose:jni") == 0) {
            gDvm.verboseJni = true;
        } else if (strcmp(argv[i], "-verbose:gc") == 0) {
            gDvm.verboseGc = true;
        } else if (strcmp(argv[i], "-verbose:shutdown") == 0) {
            gDvm.verboseShutdown = true;

        } else if (strncmp(argv[i], "-enableassertions", 17) == 0) {
            enableAssertions(argv[i] + 17, true);
        } else if (strncmp(argv[i], "-ea", 3) == 0) {
            enableAssertions(argv[i] + 3, true);
        } else if (strncmp(argv[i], "-disableassertions", 18) == 0) {
            enableAssertions(argv[i] + 18, false);
        } else if (strncmp(argv[i], "-da", 3) == 0) {
            enableAssertions(argv[i] + 3, false);
        } else if (strcmp(argv[i], "-enablesystemassertions") == 0 ||
                   strcmp(argv[i], "-esa") == 0)
        {
            enableAssertions(NULL, true);
        } else if (strcmp(argv[i], "-disablesystemassertions") == 0 ||
                   strcmp(argv[i], "-dsa") == 0)
        {
            enableAssertions(NULL, false);

        } else if (strncmp(argv[i], "-Xcheck:jni", 11) == 0) {
            /* nothing to do now -- was handled during JNI init */

        } else if (strcmp(argv[i], "-Xdebug") == 0) {
            /* accept but ignore */

        } else if (strncmp(argv[i], "-Xrunjdwp:", 10) == 0 ||
            strncmp(argv[i], "-agentlib:jdwp=", 15) == 0)
        {
            const char* tail;

            if (argv[i][1] == 'X')
                tail = argv[i] + 10;
            else
                tail = argv[i] + 15;

            if (strncmp(tail, "help", 4) == 0 || !parseJdwpOptions(tail)) {
                showJdwpHelp();
                return 1;
            }
        } else if (strcmp(argv[i], "-Xrs") == 0) {
            gDvm.reduceSignals = true;
        } else if (strcmp(argv[i], "-Xnoquithandler") == 0) {
            /* disables SIGQUIT handler thread while still blocking SIGQUIT */
            /* (useful if we don't want thread but system still signals us) */
            gDvm.noQuitHandler = true;
        } else if (strcmp(argv[i], "-Xzygote") == 0) {
            gDvm.zygote = true;
#if defined(WITH_JIT)
            gDvmJit.runningInAndroidFramework = true;
#endif
        } else if (strncmp(argv[i], "-Xdexopt:", 9) == 0) {
            if (strcmp(argv[i] + 9, "none") == 0)
                gDvm.dexOptMode = OPTIMIZE_MODE_NONE;
            else if (strcmp(argv[i] + 9, "verified") == 0)
                gDvm.dexOptMode = OPTIMIZE_MODE_VERIFIED;
            else if (strcmp(argv[i] + 9, "all") == 0)
                gDvm.dexOptMode = OPTIMIZE_MODE_ALL;
            else if (strcmp(argv[i] + 9, "full") == 0)
                gDvm.dexOptMode = OPTIMIZE_MODE_FULL;
            else {
                dvmFprintf(stderr, "Unrecognized dexopt option '%s'\n",argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-Xverify:", 9) == 0) {
            if (strcmp(argv[i] + 9, "none") == 0)
                gDvm.classVerifyMode = VERIFY_MODE_NONE;
            else if (strcmp(argv[i] + 9, "remote") == 0)
                gDvm.classVerifyMode = VERIFY_MODE_REMOTE;
            else if (strcmp(argv[i] + 9, "all") == 0)
                gDvm.classVerifyMode = VERIFY_MODE_ALL;
            else {
                dvmFprintf(stderr, "Unrecognized verify option '%s'\n",argv[i]);
                return -1;
            }
        } else if (strncmp(argv[i], "-Xjnigreflimit:", 15) == 0) {
            // Ignored for backwards compatibility.
        } else if (strncmp(argv[i], "-Xjnitrace:", 11) == 0) {
            gDvm.jniTrace = strdup(argv[i] + 11);
        } else if (strcmp(argv[i], "-Xlog-stdio") == 0) {
            gDvm.logStdio = true;

        } else if (strncmp(argv[i], "-Xint", 5) == 0) {
            if (argv[i][5] == ':') {
                if (strcmp(argv[i] + 6, "portable") == 0)
                    gDvm.executionMode = kExecutionModeInterpPortable;
                else if (strcmp(argv[i] + 6, "fast") == 0)
                    gDvm.executionMode = kExecutionModeInterpFast;
#ifdef WITH_JIT
                else if (strcmp(argv[i] + 6, "jit") == 0)
                    gDvm.executionMode = kExecutionModeJit;
#endif
                else {
                    dvmFprintf(stderr,
                        "Warning: Unrecognized interpreter mode %s\n",argv[i]);
                    /* keep going */
                }
            } else {
                /* disable JIT if it was enabled by default */
                gDvm.executionMode = kExecutionModeInterpFast;
            }

        } else if (strncmp(argv[i], "-Xlockprofthreshold:", 20) == 0) {
            gDvm.lockProfThreshold = atoi(argv[i] + 20);

#ifdef WITH_JIT
        } else if (strncmp(argv[i], "-Xjitop", 7) == 0) {
            processXjitop(argv[i]);
        } else if (strncmp(argv[i], "-Xjitmethod:", 12) == 0) {
            processXjitmethod(argv[i] + strlen("-Xjitmethod:"), true);
        } else if (strncmp(argv[i], "-Xjitclass:", 11) == 0) {
            processXjitmethod(argv[i] + strlen("-Xjitclass:"), false);
        } else if (strncmp(argv[i], "-Xjitoffset:", 12) == 0) {
            processXjitoffset(argv[i] + strlen("-Xjitoffset:"));
        } else if (strncmp(argv[i], "-Xjitconfig:", 12) == 0) {
            processXjitconfig(argv[i] + strlen("-Xjitconfig:"));
        } else if (strncmp(argv[i], "-Xjitblocking", 13) == 0) {
          gDvmJit.blockingMode = true;
        } else if (strncmp(argv[i], "-Xjitthreshold:", 15) == 0) {
          gDvmJit.threshold = atoi(argv[i] + 15);
        } else if (strncmp(argv[i], "-Xjitcodecachesize:", 19) == 0) {
          gDvmJit.codeCacheSize = atoi(argv[i] + 19) * 1024;
          if (gDvmJit.codeCacheSize == 0) {
            gDvm.executionMode = kExecutionModeInterpFast;
          }
        } else if (strncmp(argv[i], "-Xincludeselectedop", 19) == 0) {
          gDvmJit.includeSelectedOp = true;
        } else if (strncmp(argv[i], "-Xincludeselectedmethod", 23) == 0) {
          gDvmJit.includeSelectedMethod = true;
        } else if (strncmp(argv[i], "-Xjitcheckcg", 12) == 0) {
          gDvmJit.checkCallGraph = true;
          /* Need to enable blocking mode due to stack crawling */
          gDvmJit.blockingMode = true;
        } else if (strncmp(argv[i], "-Xjitdumpbin", 12) == 0) {
          gDvmJit.printBinary = true;
        } else if (strncmp(argv[i], "-Xjitverbose", 12) == 0) {
          gDvmJit.printMe = true;
        } else if (strncmp(argv[i], "-Xjitprofile", 12) == 0) {
          gDvmJit.profileMode = kTraceProfilingContinuous;
        } else if (strncmp(argv[i], "-Xjitdisableopt", 15) == 0) {
          /* Disable selected optimizations */
          if (argv[i][15] == ':') {
              sscanf(argv[i] + 16, "%x", &gDvmJit.disableOpt);
          /* Disable all optimizations */
          } else {
              gDvmJit.disableOpt = -1;
          }
        } else if (strncmp(argv[i], "-Xjitsuspendpoll", 16) == 0) {
          gDvmJit.genSuspendPoll = true;
#endif

        } else if (strncmp(argv[i], "-Xstacktracefile:", 17) == 0) {
            gDvm.stackTraceFile = strdup(argv[i]+17);

        } else if (strcmp(argv[i], "-Xgenregmap") == 0) {
            gDvm.generateRegisterMaps = true;
        } else if (strcmp(argv[i], "-Xnogenregmap") == 0) {
            gDvm.generateRegisterMaps = false;

        } else if (strcmp(argv[i], "Xverifyopt:checkmon") == 0) {
            gDvm.monitorVerification = true;
        } else if (strcmp(argv[i], "Xverifyopt:nocheckmon") == 0) {
            gDvm.monitorVerification = false;

        } else if (strncmp(argv[i], "-Xgc:", 5) == 0) {
            if (strcmp(argv[i] + 5, "precise") == 0)
                gDvm.preciseGc = true;
            else if (strcmp(argv[i] + 5, "noprecise") == 0)
                gDvm.preciseGc = false;
            else if (strcmp(argv[i] + 5, "preverify") == 0)
                gDvm.preVerify = true;
            else if (strcmp(argv[i] + 5, "nopreverify") == 0)
                gDvm.preVerify = false;
            else if (strcmp(argv[i] + 5, "postverify") == 0)
                gDvm.postVerify = true;
            else if (strcmp(argv[i] + 5, "nopostverify") == 0)
                gDvm.postVerify = false;
            else if (strcmp(argv[i] + 5, "concurrent") == 0)
                gDvm.concurrentMarkSweep = true;
            else if (strcmp(argv[i] + 5, "noconcurrent") == 0)
                gDvm.concurrentMarkSweep = false;
            else if (strcmp(argv[i] + 5, "verifycardtable") == 0)
                gDvm.verifyCardTable = true;
            else if (strcmp(argv[i] + 5, "noverifycardtable") == 0)
                gDvm.verifyCardTable = false;
            else {
                dvmFprintf(stderr, "Bad value for -Xgc");
                return -1;
            }
            ALOGV("Precise GC configured %s", gDvm.preciseGc ? "ON" : "OFF");

        } else if (strcmp(argv[i], "-Xcheckdexsum") == 0) {
            gDvm.verifyDexChecksum = true;

        } else if (strcmp(argv[i], "-Xprofile:threadcpuclock") == 0) {
            gDvm.profilerClockSource = kProfilerClockSourceThreadCpu;
        } else if (strcmp(argv[i], "-Xprofile:wallclock") == 0) {
            gDvm.profilerClockSource = kProfilerClockSourceWall;
        } else if (strcmp(argv[i], "-Xprofile:dualclock") == 0) {
            gDvm.profilerClockSource = kProfilerClockSourceDual;

        } else {
            if (!ignoreUnrecognized) {
                dvmFprintf(stderr, "Unrecognized option '%s'\n", argv[i]);
                return -1;
            }
        }
    }

    return 0;
}
```

-Xbootclasspath:
----------------------------------------

"-Zbootclasspath:"选项用于设置gDvm.bootClassPathStr,具体操作如下所示：

```
        } else if (strncmp(argv[i], "-Xbootclasspath:",
                sizeof("-Xbootclasspath:")-1) == 0)
        {
            /* set bootclasspath */
            const char* path = argv[i] + sizeof("-Xbootclasspath:")-1;

            if (*path == '\0') {
                dvmFprintf(stderr, "Missing bootclasspath path list\n");
                return -1;
            }
            free(gDvm.bootClassPathStr);
            gDvm.bootClassPathStr = strdup(path);
        ...
```

-Xbootclasspath/a:
----------------------------------------

"-Xbootclasspath/a:"选项用于将app的路径添加到gDvm.bootClassPathStr中去，
具体实现如下所示：

```
        } else if (strncmp(argv[i], "-Xbootclasspath/a:",
                sizeof("-Xbootclasspath/a:")-1) == 0) {
            const char* appPath = argv[i] + sizeof("-Xbootclasspath/a:")-1;

            if (*(appPath) == '\0') {
                dvmFprintf(stderr, "Missing appending bootclasspath path list\n");
                return -1;
            }
            char* allPath;

            if (asprintf(&allPath, "%s:%s", gDvm.bootClassPathStr, appPath) < 0) {
                dvmFprintf(stderr, "Can't append to bootclasspath path list\n");
                return -1;
            }
            free(gDvm.bootClassPathStr);
            gDvm.bootClassPathStr = allPath;
        ...
```

-Xbootclasspath/p:
----------------------------------------

"-Xbootclasspath/p:"选项用于将预编译好的包添加到gDvm.bootClassPathStr中去,
具体实现如下所示：

```
        } else if (strncmp(argv[i], "-Xbootclasspath/p:",
                sizeof("-Xbootclasspath/p:")-1) == 0) {
            const char* prePath = argv[i] + sizeof("-Xbootclasspath/p:")-1;

            if (*(prePath) == '\0') {
                dvmFprintf(stderr, "Missing prepending bootclasspath path list\n");
                return -1;
            }
            char* allPath;

            if (asprintf(&allPath, "%s:%s", prePath, gDvm.bootClassPathStr) < 0) {
                dvmFprintf(stderr, "Can't prepend to bootclasspath path list\n");
                return -1;
            }
            free(gDvm.bootClassPathStr);
            gDvm.bootClassPathStr = allPath;
        ...
```