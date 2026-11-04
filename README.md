# TCM232Device

TCM232 独立设备官邸的串口层，目标 `Lidar::Tcm232Serial`。基于 Qt Core、HandleCore 的 Serial 可选模块和 libserialport。使用方继承 `Tcm232::Handle` 实现收包钩子 `onData`，末端析构先同步 `close()`。

串口号由调用方传入，`parameters(port)` 采用 57600 baud、8N1、无流控。下发 ASCII 命令主体，例如 `TC1:TCACTTEMP?`；Handle 检查非空且不含 CR/LF，在串口上追加一个 CR（0x0D）。接收只以 CR 分隔，当次交完所有非空主体，结尾半行保留到下次读取，断线后随连接丢弃。每行使用收到结束符时的时间。接收缓冲上限 4096 字节是本地保护值；超长时整行丢弃，遇到 CR 后恢复。收完一行后，无 `#` 的主体直接上传；有 `#` 时要求后缀为 `@地址#YY`（地址为 0～255 的十进制数，YY 为两位大写十六进制数）。从首字节到 `#`（含）逐字节异或，匹配后只剥掉 `#YY`，保留地址。校验失败或后缀残缺只丢弃当前行。参考厂家《数控 RS232 通讯协议 V3.0》§3.7、§4.2、§4.6、§8。

设备后端目标 `Lidar::Tcm232Device` 提供 final 类 `Tcm232::Tcm232Device`，直接继承 `Device<Tcm232::Handle>`。连接和锁都属于这个对象；末端析构先关停收包。桥接器只需串口层，可配置 `-DTCM232_WITH_DEVICE=OFF`。

串口 Handle 的发送入口接受一条命令主体。厂家协议 §4.6 规定相邻命令间隔大于 50 ms；当前软件不限制发送间隔，仅在实机频繁漏响应时再评估。协议入口 `Protocol::parse(body)` 一次返回 `Packet{address, module, command, value}` 或具体错误。传入已去掉 CR 和校验的主体，返回对象自持。缺省地址为 `std::nullopt`，显式 `@0` 为有效地址。接着根据模块名、操作符和命令名识别报文；模块和命令文本用独立哈希索引查找。命令覆盖型号表 157 个不同参数名、随包 TCM232 型号文件补充的 5 个参数和通用应答 REPLY。模块和命令仍可自由组合，未登记的名称返回错误。

`=` 根据命令登记的类型生成 QVariant。NUM 为有限 double，普通 OPT 和 BINARYCODE 为整数，REPLY 为 0～8 的整数，其余以 QString 保存（包括空串及前导零）。转换失败返回解析错误。`?` 和 `!` 由同一个 ValueType 枚举的 Query、Save 值表示，使用 QVariant::fromValue 存入；先辨元类型，再读枚举值，空字符串赋值仍是 QString。设备通过 `packetReceived(RxTime at, const Tcm232::Packet &packet)` 从收包线程直接上传每条有效回报。方向异常的查询、保存及解析错误记录日志；合法的设备错误应答同样上传。

界面端链接 `Lidar::Tcm232Qml` 并导入 `Lidar.Tcm232`（静态应用导入 Tcm232TypesPlugin）。`tcm232Packet` 只读开放 module、command、value、address。未带地址在 QML 中为 undefined，显式 @0 为数字 0。以 `packet.module === Tcm232Protocol.CMD && packet.command === Tcm232Protocol.REPLY` 识别命令应答；温度可通过 TC1 / TC2 与 TCACTTEMP 分流。类型登记在可选 QML 目标，`TCM232_WITH_QML=OFF` 只构建设备后端，`TCM232_WITH_DEVICE=OFF` 只构建串口传输。`Tcm232Module` 自持一个串口后端。温控1和风扇1分区仅显示各自回报，温控开关、实际输出状态与故障分开显示。连接后立即查询并每秒刷新，断开清空回报。调节温度和风扇启动温度的输入均为独立草稿；设定发送赋值并回查，保存直接发 `!`，不会先写入草稿。

下行使用 `Protocol::pack(const Packet &packet)`；结果为可直接交给 Handle 的命令主体，失败返回空 QByteArray。ValueType::Query / ValueType::Save 枚举值分别产生 `?` / `!`，其他值按命令配置编码为 ASCII 赋值。地址省略与显式 0 分别生成无后缀与 `@0`。例如 `pack({0, Module::TC1, Command::TCSW, 1})` 返回 `TC1:TCSW=1@0`。封装不主动加可选异或校验，串口层负责追加 CR。收发使用同一命令描述表；`nameOf(Command)` 可单独取协议拼写。

## 验证

配置 Qt 6 和 libserialport 的搜索路径后，在仓库根目录执行。当前单参数 Device 版本为本地提交，下例使用相邻 DeviceCore 检出：

```sh
cmake -S . -B build -DBUILD_TESTING=ON -DFETCHCONTENT_SOURCE_DIR_LIDAR_DEVICECORE="$PWD/../DeviceCore"
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

测试覆盖半帧、多帧同读、残留半帧、空行、超长恢复，以及生产 Handle 经过模拟串口的收发边界，无需连接实物。

启用 QML 时，测试用 QQmlEngine 的 Connections 接收生产信号并按枚举分流，在主线程逐包核对原始 RxTime、地址和值。

页面右栏依次装配设备连接、温控1、风扇1和日志。左侧沿用激光器的三联趋势图：温控1和风扇1实际温度共用一图，实际输出电流和电压各一图。每条绘图测量回报按接收时间追加对齐快照，未回报的通道留空；转速、开关、故障和设定值仅更新右栏。
