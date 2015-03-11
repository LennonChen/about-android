path: dalvik/vm/Init.cpp
```
/*
 * Set defaults for fields altered or modified by arguments.
 *
 * Globals are initialized to 0 (a/k/a NULL or false).
 */
static void setCommandLineDefaults()
{
    const char* envStr = getenv("CLASSPATH");
    if (envStr != NULL) {
        gDvm.classPathStr = strdup(envStr);
    } else {
        gDvm.classPathStr = strdup(".");
    }
    envStr = getenv("BOOTCLASSPATH");
    if (envStr != NULL) {
        gDvm.bootClassPathStr = strdup(envStr);
    } else {
        gDvm.bootClassPathStr = strdup(".");
    }

    gDvm.properties = new std::vector<std::string>();

    /* Defaults overridden by -Xms and -Xmx.
     * TODO: base these on a system or application-specific default
     */
    gDvm.heapStartingSize = 2 * 1024 * 1024;  // Spec says 16MB; too big for us.
    gDvm.heapMaximumSize = 16 * 1024 * 1024;  // Spec says 75% physical mem
    gDvm.heapGrowthLimit = 0;  // 0 means no growth limit
    gDvm.lowMemoryMode = false;
    gDvm.stackSize = kDefaultStackSize;
    gDvm.mainThreadStackSize = kDefaultStackSize;
    // When the heap is less than the maximum or growth limited size,
    // fix the free portion of the heap. The utilization is the ratio
    // of live to free memory, 0.5 implies half the heap is available
    // to allocate into before a GC occurs. Min free and max free
    // force the free memory to never be smaller than min free or
    // larger than max free.
    gDvm.heapTargetUtilization = 0.5;
    gDvm.heapMaxFree = 2 * 1024 * 1024;
    gDvm.heapMinFree = gDvm.heapMaxFree / 4;

    gDvm.concurrentMarkSweep = true;

    /* gDvm.jdwpSuspend = true; */

    /* allowed unless zygote config doesn't allow it */
    gDvm.jdwpAllowed = true;

    /* default verification and optimization modes */
    gDvm.classVerifyMode = VERIFY_MODE_ALL;
    gDvm.dexOptMode = OPTIMIZE_MODE_VERIFIED;
    gDvm.monitorVerification = false;
    gDvm.generateRegisterMaps = true;
    gDvm.registerMapMode = kRegisterMapModeTypePrecise;

    /*
     * Default execution mode.
     *
     * This should probably interact with the mterp code somehow, e.g. if
     * we know we're using the "desktop" build we should probably be
     * using "portable" rather than "fast".
     */
#if defined(WITH_JIT)
    gDvm.executionMode = kExecutionModeJit;
    gDvmJit.num_entries_pcTable = 0;
    gDvmJit.includeSelectedMethod = false;
    gDvmJit.includeSelectedOffset = false;
    gDvmJit.methodTable = NULL;
    gDvmJit.classTable = NULL;
    gDvmJit.codeCacheSize = DEFAULT_CODE_CACHE_SIZE;

    gDvm.constInit = false;
    gDvm.commonInit = false;
#else
    gDvm.executionMode = kExecutionModeInterpFast;
#endif

    /*
     * SMP support is a compile-time define, but we may want to have
     * dexopt target a differently-configured device.
     */
    gDvm.dexOptForSmp = (ANDROID_SMP != 0);

    /*
     * Default profiler configuration.
     */
    gDvm.profilerClockSource = kProfilerClockSourceDual;
}
```