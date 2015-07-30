dvmClassStartup
========================================

path: dalvik/vm/oo/Class.cpp
```
/*
 * Initialize the bootstrap class loader.
 *
 * Call this after the bootclasspath string has been finalized.
 */
bool dvmClassStartup()
{
    /* make this a requirement -- don't currently support dirs in path */
    if (strcmp(gDvm.bootClassPathStr, ".") == 0) {
        ALOGE("ERROR: must specify non-'.' bootclasspath");
        return false;
    }

    gDvm.loadedClasses =
        dvmHashTableCreate(256, (HashFreeFunc) dvmFreeClassInnards);

    gDvm.pBootLoaderAlloc = dvmLinearAllocCreate(NULL);
    if (gDvm.pBootLoaderAlloc == NULL)
        return false;

    if (false) {
        linearAllocTests();
        exit(0);
    }

    /*
     * Class serial number.  We start with a high value to make it distinct
     * in binary dumps (e.g. hprof).
     */
    gDvm.classSerialNumber = INITIAL_CLASS_SERIAL_NUMBER;

    /*
     * Set up the table we'll use for tracking initiating loaders for
     * early classes.
     * If it's NULL, we just fall back to the InitiatingLoaderList in the
     * ClassObject, so it's not fatal to fail this allocation.
     */
    gDvm.initiatingLoaderList = (InitiatingLoaderList*)
        calloc(ZYGOTE_CLASS_CUTOFF, sizeof(InitiatingLoaderList));

    /*
     * Create the initial classes. These are the first objects constructed
     * within the nascent VM.
     */
    if (!createInitialClasses()) {
        return false;
    }

    /*
     * Process the bootstrap class path.  This means opening the specified
     * DEX or Jar files and possibly running them through the optimizer.
     */
    assert(gDvm.bootClassPath == NULL);
    processClassPath(gDvm.bootClassPathStr, true);

    if (gDvm.bootClassPath == NULL)
        return false;

    return true;
}
```

检查gDvm.bootClassPathStr是否合法:
----------------------------------------

gDvm.bootClassPathStr的初始化如下所示:

1. 在setCommandLineDefaults函数中通过环境变量BOOTCLASSPATH来初始化的.

https://github.com/leeminghao/about-android/blob/master/dalvik/start/setCommandLineDefaults.md

2. 在processOptions中通过参数选项"-Xbootclasspath:", "-Xbootclasspath/a:" 和
"-Xbootclasspath/p:"来进行设置, 具体实现如下所示：

https://github.com/leeminghao/about-android/blob/master/dalvik/start/processOptions.md

检查gDvm.bootClassPathStr是否等于当前路径".", 如果等于则出错返回.

为gDvm.loadedClasses HashTable分配空间:
----------------------------------------

gDvm.loadedClasses是一个Hash表，用来保存对应类名所对应的ClassObject对象的.

```
    gDvm.loadedClasses =
        dvmHashTableCreate(256, (HashFreeFunc) dvmFreeClassInnards);
```

为gDvm.pBootLoaderAlloc分配空间:
----------------------------------------

```
    gDvm.pBootLoaderAlloc = dvmLinearAllocCreate(NULL);
    if (gDvm.pBootLoaderAlloc == NULL)
        return false;
```

接下来初始化gDvm.classSerialNumber和gDvm.initiatingLoaderList,初始化完毕之后
调用createInitialClasses函数来创建类"Ljava/lang/Class". 具体实现如下所示:

createInitialClasses
----------------------------------------

