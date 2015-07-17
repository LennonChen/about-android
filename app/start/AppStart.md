Android Application Starting
========================================

Android应用程序框架层创建的应用程序进程具有两个特点：

* 一．是进程的入口函数是ActivityThread.main；

* 二．进程支持Binder进程间通信机制；

Android 应用程序启动过程
----------------------------------------

下面我们分析下Android应用程序的启动过程：

在调用ActivityManagerService的成员方法startProcessLocked函数来
启动一个新的进程来运行一个Application的过程中我们会调用Process类的成员方法start来启动一个Process，

其启动过程如下所示：

### ActivityManagerService.java


##### ActivityManagerSerivce.startProcessLocked

path: frameworks/base/services/java/com/android/server/am/ActvityManagerService.java

```
private final void startProcessLocked(ProcessRecord app,
                   String hostingType, String hostingNameStr) {
   ...
   // Start the process.  It will either succeed and return a result containing
   // the PID of the new process, or else throw a RuntimeException.

   /* 这里主要是调用Process.start接口来创建一个新的进程，新的进程会导入
   ** android.app.ActivityThread类，并且执行它的main函数，这就是为什么
   ** 每一个应用程序都有一个ActivityThread实例来对应的原因。*/

   Process.ProcessStartResult startResult =
       Process.start("android.app.ActivityThread",
                     app.processName, uid, uid, gids, debugFlags,
                     app.info.targetSdkVersion, null);
   ...
}
```

下面我们分析Process的start方法，分析其是如何创建一个新的Process来导入
ActivityTrhead类来执行其main方法的，其具体实现如下所示：

### Process.java

path: frameworks/base/core/java/android/os/Process.java

##### Process.start

```
public static final ProcessStartResult start(final String processClass,
                               final String niceName,
                               int uid, int gid, int[] gids,
                               int debugFlags, int targetSdkVersion,
                               String[] zygoteArgs) {

        try {
            /* 调用startViaZygote函数来完成启动工作 */
            return startViaZygote(processClass, niceName, uid, gid, gids,
                    debugFlags, targetSdkVersion, zygoteArgs);
        } catch (ZygoteStartFailedEx ex) {
            throw new RuntimeException(
                    "Starting VM process through Zygote failed", ex);
        }
}
```

##### Process.startViaZygote

```
private static ProcessStartResult startViaZygote(final String processClass,
                                  final String niceName,
                                  final int uid, final int gid,
                                  final int[] gids,
                                  int debugFlags, int targetSdkVersion,
                                  String[] extraArgs)
                                  throws ZygoteStartFailedEx {

        synchronized(Process.class) {

            ArrayList<String> argsForZygote = new ArrayList<String>();

            // --runtime-init, --setuid=, --setgid=,
            // and --setgroups= must go first
            // 设置一个参数列表
            // "--runtime-init"表示要为新创建的进程初始化运行时库

            argsForZygote.add("--runtime-init");
            argsForZygote.add("--setuid=" + uid);
            argsForZygote.add("--setgid=" + gid);
            if ((debugFlags & Zygote.DEBUG_ENABLE_JNI_LOGGING) != 0) {
                argsForZygote.add("--enable-jni-logging");
            }

            if ((debugFlags & Zygote.DEBUG_ENABLE_SAFEMODE) != 0) {
                argsForZygote.add("--enable-safemode");
            }

            if ((debugFlags & Zygote.DEBUG_ENABLE_DEBUGGER) != 0) {
                argsForZygote.add("--enable-debugger");
            }

            if ((debugFlags & Zygote.DEBUG_ENABLE_CHECKJNI) != 0) {
                argsForZygote.add("--enable-checkjni");
            }

            if ((debugFlags & Zygote.DEBUG_ENABLE_ASSERT) != 0) {
                argsForZygote.add("--enable-assert");
            }

            argsForZygote.add("--target-sdk-version=" + targetSdkVersion);

            //TODO optionally enable debuger
            //argsForZygote.add("--enable-debugger");

            // --setgroups is a comma-separated list
            if (gids != null && gids.length > 0) {
                StringBuilder sb = new StringBuilder();
                sb.append("--setgroups=");
                int sz = gids.length;
                for (int i = 0; i < sz; i++) {
                    if (i != 0) {
                        sb.append(',');
                    }
                    sb.append(gids[i]);
                }
                argsForZygote.add(sb.toString());
            }


            if (niceName != null) {
                argsForZygote.add("--nice-name=" + niceName);
            }

            argsForZygote.add(processClass);

            if (extraArgs != null) {
                for (String arg : extraArgs) {
                    argsForZygote.add(arg);
                }
            }

            /* 参数列表设置完成后通过调用zygoteSendArgsAndGetResult函数来实现创建启动工作 */
            return zygoteSendArgsAndGetResult(argsForZygote);
        }
}
```

