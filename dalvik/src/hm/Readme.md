hm sample
========================================

下面我们使用hm这个简单的例子来学习Android虚拟机的工作原理.

usage
----------------------------------------

```
$ adb root && adb remount
$ adb push hm.jar /system/frameworks/
$ adb push hm /system/bin/
$ adb shell
# hm
```

hm脚本如下
----------------------------------------

```
# Script to start "hm" on the device, which has a very rudimentary
# shell.
#
base=/system
export CLASSPATH=$base/framework/hm.jar
exec app_process $base/bin com.android.commands.hm.Hm "$@"
```

hm脚本在运行过程需要依赖于hm.jar包,hm.jar包生成过程如下所示:

hm.jar的生成过程
----------------------------------------

* https://github.com/leeminghao/about-android/blob/master/dalvik/src/hm/build.md

app_process执行Hm类的过程
----------------------------------------

从hm脚本可以看出android是直接调用app_process来加载Hm类来执行的，
app_process的具体实现过程如下所示：

path: frameworks/base/cmds/app_process/app_main.cpp
```
```