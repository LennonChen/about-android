Android userdata Encryption
========================================

1.CryptKeeperConfirm.onCreate
----------------------------------------

path: packages/apps/Settings/src/com/android/settings/CryptKeeperConfirm.java
```
public class CryptKeeperConfirm extends Fragment {

    private static final String TAG = "CryptKeeperConfirm";

    public static class Blank extends Activity {
        private Handler mHandler = new Handler();

        @Override
        public void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);

            setContentView(R.layout.crypt_keeper_blank);

            if (Utils.isMonkeyRunning()) {
                finish();
            }

            StatusBarManager sbm = (StatusBarManager) getSystemService(Context.STATUS_BAR_SERVICE);
            sbm.disable(StatusBarManager.DISABLE_EXPAND
                    | StatusBarManager.DISABLE_NOTIFICATION_ICONS
                    | StatusBarManager.DISABLE_NOTIFICATION_ALERTS
                    | StatusBarManager.DISABLE_SYSTEM_INFO
                    | StatusBarManager.DISABLE_HOME
                    | StatusBarManager.DISABLE_SEARCH
                    | StatusBarManager.DISABLE_RECENT
                    | StatusBarManager.DISABLE_BACK);

            // Post a delayed message in 700 milliseconds to enable encryption.
            // NOTE: The animation on this activity is set for 500 milliseconds
            // I am giving it a little extra time to complete.
            mHandler.postDelayed(new Runnable() {
                public void run() {
                    IBinder service = ServiceManager.getService("mount");
                    if (service == null) {
                        Log.e("CryptKeeper", "Failed to find the mount service");
                        finish();
                        return;
                    }

                    IMountService mountService = IMountService.Stub.asInterface(service);
                    try {
                        Bundle args = getIntent().getExtras();
                        mountService.encryptStorage(
                            args.getInt("type", -1), args.getString("password"));
                    } catch (Exception e) {
                        Log.e("CryptKeeper", "Error while encrypting...", e);
                    }
                }
            }, 700);
        }
    }

    ...
}
```

接下来调用MountService的加密函数encryptStorage来进行加密:

2.MountService.encryptStorage
----------------------------------------

path: frameworks/base/services/core/java/com/android/server/MountService.java
```
    ...
    public int encryptStorage(int type, String password) {
        if (TextUtils.isEmpty(password) && type != StorageManager.CRYPT_TYPE_DEFAULT) {
            throw new IllegalArgumentException("password cannot be empty");
        }

        mContext.enforceCallingOrSelfPermission(Manifest.permission.CRYPT_KEEPER,
            "no permission to access the crypt keeper");

        waitForReady();

        if (DEBUG_EVENTS) {
            Slog.i(TAG, "encrypting storage...");
        }

        try {
            mConnector.execute("cryptfs", "enablecrypto", "inplace", CRYPTO_TYPES[type],
                               new SensitiveArg(toHex(password)));
        } catch (NativeDaemonConnectorException e) {
            // Encryption failed
            return e.getCode();
        }

        return 0;
    }
    ...
```

3.CommandListener::CryptfsCmd::runCommand
----------------------------------------

path: system/vold/CommandListener.cpp
```
...
int CommandListener::CryptfsCmd::runCommand(SocketClient *cli, int argc, char **argv) {

    ...

    int rc = 0;

    ...

    } else if (!strcmp(argv[1], "enablecrypto")) {
        const char* syntax = "Usage: cryptfs enablecrypto <wipe|inplace> "
                             "default|password|pin|pattern [passwd]";
        if ( (argc != 4 && argc != 5)
             || (strcmp(argv[2], "wipe") && strcmp(argv[2], "inplace")) ) {
            cli->sendMsg(ResponseCode::CommandSyntaxError, syntax, false);
            return 0;
        }
        dumpArgs(argc, argv, 4);

        int tries;
        for (tries = 0; tries < 2; ++tries) {
            int type = getType(argv[3]);
            if (type == -1) {
                cli->sendMsg(ResponseCode::CommandSyntaxError, syntax,
                             false);
                return 0;
            } else if (type == CRYPT_TYPE_DEFAULT) {
              rc = cryptfs_enable_default(argv[2], /*allow_reboot*/false);
            } else {
                rc = cryptfs_enable(argv[2], type, argv[4],
                                    /*allow_reboot*/false);
            }

            if (rc == 0) {
                break;
            } else if (tries == 0) {
                Process::killProcessesWithOpenFiles(DATA_MNT_POINT, 2);
            }
        }
    } else if (!strcmp(argv[1], "changepw")) {

    ...

    return 0;
}
...
```