##### Process.openZygoteSocketIfNeeded

```
private static void openZygoteSocketIfNeeded()
            throws ZygoteStartFailedEx {

        int retryCount;

        if (sPreviousZygoteOpenFailed) {

            /*
             * If we've failed before, expect that we'll fail again and
             * don't pause for retries.
             */
            retryCount = 0;

        } else {

            retryCount = 10;

        }

        /*
         * See bug #811181: Sometimes runtime can make it up before zygote.
         * Really, we'd like to do something better to avoid this condition,
         * but for now just wait a bit...
         */
        for (int retry = 0
                ; (sZygoteSocket == null) && (retry < (retryCount + 1))
                ; retry++ ) {

            if (retry > 0) {
                try {
                    Log.i("Zygote", "Zygote not up yet, sleeping...");
                    Thread.sleep(ZYGOTE_RETRY_MILLIS);
                } catch (InterruptedException ex) {
                    // should never happen
                }
             }

            try {

                /* 创建一个管理本地Socket的类 */
                sZygoteSocket = new LocalSocket();

                /* 链接到名称为zygote的Socket Server上，这个socket Server会在启动Zygote进程时创建 */
                sZygoteSocket.connect(new LocalSocketAddress(ZYGOTE_SOCKET,
                        LocalSocketAddress.Namespace.RESERVED));

                /* 创建连接到zygote socket的读写流 */
                sZygoteInputStream
                        = new DataInputStream(sZygoteSocket.getInputStream());

                sZygoteWriter = new BufferedWriter(new OutputStreamWriter(
                                    sZygoteSocket.getOutputStream()), 256);

                sPreviousZygoteOpenFailed = false;
                break;

            } catch (IOException ex) {
                ...
        }

        if (sZygoteSocket == null) {
            sPreviousZygoteOpenFailed = true;
            throw new ZygoteStartFailedEx("connect failed");
        }
}
```

##### Process.zygoteSendArgsAndGetResult

```
private static ProcessStartResult zygoteSendArgsAndGetResult(ArrayList<String> args)
            throws ZygoteStartFailedEx {

        openZygoteSocketIfNeeded();

        try {
            ...
            /* 向名称为zygote的Socket Server发送上面组织的参数列表 */
            sZygoteWriter.write(Integer.toString(args.size()));
            sZygoteWriter.newLine();

            int sz = args.size();
            for (int i = 0; i < sz; i++) {
                String arg = args.get(i);
                if (arg.indexOf('\n') >= 0) {
                    throw new ZygoteStartFailedEx(
                            "embedded newlines not allowed");
                }

                sZygoteWriter.write(arg);
                sZygoteWriter.newLine();
            }

            /* 刷新流 */
            sZygoteWriter.flush();

            // Should there be a timeout on this?
            /* 读取返回的结果 */
            ProcessStartResult result = new ProcessStartResult();
            result.pid = sZygoteInputStream.readInt();
            if (result.pid < 0) {
                throw new ZygoteStartFailedEx("fork() failed");
            }

            result.usingWrapper = sZygoteInputStream.readBoolean();
            return result;
        } catch (IOException ex) {
            ...
            throw new ZygoteStartFailedEx(ex);
        }
    }
}
```

