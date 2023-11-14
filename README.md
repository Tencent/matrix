![Matrix-icon](assets/img/readme/header.png)
[![license](http://img.shields.io/badge/license-BSD3-brightgreen.svg?style=flat)](https://github.com/Tencent/matrix/blob/master/LICENSE)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/Tencent/matrix/pulls)
[![WeChat Approved](https://img.shields.io/badge/Wechat%20Approved-2.1.0-red.svg)](https://github.com/Tencent/matrix/wiki)
[![CircleCI](https://circleci.com/gh/Tencent/matrix.svg?style=shield)](https://app.circleci.com/pipelines/github/Tencent/matrix)

(中文版本请参看[这里](#matrix_cn))  

[Matrix for iOS/macOS 中文版](#matrix_ios_cn)  
[Matrix for android 中文版](#matrix_android_cn)   
[Matrix for iOS/macOS](#matrix_ios_en)   
[Matrix for android](#matrix_android_en)   

**Matrix** is an APM (Application Performance Manage) used in Wechat to monitor, locate and analyse performance problems. It is a **plugin style**, **non-invasive** solution and is currently available on iOS, macOS and Android.

# <a name='matrix_ios_en'> Matrix for iOS/macOS </a>

The monitoring scope of the current tool includes: crash, lag, and memory, which includes the following three plugins:

* **WCCrashBlockMonitorPlugin：** Based on [KSCrash](https://github.com/kstenerud/KSCrash) framework, it features cutting-edge lag stack capture capabilities with crash capture.

* **WCMemoryStatPlugin：** A memory monitoring tool that captures memory allocation and the callstack of an application's memory event.

* **WCFPSMonitorPlugin：** A fps monitoring tool that captures main thread's callstack while user scrolling.

## Features

#### WCCrashBlockMonitorPlugin

* Easy integration, no code intrusion.
* Determine whether the app is stuck by checking the running status of the Runloop, and support both the iOS and macOS platform.
* Add time-consuming stack fetching, attaching the most time-consuming main thread stack to the thread snapshot log.

#### WCMemoryStatPlugin

* Live recording every object's creating and the corresponding callstack of its creation, and report it when the application out-of-memory is detected.

## Getting Started
#### Install

* **Install with static framework**
  1. Get source code of Matrix;
  2. Open terminal, execute `make` in the `matrix/matrix-iOS` directory to compile and generate static library. After compiling, the iOS platform library is in the `matrix/matrix-iOS/build_ios` directory, and the macOS platform library is in the `matrix/matrix-iOS/build_macos` directory.
  3. Link with static framework in the project:
    * iOS : Use `Matrix.framework` under the `matrix/matrix-iOS/build_ios` path, link `Matrix.framework` to the project as a static library;
    * macOS : Use `Matrix.framework` under the `matrix/matrix-iOS/build_macos` path, link `Matrix.framework` to the project as a static library.
  4. Add `#import <Matrix/Matrix.h>`, then you can use the performance probe tool of WeChat.

#### Start the plugins

In the following places:

* Program `main` function;
* `application:didFinishLaunchingWithOptions:` of  `AppDelegate`;
* Or other places running as earlier as possible after application launching.

Add a code similar to the following to start the plugin:

```objective-c
#import <Matrix/Matrix.h>
  
Matrix *matrix = [Matrix sharedInstance];
MatrixBuilder *curBuilder = [[MatrixBuilder alloc] init];
curBuilder.pluginListener = self; // get the related event of plugin via the callback of the pluginListener
    
WCCrashBlockMonitorPlugin *crashBlockPlugin = [[WCCrashBlockMonitorPlugin alloc] init];    
[curBuilder addPlugin:crashBlockPlugin]; // add lag and crash monitor.
    
WCMemoryStatPlugin *memoryStatPlugin = [[WCMemoryStatPlugin alloc] init];
[curBuilder addPlugin:memoryStatPlugin]; // add memory monitor.

WCFPSMonitorPlugin *fpsMonitorPlugin = [[WCFPSMonitorPlugin alloc] init];
[curBuilder addPlugin:fpsMonitorPlugin]; // add fps monitor.
    
[matrix addMatrixBuilder:curBuilder];
    
[crashBlockPlugin start]; // start the lag and crash monitor.
[memoryStatPlugin start]; // start memory monitor
[fpsMonitorPlugin start]; // start fps monitor
```

#### Receive callbacks to obtain monitoring data

Set `pluginListener` of the `MatrixBuilder` object, implement the `MatrixPluginListenerDelegate`

```objective-c
// set delegate

MatrixBuilder *curBuilder = [[MatrixBuilder alloc] init];
curBuilder.pluginListener = <object conforms to MatrixPluginListenerDelegate>; 

// MatrixPluginListenerDelegate

- (void)onInit:(id<MatrixPluginProtocol>)plugin;
- (void)onStart:(id<MatrixPluginProtocol>)plugin;
- (void)onStop:(id<MatrixPluginProtocol>)plugin;
- (void)onDestroy:(id<MatrixPluginProtocol>)plugin;
- (void)onReportIssue:(MatrixIssue *)issue;
```

Each plugin added to `MatrixBuilder` will call back the corresponding event via `pluginListener`.

**Important: Get the monitoring data of the Matrix via `onReportIssue:`, the data format info reference to [Matrix for iOS/macOS Data Format Description](https://github.com/Tencent/matrix/wiki/Matrix-for-iOS-macOS-Data-Format-Description)**

## Tutorials

At this point, Matrix has been integrated into the app and is beginning to collect crash, lag, and memory data. If you still have questions, check out the example: `samples/sample-iOS/MatrixDemo`.



# <a name='matrix_android_en'> Matrix for android </a>

## Plugins

- **APK Checker:**

  Analyse the APK package, give suggestions of reducing the APK's size; Compare two APK and find out the most significant increment on size

- **Resource Canary:**

  Detect the activity leak and bitmap duplication basing on WeakReference and [Square Haha](https://github.com/square/haha) 

- **Trace Canary:**

  FPS Monitor, Startup Performance, ANR, UI-Block / Slow Method Detection

- **SQLite Lint:**

  Evaluate the quality of SQLite statement automatically by using SQLite official tools

- **IO Canary:**

  Detect the file IO issues, including performance of file IO and closeable leak 

- **Battery Canary:**

  App thread activities monitor (Background watch & foreground loop watch), Sonsor usage monitor (WakeLock/Alarm/Gps/Wifi/Bluetooth), Background network activities (Wifi/Mobile) monitor.
  
- **MemGuard**

  Detect heap memory overlap, use-after-free and double free issues.


## Features
#### APK Checker

- **Easy-to-use.** Matrix provides a JAR tool, which is more convenient to apply to your integration systems. 
- **More features.** In addition to APK Analyzer, Matrix find out the R redundancies, the dynamic libraries statically linked STL, unused resources, and supports custom checking rules.
- **Visual Outputs.** supports HTML and JSON outputs.

#### Resource Canary

- **Separated detection and analysis.** Make possible to use in automated test and in release versions (monitor only).
- **Pruned Hprof.** Remove the useless data in hprof and easier to upload.
- **Detection of duplicate bitmap.** 

#### Trace Canary

- **High performance.** Dynamically modify bytecode at compile time, record function cost and call stack with little performance loss.
- **Accurate call stack of ui-block.** Provide informations such as call stack, function cost, execution times to solve the problem of ui-block quickly.
- **Non-hack.** High compatibility to Android versions.
- **More features.** Automatically covers multiple fluency indicators such as ui-block, startup time, activity switching, slow function detection.
- **High-accuracy ANR detector.**  Detect ANRs accurately and give ANR trace file with high compatibility and high stability.

#### SQLite Lint

- **Easy-to-use.** Non-invasive.
- **High applicability.** Regardless of the amount of data, you can discover SQLite performance problems during development and testing.
- **High standards.** Detection algorithms based on best practices, make SQLite statements to the highest quality.
- **May support multi-platform.** Implementing in C++ makes it possible to support multi-platform.

#### IO Canary
- **Easy-to-use.** Non-invasive.
- **More feature.** Including performance of file IO and closeable leak.
- **Compatible with Android P.**

#### Battery Canary
- **Easy-to-use.** Use out of box (unit tests as example).
- **More feature.** Flexible extending with base and utils APIs.

#### Memory Hook

- A native memory leak detection tool for Android.
- **Non-invasive.** It is based on PLT-hook([iqiyi/xHook](https://github.com/iqiyi/xHook)), so we do NOT need to recompile the native libraries.
- **High performance.** we use WeChat-Backtrace for fast unwinding which supports both aarch64 and armeabi-v7a architectures.

#### Pthread Hook

- A Java and native thread leak detection and native thread stack space trimming tool for Android.
- **Non-invasive.** It is based on PLT-hook([iqiyi/xHook](https://github.com/iqiyi/xHook)), so we do NOT need to recompile the native libraries.
- It saves virtual memory overhead by trimming default stack size of native thread in half, which can reduce crashes caused by virtual memory insufficient under 32bit environment.

#### WVPreAllocHook

+ A tool for saving virtual memory overhead caused by WebView preloading when WebView is not actually used. It's useful for reducing crashes caused by virtual memory insufficient under 32bit environment.
+ **Non-invasive.** It is based on PLT-hook([iqiyi/xHook](https://github.com/iqiyi/xHook)), so we do NOT need to recompile the native libraries.
+ WebView still works after using this tool.

#### MemGuard

+ A tool base on GWP-Asan to detect heap memory issues.
+ **Non-invasive.** It is based on PLT-hook([iqiyi/xHook](https://github.com/iqiyi/xHook)), so we do NOT need to recompile the native libraries.
+ It's able to apply on specific libraries that needs to be detected by RegEx.

+ It detects heap memory accessing overlap, use-after-free and double free issues.


#### Backtrace Component

- A fast native backtrace component designed by Matrix based on quicken unwind tables that are generated and simplified from DWARF and ARM exception handling informations. It is about 15x ~ 30x faster than libunwindstack.


## Getting Started
***The JCenter repository will stop service on February 1, 2022. So we uploaded Matrix(since 0.8.0) to the MavenCentral repository.***

1. Configure `MATRIX_VERSION` in gradle.properties.
``` gradle
  MATRIX_VERSION=2.1.0
```

2. Add `matrix-gradle-plugin` in your build.gradle:
``` gradle 
  dependencies {
      classpath ("com.tencent.matrix:matrix-gradle-plugin:${MATRIX_VERSION}") { changing = true }
  }
 
```
3. Add dependencies to your app/build.gradle.

``` gradle
  dependencies {
    implementation group: "com.tencent.matrix", name: "matrix-android-lib", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-android-commons", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-trace-canary", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-resource-canary-android", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-resource-canary-common", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-io-canary", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-sqlite-lint-android-sdk", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-battery-canary", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-hooks", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-backtrace", version: MATRIX_VERSION, changing: true
  }
  
  apply plugin: 'com.tencent.matrix-plugin'
  matrix {
    trace {
        enable = true	//if you don't want to use trace canary, set false
        baseMethodMapFile = "${project.buildDir}/matrix_output/Debug.methodmap"
        blackListFile = "${project.projectDir}/matrixTrace/blackMethodList.txt"
    }
  }
```

4. Implement `PluginListener` to receive data processed by Matrix.

``` java
  public class TestPluginListener extends DefaultPluginListener {
    public static final String TAG = "Matrix.TestPluginListener";
    public TestPluginListener(Context context) {
        super(context);
        
    }

    @Override
    public void onReportIssue(Issue issue) {
        super.onReportIssue(issue);
        MatrixLog.e(TAG, issue.toString());
        
        //add your code to process data
    }
}
```

Matrix gradle plugin could work with Android Gradle Plugin 3.5.0/4.0.0/4.1.0 currently.

5. Implement `DynamicConfig` to change parameters of Matrix.
``` java
  public class DynamicConfigImplDemo implements IDynamicConfig {
    public DynamicConfigImplDemo() {}

    public boolean isFPSEnable() { return true;}
    public boolean isTraceEnable() { return true; }
    public boolean isMatrixEnable() { return true; }
    public boolean isDumpHprof() {  return false;}

    @Override
    public String get(String key, String defStr) {
        //hook to change default values
    }

    @Override
    public int get(String key, int defInt) {
      //hook to change default values
    }

    @Override
    public long get(String key, long defLong) {
        //hook to change default values
    }

    @Override
    public boolean get(String key, boolean defBool) {
        //hook to change default values
    }

    @Override
    public float get(String key, float defFloat) {
        //hook to change default values
    }
}
```

6. Init Matrix in the ```onCreate``` of your application. 

``` java 
  Matrix.Builder builder = new Matrix.Builder(application); // build matrix
  builder.patchListener(new TestPluginListener(this)); // add general pluginListener
  DynamicConfigImplDemo dynamicConfig = new DynamicConfigImplDemo(); // dynamic config
  
  // init plugin 
  IOCanaryPlugin ioCanaryPlugin = new IOCanaryPlugin(new IOConfig.Builder()
                    .dynamicConfig(dynamicConfig)
                    .build());
  //add to matrix               
  builder.plugin(ioCanaryPlugin);
  
  //init matrix
  Matrix.init(builder.build());

  // start plugin 
  ioCanaryPlugin.start();
```
For more Matrix configurations, look at the [sample](https://github.com/Tencent/matrix/tree/dev/samples/sample-android).

Note:
1. Since Matrix for Android has migrated to AndroidX since v0.9.0. You may need to add 'android.useAndroidX=true' flag to gradle.properties
2. You can get more about Matrix output at the wiki [The output of Matrix](https://github.com/Tencent/matrix/wiki/Matrix-Android--data-format); 


#### Battery Canary Usage

Init BatteryCanary as the following codes:
```java
BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
        .enable(JiffiesMonitorFeature.class)
        .enableStatPidProc(true)
        .greyJiffiesTime(30 * 1000L)
        .setCallback(new BatteryMonitorCallback.BatteryPrinter())
        .build();

BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
```

For detail usage, please reference showcase tests at `com.tencent.matrix.batterycanary.ApisTest` or `sample.tencent.matrix.battery.BatteryCanaryInitHelper`.

#### Backtrace Component Usage

How to init backtrace component：
```java
WeChatBacktrace.instance().configure(getApplicationContext()).commit();
```

Then other components in Matrix could use Quikcen Backtrace to unwind stacktrace. See more configuration comments in 'WeChatBacktrace.Configuration'.

#### APK Checker Usage

APK Checker can run independently in Jar ([matrix-apk-canary-2.1.0.jar](https://repo.maven.apache.org/maven2/com/tencent/matrix/matrix-apk-canary/2.1.0/matrix-apk-canary-2.1.0.jar)）  mode, usage:


```shell
java -jar matrix-apk-canary-2.1.0.jar
Usages: 
    --config CONFIG-FILE-PATH
or
    [--input INPUT-DIR-PATH] [--apk APK-FILE-PATH] [--unzip APK-UNZIP-PATH] [--mappingTxt MAPPING-FILE-PATH] [--resMappingTxt RESGUARD-MAPPING-FILE-PATH] [--output OUTPUT-PATH] [--format OUTPUT-FORMAT] [--formatJar OUTPUT-FORMAT-JAR] [--formatConfig OUTPUT-FORMAT-CONFIG (json-array format)] [Options]
    
Options:
-manifest
     Read package info from the AndroidManifest.xml.
-fileSize [--min DOWN-LIMIT-SIZE (KB)] [--order ORDER-BY ('asc'|'desc')] [--suffix FILTER-SUFFIX-LIST (split by ',')]
     Show files whose size exceed limit size in order.
-countMethod [--group GROUP-BY ('class'|'package')]
     Count methods in dex file, output results group by class name or package name.
-checkResProguard
     Check if the resguard was applied.
-findNonAlphaPng [--min DOWN-LIMIT-SIZE (KB)]
     Find out the non-alpha png-format files whose size exceed limit size in desc order.
-checkMultiLibrary
     Check if there are more than one library dir in the 'lib'.
-uncompressedFile [--suffix FILTER-SUFFIX-LIST (split by ',')]
     Show uncompressed file types.
-countR
     Count the R class.
-duplicatedFile
     Find out the duplicated resource files in desc order.
-checkMultiSTL  --toolnm TOOL-NM-PATH
     Check if there are more than one shared library statically linked the STL.
-unusedResources --rTxt R-TXT-FILE-PATH [--ignoreResources IGNORE-RESOURCES-LIST (split by ',')]
     Find out the unused resources.
-unusedAssets [--ignoreAssets IGNORE-ASSETS-LIST (split by ',')]
     Find out the unused assets file.
-unstrippedSo  --toolnm TOOL-NM-PATH
     Find out the unstripped shared library file.
```

Learn more about [Matrix-APKChecker](https://github.com/Tencent/matrix/wiki/Matrix-Android-ApkChecker) 


# Support

Any problem?

1. Learn more from [Sample](https://github.com/Tencent/matrix/tree/master/samples/)
2. [Source Code](https://github.com/Tencent/matrix/tree/master/matrix)
3. [Wiki](https://github.com/Tencent/matrix/wiki) & [FAQ](https://github.com/Tencent/Matrix/wiki/Matrix-%E5%B8%B8%E8%A7%81%E9%97%AE%E9%A2%98)
4. Contact us for help

# Contributing

If you are interested in contributing, check out the [CONTRIBUTING.md](https://github.com/Tencent/Matrix/blob/master/CONTRIBUTING.md), also join our [Tencent OpenSource Plan](https://opensource.tencent.com/contribution).

# License

Matrix is under the BSD license. See the [LICENSE](https://github.com/Tencent/Matrix/blob/master/LICENSE) file for details

------

# <a name="matrix_cn">Matrix</a>
![Matrix-icon](assets/img/readme/header.png)
[![license](http://img.shields.io/badge/license-BSD3-brightgreen.svg?style=flat)](https://github.com/Tencent/matrix/blob/master/LICENSE)[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/Tencent/matrix/pulls)  [![WeChat Approved](https://img.shields.io/badge/Wechat%20Approved-2.1.0-red.svg)](https://github.com/Tencent/matrix/wiki)

**Matrix** 是一款微信研发并日常使用的应用性能接入框架，支持iOS, macOS和Android。
Matrix 通过接入各种性能监控方案，对性能监控项的异常数据进行采集和分析，输出相应的问题分析、定位与优化建议，从而帮助开发者开发出更高质量的应用。

# <a name='matrix_ios_cn'>Matrix for iOS/macOS </a>

Matrix-iOS 当前工具监控范围包括：崩溃、卡顿和内存，包含以下三款插件：

* **WCCrashBlockMonitorPlugin：** 基于 [KSCrash](https://github.com/kstenerud/KSCrash) 框架开发，具有业界领先的卡顿堆栈捕获能力，同时兼备崩溃捕获能力

* **WCMemoryStatPlugin：** 一款性能极致的内存监控工具，能够全面捕获应用 OOM 时的内存分配以及调用堆栈情况

* **WCFPSMonitorPlugin：** 一款 FPS 监控工具，当用户滑动界面时，记录主线程调用栈

## 特性

#### WCCrashBlockMonitorPlugin

* 接入简单，代码无侵入
* 通过检查 Runloop 运行状态判断应用是否卡顿，同时支持 iOS/macOS 平台
* 增加耗时堆栈提取，卡顿线程快照日志中附加最近时间最耗时的主线程堆栈

#### WCMemoryStatPlugin

* 在应用运行期间获取对象存活以及相应的堆栈信息，在检测到应用 OOM 时进行上报

## 使用方法

#### 安装

* **通过静态库安装**
  1. 获取 Matrix 源码；
  2. 打开命令行，在 `matrix/matrix-iOS` 代码目录下执行 `make` 进行编译生成静态库；编译完成后，iOS 平台的库在 `matrix/matrix-iOS/build_ios` 目录下，macOS 平台的库在 `matrix/matrix-iOS/build_macos` 目录下；
  3. 工程引入静态库：
    * iOS 平台：使用 `matrix/matrix-iOS/build_ios` 路径下的 `Matrix.framework`，将 `Matrix.framework` 以静态库的方式引入工程；
    * macOS 平台：使用 `matrix/matrix-iOS/build_macos` 路径下的 `Matrix.framework`，将 `Matrix.framework` 以静态库的方式引入工程。
  4. 添加头文件 `#import <Matrix/Matrix.h>`，就可以接入微信的性能探针工具了！


#### 启动监控

在以下地方：

* 程序 `main` 函数入口；
* `AppDelegate` 中的 `application:didFinishLaunchingWithOptions:`；
* 或者其他应用启动比较早的时间点。

添加类似如下代码，启动插件：

```
#import <Matrix/Matrix.h>
  
Matrix *matrix = [Matrix sharedInstance];
MatrixBuilder *curBuilder = [[MatrixBuilder alloc] init];
curBuilder.pluginListener = self; // pluginListener 回调 plugin 的相关事件
    
WCCrashBlockMonitorPlugin *crashBlockPlugin = [[WCCrashBlockMonitorPlugin alloc] init];    
[curBuilder addPlugin:crashBlockPlugin]; // 添加卡顿和崩溃监控
    
WCMemoryStatPlugin *memoryStatPlugin = [[WCMemoryStatPlugin alloc] init];
[curBuilder addPlugin:memoryStatPlugin]; // 添加内存监控功能

WCFPSMonitorPlugin *fpsMonitorPlugin = [[WCFPSMonitorPlugin alloc] init];
[curBuilder addPlugin:fpsMonitorPlugin]; // 添加 fps 监控功能
    
[matrix addMatrixBuilder:curBuilder];
    
[crashBlockPlugin start]; // 开启卡顿和崩溃监控
[memoryStatPlugin start]; // 开启内存监控
[fpsMonitorPlugin start]; // 开启 fps 监控
```

#### 接收回调获得监控数据

设置 MatrixBuilder 对象中的 pluginListener，实现 MatrixPluginListenerDelegate。

```
// 设置 delegate

MatrixBuilder *curBuilder = [[MatrixBuilder alloc] init];
curBuilder.pluginListener = <一个遵循 MatrixPluginListenerDelegate 的对象>; 

// MatrixPluginListenerDelegate

- (void)onInit:(id<MatrixPluginProtocol>)plugin;
- (void)onStart:(id<MatrixPluginProtocol>)plugin;
- (void)onStop:(id<MatrixPluginProtocol>)plugin;
- (void)onDestroy:(id<MatrixPluginProtocol>)plugin;
- (void)onReportIssue:(MatrixIssue *)issue;
```

各个添加到 MatrixBuilder 的 plugin 会将对应的事件通过 pluginListener 回调。

**重要：通过 `onReportIssue:` 获得 Matrix 处理后的数据，监控数据格式详见：[Matrix for iOS/macOS 数据格式说明](https://github.com/Tencent/matrix/wiki/Matrix-for-iOS-macOS-%E6%95%B0%E6%8D%AE%E6%A0%BC%E5%BC%8F%E8%AF%B4%E6%98%8E)**

## Demo

至此，Matrix 已经集成到应用中并且开始收集崩溃、卡顿和爆内存数据，如仍有疑问，请查看示例：`samples/sample-iOS/MatrixDemo`


# <a name='matrix_android_cn'>Matrix for Android </a>

Matrix-android 当前监控范围包括：应用安装包大小，帧率变化，启动耗时，卡顿，慢方法，SQLite 操作优化，文件读写，内存泄漏等等。
- APK Checker:
  针对 APK 安装包的分析检测工具，根据一系列设定好的规则，检测 APK 是否存在特定的问题，并输出较为详细的检测结果报告，用于分析排查问题以及版本追踪
  
- Resource Canary:
  基于 WeakReference 的特性和 [Square Haha](https://github.com/square/haha) 库开发的 Activity 泄漏和 Bitmap 重复创建检测工具
  
- Trace Canary:
  监控ANR、界面流畅性、启动耗时、页面切换耗时、慢函数及卡顿等问题
  
- SQLite Lint:
  按官方最佳实践自动化检测 SQLite 语句的使用质量
  
- IO Canary:
  检测文件 IO 问题，包括：文件 IO 监控和 Closeable Leak 监控
  
- Battery Canary:
  监控 App 活跃线程（待机状态 & 前台 Loop 监控）、ASM 调用 (WakeLock/Alarm/Gps/Wifi/Bluetooth 等传感器)、 后台流量 (Wifi/移动网络)等 Battery Historian 统计 App 耗电的数据
  
- MemGuard

  检测堆内存访问越界、使用释放后的内存、重复释放等问题

## 特性

与常规的 APM 工具相比，Matrix 拥有以下特点：

#### APK Checker

- 具有更好的可用性：JAR 包方式提供，更方便应用到持续集成系统中，从而追踪和对比每个 APK 版本之间的变化
- 更多的检查分析功能：除具备 APKAnalyzer 的功能外，还支持统计 APK 中包含的 R 类、检查是否有多个动态库静态链接了 STL 、搜索 APK 中包含的无用资源，以及支持自定义检查规则等
- 输出的检查结果更加详实：支持可视化的 HTML 格式，便于分析处理的 JSON ，自定义输出等等

#### Resource Canary

- 分离了检测和分析部分，便于在不打断自动化测试的前提下持续输出分析后的检测结果
- 对检测部分生成的 Hprof 文件进行了裁剪，移除了大部分无用数据，降低了传输 Hprof 文件的开销
- 增加了重复 Bitmap 对象检测，方便通过减少冗余 Bitmap 数量，降低内存消耗

#### Trace Canary

- 编译期动态修改字节码, 高性能记录执行耗时与调用堆栈
- 准确的定位到发生卡顿的函数，提供执行堆栈、执行耗时、执行次数等信息，帮助快速解决卡顿问题
- 自动涵盖卡顿、启动耗时、页面切换、慢函数检测等多个流畅性指标
- 准确监控ANR，并且能够高兼容性和稳定性地保存系统产生的ANR Trace文件

#### SQLite Lint

- 接入简单，代码无侵入
- 数据量无关，开发、测试阶段即可发现SQLite性能隐患
- 检测算法基于最佳实践，高标准把控SQLite质量*
- 底层是 C++ 实现，支持多平台扩展

#### IO Canary

- 接入简单，代码无侵入
- 性能、泄漏全面监控，对 IO 质量心中有数
- 兼容到 Android P

#### Battery Canary

- 接入简单，开箱即用
- 预留 Base 类和 Utility 工具以便扩展监控特性

#### Memory Hook

- 一个检测 Android native 内存泄漏的工具
- 无侵入，基于 PLT-hook([iqiyi/xHook](https://github.com/iqiyi/xHook))，无需重编 native 库
- 高性能，基于 Wechat-Backtrace 进行快速 unwind 堆栈，支持 aarch64 和 armeabi-v7a 架构

#### Pthread Hook

- 一个检测 Android Java 和 native 线程泄漏及缩减 native 线程栈空间的工具
- 无侵入，基于 PLT-hook([iqiyi/xHook](https://github.com/iqiyi/xHook))，无需重编 native 库
- 通过对 native 线程的默认栈大小进行减半降低线程带来的虚拟内存开销，在 32 位环境下可缓解虚拟内存不足导致的崩溃问题

#### WVPreAllocHook

+ 一个用于安全释放 WebView 预分配内存以在不加载 WebView 时节省虚拟内存的工具，在 32 位环境下可缓解虚拟内存不足导致的崩溃问题
+ 无侵入，基于 PLT-hook([iqiyi/xHook](https://github.com/iqiyi/xHook))，无需重编 native 库
+ 使用该工具后 WebView 仍可正常工作

#### MemGuard

+ 一个基于 GWP-Asan 修改的堆内存问题检测工具
+ 无侵入，基于 PLT-hook([iqiyi/xHook](https://github.com/iqiyi/xHook))，无需重编 native 库
+ 可根据正则表达式指定被检测的目标库
+ 可检测堆内存访问越界、使用释放后的内存和双重释放等问题

#### Backtrace Component
- 基于 DWARF 以及 ARM 异常处理数据进行简化并生成全新的 quicken unwind tables 数据，用于实现可快速回溯 native 调用栈的 backtrace 组件。回溯速度约是 libunwindstack 的 15x ~ 30x 左右。

## 使用方法

***由于 JCenter 服务将于 2022 年 2 月 1 日下线，我们已将 Matrix 新版本（>= 0.8.0) maven repo 发布至 MavenCentral。***

1. 在你项目根目录下的 gradle.properties 中配置要依赖的 Matrix 版本号，如：
``` gradle
  MATRIX_VERSION=2.1.0
```

2. 在你项目根目录下的 build.gradle 文件添加 Matrix 依赖，如：
``` gradle 
  dependencies {
      classpath ("com.tencent.matrix:matrix-gradle-plugin:${MATRIX_VERSION}") { changing = true }
  }
```

3. 接着，在 app/build.gradle 文件中添加 Matrix 各模块的依赖，如：
``` gradle
  dependencies {
    implementation group: "com.tencent.matrix", name: "matrix-android-lib", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-android-commons", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-trace-canary", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-resource-canary-android", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-resource-canary-common", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-io-canary", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-sqlite-lint-android-sdk", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-battery-canary", version: MATRIX_VERSION, changing: true
    implementation group: "com.tencent.matrix", name: "matrix-hooks", version: MATRIX_VERSION, changing: true
  }

  apply plugin: 'com.tencent.matrix-plugin'
  matrix {
    trace {
        enable = true	//if you don't want to use trace canary, set false
        baseMethodMapFile = "${project.buildDir}/matrix_output/Debug.methodmap"
        blackListFile = "${project.projectDir}/matrixTrace/blackMethodList.txt"
    }
  }
```

目前 Matrix gradle plugin 支持 Android Gradle Plugin 3.5.0/4.0.0/4.1.0。

4. 实现 PluginListener，接收 Matrix 处理后的数据, 如：
``` java
  public class TestPluginListener extends DefaultPluginListener {
    public static final String TAG = "Matrix.TestPluginListener";
    public TestPluginListener(Context context) {
        super(context);
        
    }

    @Override
    public void onReportIssue(Issue issue) {
        super.onReportIssue(issue);
        MatrixLog.e(TAG, issue.toString());
        
        //add your code to process data
    }
}
```

5. 实现动态配置接口， 可修改 Matrix 内部参数. 在 sample-android 中 我们有个简单的动态接口实例[DynamicConfigImplDemo.java](samples/sample-android/app/src/main/java/sample/tencent/matrix/config/DynamicConfigImplDemo.java), 其中参数对应的 key 位于文件 [MatrixEnum](samples/sample-android/app/src/main/java/sample/tencent/matrix/config/MatrixEnum.java)中， 摘抄部分示例如下：
``` java
  public class DynamicConfigImplDemo implements IDynamicConfig {
    public DynamicConfigImplDemo() {}

    public boolean isFPSEnable() { return true;}
    public boolean isTraceEnable() { return true; }
    public boolean isMatrixEnable() { return true; }
    public boolean isDumpHprof() {  return false;}

    @Override
    public String get(String key, String defStr) {
        //hook to change default values
    }

    @Override
    public int get(String key, int defInt) {
         //hook to change default values
    }

    @Override
    public long get(String key, long defLong) {
        //hook to change default values
    }

    @Override
    public boolean get(String key, boolean defBool) {
        //hook to change default values
    }

    @Override
    public float get(String key, float defFloat) {
        //hook to change default values
    }
}

```
6. 选择程序启动的位置对 Matrix 进行初始化，如在 Application 的继承类中， Init 核心逻辑如下：
``` java 
  Matrix.Builder builder = new Matrix.Builder(application); // build matrix
  builder.pluginListener(new TestPluginListener(this)); // add general pluginListener
  DynamicConfigImplDemo dynamicConfig = new DynamicConfigImplDemo(); // dynamic config
  
  // init plugin 
  IOCanaryPlugin ioCanaryPlugin = new IOCanaryPlugin(new IOConfig.Builder()
                    .dynamicConfig(dynamicConfig)
                    .build());
  //add to matrix               
  builder.plugin(ioCanaryPlugin);
  
  //init matrix
  Matrix.init(builder.build());

  // start plugin 
  ioCanaryPlugin.start();
```

至此，Matrix就已成功集成到你的项目中，并且开始收集和分析性能相关异常数据，如仍有疑问，请查看 [示例](https://github.com/Tencent/Matrix/tree/dev/samples/sample-android/).

PS：
1. 从 v0.9.0 开始，Matrix for Android 迁移到了 AndroidX. 你可能需要添加 'android.useAndroidX=true' 标志到 gradle.properties 文件里。
2. Matrix 分析后的输出字段的含义请查看 [Matrix 输出内容的含义解析](https://github.com/Tencent/matrix/wiki/Matrix-Android--data-format)


#### Battery Canary Usage

相关初始化代码如下：
```java
BatteryMonitorConfig config = new BatteryMonitorConfig.Builder()
        .enable(JiffiesMonitorFeature.class)
        .enableStatPidProc(true)
        .greyJiffiesTime(30 * 1000L)
        .setCallback(new BatteryMonitorCallback.BatteryPrinter())
        .build();

BatteryMonitorPlugin plugin = new BatteryMonitorPlugin(config);
```

具体使用方式，请参考单元测试里相关用例的代码： `com.tencent.matrix.batterycanary.ApisTest` 或 `sample.tencent.matrix.battery.BatteryCanaryInitHelper`.

#### Backtrace Component Usage

如何初始化 backtrace 组件：
```java
WeChatBacktrace.instance().configure(getApplicationContext()).commit();
```

初始化后其他 Matrix 组件就可以使用 Quicken Backtrace 进行回溯。更多参数的配置请查看 WeChatBacktrace.Configuration 的接口注释。

#### APK Checker

APK Check 以独立的 jar 包提供 ([matrix-apk-canary-2.1.0.jar](https://repo.maven.apache.org/maven2/com/tencent/matrix/matrix-apk-canary/2.1.0/matrix-apk-canary-2.1.0.jar)），你可以运行：

```cmd
java -jar matrix-apk-canary-2.1.0.jar
```

查看 Usages 来使用它。

```
Usages: 
    --config CONFIG-FILE-PATH
or
    [--input INPUT-DIR-PATH] [--apk APK-FILE-PATH] [--unzip APK-UNZIP-PATH] [--mappingTxt MAPPING-FILE-PATH] [--resMappingTxt RESGUARD-MAPPING-FILE-PATH] [--output OUTPUT-PATH] [--format OUTPUT-FORMAT] [--formatJar OUTPUT-FORMAT-JAR] [--formatConfig OUTPUT-FORMAT-CONFIG (json-array format)] [Options]
    
Options:
-manifest
     Read package info from the AndroidManifest.xml.
-fileSize [--min DOWN-LIMIT-SIZE (KB)] [--order ORDER-BY ('asc'|'desc')] [--suffix FILTER-SUFFIX-LIST (split by ',')]
     Show files whose size exceed limit size in order.
-countMethod [--group GROUP-BY ('class'|'package')]
     Count methods in dex file, output results group by class name or package name.
-checkResProguard
     Check if the resguard was applied.
-findNonAlphaPng [--min DOWN-LIMIT-SIZE (KB)]
     Find out the non-alpha png-format files whose size exceed limit size in desc order.
-checkMultiLibrary
     Check if there are more than one library dir in the 'lib'.
-uncompressedFile [--suffix FILTER-SUFFIX-LIST (split by ',')]
     Show uncompressed file types.
-countR
     Count the R class.
-duplicatedFile
     Find out the duplicated resource files in desc order.
-checkMultiSTL  --toolnm TOOL-NM-PATH
     Check if there are more than one shared library statically linked the STL.
-unusedResources --rTxt R-TXT-FILE-PATH [--ignoreResources IGNORE-RESOURCES-LIST (split by ',')]
     Find out the unused resources.
-unusedAssets [--ignoreAssets IGNORE-ASSETS-LIST (split by ',')]
     Find out the unused assets file.
-unstrippedSo  --toolnm TOOL-NM-PATH
     Find out the unstripped shared library file.
```

由于篇幅影响，此次不再赘述，我们在 [Matrix-APKChecker](https://github.com/Tencent/matrix/wiki/Matrix-Android-ApkChecker) 中进行了详细说明。


# Support

还有其他问题？

1. 查看[示例](https://github.com/Tencent/matrix/tree/master/samples)；
2. 阅读[源码](https://github.com/Tencent/matrix/tree/master/matrix)；
3. 阅读 [Wiki](https://github.com/Tencent/matrix/wiki) 或 [FAQ](https://github.com/Tencent/Matrix/wiki/Matrix-%E5%B8%B8%E8%A7%81%E9%97%AE%E9%A2%98)；
4. 联系我们。

# 参与贡献

关于 Matrix 分支管理、issue 以及 pr 规范，请阅读 [Matrix Contributing Guide](https://github.com/Tencent/Matrix/blob/master/CONTRIBUTING.md)。

[腾讯开源激励计划](https://opensource.tencent.com/contribution) 鼓励开发者的参与和贡献，期待你的加入。

# License

Matrix is under the BSD license. See the [LICENSE](https://github.com/Tencent/Matrix/blob/master/LICENSE) file for details

# 信息公示

- SDK名称：Matrix
- 版本号：2.1.0
- 开发者：深圳市腾讯计算机系统有限公司
- 主要功能：Matrix是微信研发并日常使用的应用性能监控工具，支持iOS、macOS和Android。Matrix通过接入闪退、卡顿、耗电、内存等方面的监控方案，对性能监控项的异常数据进行采集和分析，输出相应的问题分析、定位与优化建议，从而帮助开发者开发出更高质量的应用。
- [Matrix SDK使用说明](https://github.com/Tencent/matrix)
- [Matrix SDK个人信息保护规则](https://support.weixin.qq.com/cgi-bin/mmsupportacctnodeweb-bin/pages/yTezupX6yF028Mpf)
