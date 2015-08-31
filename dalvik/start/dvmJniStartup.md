dvmJniStartup
========================================

path: dalvik/vm/Jni.cpp
```
bool dvmJniStartup() {
    if (!gDvm.jniGlobalRefTable.init(kGlobalRefsTableInitialSize,
                                 kGlobalRefsTableMaxSize,
                                 kIndirectKindGlobal)) {
        return false;
    }
    if (!gDvm.jniWeakGlobalRefTable.init(kWeakGlobalRefsTableInitialSize,
                                 kGlobalRefsTableMaxSize,
                                 kIndirectKindWeakGlobal)) {
        return false;
    }

    dvmInitMutex(&gDvm.jniGlobalRefLock);
    dvmInitMutex(&gDvm.jniWeakGlobalRefLock);

    if (!dvmInitReferenceTable(&gDvm.jniPinRefTable, kPinTableInitialSize, kPinTableMaxSize)) {
        return false;
    }

    dvmInitMutex(&gDvm.jniPinRefLock);

    return true;
}
```

函数dvmJniStartup用来初始化全局引用表，以及加载一些与Direct Buffer相关的类，如DirectBuffer、
PhantomReference和ReferenceQueue等。我们在一个JNI方法中，可能会需要访问一些Java对象，这样就
需要通知GC，这些Java对象现在正在被Native Code引用，不能回收。这些被Native Code引用的Java对象
就会被记录在一个全局引用表中，具体的做法就是调用JNI环境对象(JNIEnv)的成员函数
NewLocalRef/DeleteLocalRef和NewGlobalRef/DeleteGlobalRef等来显式地引用或者释放Java对象。有时候
我们需要在Java代码中，直接在Native层分配内存，也就直接使用malloc来分配内存。
这些Native内存不同于在Java堆中分配的内存，区别在于前者需要不接受GC管理，而后者接受GC管理。
这些直接在Native层分配的内存有什么用呢？考虑一个场景，我们需要在Java代码中从一个IO设备中
读取数据。从IO设备读取数据意味着要调用由本地操作系统提供的read接口来实现。这样我们就有两种
做法。第一种做法在Native层临时分配一个缓冲区，用来保存从IO设备read回来的数据，然后再将这个
数据拷贝到Java层中去，也就是拷贝到Java堆去使用。第二种做法是在Java层创建一个对象，这个对象
在Native层直接关联有一块内存，从IO设备read回来的数据就直接保存这块内存中。第二种方法和
第一种方法相比，减少了一次内存拷贝，因而可以提高性能。
我们将这种能够直接在Native层中分配内存的Java对象就称为DirectBuffer。由于DirectBuffer使用的
内存是不接受GC管理的，因此，我们就需要通过其它的方式来管理它们。具体做法就是为每一个
DirectBuffer对象创建一个PhantomReference引用。注意，DirectBuffer对象本身是一个Java对象，
它是接受GC管理的。当GC准备回收一个DirectBuffer对象时，如果发现它还有PhantomReference引用，
那就会在回收它之前，把相应的PhantomReference引用加入到与之关联的一个ReferenceQueue队列中去。
这样我们就可以通过判断一个DirectBuffer对象的PhantomReference引用是否已经加入到一个相关的
ReferenceQueue队列中。如果已经加入了的话，那么就可以在该DirectBuffer对象被回收之前，
释放掉之前为它在Native层分配的内存.