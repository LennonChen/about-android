cryptfs_init_crypt_mnt_ftr
========================================

path: system/vold/cryptfs.c
```
/* Initialize a crypt_mnt_ftr structure.  The keysize is
 * defaulted to 16 bytes, and the filesystem size to 0.
 * Presumably, at a minimum, the caller will update the
 * filesystem size and crypto_type_name after calling this function.
 */
static int cryptfs_init_crypt_mnt_ftr(struct crypt_mnt_ftr *ftr)
{
    off64_t off;

    /* 1.初始化实例. */
    memset(ftr, 0, sizeof(struct crypt_mnt_ftr));
    ftr->magic = CRYPT_MNT_MAGIC;
    ftr->major_version = CURRENT_MAJOR_VERSION;
    ftr->minor_version = CURRENT_MINOR_VERSION;
    ftr->ftr_size = sizeof(struct crypt_mnt_ftr);
    ftr->keysize = KEY_LEN_BYTES;

    switch (keymaster_check_compatibility()) {
    case 1:
        ftr->kdf_type = KDF_SCRYPT_KEYMASTER;
        break;

    case 0:
        ftr->kdf_type = KDF_SCRYPT;
        break;

    default:
        SLOGE("keymaster_check_compatibility failed");
        return -1;
    }

    get_device_scrypt_params(ftr);

    ftr->persist_data_size = CRYPT_PERSIST_DATA_SIZE;
    if (get_crypt_ftr_info(NULL, &off) == 0) {
        ftr->persist_data_offset[0] = off + CRYPT_FOOTER_TO_PERSIST_OFFSET;
        ftr->persist_data_offset[1] = off + CRYPT_FOOTER_TO_PERSIST_OFFSET +
                                    ftr->persist_data_size;
    }

    return 0;
}
```

keymaster_check_compatibility
----------------------------------------

path: system/vold/cryptfs.c
```
/* Should we use keymaster? */
static int keymaster_check_compatibility()
{
    keymaster_device_t *keymaster_dev = 0;
    int rc = 0;

    if (keymaster_init(&keymaster_dev)) {
        SLOGE("Failed to init keymaster");
        rc = -1;
        goto out;
    }

    SLOGI("keymaster version is %d", keymaster_dev->common.module->module_api_version);

    if (keymaster_dev->common.module->module_api_version
            < KEYMASTER_MODULE_API_VERSION_0_3) {
        rc = 0;
        goto out;
    }

    if (keymaster_dev->flags & KEYMASTER_BLOBS_ARE_STANDALONE) {
        rc = 1;
    }

out:
    keymaster_close(keymaster_dev);
    return rc;
}
```

keymaster_init
----------------------------------------

path: system/vold/cryptfs.c
```
static int keymaster_init(keymaster_device_t **keymaster_dev)
{
    int rc;

    const hw_module_t* mod;
    rc = hw_get_module_by_class(KEYSTORE_HARDWARE_MODULE_ID, NULL, &mod);
    if (rc) {
        ALOGE("could not find any keystore module");
        goto out;
    }

    rc = keymaster_open(mod, keymaster_dev);
    if (rc) {
        ALOGE("could not open keymaster device in %s (%s)",
            KEYSTORE_HARDWARE_MODULE_ID, strerror(-rc));
        goto out;
    }

    return 0;

out:
    *keymaster_dev = NULL;
    return rc;
}
```