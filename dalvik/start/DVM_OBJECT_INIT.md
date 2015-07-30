DVM_OBJECT_INIT
========================================

path: dalvik/vm/oo/Object.h
```
/*
 * Properly initialize an Object.
 * void DVM_OBJECT_INIT(Object *obj, ClassObject *clazz_)
 */
#define DVM_OBJECT_INIT(obj, clazz_) \
    dvmSetFieldObject(obj, OFFSETOF_MEMBER(Object, clazz), clazz_)
```

OFFSETOF_MEMBER
----------------------------------------

OFFSETOF_MEMBER 用于计算f在对应t中的偏移量，字节表示.

path: dalvik/vm/Common.h
```
#define OFFSETOF_MEMBER(t, f)         \
  (reinterpret_cast<char*>(           \
     &reinterpret_cast<t*>(16)->f) -  \
   reinterpret_cast<char*>(16))
```

dvmSetFieldObject
----------------------------------------

DVM_OBJECT_INIT宏调用函数dvmSetFieldObject来进行真正的工作，具体实现如下：

path: dalvik/vm/oo/ObjectInlines.h
```
INLINE void dvmSetFieldObject(Object* obj, int offset, Object* val) {
    JValue* lhs = (JValue*)BYTE_OFFSET(obj, offset);
    lhs->l = val;
    if (val != NULL) {
        dvmWriteBarrierField(obj, &lhs->l);
    }
}
```