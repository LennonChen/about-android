dex2oat
========================================

dex2oat 顾名思义 dex file to oat file，就是在新旧两种运行时文件的转换。他是一个可执行 ELF 文件，
adb 连接手机以后也可以直接调用。dex2oat 的源码只有一个文件 /art/dex2oat/dex2oat.cc。

oat 文件在哪里？

我们知道选择 ART 作为运行时后，需要重启手机。然后系统会使用 dex2oat 把所有 App 编译成 oat 文件。
oat在 /data/dalvik-cache/*@classes.dex。

PMS 服务启动之后会监控某些目录（/data/app/）。如果目录多了某个文件，PMS 会调用一些方法对文件进行
处理。扫描监控目录的方法是：scanDirLI()

1.scanDirLI
----------------------------------------

path: framework/base/services/core/java/com/android/server/pm/PackageManagerService.java
```
    private void scanDirLI(File dir, int parseFlags, int scanFlags, long currentTime) {
        final File[] files = dir.listFiles();
        if (ArrayUtils.isEmpty(files)) {
            Log.d(TAG, "No files in app dir " + dir);
            return;
        }

        if (DEBUG_PACKAGE_SCANNING) {
            Log.d(TAG, "Scanning app dir " + dir + " scanFlags=" + scanFlags
                    + " flags=0x" + Integer.toHexString(parseFlags));
        }

        for (File file : files) {
            final boolean isPackage = (isApkFile(file) || file.isDirectory())
                    && !PackageInstallerService.isStageName(file.getName());
            // MIUI MOD:
            if (!isPackage) {
                // Ignore entries which are not packages
                continue;
            }
            try {
                scanPackageLI(file, parseFlags | PackageParser.PARSE_MUST_BE_APK,
                        scanFlags, currentTime, null);
            } catch (PackageManagerException e) {
                Slog.w(TAG, "Failed to parse " + file + ": " + e.getMessage());

                // Delete invalid userdata apps
                if ((parseFlags & PackageParser.PARSE_IS_SYSTEM) == 0 &&
                        e.error == PackageManager.INSTALL_FAILED_INVALID_APK) {
                    logCriticalInfo(Log.WARN, "Deleting invalid package at " + file);
                    if (file.isDirectory()) {
                        mInstaller.rmPackageDir(file.getAbsolutePath());
                    } else {
                        file.delete();
                    }
                }
            }
        }
    }
```

2.scanPackageLI
----------------------------------------

```
   /*
     *  Scan a package and return the newly parsed package.
     *  Returns null in case of errors and the error code is stored in mLastScanError
     */
    private PackageParser.Package scanPackageLI(File scanFile, int parseFlags, int scanFlags,
            long currentTime, UserHandle user) throws PackageManagerException {
        if (DEBUG_INSTALL) Slog.d(TAG, "Parsing: " + scanFile);
        parseFlags |= mDefParseFlags;
        PackageParser pp = new PackageParser();
        pp.setSeparateProcesses(mSeparateProcesses);
        pp.setOnlyCoreApps(mOnlyCore);
        pp.setDisplayMetrics(mMetrics);

        if ((scanFlags & SCAN_TRUSTED_OVERLAY) != 0) {
            parseFlags |= PackageParser.PARSE_TRUSTED_OVERLAY;
        }

        final PackageParser.Package pkg;
        try {
            pkg = pp.parsePackage(scanFile, parseFlags);
        } catch (PackageParserException e) {
            throw PackageManagerException.from(e);
        }

        ...

        // Note that we invoke the following method only if we are about to unpack an application
        PackageParser.Package scannedPkg = scanPackageLI(pkg, parseFlags, scanFlags
                | SCAN_UPDATE_SIGNATURE, currentTime, user);

        /*
         * If the system app should be overridden by a previously installed
         * data, hide the system app now and let the /data/app scan pick it up
         * again.
         */
        if (shouldHideSystemApp) {
            synchronized (mPackages) {
                mSettings.disableSystemPackageLPw(pkg.packageName);
            }
        }

        return scannedPkg;
    }
