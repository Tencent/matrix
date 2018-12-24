## Issue/提问须知
**在提交issue之前，我们应该先查询是否已经有相关的issue以及[常见问题](https://github.com/tencent/matrix/wiki/Matrix-%E5%B8%B8%E8%A7%81%E9%97%AE%E9%A2%98)。提交issue时，我们需要写明issue的原因，以及编译或运行过程的日志。issue需要以下面的格式：**

```
异常类型：app运行时异常/编译异常

手机型号：如:Nexus 5(如是编译异常，则可以不填)

手机系统版本：如:Android 5.0 (如是编译异常，则可以不填)

matrix版本：如:0.4.5

gradle版本：如:2.10

问题描述：如：在android O 出现系统不兼容 

堆栈/日志：
1. 如是编译异常，请在执行gradle命令时，加上--stacktrace;
2. 日志我们需要过滤"Matrix."关键字;
```

提问题时若使用`不能用/没效果/有问题/报错`此类模糊表达，但又没给出任何代码截图报错的，将绝对不会有任何反馈。这种issue也是一律直接关闭的,大家可以参阅[提问的智慧](https://github.com/tvvocold/How-To-Ask-Questions-The-Smart-Way)。

Matrix是一个开源项目，希望大家遇到问题时要学会先思考，看看Sample与Matrix的源码，更鼓励大家给我们提pr.
