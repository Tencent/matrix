微信自研 APM 利器，Matrix 正式开源
----------

Matrix 是一款微信团队研发并日常使用的 APM (Application Performance Manage) ，当前主要运行在 Android 平台上。Matrix  建立了一套统一的应用性能接入框架，通过对各种性能监控方案快速集成，对性能监控项的异常数据进行采集和分析，输出相应问题的分析、定位与优化建议，从而帮助开发者开发出更高质量的应用。

在经历微信内 1 年多的验证与迭代，现在 Matrix 终于开源了：

https://github.com/tencent/matrix 

欢迎 Star,  提 Issue 和 PR。

## Matrix 有哪些功能？

Matrix 当前监控范围包括：应用安装包大小，SQLite 操作优化，帧率变化，卡顿，启动耗时，页面切换耗时，慢方法，文件读写性能，I/O 句柄泄漏， 内存泄漏等。

### APK Checker

APK Checker 是针对 Android 安装包的分析检测工具，根据一系列设定好的规则检测 APK 是否存在特定的问题，并输出较为详细的检测结果报告，用于分析排查问题以及版本追踪。当前，APK Checker 主要包含以下功能：

- 读取 manifest 的信息
- 按文件大小排序列出 APK 中的所有文件
- 统计方法数
- 统计 class 数目
- 检查是否经过资源混淆（AndResguard）
- 搜索不含 alpha 通道的 png 文件
- 搜索未经压缩的文件类型
- 检查是否包含多 ABI 版本的动态库
- 统计 APK 中包含的 R 类以及 R 类中的 field count
- 搜索冗余的文件
- 检查是否有多个动态库静态链接了 STL
- 搜索 APK 中包含的无用资源
- 搜索 APK 中包含的无用 assets 文件
- 搜索 APK 中未经裁剪的动态库

### SQLite Lint

SQLite Lint是一个 SQLite 使用质量的自动化检测工具，犹如一个 SQLite 优化高手在开发或者测试过程中不厌其烦地、仔细地 review 你的 SQL 语句，是团队高质量 SQLite 实践中的一个有用工具。SQLite Lint 当前支持的检测能力包括：

* 检测索引使用问题
* 检测冗余索引问题
* 检测 select * 问题
* 检测 Autoincrement 问题
* 检测建议使用 prepared statement
* 检测建议使用 without rowid 特性

### Trace Canary

Trace Canary 通过 choreographer 回调、编译期插桩的方式，实现了高准确率、高性能的卡顿检测、定位方案，并扩展支持了多个其它流畅性指标，包括：

* 界面流畅性评估
* 卡顿定位
* ANR 监控
* 应用启动及界面切换耗时监控

### Resource Canary

Resource Canary 是基于 Weak Reference 的特性和 Haha 库开发的 Activity 泄漏和 Bitmap 重复创建检测工具。其中检测采集部分部署在客户端，分析部分部署在服务端，通过分离检测和分析两部分逻辑使该工具的流程对持续集成更友好。当前 ResourceCanary 主要包含以下功能：

- 检测疑似泄漏的 Activity ，输出其类名和引用链
- 检测内存中图像数据完全一样的重复 Bitmap 对象，输出其类名和引用链

### IO Canary

IO Canary 是一个在开发、测试或者灰度阶段辅助发现 IO 问题的工具，目前主要包括文件 IO 监控和 Closeable Leak 监控两部分，提供了 IO 的大盘监控，从而做到心中有数。具体功能包括：

- 检测主线程 IO 使用不当
- 检测读写 Buffer 过小
- 检测重复读操作
- 检测 Closeable Leak 操作，包括文件读写、cursor 未及时关闭等

## Matrix 有哪些优势？
与常规的 APM 工具相比，Matrix 拥有以下特点：

### APK Checker

- 具有更好的可用性：JAR 包方式提供，更方便应用到持续集成系统中，从而追踪和对比每个 APK 版本之间的变化
- 更多的检查分析功能：除具备 APKAnalyzer 的功能外，还支持统计 APK 中包含的 R 类、检查是否有多个动态库静态链接了 STL 、搜索 APK 中包含的无用资源，以及支持自定义检查规则等
- 输出的检查结果更加详实：支持可视化的 HTML ，便于分析处理的 JSON ，自定义输出等等

### SQLite Lint

* 接入简单，代码无侵入
* 数据量无关，开发、测试阶段即可发现SQLite性能隐患
* 检测算法基于最佳实践，高标准把控SQLite质量
* 底层是 C++ 实现，支持多平台扩展

### Resource Canary

- 分离了检测和分析部分，便于在不打断自动化测试的前提下持续输出分析后的检测结果
- 对检测部分生成的 Hprof 文件进行了裁剪，移除了大部分无用数据，降低了传输 Hprof 文件的开销
- 增加了重复 Bitmap 对象检测，方便通过减少冗余 Bitmap 数量，降低内存消耗

### Trace Canary

- 编译期动态修改字节码, 高性能记录执行耗时与调用堆栈
- 准确的定位到发生卡顿的函数，提供执行堆栈、执行耗时、执行次数等信息，帮助快速解决卡顿问题
- 自动涵盖卡顿、启动耗时、页面切换、慢函数检测等多个流畅性指标

### IO Canary

- 接入简单，代码无侵入
- 性能、泄漏全面监控，对 IO 质量心中有数
- 兼容到 Android P


## 未来规划
1. 功能完善，增加新的监控项，如电量，线程资源，内存监控等等
2. 平台扩展，提供 iOS 等多语言 SDK
3. 打造一套完整的云解决方案

## 开源地址

[Matrix] https://github.com/tencent/matrix


请给 Matrix  一个 Star !

欢迎提出你的 issue 和 PR