### ZygoteInit

path: frameworks/base/core/java/com/android/internal/os/ZygoteInit.java

##### ZygoteInit.runSelectLoopMode

我们会在ZygoteInit类的成员函数runSelectLoopMode函数中读取ActivityManagerService
发送过来的启动进程的请求并处理：

```
/**
 * Runs the zygote process's select loop. Accepts new connections as
 * they happen, and reads commands from connections one spawn-request's
 * worth at a time.
 *
 * @throws MethodAndArgsCaller in a child process when a main() should
 * be executed.
 */
private static void runSelectLoopMode() throws MethodAndArgsCaller {
        ArrayList<FileDescriptor> fds = new ArrayList();
        ArrayList<ZygoteConnection> peers = new ArrayList();

        /* 首先创建一个大小为4的Socket文件描述符数组fdArray,表示Zygote进程
        ** 最多能同时处理4个Socket链接
        */

        FileDescriptor[] fdArray = new FileDescriptor[4];

        /* 接着将ZygoteInit类的静态成员变量sServerSocket所描述的一个Socket
        ** 的文件描述符添加到Socket文件描述符列表fds中 */
        fds.add(sServerSocket.getFileDescriptor());
        peers.add(null);

        /* 使用一个无限循环等待ActivityManagerService请求Zygote进程创建新的应用程序进程 */
        int loopCount = GC_LOOP_COUNT;
        while (true) {

            int index;

            /*
             * Call gc() before we block in select().
             * It's work that has to be done anyway, and it's better
             * to avoid making every child do it.  It will also
             * madvise() any free memory as a side-effect.
             *
             * Don't call it every time, because walking the entire
             * heap is a lot of overhead to free a few hundred bytes.
             */

            if (loopCount <= 0) {
                gc();
                loopCount = GC_LOOP_COUNT;
            } else {
                loopCount--;
            }

            try {

                /* 将保存在Socket文件描述符fds中的Socket文件描述符转移到fdArray中 */
                fdArray = fds.toArray(fdArray);

                /* 调用静态成员函数selectReadable来监控保存在这个数组中的
                ** Socket是否有数据可读，一个Socket有数据可读就意味着它
                ** 接收到了一个连接或者一个请求, selectReadable是一个native
                ** 函数其最终是调用linux的系统调用select来选择一个有数据可
                ** 读的连接 */
                index = selectReadable(fdArray);

            } catch (IOException ex) {
                throw new RuntimeException("Error in select()", ex);
            }

            if (index < 0) {
                throw new RuntimeException("Error in select()");

            /* 如果变量index的值等于0, 那么就说明ActivityManagerService
            ** 通过ZygoteInit类的静态成员变量sServerSocket所描述的一个
            ** Socket与Zygote进程建立了新的连接.
            ** 注意：这时候ActivitManagerService只是与Zygote进程建立了一个
            ** 链接，但是还没有请求Zygote进程为它创建一个新的应用程序进程
            ** 因此,接下来的代码只是将这个链接添加到Socket链接列表peers中
            ** 并且将用来描述这个链接的一个Socket文件描述符添加到列表fds中
            ** 以便接下来可以接收到ActivityManagerService发送过来的创建应用
            ** 程序进程请求 */
            } else if (index == 0) {
                ZygoteConnection newPeer = acceptCommandPeer();
                peers.add(newPeer);
                fds.add(newPeer.getFileDesciptor());

            /* 如果index的值大于0，那么就说明ActivityManagerService向Zygote
            ** 进程发送了一个创建应用程序进程的请求，那么，下列代码就会调用
            ** 前面已经建立起来的一个链接的成员函数runOnce来创建一个新的应用
            ** 程序进程，并且分别将这个连接从peers和fds中删除，因为已经处理
            ** 完成这个连接了 */
            } else {
                boolean done;
                done = peers.get(index).runOnce();
                if (done) {
                    peers.remove(index);
                    fds.remove(index);
                }
            }
        }
}
```

