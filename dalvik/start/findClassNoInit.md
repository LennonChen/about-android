findClassNoInit
========================================

path: dalvik/vm/oo/Class.cpp
```
/*
 * Find the named class (by descriptor). If it's not already loaded,
 * we load it and link it, but don't execute <clinit>. (The VM has
 * specific limitations on which events can cause initialization.)
 *
 * If "pDexFile" is NULL, we will search the bootclasspath for an entry.
 *
 * On failure, this returns NULL with an exception raised.
 *
 * TODO: we need to return an indication of whether we loaded the class or
 * used an existing definition.  If somebody deliberately tries to load a
 * class twice in the same class loader, they should get a LinkageError,
 * but inadvertent simultaneous class references should "just work".
 */
static ClassObject* findClassNoInit(const char* descriptor, Object* loader,
    DvmDex* pDvmDex)
{
    Thread* self = dvmThreadSelf();
    ClassObject* clazz;
    bool profilerNotified = false;

    if (loader != NULL) {
        LOGVV("#### findClassNoInit(%s,%p,%p)", descriptor, loader,
            pDvmDex->pDexFile);
    }

    /*
     * We don't expect an exception to be raised at this point.  The
     * exception handling code is good about managing this.  This *can*
     * happen if a JNI lookup fails and the JNI code doesn't do any
     * error checking before doing another class lookup, so we may just
     * want to clear this and restore it on exit.  If we don't, some kinds
     * of failures can't be detected without rearranging other stuff.
     *
     * Most often when we hit this situation it means that something is
     * broken in the VM or in JNI code, so I'm keeping it in place (and
     * making it an informative abort rather than an assert).
     */
    if (dvmCheckException(self)) {
        ALOGE("Class lookup %s attempted with exception pending", descriptor);
        ALOGW("Pending exception is:");
        dvmLogExceptionStackTrace();
        dvmDumpAllThreads(false);
        dvmAbort();
    }

    clazz = dvmLookupClass(descriptor, loader, true);
    if (clazz == NULL) {
        const DexClassDef* pClassDef;

        dvmMethodTraceClassPrepBegin();
        profilerNotified = true;

#if LOG_CLASS_LOADING
        u8 startTime = dvmGetThreadCpuTimeNsec();
#endif

        if (pDvmDex == NULL) {
            assert(loader == NULL);     /* shouldn't be here otherwise */
            pDvmDex = searchBootPathForClass(descriptor, &pClassDef);
        } else {
            pClassDef = dexFindClass(pDvmDex->pDexFile, descriptor);
        }

        if (pDvmDex == NULL || pClassDef == NULL) {
            if (gDvm.noClassDefFoundErrorObj != NULL) {
                /* usual case -- use prefabricated object */
                dvmSetException(self, gDvm.noClassDefFoundErrorObj);
            } else {
                /* dexopt case -- can't guarantee prefab (core.jar) */
                dvmThrowNoClassDefFoundError(descriptor);
            }
            goto bail;
        }

        /* found a match, try to load it */
        clazz = loadClassFromDex(pDvmDex, pClassDef, loader);
        if (dvmCheckException(self)) {
            /* class was found but had issues */
            if (clazz != NULL) {
                dvmFreeClassInnards(clazz);
                dvmReleaseTrackedAlloc((Object*) clazz, NULL);
            }
            goto bail;
        }

        /*
         * Lock the class while we link it so other threads must wait for us
         * to finish.  Set the "initThreadId" so we can identify recursive
         * invocation.  (Note all accesses to initThreadId here are
         * guarded by the class object's lock.)
         */
        dvmLockObject(self, (Object*) clazz);
        clazz->initThreadId = self->threadId;

        /*
         * Add to hash table so lookups succeed.
         *
         * [Are circular references possible when linking a class?]
         */
        assert(clazz->classLoader == loader);
        if (!dvmAddClassToHash(clazz)) {
            /*
             * Another thread must have loaded the class after we
             * started but before we finished.  Discard what we've
             * done and leave some hints for the GC.
             *
             * (Yes, this happens.)
             */
            //ALOGW("WOW: somebody loaded %s simultaneously", descriptor);
            clazz->initThreadId = 0;
            dvmUnlockObject(self, (Object*) clazz);

            /* Let the GC free the class.
             */
            dvmFreeClassInnards(clazz);
            dvmReleaseTrackedAlloc((Object*) clazz, NULL);

            /* Grab the winning class.
             */
            clazz = dvmLookupClass(descriptor, loader, true);
            assert(clazz != NULL);
            goto got_class;
        }
        dvmReleaseTrackedAlloc((Object*) clazz, NULL);

#if LOG_CLASS_LOADING
        logClassLoadWithTime('>', clazz, startTime);
#endif
        /*
         * Prepare and resolve.
         */
        if (!dvmLinkClass(clazz)) {
            assert(dvmCheckException(self));

            /* Make note of the error and clean up the class.
             */
            removeClassFromHash(clazz);
            clazz->status = CLASS_ERROR;
            dvmFreeClassInnards(clazz);

            /* Let any waiters know.
             */
            clazz->initThreadId = 0;
            dvmObjectNotifyAll(self, (Object*) clazz);
            dvmUnlockObject(self, (Object*) clazz);

#if LOG_CLASS_LOADING
            ALOG(LOG_INFO, "DVMLINK FAILED FOR CLASS ", "%s in %s",
                clazz->descriptor, get_process_name());

            /*
             * TODO: It would probably be better to use a new type code here (instead of '<') to
             * indicate the failure.  This change would require a matching change in the parser
             * and analysis code in frameworks/base/tools/preload.
             */
            logClassLoad('<', clazz);
#endif
            clazz = NULL;
            if (gDvm.optimizing) {
                /* happens with "external" libs */
                ALOGV("Link of class '%s' failed", descriptor);
            } else {
                ALOGW("Link of class '%s' failed", descriptor);
            }
            goto bail;
        }
        dvmObjectNotifyAll(self, (Object*) clazz);
        dvmUnlockObject(self, (Object*) clazz);

        /*
         * Add class stats to global counters.
         *
         * TODO: these should probably be atomic ops.
         */
        gDvm.numLoadedClasses++;
        gDvm.numDeclaredMethods +=
            clazz->virtualMethodCount + clazz->directMethodCount;
        gDvm.numDeclaredInstFields += clazz->ifieldCount;
        gDvm.numDeclaredStaticFields += clazz->sfieldCount;

        /*
         * Cache pointers to basic classes.  We want to use these in
         * various places, and it's easiest to initialize them on first
         * use rather than trying to force them to initialize (startup
         * ordering makes it weird).
         */
        if (gDvm.classJavaLangObject == NULL &&
            strcmp(descriptor, "Ljava/lang/Object;") == 0)
        {
            /* It should be impossible to get here with anything
             * but the bootclasspath loader.
             */
            assert(loader == NULL);
            gDvm.classJavaLangObject = clazz;
        }

#if LOG_CLASS_LOADING
        logClassLoad('<', clazz);
#endif

    } else {
got_class:
        if (!dvmIsClassLinked(clazz) && clazz->status != CLASS_ERROR) {
            /*
             * We can race with other threads for class linking.  We should
             * never get here recursively; doing so indicates that two
             * classes have circular dependencies.
             *
             * One exception: we force discovery of java.lang.Class in
             * dvmLinkClass(), and Class has Object as its superclass.  So
             * if the first thing we ever load is Object, we will init
             * Object->Class->Object.  The easiest way to avoid this is to
             * ensure that Object is never the first thing we look up, so
             * we get Foo->Class->Object instead.
             */
            dvmLockObject(self, (Object*) clazz);
            if (!dvmIsClassLinked(clazz) &&
                clazz->initThreadId == self->threadId)
            {
                ALOGW("Recursive link on class %s", clazz->descriptor);
                dvmUnlockObject(self, (Object*) clazz);
                dvmThrowClassCircularityError(clazz->descriptor);
                clazz = NULL;
                goto bail;
            }
            //ALOGI("WAITING  for '%s' (owner=%d)",
            //    clazz->descriptor, clazz->initThreadId);
            while (!dvmIsClassLinked(clazz) && clazz->status != CLASS_ERROR) {
                dvmObjectWait(self, (Object*) clazz, 0, 0, false);
            }
            dvmUnlockObject(self, (Object*) clazz);
        }
        if (clazz->status == CLASS_ERROR) {
            /*
             * Somebody else tried to load this and failed.  We need to raise
             * an exception and report failure.
             */
            throwEarlierClassFailure(clazz);
            clazz = NULL;
            goto bail;
        }
    }

    /* check some invariants */
    assert(dvmIsClassLinked(clazz));
    assert(gDvm.classJavaLangClass != NULL);
    assert(clazz->clazz == gDvm.classJavaLangClass);
    assert(dvmIsClassObject(clazz));
    assert(clazz == gDvm.classJavaLangObject || clazz->super != NULL);
    if (!dvmIsInterfaceClass(clazz)) {
        //ALOGI("class=%s vtableCount=%d, virtualMeth=%d",
        //    clazz->descriptor, clazz->vtableCount,
        //    clazz->virtualMethodCount);
        assert(clazz->vtableCount >= clazz->virtualMethodCount);
    }

bail:
    if (profilerNotified)
        dvmMethodTraceClassPrepEnd();
    assert(clazz != NULL || dvmCheckException(self));
    return clazz;
}
```