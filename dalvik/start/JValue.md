JValue
========================================

JValue union结构体定义如下所示:

path: dalvik/vm/Common.h
```
union JValue {
#if defined(HAVE_LITTLE_ENDIAN)
    u1      z;
    s1      b;
    u2      c;
    s2      s;
    s4      i;
    s8      j;
    float   f;
    double  d;
    Object* l;
#endif
#if defined(HAVE_BIG_ENDIAN)
    struct {
        u1    _z[3];
        u1    z;
    };
    struct {
        s1    _b[3];
        s1    b;
    };
    struct {
        u2    _c;
        u2    c;
    };
    struct {
        s2    _s;
        s2    s;
    };
    s4      i;
    s8      j;
    float   f;
    double  d;
    void*   l;
#endif
};
```