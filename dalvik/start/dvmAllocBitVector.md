dvmAllocBitVector
========================================

path: dalvik/vm/Thread.cpp
```
#define kMaxThreadId        ((1 << 16) - 1)   // 65535
#define kMainThreadId       1
...
gDvm.threadIdMap = dvmAllocBitVector(kMaxThreadId, false);
```

path: dalvik/vm/Globals.h
```
    /*
     * Thread ID bitmap.  We want threads to have small integer IDs so
     * we can use them in "thin locks".
     */
    BitVector*  threadIdMap;
```

BitVector的结构如下所示:

path: dalvik/vm/BitVector.h
```
/*
 * Expanding bitmap, used for tracking resources.  Bits are numbered starting
 * from zero.
 *
 * All operations on a BitVector are unsynchronized.
 */
struct BitVector {
    bool    expandable;     /* expand bitmap if we run out? */
    u4      storageSize;    /* current size, in 32-bit words */
    u4*     storage;
};
```

path: dalvik/vm/BitVector.cpp
```
/*
 * Allocate a bit vector with enough space to hold at least the specified
 * number of bits.
 */
BitVector* dvmAllocBitVector(unsigned int startBits, bool expandable)
{
    BitVector* bv;
    unsigned int count;

    assert(sizeof(bv->storage[0]) == 4);        /* assuming 32-bit units */

    bv = (BitVector*) malloc(sizeof(BitVector));

    count = (startBits + 31) >> 5; // (65535 + 31) >> 5 ==> 2048

    bv->storageSize = count;
    bv->expandable = expandable;
    bv->storage = (u4*) calloc(count, sizeof(u4));
    return bv;
}
```