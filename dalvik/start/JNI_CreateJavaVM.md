JNI_CreateJavaVM
========================================

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

    ...

    ALOGV("CreateJavaVM succeeded");
    return JNI_OK;
}
```

JNI_CreateJavaVM主要完成如下工作:

1.为当前进程创建一个Dalvik虚拟机实例
----------------------------------------

为当前进程创建一个Dalvik虚拟机实例，即一个JavaVMExt对象.

```
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
```

### JavaVMExt

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

#### JNIInvokeInterface

JNIInvokeInterface的定义如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/JNIInvokeInterface.md

2.处理Dalvik虚拟机的启动选项.
----------------------------------------

```
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
```

3.为当前线程创建和初始化一个JNI环境
----------------------------------------

即一个JNIEnvExt对象,这是通过调用函数dvmCreateJNIEnv来完成的

```
    /*
     * Create a JNIEnv for the main thread.  We need to have something set up
     * here because some of the class initialization we do when starting
     * up the VM will call into native code.
     */
    /* 3.为当前线程创建和初始化一个JNI环境，即一个JNIEnvExt对象，这是通过调用函数
     * dvmCreateJNIEnv来完成的。
     */
    JNIEnvExt* pEnv = (JNIEnvExt*) dvmCreateJNIEnv(NULL);
```

### JNIEnvExt

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

#### JNINativeInterface

JNINativeInterface的定义如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/JNINativeInterface.md

4.dvmStartup
----------------------------------------

调用函数dvmStartup来初始化前面所创建的Dalvik虚拟机实例.

```
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
```

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmStartup.md

5.dvmChangeStatus
----------------------------------------

调用函数dvmChangeStatus将当前线程的状态设置为正在执行NATIVE代码.

```
    /*
     * Success!  Return stuff to caller.
     */
    /* 5.调用函数dvmChangeStatus将当前线程的状态设置为正在执行NATIVE代码，
     * 并且将面所创建和初始化好的JavaVMExt对象和JNIEnvExt对象通过输出参数p_vm和p_env返回给调用者。
     */
    dvmChangeStatus(NULL, THREAD_NATIVE);
```

6.返回参数
----------------------------------------

将面所创建和初始化好的JavaVMExt对象和JNIEnvExt对象通过输出参数p_vm和p_env返回给调用者.

```
    *p_env = (JNIEnv*) pEnv;
    *p_vm = (JavaVM*) pVM;
```