4.cryptfs_enable vs cryptfs_enable_default
----------------------------------------

path: system/vold/cryptfs.c
```
int cryptfs_enable(char *howarg, int type, char *passwd, int allow_reboot)
{
    char* adjusted_passwd = adjust_passwd(passwd);
    if (adjusted_passwd) {
        passwd = adjusted_passwd;
    }

    int rc = cryptfs_enable_internal(howarg, type, passwd, allow_reboot);

    free(adjusted_passwd);
    return rc;
}

// 如果使用默认密码，就走这个流程
int cryptfs_enable_default(char *howarg, int allow_reboot)
{
    return cryptfs_enable_internal(howarg, CRYPT_TYPE_DEFAULT,
                          DEFAULT_PASSWORD, allow_reboot);
}
```

5.cryptfs_enable_internal
----------------------------------------

path: system/vold/cryptfs.c
```
int cryptfs_enable_internal(char *howarg, int crypt_type, char *passwd,
                            int allow_reboot)
{
    int how = 0;
    char crypto_blkdev[MAXPATHLEN], real_blkdev[MAXPATHLEN];
    unsigned long nr_sec;
    unsigned char decrypted_master_key[KEY_LEN_BYTES];
    int rc=-1, fd, i, ret;
    struct crypt_mnt_ftr crypt_ftr;
    struct crypt_persist_data *pdata;
    char encrypted_state[PROPERTY_VALUE_MAX];
    char lockid[32] = { 0 };
    char key_loc[PROPERTY_VALUE_MAX];
    char fuse_sdcard[PROPERTY_VALUE_MAX];
    char *sd_mnt_point;
    int num_vols;
    struct volume_info *vol_list = 0;
    off64_t previously_encrypted_upto = 0;

    if (!strcmp(howarg, "wipe")) {
      how = CRYPTO_ENABLE_WIPE;
    } else if (! strcmp(howarg, "inplace")) {
      how = CRYPTO_ENABLE_INPLACE;
    } else {
      /* Shouldn't happen, as CommandListener vets the args */
      goto error_unencrypted;
    }

    ...

    return rc;

error_unencrypted:
    free(vol_list);
    property_set("vold.encrypt_progress", "error_not_encrypted");
    if (lockid[0]) {
        release_wake_lock(lockid);
    }
    return -1;

error_shutting_down:
    /* we failed, and have not encrypted anthing, so the users's data is still intact,
     * but the framework is stopped and not restarted to show the error, so it's up to
     * vold to restart the system.
     */
    SLOGE("Error enabling encryption after framework is shutdown, no data changed, restarting system");
    cryptfs_reboot(reboot);

    /* shouldn't get here */
    property_set("vold.encrypt_progress", "error_shutting_down");
    free(vol_list);
    if (lockid[0]) {
        release_wake_lock(lockid);
    }
    return -1;
}
```

### 检查上次是否加密过