##### ZygoteInit.selectReadable

static native int selectReadable(FileDescriptor[] fds) throws IOException;

JNI方法selectReadable的具体实现如下所示, 使用其监听服务端Socket是否有来自于Client的请求：

path:frameworks/base/core/jni/com_android_internal_os_ZygoteInit.cpp

```
static jint com_android_internal_os_ZygoteInit_selectReadable (
        JNIEnv *env, jobject clazz, jobjectArray fds)
{
    if (fds == NULL) {
        jniThrowNullPointerException(env, "fds == null");
        return -1;
    }

    jsize length = env->GetArrayLength(fds);  // 获取fds数组的大小
    fd_set fdset;

    if (env->ExceptionOccurred() != NULL) {
        return -1;
    }

    FD_ZERO(&fdset);
    int nfds = 0;
    for (jsize i = 0; i < length; i++) {
        jobject fdObj = env->GetObjectArrayElement(fds, i);
        if (env->ExceptionOccurred() != NULL) {
            return -1;
        }

        if (fdObj == NULL) {
            continue;
        }

        /* 取出用于监听的文件描述符 */
        int fd = jniGetFDFromFileDescriptor(env, fdObj);
        if  (env->ExceptionOccurred() != NULL) {
            return -1;
        }

        /* 将其添加到文件描述符集合中去 */
        FD_SET(fd, &fdset);
        if (fd >= nfds) {
            nfds = fd + 1;
        }
    }

    int err;
    do {
        err = select (nfds, &fdset, NULL, NULL, NULL);  // 监听是否有fd需要I/O
    } while (err < 0 && errno == EINTR);

    if (err < 0) {
        jniThrowIOException(env, errno);
        return -1;
    }

    for (jsize i = 0; i < length; i++) {
        jobject fdObj = env->GetObjectArrayElement(fds, i);

        if (env->ExceptionOccurred() != NULL) {
            return -1;
        }

        if (fdObj == NULL) {
            continue;
        }

        int fd = jniGetFDFromFileDescriptor(env, fdObj);
        if  (env->ExceptionOccurred() != NULL) {
            return -1;
        }

        /* 返回需要I/O数据的文件描述符的索引 */
        if (FD_ISSET(fd, &fdset)) {
            return (jint)i;
        }
    }
    return -1;
}
```

### ZygoteConnection

path: frameworks/base/core/java/com/android/internal/os/ZygoteConnection.java

##### ZygoteConnection.runOnce

接着我们在ZygoteConnection的成员函数runOnce来处理ActivityManagerService
发送过来的请求创建一个新的应用程序进程，其具体实现如下所示：

