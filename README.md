##  Matrix
![Matrix-icon](assets/img/readme/header.png)

[![license](http://img.shields.io/badge/license-BSD3-brightgreen.svg?style=flat)](https://github.com/Tencent/matrix/blob/master/LICENSE)[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/Tencent/matrix/pulls)[![WeChat Approved](https://img.shields.io/badge/Wechat%20Approved-0.4.7-red.svg)](https://github.com/Tencent/matrix/wiki)

(中文版本请参看[这里](#matrix_cn))  

**Matrix** is an APM (Application Performance Manage) used in Wechat to monitor, locate and analyse performance problems. It is a **plugin style**, **non-invasive** solution on Android.  It includes:

- **APK Checker:**

  Analyse the APK package, give suggestions of reducing the APK's size; Compare two APK and find out the most significant increment on size

- **Resource Canary:**

  Detect the activity leak and bitmap duplication basing on WeakReference and [Square Haha](https://github.com/square/haha) 

- **Trace Canary:**

  FPS Monitor, Startup Performance, UI-Block / Slow Method Detection

- **SQLite Lint:**

  Evaluate the quality of SQLite statement automatically by using SQLite official tools

- **IO Canary:**

  Detect the file IO issues, including performance of file IO and closeable leak 

## Features
### APK Checker

- **Easy-to-use.** Matrix provides a JAR tool, which is more convenient to apply to your integration systems. 
- **More features.** In addition to APK Analyzer, Matrix find out the R redundancies, the dynamic libraries statically linked STL, unused resources, and supports custom checking rules.
- **Visual Outputs.** supports HTML and JSON outputs.

### Resource Canary

- **Separated detection and analysis.** Make possible to use in automated test and in release versions (monitor only).
- **Pruned Hprof.** Remove the useless data in hprof and easier to upload.
- **Detection of duplicate bitmap.** 

### Trace Canary

- **High performance.** Dynamically modify bytecode at compile time, record function cost and call stack with little performance loss.
- **Accurate call stack of ui-block.** Provide informations such as call stack, function cost, execution times to solve the problem of ui-block quickly.
- **Non-hack.** High compatibility to Android versions.
- **More features.** Automatically covers multiple fluency indicators such as ui-block, startup time, activity switching, slow function detection.

### SQLite Lint

- **Easy-to-use.** Non-invasive.
- **High applicability.** Regardless of the amount of data, you can discover SQLite performance problems during development and testing.
- **High standards.** Detection algorithms based on best practices, make SQLite statements to the highest quality.
- **May support multi-platform.** Implementing in C++ makes it possible to support multi-platform.

### IO Canary
- **Easy-to-use.** Non-invasive.
- **More feature.** Including performance of file IO and closeable leak.
- **Compatible with Android P.**

## Getting Started

1. Configure `MATRIX_VERSION` in gradle.properties.
``` gradle
  MATRIX_VERSION=0.4.10
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

5. Implement `DynamicConfig` to change parametes of Matrix.
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
You can get more about Matrix output at the wiki [The output of Matrix](https://github.com/Tencent/matrix/wiki/Matrix--%E8%BE%93%E5%87%BA%E5%86%85%E5%AE%B9%E7%9A%84%E5%90%AB%E4%B9%89%E8%A7%A3%E6%9E%90); 

### APK Checker Usage

APK Checker can run independently in Jar ([matrix-apk-canary-0.4.10.jar](https://jcenter.bintray.com/com/tencent/matrix/matrix-apk-canary/0.4.10/matrix-apk-canary-0.4.10.jar)）  mode, usage:


```shell
java -jar matrix-apk-canary-0.4.10.jar
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

Learn more about [Matrix-APKChecker](https://github.com/Tencent/matrix/wiki/Matrix-ApkChecker#matrix-apkchecker-%E7%9A%84%E4%BD%BF%E7%94%A8) 


## Support

Any problem?

1. Learn more from [Sample](https://github.com/Tencent/matrix/tree/master/samples/sample-android)
2. [Source Code](https://github.com/Tencent/matrix/tree/master)
3. [Wiki](https://github.com/Tencent/matrix/wiki) & [FAQ](https://github.com/Tencent/Matrix/wiki/Matrix-%E5%B8%B8%E8%A7%81%E9%97%AE%E9%A2%98)
4. Contact us for help

## Contributing

If you are interested in contributing, check out the [CONTRIBUTING.md](https://github.com/Tencent/Matrix/blob/master/CONTRIBUTING.md), also join our [Tencent OpenSource Plan](https://opensource.tencent.com/contribution).

## License

Matrix is under the BSD license. See the [LICENSE](https://github.com/Tencent/Matrix/blob/master/LICENSE) file for details

------

## <a name="matrix_cn">Matrix</a>
![Matrix-icon](assets/img/readme/header.png)
[![license](http://img.shields.io/badge/license-BSD3-brightgreen.svg?style=flat)](https://github.com/Tencent/matrix/blob/master/LICENSE)[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/Tencent/matrix/pulls)  [![WeChat Approved](https://img.shields.io/badge/Wechat%20Approved-0.4.7-red.svg)](https://github.com/Tencent/matrix/wiki)

**Matrix** 是一款微信研发并日常使用的 APM（Application Performance Manage），当前主要运行在 Android 平台上。
Matrix 的目标是建立统一的应用性能接入框架，通过各种性能监控方案，对性能监控项的异常数据进行采集和分析，输出相应的问题分析、定位与优化建议，从而帮助开发者开发出更高质量的应用。

Matrix 当前监控范围包括：应用安装包大小，帧率变化，启动耗时，卡顿，慢方法，SQLite 操作优化，文件读写，内存泄漏等等。
- APK Checker:
  针对 APK 安装包的分析检测工具，根据一系列设定好的规则，检测 APK 是否存在特定的问题，并输出较为详细的检测结果报告，用于分析排查问题以及版本追踪
- Resource Canary:
  基于 WeakReference 的特性和 [Square Haha](https://github.com/square/haha) 库开发的 Activity 泄漏和 Bitmap 重复创建检测工具
- Trace Canary:
  监控界面流畅性、启动耗时、页面切换耗时、慢函数及卡顿等问题
- SQLite Lint:
  按官方最佳实践自动化检测 SQLite 语句的使用质量
- IO Canary:
  检测文件 IO 问题，包括：文件 IO 监控和 Closeable Leak 监控

## 特性

与常规的 APM 工具相比，Matrix 拥有以下特点：

### APK Checker

- 具有更好的可用性：JAR 包方式提供，更方便应用到持续集成系统中，从而追踪和对比每个 APK 版本之间的变化
- 更多的检查分析功能：除具备 APKAnalyzer 的功能外，还支持统计 APK 中包含的 R 类、检查是否有多个动态库静态链接了 STL 、搜索 APK 中包含的无用资源，以及支持自定义检查规则等
- 输出的检查结果更加详实：支持可视化的 HTML 格式，便于分析处理的 JSON ，自定义输出等等

### Resource Canary

- 分离了检测和分析部分，便于在不打断自动化测试的前提下持续输出分析后的检测结果
- 对检测部分生成的 Hprof 文件进行了裁剪，移除了大部分无用数据，降低了传输 Hprof 文件的开销
- 增加了重复 Bitmap 对象检测，方便通过减少冗余 Bitmap 数量，降低内存消耗

### Trace Canary

- 编译期动态修改字节码, 高性能记录执行耗时与调用堆栈
- 准确的定位到发生卡顿的函数，提供执行堆栈、执行耗时、执行次数等信息，帮助快速解决卡顿问题
- 自动涵盖卡顿、启动耗时、页面切换、慢函数检测等多个流畅性指标

### SQLite Lint

- 接入简单，代码无侵入
- 数据量无关，开发、测试阶段即可发现SQLite性能隐患
- 检测算法基于最佳实践，高标准把控SQLite质量*
- 底层是 C++ 实现，支持多平台扩展

### IO Canary

- 接入简单，代码无侵入
- 性能、泄漏全面监控，对 IO 质量心中有数
- 兼容到 Android P

## 使用方法

1. 在你项目根目录下的 gradle.properties 中配置要依赖的 Matrix 版本号，如：
``` gradle
  MATRIX_VERSION=0.4.10
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

至此，Matrix就已成功集成到你的项目中，并且开始收集和分析性能相关异常数据，如仍有疑问，请查看 [示例](https://github.com/Tencent/Matrix/tree/dev/samples/sample-android/).

PS：
Matrix 分析后的输出字段的含义请查看 [Matrix 输出内容的含义解析](https://github.com/Tencent/matrix/wiki/Matrix--%E8%BE%93%E5%87%BA%E5%86%85%E5%AE%B9%E7%9A%84%E5%90%AB%E4%B9%89%E8%A7%A3%E6%9E%90)

### APK Checker

APK Check 以独立的 jar 包提供 ([matrix-apk-canary-0.4.10.jar](https://jcenter.bintray.com/com/tencent/matrix/matrix-apk-canary/0.4.10/matrix-apk-canary-0.4.10.jar)），你可以运行：

```cmd
java -jar matrix-apk-canary-0.4.10.jar
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

由于篇幅影响，此次不再赘述，我们在 [Matrix-APKChecker](https://github.com/Tencent/matrix/wiki/Matrix-ApkChecker#matrix-apkchecker-%E7%9A%84%E4%BD%BF%E7%94%A8) 中进行了详细说明。

## Support

还有其他问题？

1. 查看[示例](https://github.com/Tencent/matrix/tree/master/samples/sample-android)；
2. 阅读[源码](https://github.com/Tencent/matrix/tree/master)；
3. 阅读 [Wiki](https://github.com/Tencent/matrix/wiki) 或 [FAQ](https://github.com/Tencent/Matrix/wiki/Matrix-%E5%B8%B8%E8%A7%81%E9%97%AE%E9%A2%98)；
4. 联系我们。

## 参与贡献

关于 Matrix 分支管理、issue 以及 pr 规范，请阅读 [Matrix Contributing Guide](https://github.com/Tencent/Matrix/blob/master/CONTRIBUTING.md)。

[腾讯开源激励计划](https://opensource.tencent.com/contribution) 鼓励开发者的参与和贡献，期待你的加入。

## License

Matrix is under the BSD license. See the [LICENSE](https://github.com/Tencent/Matrix/blob/master/LICENSE) file for details
