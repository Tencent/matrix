# libwxperf-jni

微信性能相关工具集

# 分支说明

- master
    要合主工程主干的分支
- experimental
    要合主工程红版的分支, 禁止回流 master
- unstable/<master/experimental>/your_feature
    你的开发分支, 建议基于此分支打 SNAPSHOT 版本
       
# module 说明

- wxperf-fd
    用于读取 FD 实际路径的工具
- wxperf-hook
    PLT hook 工具: 目前包括 MemoryHook、PthreadHook、EGLHook、FPUnwind
- wxperf-jectl
    hack jemalloc 5.x opt_retained, 缓解 Android 10 32 位机器上的 VmSize 问题
    
# TODO

- 抽离公共工具到 wxperf-common
- 抽离第三方库, 以组件的形式接入