```
boolean runOnce() throws ZygoteInit.MethodAndArgsCaller {

        String args[];
        Arguments parsedArgs = null;
        FileDescriptor[] descriptors;

        try {
           // 读取ActivityManagerService发送过来的参数列表
            args = readArgumentList();
            descriptors = mSocket.getAncillaryFileDescriptors();
        } catch (IOException ex) {
            closeSocket();
            return true;
        }

        if (args == null) {
            // EOF reached.
            closeSocket();
            return true;
        }

        /** the stderr of the most recent request, if avail */
        PrintStream newStderr = null;

        if (descriptors != null && descriptors.length >= 3) {
            newStderr = new PrintStream(
                    new FileOutputStream(descriptors[2]));
        }

        int pid = -1;
        FileDescriptor childPipeFd = null;
        FileDescriptor serverPipeFd = null;

        try {
            parsedArgs = new Arguments(args);

            /* 更新对应的属性或者策略信息 */

            applyUidSecurityPolicy(parsedArgs, peer);
            applyRlimitSecurityPolicy(parsedArgs, peer);
            applyCapabilitiesSecurityPolicy(parsedArgs, peer);
            applyInvokeWithSecurityPolicy(parsedArgs, peer);

            applyDebuggerSystemProperty(parsedArgs);
            applyInvokeWithSystemProperty(parsedArgs);

            int[][] rlimits = null;

            if (parsedArgs.rlimits != null) {
                rlimits = parsedArgs.rlimits.toArray(intArray2d);
            }

            if (parsedArgs.runtimeInit && parsedArgs.invokeWith != null) {
                FileDescriptor[] pipeFds = Libcore.os.pipe();  // 创建一对pipe
                childPipeFd = pipeFds[1];
                serverPipeFd = pipeFds[0];
                ZygoteInit.setCloseOnExec(serverPipeFd, true);
            }

            /* 真正创建进程的地方在这里，其功能类似与linux的系统调用
            ** fork, 个函数会创建一个进程，而且有两个返回值，一个是在当前进程中
            ** 返回的，一个是在新创建的进程中返回，即在当前进程的子进程中返回，
            ** 在当前进程中的返回值就是新创建的子进程的pid值，而在子进程中的返回值是0 */
            pid = Zygote.forkAndSpecialize(parsedArgs.uid, parsedArgs.gid,
                    parsedArgs.gids, parsedArgs.debugFlags, rlimits);

        } catch (IOException ex) {
            logAndPrintError(newStderr, "Exception creating pipe", ex);
        } catch (ErrnoException ex) {
            logAndPrintError(newStderr, "Exception creating pipe", ex);
        } catch (IllegalArgumentException ex) {
            logAndPrintError(newStderr, "Invalid zygote arguments", ex);
        } catch (ZygoteSecurityException ex) {
            logAndPrintError(newStderr,
                    "Zygote security policy prevents request: ", ex);
        }

        try {

            if (pid == 0) {
                // in child
                IoUtils.closeQuietly(serverPipeFd);
                serverPipeFd = null;

                handleChildProc(parsedArgs, descriptors, childPipeFd, newStderr);

                // should never get here, the child is expected to either
                // throw ZygoteInit.MethodAndArgsCaller or exec().
                return true;
            } else {

                // in parent...pid of < 0 means failure
                IoUtils.closeQuietly(childPipeFd);
                childPipeFd = null;
                return handleParentProc(pid, descriptors, serverPipeFd, parsedArgs);
            }

        } finally {
            IoUtils.closeQuietly(childPipeFd);
            IoUtils.closeQuietly(serverPipeFd);
        }
}
```

##### ZygoteConnection.handleChildProc

```
private void handleChildProc(Arguments parsedArgs,
    FileDescriptor[] descriptors, FileDescriptor pipeFd, PrintStream newStderr)
    throws ZygoteInit.MethodAndArgsCaller {

        ...

        /* 指定了"--runtime-init"参数，表示要为新创建的进程初始化运行时库，因此，
        ** 这里的parseArgs.runtimeInit值为true，于是就继续执行RuntimeInit.zygoteInit
        ** 进一步处理了。*/
        if (parsedArgs.runtimeInit) {
            if (parsedArgs.invokeWith != null) {
                WrapperInit.execApplication(parsedArgs.invokeWith,
                        parsedArgs.niceName, parsedArgs.targetSdkVersion,
                        pipeFd, parsedArgs.remainingArgs);
            } else {
                RuntimeInit.zygoteInit(parsedArgs.targetSdkVersion,
                        parsedArgs.remainingArgs);
            }

        } else {
            ...
        }
}
```

### RuntimeInit

path: frameworks/base/core/java/com/android/internal/os/RuntimeInit.java

##### RuntimeInit.zygoteInit

接下来我们继续分析RuntimeInit类的成员函数zygoteInit的具体实现如下所示：

```
public static final void zygoteInit(int targetSdkVersion, String[] argv)
            throws ZygoteInit.MethodAndArgsCaller {

       /* 重定向LOG的输入输出流 */
        redirectLogStreams();

        commonInit();  // 一般的初始化工作

        nativeZygoteInit();

        applicationInit(targetSdkVersion, argv);
}
```