path: dalvik/vm/oo/Class.cpp
```
/*
 * Create the initial class instances. These consist of the class
 * Class and all of the classes representing primitive types.
 */
static bool createInitialClasses() {
    /*
     * Initialize the class Class. This has to be done specially, particularly
     * because it is an instance of itself.
     */
    ClassObject* clazz = (ClassObject*)
        dvmMalloc(classObjectSize(CLASS_SFIELD_SLOTS), ALLOC_NON_MOVING);
    if (clazz == NULL) {
        return false;
    }
    DVM_OBJECT_INIT(clazz, clazz);
    SET_CLASS_FLAG(clazz, ACC_PUBLIC | ACC_FINAL | CLASS_ISCLASS);
    clazz->descriptor = "Ljava/lang/Class;";
    gDvm.classJavaLangClass = clazz;
    LOGVV("Constructed the class Class.");

    /*
     * Initialize the classes representing primitive types. These are
     * instances of the class Class, but other than that they're fairly
     * different from regular classes.
     */
    bool ok = true;
    ok &= createPrimitiveType(PRIM_VOID,    &gDvm.typeVoid);
    ok &= createPrimitiveType(PRIM_BOOLEAN, &gDvm.typeBoolean);
    ok &= createPrimitiveType(PRIM_BYTE,    &gDvm.typeByte);
    ok &= createPrimitiveType(PRIM_SHORT,   &gDvm.typeShort);
    ok &= createPrimitiveType(PRIM_CHAR,    &gDvm.typeChar);
    ok &= createPrimitiveType(PRIM_INT,     &gDvm.typeInt);
    ok &= createPrimitiveType(PRIM_LONG,    &gDvm.typeLong);
    ok &= createPrimitiveType(PRIM_FLOAT,   &gDvm.typeFloat);
    ok &= createPrimitiveType(PRIM_DOUBLE,  &gDvm.typeDouble);

    // Add barrier to force all metadata writes to main memory to complete
    ANDROID_MEMBAR_FULL();

    return ok;
}
```

### 1.创建一个ClassObject的结构体clazz.

A.设置clazz的descriptor为"Ljava/lang/Class".

B.将clazz赋值给gDvm.classJavaLangClass.

### 2.调用createPrimitiveType函数创建数据类型的类结构.

#### PrimitiveType

path: dalvik/libdex/DexFile.h
```
/*
 * Enumeration of all the primitive types.
 */
enum PrimitiveType {
    PRIM_NOT        = 0,       /* value is a reference type, not a primitive type */
    PRIM_VOID       = 1,
    PRIM_BOOLEAN    = 2,
    PRIM_BYTE       = 3,
    PRIM_SHORT      = 4,
    PRIM_CHAR       = 5,
    PRIM_INT        = 6,
    PRIM_LONG       = 7,
    PRIM_FLOAT      = 8,
    PRIM_DOUBLE     = 9,
};
```

枚举类型PrimitiveType声明的变量分别对应于gDvm中的ClassObject:

path: dalvik/vm/Globals.h
```
    /* synthetic classes representing primitive types */
    ClassObject* typeVoid;
    ClassObject* typeBoolean;
    ClassObject* typeByte;
    ClassObject* typeShort;
    ClassObject* typeChar;
    ClassObject* typeInt;
    ClassObject* typeLong;
    ClassObject* typeFloat;
    ClassObject* typeDouble;
```

#### createPrimitiveType

path: dalvik/vm/oo/Class.cpp
```
/*
 * Synthesize a primitive class.
 *
 * Just creates the class and returns it (does not add it to the class list).
 */
static bool createPrimitiveType(PrimitiveType primitiveType, ClassObject** pClass)
{
    /*
     * Fill out a few fields in the ClassObject.
     *
     * Note that primitive classes do not sub-class the class Object.
     * This matters for "instanceof" checks. Also, we assume that the
     * primitive class does not override finalize().
     */

    const char* descriptor = dexGetPrimitiveTypeDescriptor(primitiveType);
    assert(descriptor != NULL);

    ClassObject* newClass = (ClassObject*) dvmMalloc(sizeof(*newClass), ALLOC_NON_MOVING);
    if (newClass == NULL) {
        return false;
    }

    DVM_OBJECT_INIT(newClass, gDvm.classJavaLangClass);
    dvmSetClassSerialNumber(newClass);
    SET_CLASS_FLAG(newClass, ACC_PUBLIC | ACC_FINAL | ACC_ABSTRACT);
    newClass->primitiveType = primitiveType;
    newClass->descriptorAlloc = NULL;
    newClass->descriptor = descriptor;
    newClass->super = NULL;
    newClass->status = CLASS_INITIALIZED;

    /* don't need to set newClass->objectSize */

    LOGVV("Constructed class for primitive type '%s'", newClass->descriptor);

    *pClass = newClass;
    dvmReleaseTrackedAlloc((Object*) newClass, NULL);

    // Add barrier to force all metadata writes to main memory to complete
    ANDROID_MEMBAR_FULL();

    return true;
}
```

##### dexGetPrimitiveTypeDescriptor

