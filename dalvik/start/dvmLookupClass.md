dvmLookupClass
========================================

path: dalvik/vm/oo/Class.cpp
```
/*
 * Search through the hash table to find an entry with a matching descriptor
 * and an initiating class loader that matches "loader".
 *
 * The table entries are hashed on descriptor only, because they're unique
 * on *defining* class loader, not *initiating* class loader.  This isn't
 * great, because it guarantees we will have to probe when multiple
 * class loaders are used.
 *
 * Note this does NOT try to load a class; it just finds a class that
 * has already been loaded.
 *
 * If "unprepOkay" is set, this will return classes that have been added
 * to the hash table but are not yet fully loaded and linked.  Otherwise,
 * such classes are ignored.  (The only place that should set "unprepOkay"
 * is findClassNoInit(), which will wait for the prep to finish.)
 *
 * Returns NULL if not found.
 */
ClassObject* dvmLookupClass(const char* descriptor, Object* loader,
    bool unprepOkay)
{
    ClassMatchCriteria crit;
    void* found;
    u4 hash;

    crit.descriptor = descriptor;
    crit.loader = loader;
    hash = dvmComputeUtf8Hash(descriptor);

    LOGVV("threadid=%d: dvmLookupClass searching for '%s' %p",
        dvmThreadSelf()->threadId, descriptor, loader);

    dvmHashTableLock(gDvm.loadedClasses);
    found = dvmHashTableLookup(gDvm.loadedClasses, hash, &crit,
                hashcmpClassByCrit, false);
    dvmHashTableUnlock(gDvm.loadedClasses);

    /*
     * The class has been added to the hash table but isn't ready for use.
     * We're going to act like we didn't see it, so that the caller will
     * go through the full "find class" path, which includes locking the
     * object and waiting until it's ready.  We could do that lock/wait
     * here, but this is an extremely rare case, and it's simpler to have
     * the wait-for-class code centralized.
     */
    if (found && !unprepOkay && !dvmIsClassLinked((ClassObject*)found)) {
        ALOGV("Ignoring not-yet-ready %s, using slow path",
            ((ClassObject*)found)->descriptor);
        found = NULL;
    }

    return (ClassObject*) found;
}
```

dvmLookupClass通过类名在HashTable gDvm.loadedClasses 中找对应的ClassObject.
具体工作如下所示：

1. 计算对应类名的hash值.

2. 调用函数dvmHashTableLookup来寻找对应的ClassObject.

gDvm.loadedClasses的初始化工作:
----------------------------------------

https://github.com/leeminghao/about-android/blob/master/dalvik/start/dvmClassStartup.md