```
    ...
    /* See if an encryption was underway and interrupted */
    if (how == CRYPTO_ENABLE_INPLACE
          && get_crypt_ftr_and_key(&crypt_ftr) == 0
          && (crypt_ftr.flags & CRYPT_ENCRYPTION_IN_PROGRESS)) {
        previously_encrypted_upto = crypt_ftr.encrypted_upto;
        crypt_ftr.encrypted_upto = 0;
        crypt_ftr.flags &= ~CRYPT_ENCRYPTION_IN_PROGRESS;

        /* At this point, we are in an inconsistent state. Until we successfully
           complete encryption, a reboot will leave us broken. So mark the
           encryption failed in case that happens.
           On successfully completing encryption, remove this flag */
        crypt_ftr.flags |= CRYPT_INCONSISTENT_STATE;

        put_crypt_ftr_and_key(&crypt_ftr);
    }

    // previously_encrypted_upto表示上一次加密的进度，如果是0的话，
    // 而加密状态又是"encrypted"，这就是有问题啊
    property_get("ro.crypto.state", encrypted_state, "");
    if (!strcmp(encrypted_state, "encrypted") && !previously_encrypted_upto) {
        SLOGE("Device is already running encrypted, aborting");
        goto error_unencrypted;
    }
    ...
```

### 取出key存储的路径和待加密的路径

查看对应的fstab

```
/dev/block/bootdevice/by-name/userdata       /data        ext4    nosuid,nodev,barrier=1,noauto_da_alloc,discard      wait,check,encryptable=/dev/block/bootdevice/by-name/bk1
```

从对应的fstab条目中我们可以得知:

key_loc=/dev/block/bootdevice/by-name/bk1
real_blkdev=/dev/block/bootdevice/by-name/userdata

```
    ...
    // TODO refactor fs_mgr_get_crypt_info to get both in one call
    fs_mgr_get_crypt_info(fstab, key_loc, 0, sizeof(key_loc));
    fs_mgr_get_crypt_info(fstab, 0, real_blkdev, sizeof(real_blkdev));
    ...
```

### 打开待加密设备并获取对应块个数

```
    ...
    /* Get the size of the real block device */
    fd = open(real_blkdev, O_RDONLY);
    if ( (nr_sec = get_blkdev_size(fd)) == 0) {
        SLOGE("Cannot get size of block device %s\n", real_blkdev);
        goto error_unencrypted;
    }
    close(fd);
    ...
```

### 检查key的存储路径

```
   ...
    /* If doing inplace encryption, make sure the orig fs doesn't include the crypto footer */
    if ((how == CRYPTO_ENABLE_INPLACE) && (!strcmp(key_loc, KEY_IN_FOOTER))) {
        unsigned int fs_size_sec, max_fs_size_sec;
        fs_size_sec = get_fs_size(real_blkdev);
        if (fs_size_sec == 0)
            fs_size_sec = get_f2fs_filesystem_size_sec(real_blkdev);

        max_fs_size_sec = nr_sec - (CRYPT_FOOTER_OFFSET / CRYPT_SECTOR_SIZE);

        if (fs_size_sec > max_fs_size_sec) {
            SLOGE("Orig filesystem overlaps crypto footer region.  Cannot encrypt in place.");
            goto error_unencrypted;
        }
    }
    ...
```

### 加密耗电获取wakelock

```
    ...
    /* Get a wakelock as this may take a while, and we don't want the
     * device to sleep on us.  We'll grab a partial wakelock, and if the UI
     * wants to keep the screen on, it can grab a full wakelock.
     */
    snprintf(lockid, sizeof(lockid), "enablecrypto%d", (int) getpid());
    acquire_wake_lock(PARTIAL_WAKE_LOCK, lockid);
    ...
```

### 做点其它事情

```
    ...
    /* Get the sdcard mount point */
    sd_mnt_point = getenv("EMULATED_STORAGE_SOURCE");
    if (!sd_mnt_point) {
       sd_mnt_point = getenv("EXTERNAL_STORAGE");
    }
    if (!sd_mnt_point) {
        sd_mnt_point = "/mnt/sdcard";
    }

    /* TODO
     * Currently do not have test devices with multiple encryptable volumes.
     * When we acquire some, re-add support.
     */
    num_vols=vold_getNumDirectVolumes();
    vol_list = malloc(sizeof(struct volume_info) * num_vols);
    vold_getDirectVolumeList(vol_list);

    for (i=0; i<num_vols; i++) {
        if (should_encrypt(&vol_list[i])) {
            SLOGE("Cannot encrypt if there are multiple encryptable volumes"
                  "%s\n", vol_list[i].label);
            goto error_unencrypted;
        }
    }
    ...
```

