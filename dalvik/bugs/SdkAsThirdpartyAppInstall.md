安装sdk重启后发生FC的原因是：
========================================

1. 当 sdk安装时，data@app@com.miui.sdk-N.apk@classes.dex会被生成
2. PackageManagerService 会删除data下的cache文件
    当miui.apk安装后，会引起其他一些apk更新，导致android会删除所有第三方的cache。见PackageManagerService.java
    由于这个原因，sdk的cache被删除
3. 当dexopt加载一个apk的时候，dexopt会加载bootclasspath下的cache。这时会发现sdk的cache不存在，于是，
    dexopt就是重现生成 sdk的cache。(见代码dvmJarFileOpen调用dvmOpenCachedDexFile)。这时给dvmOpenCachedDexFile的
参数createIfMissing=true。
    由于sdk的cache不存在，因此，fdState.st_size == 0，导致dvmOpenCachedDexFile调用dexOptCreateEmptyHeader
生成一个空的文件头，空的文件头就是用0xff填充文件头40字节部分。
4. dexopt调用dvmOptimizeDexFile生成cache。该函数发现gDvm.optimizing= true,就会退出，并打出"Rejecting recursive optimization attempt on %s"的信息

因此，sdk的cache就变成了文件头用0xff填充的无效文件。


这里的关键需要理解dexopt与zygote等进程区分。
1. dexopt进程只负责生成cache文件，它不仅负责生成第三方的cache，也包括bootclasspath内部的文件；
2. zygote进程启动时，他会依据bootclasspath的列表顺序，依次调用dexopt来生成cache，这时候是通过dvmOptimizeDexFile来产生一个dexopt进程；
3. dexopt进程也需要事先加载bootclasspath，但是由于被生成的对象，可能就是bootclasspath内部的，因此dexopt的策略是加载所有能够加载的cache为止，如果加载不上，就忽略；
4. dexopt另外调用dvmContinueOptimization来生成目标cache。

因为dexopt调用了dvmOptimizeDexFile函数，而该函数又生成了dexopt进程，为了防止递归调用，dexopt使用gDvm.optimizing= true来防止这种情况的发生。

这就是为什么会出现numDeps=23呢？ 因为dexopt在加载bootclasspath的过程中，发现com.miui.sdk文件不存在或者错误，但是在dexopt进程内，不允许调用dvmOptimizeDexFiile函数，
因此，导致bootclasspath解析到com.miui.sdk后就失败停止，而dexopt又忽略了该错误，后续的加载也都失败。因此，dexopt就使用之前的23个作为有效依赖，写入到了cache文件中。

在分析第一次启动时的情况：
1. 当zygote启动时，因为包括com.miui.sdk在内的前24个都是正确的，可以顺利加载，而从第25给开始，cache重新生成；
2. pm server启动后，发现bootclasspath又更新，所以删除所有的data@app开头的cache
3. 当继续加载某个apk时，发现该apk的cache内的依赖关系与bootclasspath不匹配，因此启动dexopt，生成该apk的cache
4. dexopt在加载bootclasspath的时候，发现com.miui.sdk.的cache不存在或者错误，就试图生成sdk的cache，结果就填充了文件头，在正式生成的时候，就失败被忽略了。

这里有个细节需要注意：第3步和第4步都会加载bootclasspath的cache，但是第3步没有试图生成sdk的cache，这是因为，虽然这两部都是使用同一个函数dvmJarFileOpen
来加载，但是第3步传递的参数createIfMissing为false,第4步为true。

虽然com.miui.sdk的cache是错误的，但是不妨碍dexopt更新所有的依赖关系，所以，当第二次重启时，zynote进程重新检查和生成了sdk的cache，却不会引起
其他bootclasspath的更新，也就不会引起第三方的cache更新，一切也就趋于正常了。

较好的修改方法是，在pm server中，排除对com.miui.sdk的cache的删除。
