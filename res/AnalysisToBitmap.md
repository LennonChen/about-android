DumpBitmap
========================================

sources
----------------------------------------

```
/**
 * @hide
 */
public class DumpBitmapInfoUtils {

    static final boolean ENABLE = MiuiFeatureUtils.isSystemFeatureSupported("DumpBitmapInfo", true);
    static int sBitmapThresholdSize;
    static int sCurrProcess;
    static WeakHashMap<Bitmap, CharSequence> sBitmapTitles;

    static {
        if (ENABLE) {
            sBitmapTitles = new WeakHashMap<Bitmap, CharSequence>();
        }
    }

    /**
     * @hide
     */
    static public void putBitmap(Bitmap bmp, CharSequence title) {
        if (!ENABLE) return;

        try {
            if (!isTrackingNeeded(bmp)) return;

            synchronized (sBitmapTitles) {
                sBitmapTitles.put(bmp, title);
            }
        } catch (Throwable ex) {
            ex.printStackTrace();
        }
    }

    /**
     * @hide
     */
    static public void dumpBitmapInfo(FileDescriptor fd, String[] args) {
        if (!ENABLE) return;

        boolean isDumpBitmap = false;
        boolean isExportBitmap = false;
        boolean isSaveToSdcard = false;
        boolean isNoGC = false;
        for (String arg : args) {
            if ("--bitmap".equalsIgnoreCase(arg) || "-b".equalsIgnoreCase(arg)) isDumpBitmap = true;
            if ("--exportbitmap".equalsIgnoreCase(arg) || "-e".equalsIgnoreCase(arg)) isExportBitmap = true;
            if ("--exporttosdcard".equalsIgnoreCase(arg)) isSaveToSdcard = true;
            if ("--nogc".equalsIgnoreCase(arg)) isNoGC = true;
        }
        if (!isDumpBitmap && !isExportBitmap) return;

        if (!isNoGC) {
            System.gc();
        }

        String strExportBitmapFolder = "/sdcard/_exportbitmap/"+ActivityThread.currentPackageName()+"/"+ActivityThread.currentProcessName() + "/";
        File exportBitmapFolder = new File(strExportBitmapFolder);
        if (!isSaveToSdcard && ActivityThread.currentApplication() != null) {
            exportBitmapFolder = new File(ActivityThread.currentApplication().getFilesDir(), "_exportbitmap/"+ActivityThread.currentProcessName());
            strExportBitmapFolder = exportBitmapFolder.getAbsolutePath();
        }
        if (isExportBitmap) {
            if (!exportBitmapFolder.exists()) {
                exportBitmapFolder.mkdirs();
            } else {
                try {
                    libcore.io.IoUtils.deleteContents(exportBitmapFolder);
                } catch (Exception ex) {
                    ex.printStackTrace();
                }
            }
        }

        FileOutputStream fout = new FileOutputStream(fd);
        PrintWriter pw = new FastPrintWriter(fout);
        try {
            long totalSize = 0;
            int bitmapCount = 0;

            ArrayList<Map.Entry<Bitmap, CharSequence>> listBitmapTitles;
            synchronized (sBitmapTitles) {
                listBitmapTitles = new ArrayList<Map.Entry<Bitmap, CharSequence>>();
                for (Map.Entry<Bitmap, CharSequence> entry : sBitmapTitles.entrySet()) {
                    AbstractMap.SimpleEntry<Bitmap, CharSequence> newEntry = new AbstractMap.SimpleEntry<Bitmap, CharSequence>(entry);
                    if (newEntry.getKey() != null) listBitmapTitles.add(newEntry);
                }
            }

            Collections.sort(listBitmapTitles, new Comparator<Map.Entry<Bitmap, CharSequence>>() {
                public int compare(Map.Entry<Bitmap, CharSequence> e1, Map.Entry<Bitmap, CharSequence> e2) {
                    return e1.getKey().getByteCount() - e2.getKey().getByteCount();
                }
            });

            pw.printf("All big bitmaps (debug.bitmap_threshold_size = %d k):\n", sBitmapThresholdSize);
            for (Map.Entry<Bitmap, CharSequence> bmpTitle : listBitmapTitles) {
                Bitmap bmp = bmpTitle.getKey();
                if (bmp.isRecycled()) continue;

                totalSize += bmp.getByteCount();
                bitmapCount++;

                String msg = getBitmapMsg(bmp, bmpTitle.getValue(), false);
                pw.print("  " + msg + "\n");

                if (isExportBitmap) {
                    try {
                        String fileName = bitmapCount + getBitmapMsg(bmp, bmpTitle.getValue(), true);
                        FileOutputStream stream = new FileOutputStream(new File(exportBitmapFolder, fileName));
                        bmp.compress(CompressFormat.PNG, 100, stream);
                        stream.close();
                    } catch (Exception ex) {
                        ex.printStackTrace(pw);
                        ex.printStackTrace();
                    }
                }
            }

            pw.printf("Total count: %d, size: %dM\n", bitmapCount, totalSize/1024/1024);
            if (isExportBitmap) {
                pw.print("Export bitmap. Path: "+strExportBitmapFolder +"\n");
                Log.d("DumpBitmapInfo", "Export bitmaps finished. Path: " + strExportBitmapFolder);
            }
            pw.printf("\n");
        } finally {
            pw.flush();
        }
    }

    static private boolean isTrackingNeeded(Bitmap bmp) {
        if (sCurrProcess != Process.myPid()) {
            sBitmapThresholdSize = SystemProperties.getInt("debug.bitmap_threshold_size", 100);
            sCurrProcess = Process.myPid();
        }

        int size = bmp.getWidth() * bmp.getHeight() / (1024 / 4);
        return size >= sBitmapThresholdSize;
    }

    static private String getBitmapMsg(Bitmap bmp, CharSequence title, boolean forFileName) {
        LongSparseArray<Drawable.ConstantState> preDrawables = (LongSparseArray<Drawable.ConstantState>)Resources.getSystem().getPreloadedDrawables().clone();
        boolean isPreload = false;
        for (int i = 0; i < preDrawables.size(); i++) {
            Drawable.ConstantState c = preDrawables.valueAt(i);
            if (c.getBitmap() == bmp) {
                isPreload = true;
                continue;
            }
        }

        final String msgFormat = "%,7dk %dx%d %s %s";
        String msg = String.format(msgFormat, bmp.getByteCount()/1024,
                bmp.getWidth(), bmp.getHeight(), isPreload ? "preload" : "",
                title == null ? "" : title.toString());

        if (!forFileName) {
            return msg;
        }

        final int maxFileNameLen = 250;
        int overLength = msg.length() - maxFileNameLen;
        if (overLength > 0 && title != null && title.length() > overLength) {
            title = title.toString().substring(overLength);
            msg = String.format(msgFormat, bmp.getByteCount()/1024,
                    bmp.getWidth(), bmp.getHeight(), isPreload ? "preload" : "",
                    title == null ? "" : title.toString());
        }

        return msg.replace(' ', '_').replace('\\', '-').replace('/', '-') + ".png";
    }
}
```