### 禁止framework工作

```
    ...
    /* The init files are setup to stop the class main and late start when
     * vold sets trigger_shutdown_framework.
     */
    property_set("vold.decrypt", "trigger_shutdown_framework");
    SLOGD("Just asked init to shut down class main\n");
    ...
```

### 卸载所有可访问的分区

```
    ...
    if (vold_unmountAllAsecs()) {
        /* Just report the error.  If any are left mounted,
         * umounting /data below will fail and handle the error.
         */
        SLOGE("Error unmounting internal asecs");
    }

    property_get("ro.crypto.fuse_sdcard", fuse_sdcard, "");
    if (!strcmp(fuse_sdcard, "true")) {
        /* This is a device using the fuse layer to emulate the sdcard semantics
         * on top of the userdata partition.  vold does not manage it, it is managed
         * by the sdcard service.  The sdcard service was killed by the property trigger
         * above, so just unmount it now.  We must do this _AFTER_ killing the framework,
         * unlike the case for vold managed devices above.
         */
        if (wait_and_unmount(sd_mnt_point, false)) {
            goto error_shutting_down;
        }
    }

    /* Now unmount the /data partition. */
    if (wait_and_unmount(DATA_MNT_POINT, false)) {
        if (allow_reboot) {
            goto error_shutting_down;
        } else {
            goto error_unencrypted;
        }
    }
    ...
```

### 进行真正的加密工作

#### 为加密分区初始化一个crypt_mnt_ftr结构实例

```
    /* Start the actual work of making an encrypted filesystem */
    /* Initialize a crypt_mnt_ftr for the partition */
    if (previously_encrypted_upto == 0) {
        // 1.初始化一个crypt_mnt_ftr结构
        if (cryptfs_init_crypt_mnt_ftr(&crypt_ftr)) {
            goto error_shutting_down;
        }

        // 2.设置加密分区大小，单位为块
        if (!strcmp(key_loc, KEY_IN_FOOTER)) {
            crypt_ftr.fs_size = nr_sec
              - (CRYPT_FOOTER_OFFSET / CRYPT_SECTOR_SIZE);
        } else {
            crypt_ftr.fs_size = nr_sec;
        }
        /* At this point, we are in an inconsistent state. Until we successfully
           complete encryption, a reboot will leave us broken. So mark the
           encryption failed in case that happens.
           On successfully completing encryption, remove this flag */
        crypt_ftr.flags |= CRYPT_INCONSISTENT_STATE;
        crypt_ftr.crypt_type = crypt_type;
#ifndef CONFIG_HW_DISK_ENCRYPTION
        strlcpy((char *)crypt_ftr.crypto_type_name,
                "aes-cbc-essiv:sha256",
                 MAX_CRYPTO_TYPE_NAME_LEN);
#else
        if (is_hw_fde_enabled()) {
            strlcpy((char *)crypt_ftr.crypto_type_name, "aes-xts", MAX_CRYPTO_TYPE_NAME_LEN);
            wipe_hw_device_encryption_key((char*)crypt_ftr.crypto_type_name);
            if(!set_hw_device_encryption_key(passwd, (char*)crypt_ftr.crypto_type_name))
                goto error_shutting_down;
        } else {
            strlcpy((char *)crypt_ftr.crypto_type_name, "aes-cbc-essiv:sha256", MAX_CRYPTO_TYPE_NAME_LEN);
        }
#endif

        /* Make an encrypted master key */
        // 3.光有password作为key还不够，我们还需要加入盐值，最终会在master_key里边得到一组密钥。
        if (create_encrypted_random_key(passwd, crypt_ftr.master_key, crypt_ftr.salt, &crypt_ftr)) {
            SLOGE("Cannot create encrypted master key\n");
            goto error_shutting_down;
        }

        /* Write the key to the end of the partition */
        // 4.把这些信息存到key位置或者存储空间的最末尾
        put_crypt_ftr_and_key(&crypt_ftr);

        /* If any persistent data has been remembered, save it.
         * If none, create a valid empty table and save that.
         */
        if (!persist_data) {
           pdata = malloc(CRYPT_PERSIST_DATA_SIZE);
           if (pdata) {
               init_empty_persist_data(pdata, CRYPT_PERSIST_DATA_SIZE);
               persist_data = pdata;
           }
        }
        // 5.key信息是存储了两份，挨着的。当第一份被破坏的时候，还能从第二份那修复回来！
        if (persist_data) {
            save_persistent_data();
        }
    }
```