```

3.scanPackageLI --> pkg
----------------------------------------

```
   private PackageParser.Package scanPackageLI(PackageParser.Package pkg, int parseFlags,
            int scanFlags, long currentTime, UserHandle user) throws PackageManagerException {
        boolean success = false;
        try {
            final PackageParser.Package res = scanPackageDirtyLI(pkg, parseFlags, scanFlags,
                    currentTime, user);
            success = true;
            return res;
        } finally {
            if (!success && (scanFlags & SCAN_DELETE_DATA_ON_FAILURES) != 0) {
                removeDataDirsLI(pkg.volumeUuid, pkg.packageName);
            }
        }
    }
```

4.scanPackageDirtyLI
----------------------------------------

```
   private PackageParser.Package scanPackageDirtyLI(PackageParser.Package pkg, int parseFlags,
            int scanFlags, long currentTime, UserHandle user) throws PackageManagerException {
        ...
        if ((scanFlags & SCAN_NO_DEX) == 0) {
            int result = mPackageDexOptimizer.performDexOpt(pkg, null /* instruction sets */,
                    forceDex, (scanFlags & SCAN_DEFER_DEX) != 0, false /* inclDependencies */);
            if (result == PackageDexOptimizer.DEX_OPT_FAILED) {
                throw new PackageManagerException(INSTALL_FAILED_DEXOPT, "scanPackageLI");
            }
        }
        ...
    }
```

5.performDexOpt
----------------------------------------

path: framework/base/services/core/java/com/android/server/pm/PackageDexOptimizer.java
```
   /**
     * Performs dexopt on all code paths and libraries of the specified package for specified
     * instruction sets.
     *
     * <p>Calls to {@link com.android.server.pm.Installer#dexopt} are synchronized on
     * {@link PackageManagerService#mInstallLock}.
     */
    int performDexOpt(PackageParser.Package pkg, String[] instructionSets,
            boolean forceDex, boolean defer, boolean inclDependencies) {
        ArraySet<String> done;
        if (inclDependencies && (pkg.usesLibraries != null || pkg.usesOptionalLibraries != null)) {
            done = new ArraySet<String>();
            done.add(pkg.packageName);
        } else {
            done = null;
        }
        synchronized (mPackageManagerService.mInstallLock) {
            final boolean useLock = mSystemReady;
            if (useLock) {
                mDexoptWakeLock.setWorkSource(new WorkSource(pkg.applicationInfo.uid));
                mDexoptWakeLock.acquire();
            }
            try {
                return performDexOptLI(pkg, instructionSets, forceDex, defer, done);
            } finally {
                if (useLock) {
                    mDexoptWakeLock.release();
                }
            }
        }
    }
