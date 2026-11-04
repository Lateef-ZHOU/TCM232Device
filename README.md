# TCM232Device

TCM232 独立设备官邸的串口层，目标 `Lidar::Tcm232Serial`。基于 Qt Core、HandleCore 的 Serial 可选模块和 libserialport。使用方继承 `Tcm232::Handle` 实现收包钩子 `onData`，末端析构先同步 `close()`。

串口号由调用方传入，`parameters(port)` 采用 57600 baud、8N1、无流控。下发 ASCII 命令主体，例如 `TC1:TCACTTEMP?`；Handle 检查非空且不含 CR/LF，在串口上追加一个 CR（0x0D）。接收只以 CR 分隔，当次交完所有非空主体，结尾半行保留到下次读取，断线后随连接丢弃。每行使用收到结束符时的时间。接收缓冲上限 4096 字节是本地保护值；超长时整行丢弃，遇到 CR 后恢复。收完一行后，无 `#` 的主体直接上传；有 `#` 时要求后缀为 `@地址#YY`（地址为 0～255 的十进制数，YY 为两位大写十六进制数）。从首字节到 `#`（含）逐字节异或，匹配后只剥掉 `#YY`，保留地址。校验失败或后缀残缺只丢弃当前行。参考厂家《数控 RS232 通讯协议 V3.0》§3.7、§4.2、§4.6、§8。

设备后端目标 `Lidar::Tcm232Device` 提供 final 类 `Tcm232::Tcm232Device`，直接继承 `Device<Tcm232::Handle>`。连接和锁都属于这个对象；末端析构先关停收包。桥接器只需串口层，可配置 `-DTCM232_WITH_DEVICE=OFF`。

串口 Handle 的发送入口接受一条命令主体。厂家协议 §4.6 规定相邻命令间隔大于 50 ms；当前软件不限制发送间隔，仅在实机频繁漏响应时再评估。`Tcm232Protocol.h` 中的静态函数 `Protocol::parseReply` 分类并提取字段，官邸在收包线程按结果发出两类信号：`parameterReported` 携带参数全名、设备原文值，`commandReplied` 携带 0～8 的应答码。两者均带到达时刻、串口地址（-1 表示省略），跨线程消费者使用 Qt 队列投递。具体温度显示、命令编排、轮询和 UI 待后续接入。

## 验证

配置 Qt 6 和 libserialport 的搜索路径后，在仓库根目录执行。当前单参数 Device 版本为本地提交，下例使用相邻 DeviceCore 检出：

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DFETCHCONTENT_SOURCE_DIR_LIDAR_DEVICECORE="$PWD/../DeviceCore"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

测试覆盖半帧、多帧同读、残留半帧、空行、超长恢复，以及生产 Handle 经过模拟串口的收发边界，无需连接实物。