##### cryptfs_init_crypt_mnt_ftr

https://github.com/leeminghao/about-android/blob/master/hardware/cryptfs/cryptfs_init_crypt_mnt_ftr.md

##### create_encrypted_random_key

https://github.com/leeminghao/about-android/blob/master/hardware/cryptfs/create_encrypted_random_key.md

#### 为启动最小framework做准备

```
    ...
    /* Do extra work for a better UX when doing the long inplace encryption */
    if (how == CRYPTO_ENABLE_INPLACE) {
        /* Now that /data is unmounted, we need to mount a tmpfs
         * /data, set a property saying we're doing inplace encryption,
         * and restart the framework.
         */
        // 1.挂载tmpfs到/data目录。
        if (fs_mgr_do_tmpfs_mount(DATA_MNT_POINT)) {
            goto error_shutting_down;
        }
        /* Tells the framework that inplace encryption is starting */
        property_set("vold.encrypt_progress", "0");

        /* restart the framework. */
        /* Create necessary paths on /data */
        // 2.在data目录下创建一些必要的目录，否则framework将启动不了
        if (prep_data_fs()) {
            goto error_shutting_down;
        }

        /* Ugh, shutting down the framework is not synchronous, so until it
         * can be fixed, this horrible hack will wait a moment for it all to
         * shut down before proceeding.  Without it, some devices cannot
         * restart the graphics services.
         */
        sleep(2);

        /* startup service classes main and late_start */
        property_set("vold.decrypt", "trigger_restart_min_framework");
        SLOGD("Just triggered restart_min_framework\n");

        /* OK, the framework is restarted and will soon be showing a
         * progress bar.  Time to setup an encrypted mapping, and
         * either write a new filesystem, or encrypt in place updating
         * the progress bar as we work.
         */
    }
    ...
```

#### 创建device mapper设备

利用device-mapper机制创建一个device mapper设备，其实device mapper是一个驱动，
叫md（mapped device之意）; 为这个device mapper设备设置一个crypt虚拟项。这
个crypt虚拟项会和待加密设备（real_blkdev）关联起来.crypto_blkdev是上面device mapper
创建的一个虚拟项设备，命名方式为/dev/block/dm-xxx。xxx为device mapper的minor版本号。
设备加解密工作就是在这个crypto_blkdev读写过程中完成的。以后我们挂载这个
crypto_blkdev到/data分区就可以了。当从这个设备读的时候，它会从底层关联的
real_blkdev读取加密数据，然后解密传递给读者。当往这个设备写的时候，它会加密后
再写到real_blkdev中

```
    decrypt_master_key(passwd, decrypted_master_key, &crypt_ftr, 0, 0);
    create_crypto_blk_dev(&crypt_ftr, decrypted_master_key, real_blkdev, crypto_blkdev,
                          "userdata");

    /* If we are continuing, check checksums match */
    rc = 0;
    if (previously_encrypted_upto) {
        __le8 hash_first_block[SHA256_DIGEST_LENGTH];
        rc = cryptfs_SHA256_fileblock(crypto_blkdev, hash_first_block);

        if (!rc && memcmp(hash_first_block, crypt_ftr.hash_first_block,
                          sizeof(hash_first_block)) != 0) {
            SLOGE("Checksums do not match - trigger wipe");
            rc = -1;
        }
    }
```

