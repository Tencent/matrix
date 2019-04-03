# Matrix for iOS/macOS 正式开源了

Matrix for iOS/macOS 是一款微信研发并日常使用的性能探针工具，目前集成在 iOS 和macOS 平台微信的 APM（Application Performance Manage）平台中使用。Matrix for iOS/macOS 的目标是建立一套 iOS 和 macOS 平台上统一的应用性能接入框架，通过对性能监控项的异常数据进行采集，帮助开发者开发出更高质量的应用，从而提升应用的用户体验。

Matrix for iOS/macOS 的各个插件已经在微信内部稳定运行了几年，现在作为 Matrix 项目的一部分将其开源：https://github.com/Tencent/matrix/tree/master/matrix/matrix-apple 。

欢迎 Star,  提 Issue 和 PR。

## Matrix for iOS/macOS 有哪些功能

Matrix for iOS/macOS 当前监控范围包括：崩溃、卡顿和爆内存，目前包含两款插件：WCCrashBlockMonitorPlugin 和 WCMemoryStatPlugin。

#### WCCrashBlockMonitorPlugin

WCCrashBlockMonitorPlugin 是一款基于 [KSCrash](https://github.com/kstenerud/KSCrash) 框架开发，具有业界领先的卡顿堆栈捕获能力的插件。同时插件也具备崩溃捕捉功能，崩溃捕捉能力与 KSCrash 框架一致。卡顿捕捉具有如下特点：

* 通过检查 Runloop 运行状态判断应用是否卡顿，同时支持 iOS/macOS 平台;
* 提供耗时堆栈提取能力，卡顿线程快照日志中附加最近时间最耗时的主线程堆栈。

#### WCMemoryStatPlugin

WCMemoryStatPlugin 是一款性能优化到极致的爆内存监控工具，能够全面捕获应用出现爆内存时的堆栈以及内存分配情况。与现有的爆内存监控工具相比，WCMemoryStatPlugin 性能表现更加优异，并且监控的对象更加全面，它具有如下特点：

* 在应用运行期间获取对象存活以及相应的堆栈信息，在检测到应用爆内存时进行上报；
* 使用平衡二叉树存储存活对象，使用 Hash Table 存储堆栈，性能优化到极致。


## 未来规划

1. 计划扩展卡顿监控，让 Matrix for iOS/macOS 具备获取耗电堆栈的能力;
2. 打造一套完整的云解决方案。

## 开源地址

[Matrix] https://github.com/Tencent/matrix/tree/master/matrix/matrix-apple

请给 Matrix 一个 Star !

欢迎提出你的 issue 和 PR