
package com.example.outofmemoryerror;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;

import java.util.ArrayList;
import java.util.List;

public class MainActivity extends Activity {
    private final static String TAG = "MainActivity";
    private Button mTestHeapOOM = null;
    private Button mTestStackSOF = null;
    private Button mTestStackOOM = null;
    private Button mTestRuntimeConstantPoolOOM = null;
    private Button mTestMethodAreaOOM = null;
    private Button mTestDirectMemoryOOM = null;
    private static final int _1MB = 1024 * 1024;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        mTestHeapOOM = (Button)findViewById(R.id.test_heap_oom);
        mTestHeapOOM.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                testHeapOOM();
            }
        });

        mTestStackSOF = (Button)findViewById(R.id.test_stack_sof);
        mTestStackSOF.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                testStackSOF();
            }
        });

        mTestStackOOM = (Button)findViewById(R.id.test_stack_oom);
        mTestStackOOM.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                testStackOOM();
            }
        });

        mTestRuntimeConstantPoolOOM = (Button)findViewById(R.id.test_rcp_oom);
        mTestRuntimeConstantPoolOOM.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                testRuntimeConstantPoolOOM();
            }
        });

        /*mTestMethodAreaOOM = (Button)findViewById(R.id.test_method_area_oom);
        mTestMethodAreaOOM.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                testMethodAreaOOM();
            }
        });

        mTestDirectMemoryOOM = (Button)findViewById(R.id.test_direct_memory_oom);
        mTestDirectMemoryOOM.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                testDirectMemoryOOM();
            }
        });*/
    }
    
    static class OOMObject {
    }

    /* Java堆用于储存对象实例，我们只要不断地创建对象，并且保证GC Roots到对象
     * 之间有可达路径来避免垃圾回收机制清除这些对象，就会在对象数量达到最大堆
     * 的容量限制之后产生内存溢出异常. 
     *
     * 现象：Java堆内存的OOM异常是实际应用中最常见的内存溢出异常情况。出现Java
     * 堆内存溢出时，异常堆栈信息："java.lang.OutOfMemoryError"会跟着进一步提示
     * "Java heap space"。
     * dalvikvm-heap: Out of memory ......
     *  AndroidRuntime: java.lang.OutOfMemoryError
     *
     * 解决办法：切换到ddms，选中要分析的应用程序，按下update heap ，再点击
     * dump hprof file就会保存内存分析文件，然后使用MAT进行分析，也就是确认是
     * 内存泄漏(Memory Leak)还是内存溢出(Memory Overflower).
     * 1.如果是内存泄漏：可进一步通过工具MAT查看到泄漏对象到GC Roots的引用链。
     * 于是就能找到泄漏对象是通过怎样的路径与GC Roots相关联导致GC无法自动回收
     * 它们的。
     * 2.如果是内存溢出：换句话说就是内存中的对象还应该活着，那就应当检查虚拟机的
     * 堆参数(-Xmx与-Xms)，与机器物理内存对比看是否可以调大，从代码上检查是否
     * 存在某些对象生命周期过长，持有状态时间过长的情况，尝试减少程序运行期的内存
     * 消耗。
     * 
     * 对应虚拟机选项：
     * -Xms - 决定应用程序进程起始堆大小.
     * -Xmx - 决定应用程序进程最大允许堆大小.
     */
     private void testHeapOOM() {
        List<OOMObject> list = new ArrayList<OOMObject>();
        while (true) {
            list.add(new OOMObject());
        }
    }

     class JavaVMStackSOF {
         private int mStackLength = 1;

         public void stackLeak() {
             mStackLength++;
             stackLeak();
         }
     }

     /* 如果线程请求的栈深度大于虚拟机所允许的最大深度，将抛出StackOverflowError异常
      * 对应虚拟机选项：-Xss - 决定虚拟机栈大小. 
      * 在单个线程下，无论是由于栈帧太大，还是虚拟机栈容量太小，当内存无法分配时
      * 虚拟机抛出的都是StackOverflowError异常.
      * I/dalvikvm(31932): threadid=1: stack overflow on call to Lcom/example/outofmemoryerror/MainActivity$JavaVMStackSOF;.stackLeak:V
      * I/dalvikvm(31932): Shrank stack (to 0x41a29300, curFrame is 0x41a2edac)
      * W/System.err(31932): java.lang.StackOverflowError
      */
     private void testStackSOF() {
         JavaVMStackSOF sof = new JavaVMStackSOF();
         try {
             sof.stackLeak();
         } catch (Throwable e) {
             e.printStackTrace();
         }
     }

     class JavaVMStackOOM {
         private void dontStop() {
             while (true) { }
         }

         public void stackLeak() {
             while (true) {
                 Thread thread = new Thread(new Runnable() {
                     @Override
                     public void run() {
                         dontStop();
                     }
                 });

                 thread.start();
             }
         }
     }

     /* 如果虚拟机在扩展栈时无法申请到足够的内存空间，则抛出OutOfMemoryError异常 
      * 在多线程情况下，给每个线程的栈分配的内存越大，反而越容易产生内存溢出异常.
      */
     private void testStackOOM() {
         JavaVMStackOOM oom = new JavaVMStackOOM();
         oom.stackLeak();
     }

     /* 如果要向运行时常量池添加内容，最简单的做法就是使用String.intern()这个Native
      * 方法。该方法的作用是：如果池中以及给你包含一个等于此String对象的字符串，则
      * 返回代表池中这个字符串的String对象；否则，将次String对象包含的字符串添加到
      * 常量池中，并且返回此String对象的引用。
      * 由于常量池分配在Java堆区(Java堆区在Dalvik中同样作为方法区)
      * 显示这样的信息：
      * dalvikvm-heap: Out of memory on a 38-byte allocation.
      * AndroidRuntime: java.lang.OutOfMemoryError: [memory exhausted]
      */
     private void testRuntimeConstantPoolOOM() {
         List<String> list = new ArrayList<String>();
         int i = 0;
         while (true) {
             list.add(String.valueOf(i++).intern());
         }
     }

     /* 方法区用于存放Class的相关信息，如类名，访问修饰符，常量池，字段描述，
      * 方法描述等。对于这个区域的测试，基本思路是运行时产生大量的类去填满
      * 方法区，直到溢出。在这里是借助CGLib直接操作字节码运行时，生成大量
      * 的动态类。
      * NOTE: This not used in Android, because android not support cglib.
      */
     /*private void testMethodAreaOOM() {
         while (true) {
             Enhancer enhancer = new Enhancer();
             enhancer.setSuperclass(OOMObject.class);
             enhancer.setUseCache(false);
             enhancer.setCallback(new MethodInterceptor() {
                 public Object intercept(Object obj, Method method, 
                         Object[] args, MethodProxy proxy) throws Throwable {
                     return proxy.invokeSuper(obj, args);
                 }
             });
             enhancer.create();
         }
     }*/

     /* 直接通过反射获取Unsafe实例进行内存分配（Unsafe类的getUnsafe()方法限制了只有
      * 引导类加载器才会返回实例，也就是设计者希望只有rt.jar中的类才能使用Unsafe的功能）。
      * 因为，虽然使用DirectByteBuffer分配内存也会抛出内存溢出异常，但它抛出异常时并
      * 没有真正向操作系统申请分配内存，而是通过计算得知内存无法分配，于是手动抛出
      * 异常，真正申请分配内存的方法是unsafe.allocateMemory()。
      * */
     /*private void testDirectMemoryOOM() {
         Field unsafeField = Unsafe.class.getDeclaredFields()[0];
         unsafeField.setAccessible(true);
         Unsafe unsafe = (Unsafe)unsafeField.get(null);
         while (true) {
             unsafe.allocateMemory(_1MB);
         }
     }*/
}