##### create_crypto_blk_dev

#### 加密

加密数据。方法很简单，从real_blkdev读取数据，然后写到crypto_blkdev中。
我们先从real_blkdev中读取未加密的数据，从存储空间第一块位置开始读取.
然后写到crypto_bldev中。它会先加密，然后对应写回到real_blkdev第一块位置.

```
    if (!rc) {
        rc = cryptfs_enable_all_volumes(&crypt_ftr, how,
                                        crypto_blkdev, real_blkdev,
                                        previously_encrypted_upto);
    }

    /* Calculate checksum if we are not finished */
    if (!rc && crypt_ftr.encrypted_upto != crypt_ftr.fs_size) {
        rc = cryptfs_SHA256_fileblock(crypto_blkdev,
                                      crypt_ftr.hash_first_block);
        if (rc) {
            SLOGE("Error calculating checksum for continuing encryption");
            rc = -1;
        }
    }
```

#### 后期处理

```
    ...
    /* Undo the dm-crypt mapping whether we succeed or not */
    delete_crypto_blk_dev("userdata");

    free(vol_list);

    if (! rc) {
        /* Success */
        crypt_ftr.flags &= ~CRYPT_INCONSISTENT_STATE;

        if (crypt_ftr.encrypted_upto != crypt_ftr.fs_size) {
            SLOGD("Encrypted up to sector %lld - will continue after reboot",
                  crypt_ftr.encrypted_upto);
            crypt_ftr.flags |= CRYPT_ENCRYPTION_IN_PROGRESS;
        }

        if (how == CRYPTO_ENABLE_INPLACE)
            crypt_ftr.flags |= CRYPT_FDE_COMPLETED;
        put_crypt_ftr_and_key(&crypt_ftr);

        if (crypt_ftr.encrypted_upto == crypt_ftr.fs_size) {
          char value[PROPERTY_VALUE_MAX];
          property_get("ro.crypto.state", value, "");
          if (!strcmp(value, "")) {
            /* default encryption - continue first boot sequence */
            property_set("ro.crypto.state", "encrypted");
            release_wake_lock(lockid);
            cryptfs_check_passwd(DEFAULT_PASSWORD);
            cryptfs_restart_internal(1);
            return 0;
          } else {
            sleep(2); /* Give the UI a chance to show 100% progress */
            cryptfs_reboot(reboot);
          }
        } else {
            sleep(2); /* Partially encrypted, ensure writes flushed to ssd */
            cryptfs_reboot(shutdown);
        }
    } else {
        char value[PROPERTY_VALUE_MAX];

        property_get("ro.vold.wipe_on_crypt_fail", value, "0");
        if (!strcmp(value, "1")) {
            /* wipe data if encryption failed */
            SLOGE("encryption failed - rebooting into recovery to wipe data\n");
            mkdir("/cache/recovery", 0700);
            int fd = open("/cache/recovery/command", O_RDWR|O_CREAT|O_TRUNC, 0600);
            if (fd >= 0) {
                write(fd, "--wipe_data\n", strlen("--wipe_data\n") + 1);
                write(fd, "--reason=cryptfs_enable_internal\n", strlen("--reason=cryptfs_enable_internal\n") + 1);
                close(fd);
            } else {
                SLOGE("could not open /cache/recovery/command\n");
            }
            cryptfs_reboot(recovery);
        } else {
            /* set property to trigger dialog */
            property_set("vold.encrypt_progress", "error_partially_encrypted");
            release_wake_lock(lockid);
        }
        return -1;
    }

    /* hrm, the encrypt step claims success, but the reboot failed.
     * This should not happen.
     * Set the property and return.  Hope the framework can deal with it.
     */
    property_set("vold.encrypt_progress", "error_reboot_failed");
    release_wake_lock(lockid);
    ...
```