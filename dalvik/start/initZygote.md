initZygote
========================================

path: dalvik/vm/Init.cpp
```
/*
 * Do zygote-mode-only initialization.
 */
static bool initZygote()
{
    /* zygote goes into its own process group */
    setpgid(0,0);

    // See storage config details at http://source.android.com/tech/storage/
    // Create private mount namespace shared by all children
    if (unshare(CLONE_NEWNS) == -1) {
        SLOGE("Failed to unshare(): %s", strerror(errno));
        return -1;
    }

    // Mark rootfs as being a slave so that changes from default
    // namespace only flow into our children.
    if (mount("rootfs", "/", NULL, (MS_SLAVE | MS_REC), NULL) == -1) {
        SLOGE("Failed to mount() rootfs as MS_SLAVE: %s", strerror(errno));
        return -1;
    }

    // Create a staging tmpfs that is shared by our children; they will
    // bind mount storage into their respective private namespaces, which
    // are isolated from each other.
    const char* target_base = getenv("EMULATED_STORAGE_TARGET");
    if (target_base != NULL) {
        if (mount("tmpfs", target_base, "tmpfs", MS_NOSUID | MS_NODEV,
                "uid=0,gid=1028,mode=0751") == -1) {
            SLOGE("Failed to mount tmpfs to %s: %s", target_base, strerror(errno));
            return -1;
        }
    }

    // Mark /system as NOSUID | NODEV
    const char* android_root = getenv("ANDROID_ROOT");

    if (android_root == NULL) {
        SLOGE("environment variable ANDROID_ROOT does not exist?!?!");
        return -1;
    }

    std::string mountDev(getMountsDevDir(android_root));
    if (mountDev.empty()) {
        SLOGE("Unable to find mount point for %s", android_root);
        return -1;
    }

    if (mount(mountDev.c_str(), android_root, "none",
            MS_REMOUNT | MS_NOSUID | MS_NODEV | MS_RDONLY | MS_BIND, NULL) == -1) {
        SLOGE("Remount of %s failed: %s", android_root, strerror(errno));
        return -1;
    }

#ifdef HAVE_ANDROID_OS
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        // Older kernels don't understand PR_SET_NO_NEW_PRIVS and return
        // EINVAL. Don't die on such kernels.
        if (errno != EINVAL) {
            SLOGE("PR_SET_NO_NEW_PRIVS failed: %s", strerror(errno));
            return -1;
        }
    }
#endif

    return true;
}
```