```
@Override
        public void dumpGfxInfo(FileDescriptor fd, String[] args) {
            dumpGraphicsInfo(fd);
            WindowManagerGlobal.getInstance().dumpGfxInfo(fd);
            DumpBitmapInfoUtils.dumpBitmapInfo(fd, args);
        }
```


提供一个分析查看内存中Bitmap信息的方法：adb shell dumpsys gfxinfo [package name] [-b] [-e]

-b会输出所有内存大于阈值（默认100k）的Bitmap信息。
    [内存大小] [宽]x[高] [如果是preload则显示为preload] [图片资源/文件名称，可能是空]
例如，adb shell dumpsys gfxinfo com.miui.home -b：

All big bitmaps (debug.bitmap_threshold_size = 100 k):
    2,477k 1566x405 preload res/drawable-xxhdpi/overscroll_glow.png
    ...
Total count: 138, size: 25M

-e会将Bitmap保存在手机里。
例如，adb shell dumpsys gfxinfo com.miui.home -e：
  [其他的同上，但会多一行]：
Export bitmap. Path: /data/user/0/com.miui.home/files/_exportbitmap/com.miui.home
由于保存Bitmap比较耗时，可能会报错：Failure while dumping the app: ProcessRecord{43294230 1516:com.miui.home/1000}
这没关系，在logcat里等到：Export bitmaps finished. Path: /data/user/0/com.miui.home/files/_exportbitmap/com.miui.home
就表示完成了。然后可以通过adb pull /data/user/0/com.miui.home/files/_exportbitmap/com.miui.home 来取到电脑上查看。

参数 --exporttosdcard 会将图片文件保存到/sdcard/目录下。
参数 --nogc 则不会在dump之前调用一次GC（也就是说，默认情况下会在dump之前触发一次GC）。

可以通过更改prop: debug.bitmap_threshold_size 来更改阈值，默认100，单位K，只有大于等于它的才会显示出来。更改后要杀死进程重新启动app才会生效。
例如：adb shell setprop debug.bitmap_threshold_size 10

新增了一项数据：Bitmap的hashcode，这样可以通过hashcode对应上MAT内存分析的数据。
另外新增了一个参数：--recycle:[Bitmap的hashcode] 通过这个可以recycle指定的Bitmap，来检验某个Bitmap是否会被使用。

Bitmap的内存泄露通过这个方法很容易分析出来。
步骤：
1. adb shell dumpsys gfxinfo [包名] -b 。找出泄露的图片，记下其hashcode。
2. DDMS里Dump HPROF file。打开Histogram > 找到Bitmap > List Object, with outgoing references > 通过上面的hashcode找到对应的Bitmap > Merge Shortest paths to GC Roots, exclude all .... 这样就找到了持有这个Bitmap的根。

另外，如果app发生了OOM，在没关闭FC对话框前，也是可以通过上面的操作分析内存的。

新增功能昨晚进的代码。