##### RuntimeInit.applicationInit

```
private static void applicationInit(int targetSdkVersion, String[] argv)
            throws ZygoteInit.MethodAndArgsCaller {

        // If the application calls System.exit(), terminate the process
        // immediately without running any shutdown hooks.  It is not possible to
        // shutdown an Android application gracefully.  Among other things, the
        // Android runtime shutdown hooks close the Binder driver, which can cause
        // leftover running threads to crash before the process actually exits.

        nativeSetExitWithoutCleanup(true);

        // We want to be fairly aggressive about heap utilization, to avoid
        // holding on to a lot of memory that isn't needed.
        VMRuntime.getRuntime().setTargetHeapUtilization(0.75f);
        VMRuntime.getRuntime().setTargetSdkVersion(targetSdkVersion);

        final Arguments args;

        try {

            args = new Arguments(argv);

        } catch (IllegalArgumentException ex) {
            Slog.e(TAG, ex.getMessage());
            // let the process exit
            return;
        }

        // Remaining arguments are passed to the start class's static main
        invokeStaticMain(args.startClass, args.startArgs);
}
```

##### nativeZygoteInit

下面我们分析nativeZygoteInit函数是如何将启动Binder线程池的, 其是一个JNI方法，具体实现如下所示：

path: frameworks/base/core/jni/AndroidRuntime.cpp

```
/* gCurRuntime在AndroidRuntime构造函数中被初始化为this
** Zygote进程在启动时，会在进程中创建一个AppRuntime对象，而AppRuntime类又继承
** 了AndroidRuntime类，在创建AppRuntime对象时会导致AndroidRuntime类的构造函数
** 被调用，在调用的过程中，全局变量就会指向当前调用者类AppRuntime的对象，又由于
** 每一个新建的应用进程都复制了Zygote进程的地址空间，因此在每一个应用程序进程中
** 都会存在一个全局变量gCurRuntime , 当前新建进程也是如此*/
static AndroidRuntime* gCurRuntime = NULL;

static void com_android_internal_os_RuntimeInit_nativeZygoteInit(
    JNIEnv* env, jobject clazz)
{

    /* 在这里gCurRuntime指向的是一个AppRuntime对象，而AppRuntime对象重写了
    ** onZygoteInit函数所以在这里，会调用AppRuntime类的成员函数onZygoteInit */
    gCurRuntime->onZygoteInit();
}
```

##### AppRuntime.onZygoteInit

AppRuntime类的成员函数onZygoteInit具体实现如下所示：

path: frameworks/base/cmds/app_process/app_main.cpp

```
class AppRuntime : public AndroidRuntime
{
public:

    virtual void onZygoteInit()
    {

        sp<ProcessState> proc = ProcessState::self();

        ALOGV("App process: starting thread pool.\n");

        /* 调用ProcessState的成员函数startThreadPool来启动一个Binder线程池，
        ** 以便使得当前应用程序进程可以通过Binder进程间通信机制来和其它进程
        ** 通信, 每一个支持Binder进程间通信机制的进程内部都有一个唯一的
        ** ProcessState对象, 当startThreadPool执行完成之后，新创建的应用程序
        ** 进程就可以支持Binder进程间通信机制了, 这意味着我们以后在实现自己的
        ** Binder Service对象时，只需要将它启动起来并且注册到ServiceManager中
        ** 去就行了，而不用关心，其是如何通过Binder线程来接收进程间通信请求的
        ** 这样的话，默认System进程也是这样具有Binder通信机制的 */
        proc->startThreadPool();
   }
};
```

##### RuntimeInit.invokeStaticMain

接着我们分析调用invokeStaticMain函数是如何将ActivityThread的main函数设置为入口函数来
启动新创建的进程的，其具体实现如下所示：

path: frameworks/base/core/java/com/android/internal/os/RuntimeInit.java

