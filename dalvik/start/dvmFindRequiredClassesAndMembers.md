dvmFindRequiredClassesAndMembers
========================================

path: dalvik/vm/InitRefs.cpp
```
/* (documented in header) */
bool dvmFindRequiredClassesAndMembers() {
    /*
     * Note: Under normal VM use, this is called by dvmStartup()
     * in Init.c. For dex optimization, this is called as well, but in
     * that case, the call is made from DexPrepare.c.
     */

    return initClassReferences()
        && initFieldOffsets()
        && initConstructorReferences()
        && initDirectMethodReferences()
        && initVirtualMethodOffsets()
        && initFinalizerReference()
        && verifyStringOffsets()
        && verifyExtra();
}
```

dvmFindRequiredClassesAndMembers用于初始化一些必须的类和成员

initDirectMethodReferences
----------------------------------------

path: dalvik/vm/InitRefs.cpp
```
static bool initDirectMethodReferences() {
    static struct {
        Method** method;
        const char* className;
        const char* name;
        const char* descriptor;
    } methods[] = {
        { &gDvm.methJavaLangClassLoader_getSystemClassLoader, "Ljava/lang/ClassLoader;",
          "getSystemClassLoader", "()Ljava/lang/ClassLoader;" },
        { &gDvm.methJavaLangReflectProxy_constructorPrototype, "Ljava/lang/reflect/Proxy;",
          "constructorPrototype", "(Ljava/lang/reflect/InvocationHandler;)V" },
        { &gDvm.methJavaLangSystem_runFinalization, "Ljava/lang/System;",
          "runFinalization", "()V" },

        { &gDvm.methodTraceGcMethod, "Ldalvik/system/VMDebug;", "startGC", "()V" },
        { &gDvm.methodTraceClassPrepMethod, "Ldalvik/system/VMDebug;", "startClassPrep", "()V" },
        { &gDvm.methOrgApacheHarmonyLangAnnotationAnnotationFactory_createAnnotation,
          "Llibcore/reflect/AnnotationFactory;", "createAnnotation",
          "(Ljava/lang/Class;[Llibcore/reflect/AnnotationMember;)"
          "Ljava/lang/annotation/Annotation;" },
        { &gDvm.methDalvikSystemNativeStart_main, "Ldalvik/system/NativeStart;", "main", "([Ljava/lang/String;)V" },
        { &gDvm.methDalvikSystemNativeStart_run, "Ldalvik/system/NativeStart;", "run", "()V" },
        { &gDvm.methJavaLangRefFinalizerReferenceAdd,
          "Ljava/lang/ref/FinalizerReference;", "add", "(Ljava/lang/Object;)V" },
        { &gDvm.methDalvikDdmcServer_dispatch,
          "Lorg/apache/harmony/dalvik/ddmc/DdmServer;", "dispatch", "(I[BII)Lorg/apache/harmony/dalvik/ddmc/Chunk;" },
        { &gDvm.methDalvikDdmcServer_broadcast,
          "Lorg/apache/harmony/dalvik/ddmc/DdmServer;", "broadcast", "(I)V" },
        { &gDvm.methJavaLangRefReferenceQueueAdd,
          "Ljava/lang/ref/ReferenceQueue;", "add", "(Ljava/lang/ref/Reference;)V" },
        { NULL, NULL, NULL, NULL }
    };

    int i;
    for (i = 0; methods[i].method != NULL; i++) {
        if (!initDirectMethodReference(methods[i].method, methods[i].className,
                methods[i].name, methods[i].descriptor)) {
            return false;
        }
    }

    return true;
}
```

在initDirectMethodReferences函数中首先静态初始化一些Method及其对应的类名描述符等.
接下来调用initDirectMethodReference重载函数来进行真正的初始化操作.

### Method

Method的定义如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/Method.md

### initDirectMethodReference重载函数

path: dalvik/vm/InitRefs.cpp
```
static bool initDirectMethodReference(Method** pMethod, const char* className,
        const char* name, const char* descriptor) {
    ClassObject* clazz = dvmFindSystemClassNoInit(className);

    if (clazz == NULL) {
        ALOGE("Could not find essential class %s for direct method lookup", className);
        return false;
    }

    return initDirectMethodReferenceByClass(pMethod, clazz, name, descriptor);
}
```

以gDvm.methDalvikSystemNativeStart_main为例我们分析下其初始化过程:

```
{ &gDvm.methDalvikSystemNativeStart_main,
  "Ldalvik/system/NativeStart;",
  "main",
  "([Ljava/lang/String;)V"
},
```

#### dvmFindSystemClassNoInit

dvmFindSystemClassNoInit通过指定的函数名称来创建一个对应的ClassObject对象.

path: dalvik/vm/oo/Class.cpp
```
/*
 * Find the named class (by descriptor), searching for it in the
 * bootclasspath.
 *
 * On failure, this returns NULL with an exception raised.
 */
ClassObject* dvmFindSystemClassNoInit(const char* descriptor)
{
    return findClassNoInit(descriptor, NULL, NULL);
}
```

##### ClassObject

ClassObject的定义如下所示:

https://github.com/leeminghao/about-android/blob/master/dalvik/start/ClassObject.md

##### findClassNoInit

https://github.com/leeminghao/about-android/blob/master/dalvik/start/findClassNoInit.md