通过传递进来的primitiveType调用函数dexGetPrimitiveTypeDescriptor函数获取对应
Type的字符串表示:

path: dalvik/libdex/DexFile.cpp
```
/* (documented in header) */
const char* dexGetPrimitiveTypeDescriptor(PrimitiveType type) {
    switch (type) {
        case PRIM_VOID:    return "V";
        case PRIM_BOOLEAN: return "Z";
        case PRIM_BYTE:    return "B";
        case PRIM_SHORT:   return "S";
        case PRIM_CHAR:    return "C";
        case PRIM_INT:     return "I";
        case PRIM_LONG:    return "J";
        case PRIM_FLOAT:   return "F";
        case PRIM_DOUBLE:  return "D";
        default:           return NULL;
    }

    return NULL;
}
```

##### 新建一个ClassObject变量newClass

##### DVM_OBJECT_INIT


processClassPath
----------------------------------------

```
/*
 * Convert a colon-separated list of directories, Zip files, and DEX files
 * into an array of ClassPathEntry structs.
 *
 * During normal startup we fail if there are no entries, because we won't
 * get very far without the basic language support classes, but if we're
 * optimizing a DEX file we allow it.
 *
 * If entries are added or removed from the bootstrap class path, the
 * dependencies in the DEX files will break, and everything except the
 * very first entry will need to be regenerated.
 */
static ClassPathEntry* processClassPath(const char* pathStr, bool isBootstrap)
{
    ClassPathEntry* cpe = NULL;
    char* mangle;
    char* cp;
    const char* end;
    int idx, count;

    assert(pathStr != NULL);

    mangle = strdup(pathStr);

    /*
     * Run through and essentially strtok() the string.  Get a count of
     * the #of elements while we're at it.
     *
     * If the path was constructed strangely (e.g. ":foo::bar:") this will
     * over-allocate, which isn't ideal but is mostly harmless.
     */
    count = 1;
    for (cp = mangle; *cp != '\0'; cp++) {
        if (*cp == ':') {   /* separates two entries */
            count++;
            *cp = '\0';
        }
    }
    end = cp;

    /*
     * Allocate storage.  We over-alloc by one so we can set an "end" marker.
     */
    cpe = (ClassPathEntry*) calloc(count+1, sizeof(ClassPathEntry));

    /*
     * Set the global pointer so the DEX file dependency stuff can find it.
     */
    gDvm.bootClassPath = cpe;

    /*
     * Go through a second time, pulling stuff out.
     */
    cp = mangle;
    idx = 0;
    while (cp < end) {
        if (*cp == '\0') {
            /* leading, trailing, or doubled ':'; ignore it */
        } else {
            if (isBootstrap &&
                    dvmPathToAbsolutePortion(cp) == NULL) {
                ALOGE("Non-absolute bootclasspath entry '%s'", cp);
                free(cpe);
                cpe = NULL;
                goto bail;
            }

            ClassPathEntry tmp;
            tmp.kind = kCpeUnknown;
            tmp.fileName = strdup(cp);
            tmp.ptr = NULL;

            /*
             * Drop an end marker here so DEX loader can walk unfinished
             * list.
             */
            cpe[idx].kind = kCpeLastEntry;
            cpe[idx].fileName = NULL;
            cpe[idx].ptr = NULL;

            if (!prepareCpe(&tmp, isBootstrap)) {
                /* drop from list and continue on */
                free(tmp.fileName);
            } else {
                /* copy over, pointers and all */
                cpe[idx] = tmp;
                idx++;
            }
        }

        cp += strlen(cp) +1;
    }
    assert(idx <= count);
    if (idx == 0 && !gDvm.optimizing) {
        /*
         * There's no way the vm will be doing anything if this is the
         * case, so just bail out (reasonably) gracefully.
         */
        ALOGE("No valid entries found in bootclasspath '%s'", pathStr);
        gDvm.lastMessage = pathStr;
        dvmAbort();
    }

    LOGVV("  (filled %d of %d slots)", idx, count);

    /* put end marker in over-alloc slot */
    cpe[idx].kind = kCpeLastEntry;
    cpe[idx].fileName = NULL;
    cpe[idx].ptr = NULL;

    //dumpClassPath(cpe);

bail:
    free(mangle);
    gDvm.bootClassPath = cpe;
    return cpe;
}
```