```
public class RuntimeInit {
    ...
    private static void invokeStaticMain(String className, String[] argv)
            throws ZygoteInit.MethodAndArgsCaller {

        Class<?> cl;

        try {
            /* className = “android.app.ActivityThread”, 根据类名获取
            ** 一个类，加载到当前进程中System中 */
            cl = Class.forName(className);
        } catch (ClassNotFoundException ex) {
            throw new RuntimeException(
                    "Missing class when invoking static main " + className,
                    ex);
        }

        Method m;
        try {
            /* 获取它的静态成员函数main,并且保存在一个Method对象中 */
            m = cl.getMethod("main", new Class[] { String[].class });
        } catch (NoSuchMethodException ex) {
            throw new RuntimeException(
                    "Missing static main on " + className, ex);
        } catch (SecurityException ex) {
            throw new RuntimeException(
                    "Problem getting static main on " + className, ex);
        }

        int modifiers = m.getModifiers();
        if (! (Modifier.isStatic(modifiers) && Modifier.isPublic(modifiers))) {
            throw new RuntimeException(
                    "Main method is not public and static on " + className);
        }

        /*
         * This throw gets caught in ZygoteInit.main(), which responds
         * by invoking the exception's run() method. This arrangement
         * clears up all the stack frames that were required in setting
         * up the process.
         *
         * 将这个Method对象封装为一个MethodAndArgsCaller对象中,并且将这个
         * MethodAndArgsCaller作为一个异常对象抛出来给当前进程处理 */
        throw new ZygoteInit.MethodAndArgsCaller(m, argv);
    }
}
```

注意：新创建的System进程复制了Zygote进程的地址空间，因此，当前进程的调用堆栈和Zygote
的调用堆栈是一致的，在这里抛出这个异常时，当前进程就会沿着这个调用堆栈向后找到一个
合适的方法来捕获它，在这里将会由ZygoteInit的静态成员方法main来捕获这个MethodAndArgsCaller的异常：

##### ZygoteInit.main

路径：frameworks/base/core/java/com/android/internal/os/ZygoteInit.java

```
public static void main(String argv[]) {

        try {
            ...
        } catch (MethodAndArgsCaller caller) {
            // 调用MethodAndArgsCaller的成员函数run()来进一步处理
            caller.run();
        } catch (RuntimeException ex) {
            Log.e(TAG, "Zygote died with exception", ex);
            closeServerSocket();
            throw ex;
        }
}
```

##### MethodArgsCaller

```
/**
 * Helper exception class which holds a method and arguments and
 * can call them. This is used as part of a trampoline to get rid of
 * the initial process setup stack frames.
 * 这个类用于回退当前进程调用堆栈堆栈顶方法(当前调用方法)到一个合适方法
 * 中去处理进一步的操作 */
public static class MethodAndArgsCaller extends Exception
            implements Runnable {
        /** method to call */
        private final Method mMethod;

        /** argument array */
        private final String[] mArgs;

        public MethodAndArgsCaller(Method method, String[] args) {
            mMethod = method;  // mMethod指向了SystemService的main方法
            mArgs = args;  // 指向了参数列表
        }

        public void run() {

            try {

                /* 调用ActivityThread的main方法 */
                mMethod.invoke(null, new Object[] { mArgs });
            } catch (IllegalAccessException ex) {
                throw new RuntimeException(ex);
            } catch (InvocationTargetException ex) {

                Throwable cause = ex.getCause();
                if (cause instanceof RuntimeException) {
                    throw (RuntimeException) cause;
                } else if (cause instanceof Error) {
                    throw (Error) cause;
                }
                throw new RuntimeException(ex);
            }
        }
    }
}
```

注意：这绕了一个大圈子就是为了调用ActivityThread的静态成员方法main,为什么不直接在抛出
MethodAndArgsCaller异常的地方直接调用呢?因为在main方法调用之前，新创建的System进程已经执行
了相当多的代码，为了使得新创建的应用程序觉得它的入口函数就是main,而是巧妙地利用Java异常
处理机制来清理它前面的调用堆栈.