```

6.performDexOptLI
----------------------------------------

performDexOptLI 先检查 dex 是否需要优化.

```
    private int performDexOptLI(PackageParser.Package pkg, String[] targetInstructionSets,
            boolean forceDex, boolean defer, ArraySet<String> done) {
        final String[] instructionSets = targetInstructionSets != null ?
                targetInstructionSets : getAppDexInstructionSets(pkg.applicationInfo);

        if (done != null) {
            done.add(pkg.packageName);
            if (pkg.usesLibraries != null) {
                performDexOptLibsLI(pkg.usesLibraries, instructionSets, forceDex, defer, done);
            }
            if (pkg.usesOptionalLibraries != null) {
                performDexOptLibsLI(pkg.usesOptionalLibraries, instructionSets, forceDex, defer,
                        done);
            }
        }

        if ((pkg.applicationInfo.flags & ApplicationInfo.FLAG_HAS_CODE) == 0) {
            return DEX_OPT_SKIPPED;
        }

        final boolean vmSafeMode = (pkg.applicationInfo.flags & ApplicationInfo.FLAG_VM_SAFE_MODE) != 0;
        final boolean debuggable = (pkg.applicationInfo.flags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;

        final List<String> paths = pkg.getAllCodePathsExcludingResourceOnly();
        boolean performedDexOpt = false;
        // There are three basic cases here:
        // 1.) we need to dexopt, either because we are forced or it is needed
        // 2.) we are deferring a needed dexopt
        // 3.) we are skipping an unneeded dexopt
        final String[] dexCodeInstructionSets = getDexCodeInstructionSets(instructionSets);
        for (String dexCodeInstructionSet : dexCodeInstructionSets) {
            if (!forceDex && pkg.mDexOptPerformed.contains(dexCodeInstructionSet)) {
                continue;
            }

            for (String path : paths) {
                final int dexoptNeeded;
                if (forceDex) {
                    dexoptNeeded = DexFile.DEX2OAT_NEEDED;
                } else {
                    try {
                        dexoptNeeded = DexFile.getDexOptNeeded(path, pkg.packageName,
                                dexCodeInstructionSet, defer);
                    } catch (IOException ioe) {
                        Slog.w(TAG, "IOException reading apk: " + path, ioe);
                        return DEX_OPT_FAILED;
                    }
                }

                if (!forceDex && defer && dexoptNeeded != DexFile.NO_DEXOPT_NEEDED) {
                    // We're deciding to defer a needed dexopt. Don't bother dexopting for other
                    // paths and instruction sets. We'll deal with them all together when we process
                    // our list of deferred dexopts.
                    addPackageForDeferredDexopt(pkg);
                    return DEX_OPT_DEFERRED;
                }

                if (dexoptNeeded != DexFile.NO_DEXOPT_NEEDED) {
                    final String dexoptType;
                    String oatDir = null;
                    if (dexoptNeeded == DexFile.DEX2OAT_NEEDED) {
                        dexoptType = "dex2oat";
                        try {
                            oatDir = createOatDirIfSupported(pkg, dexCodeInstructionSet);
                        } catch (IOException ioe) {
                            Slog.w(TAG, "Unable to create oatDir for package: " + pkg.packageName);
                            return DEX_OPT_FAILED;
                        }
                    } else if (dexoptNeeded == DexFile.PATCHOAT_NEEDED) {
                        dexoptType = "patchoat";
                    } else if (dexoptNeeded == DexFile.SELF_PATCHOAT_NEEDED) {
                        dexoptType = "self patchoat";
                    } else {
                        throw new IllegalStateException("Invalid dexopt needed: " + dexoptNeeded);
                    }

                    Log.i(TAG, "Running dexopt (" + dexoptType + ") on: " + path + " pkg="
                            + pkg.applicationInfo.packageName + " isa=" + dexCodeInstructionSet
                            + " vmSafeMode=" + vmSafeMode + " debuggable=" + debuggable
                            + " oatDir = " + oatDir);

                    final int sharedGid = UserHandle.getSharedAppGid(pkg.applicationInfo.uid);
                    final int ret = mPackageManagerService.mInstaller.dexopt(path, sharedGid,
                            !pkg.isForwardLocked(), pkg.packageName, dexCodeInstructionSet,
                            dexoptNeeded, vmSafeMode, debuggable, oatDir);

                    // Dex2oat might fail due to compiler / verifier errors. We soldier on
                    // regardless, and attempt to interpret the app as a safety net.
                    if (ret == 0) {
                        performedDexOpt = true;
                    }
                }
            }

            // At this point we haven't failed dexopt and we haven't deferred dexopt. We must
            // either have either succeeded dexopt, or have had getDexOptNeeded tell us
            // it isn't required. We therefore mark that this package doesn't need dexopt unless
            // it's forced. performedDexOpt will tell us whether we performed dex-opt or skipped
            // it.
            pkg.mDexOptPerformed.add(dexCodeInstructionSet);
        }

        // If we've gotten here, we're sure that no error occurred and that we haven't
        // deferred dex-opt. We've either dex-opted one more paths or instruction sets or
        // we've skipped all of them because they are up to date. In both cases this
        // package doesn't need dexopt any longer.
        return performedDexOpt ? DEX_OPT_PERFORMED : DEX_OPT_SKIPPED;
    }
```

7.dexopt
----------------------------------------

path: framework/base/services/core/java/com/android/server/pm/Installer.java
```
   public int dexopt(String apkPath, int uid, boolean isPublic, String pkgName,
            String instructionSet, int dexoptNeeded, boolean vmSafeMode,
            boolean debuggable, @Nullable String outputPath) {
        if (!isValidInstructionSet(instructionSet)) {
            Slog.e(TAG, "Invalid instruction set: " + instructionSet);
            return -1;
        }
        return mInstaller.dexopt(apkPath, uid, isPublic, pkgName,
                instructionSet, dexoptNeeded, vmSafeMode,
                debuggable, outputPath);
    }
```

8.do_dexopt
----------------------------------------

path: frameworks/native/cmds/installd/installd.cpp
```
static int do_dexopt(char **arg, char reply[REPLY_MAX] __unused)
{
    /* apk_path, uid, is_public, pkgname, instruction_set,
     * dexopt_needed, vm_safe_mode, debuggable, oat_dir */
    return dexopt(arg[0], atoi(arg[1]), atoi(arg[2]), arg[3], arg[4], atoi(arg[5]),
                  atoi(arg[6]), atoi(arg[7]), arg[8]);
}
```

9.dexopt
----------------------------------------

```
int dexopt(const char *apk_path, uid_t uid, bool is_public,
           const char *pkgname, const char *instruction_set, int dexopt_needed,
           bool vm_safe_mode, bool debuggable, const char* oat_dir)
{
    struct utimbuf ut;
    struct stat input_stat;
    char out_path[PKG_PATH_MAX];
    char swap_file_name[PKG_PATH_MAX];
    const char *input_file;
    char in_odex_path[PKG_PATH_MAX];
    int res, input_fd=-1, out_fd=-1, swap_fd=-1;

    // Early best-effort check whether we can fit the the path into our buffers.
    // Note: the cache path will require an additional 5 bytes for ".swap", but we'll try to run
    // without a swap file, if necessary.
    if (strlen(apk_path) >= (PKG_PATH_MAX - 8)) {
        ALOGE("apk_path too long '%s'\n", apk_path);
        return -1;
    }

    if (oat_dir != NULL && oat_dir[0] != '!') {
        if (validate_apk_path(oat_dir)) {
            ALOGE("invalid oat_dir '%s'\n", oat_dir);
            return -1;
        }
        if (calculate_oat_file_path(out_path, oat_dir, apk_path, instruction_set)) {
            return -1;
        }
    } else {
        if (create_cache_path(out_path, apk_path, instruction_set)) {
            return -1;
        }
    }

    switch (dexopt_needed) {
        case DEXOPT_DEX2OAT_NEEDED:
            input_file = apk_path;
            break;

        case DEXOPT_PATCHOAT_NEEDED:
            if (!calculate_odex_file_path(in_odex_path, apk_path, instruction_set)) {
                return -1;
            }
            input_file = in_odex_path;
            break;

        case DEXOPT_SELF_PATCHOAT_NEEDED:
            input_file = out_path;
            break;

        default:
            ALOGE("Invalid dexopt needed: %d\n", dexopt_needed);
            exit(72);
    }

    memset(&input_stat, 0, sizeof(input_stat));
    stat(input_file, &input_stat);

    input_fd = open(input_file, O_RDONLY, 0);
    if (input_fd < 0) {
        ALOGE("installd cannot open '%s' for input during dexopt\n", input_file);
        return -1;
    }

    unlink(out_path);
    out_fd = open(out_path, O_RDWR | O_CREAT | O_EXCL, 0644);
    if (out_fd < 0) {
        ALOGE("installd cannot open '%s' for output during dexopt\n", out_path);
        goto fail;
    }
    if (fchmod(out_fd,
               S_IRUSR|S_IWUSR|S_IRGRP |
               (is_public ? S_IROTH : 0)) < 0) {
        ALOGE("installd cannot chmod '%s' during dexopt\n", out_path);
        goto fail;
    }
    if (fchown(out_fd, AID_SYSTEM, uid) < 0) {
        ALOGE("installd cannot chown '%s' during dexopt\n", out_path);
        goto fail;
    }

    // Create profile file if there is a package name present.
    if (strcmp(pkgname, "*") != 0) {
        create_profile_file(pkgname, uid);
    }

    // Create a swap file if necessary.
    if (ShouldUseSwapFileForDexopt()) {
        // Make sure there really is enough space.
        size_t out_len = strlen(out_path);
        if (out_len + strlen(".swap") + 1 <= PKG_PATH_MAX) {
            strcpy(swap_file_name, out_path);
            strcpy(swap_file_name + strlen(out_path), ".swap");
            unlink(swap_file_name);
            swap_fd = open(swap_file_name, O_RDWR | O_CREAT | O_EXCL, 0600);
            if (swap_fd < 0) {
                // Could not create swap file. Optimistically go on and hope that we can compile
                // without it.
                ALOGE("installd could not create '%s' for swap during dexopt\n", swap_file_name);
            } else {
                // Immediately unlink. We don't really want to hit flash.
                unlink(swap_file_name);
            }
        } else {
            // Swap file path is too long. Try to run without.
            ALOGE("installd could not create swap file for path %s during dexopt\n", out_path);
        }
    }

    ALOGV("DexInv: --- BEGIN '%s' ---\n", input_file);

    pid_t pid;
    pid = fork();
    if (pid == 0) {
        /* child -- drop privileges before continuing */
        if (setgid(uid) != 0) {
            ALOGE("setgid(%d) failed in installd during dexopt\n", uid);
            exit(64);
        }
        if (setuid(uid) != 0) {
            ALOGE("setuid(%d) failed in installd during dexopt\n", uid);
            exit(65);
        }
        // drop capabilities
        struct __user_cap_header_struct capheader;
        struct __user_cap_data_struct capdata[2];
        memset(&capheader, 0, sizeof(capheader));
        memset(&capdata, 0, sizeof(capdata));
        capheader.version = _LINUX_CAPABILITY_VERSION_3;
        if (capset(&capheader, &capdata[0]) < 0) {
            ALOGE("capset failed: %s\n", strerror(errno));
            exit(66);
        }
        if (set_sched_policy(0, SP_BACKGROUND) < 0) {
            ALOGE("set_sched_policy failed: %s\n", strerror(errno));
            exit(70);
        }
        if (setpriority(PRIO_PROCESS, 0, ANDROID_PRIORITY_BACKGROUND) < 0) {
            ALOGE("setpriority failed: %s\n", strerror(errno));
            exit(71);
        }
        if (flock(out_fd, LOCK_EX | LOCK_NB) != 0) {
            ALOGE("flock(%s) failed: %s\n", out_path, strerror(errno));
            exit(67);
        }

        if (dexopt_needed == DEXOPT_PATCHOAT_NEEDED
            || dexopt_needed == DEXOPT_SELF_PATCHOAT_NEEDED) {
            run_patchoat(input_fd, out_fd, input_file, out_path, pkgname, instruction_set);
        } else if (dexopt_needed == DEXOPT_DEX2OAT_NEEDED) {
            const char *input_file_name = strrchr(input_file, '/');
            if (input_file_name == NULL) {
                input_file_name = input_file;
            } else {
                input_file_name++;
            }
            run_dex2oat(input_fd, out_fd, input_file_name, out_path, swap_fd, pkgname,
                        instruction_set, vm_safe_mode, debuggable);
        } else {
            ALOGE("Invalid dexopt needed: %d\n", dexopt_needed);
            exit(73);
        }
        exit(68);   /* only get here on exec failure */
    } else {
        res = wait_child(pid);
        if (res == 0) {
            ALOGV("DexInv: --- END '%s' (success) ---\n", input_file);
        } else {
            ALOGE("DexInv: --- END '%s' --- status=0x%04x, process failed\n", input_file, res);
            goto fail;
        }
    }

    ut.actime = input_stat.st_atime;
    ut.modtime = input_stat.st_mtime;
    utime(out_path, &ut);

    close(out_fd);
    close(input_fd);
    if (swap_fd != -1) {
        close(swap_fd);
    }
    return 0;

fail:
    if (out_fd >= 0) {
        close(out_fd);
        unlink(out_path);
    }
    if (input_fd >= 0) {
        close(input_fd);
    }
    return -1;
}
```
