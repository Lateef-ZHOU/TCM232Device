# TCM232Device

TCM232 独立设备官邸的串口层，目标 `Lidar::Tcm232Serial`。基于 Qt Core、HandleCore 的 Serial 可选模块和 libserialport。使用方继承 `Tcm232::Handle` 实现收包钩子 `onData`，末端析构先同步 `close()`。

串口号由调用方传入，`parameters(port)` 采用 57600 baud、8N1、无流控。下发 ASCII 命令主体，例如 `TC1:TCACTTEMP?`；Handle 检查非空且不含 CR/LF，在串口上追加一个 CR（0x0D）。接收只以 CR 分隔，当次交完所有非空主体，结尾半行保留到下次读取，断线后随连接丢弃。每行使用收到结束符时的时间。接收缓冲上限 4096 字节是本地保护值；超长时整行丢弃，遇到 CR 后恢复。收完一行后，无 `#` 的主体直接上传；有 `#` 时要求后缀为 `@地址#YY`（地址为 0～255 的十进制数，YY 为两位大写十六进制数）。从首字节到 `#`（含）逐字节异或，匹配后只剥掉 `#YY`，保留地址。校验失败或后缀残缺只丢弃当前行。参考厂家《数控 RS232 通讯协议 V3.0》§3.7、§4.2、§4.6、§8。

设备后端目标 `Lidar::Tcm232Device` 提供 final 类 `Tcm232::Tcm232Device`，通过 `Tcm232DeviceT<Transport>` 与网口后端共用同一套报文封装与解析。连接和锁都属于这个对象；末端析构先关停收包。桥接器只需串口层，可配置 `-DTCM232_WITH_DEVICE=OFF`。

串口 Handle 的发送入口接受一条命令主体。厂家协议 §4.6 规定相邻命令间隔大于 50 ms；串口 Handle 的 onTX 校验并补 CR 后排队，返回 Queued 表示接受；实际发送失败由 connSend 记录。首条立即发送，每次写入返回后以单次精确定时器冷却 75 ms；末条发完也保留冷却，期间的新命令等待，不重复封帧。定时器到点且队列为空则自然停止。AboutToRelease 同步清队列并停表，Released 在所属线程清除自动断线时的待发送项，不跨连接重放。队列和定时器仅在所属主线程使用。页面每秒一次提交完整查询集合，手动命令直接独立提交，不再保存另一份发送队列；实际串口间隔只由 Handle 保证。协议入口 `Protocol::parse(body)` 一次返回 `Packet{address, module, command, value}` 或具体错误。传入已去掉 CR 和校验的主体，返回对象自持。缺省地址为 `std::nullopt`，显式 `@0` 为有效地址。接着根据模块名、操作符和命令名识别报文；模块和命令文本用独立哈希索引查找。命令覆盖型号表 157 个不同参数名、随包 TCM232 型号文件补充的 5 个参数和通用应答 REPLY。模块和命令仍可自由组合，未登记的名称返回错误。

`=` 根据命令登记的类型生成 QVariant。NUM 为有限 double，普通 OPT 和 BINARYCODE 为整数，REPLY 为 0～8 的整数，其余以 QString 保存（包括空串及前导零）。转换失败返回解析错误。`?` 和 `!` 由同一个 ValueType 枚举的 Query、Save 值表示，使用 QVariant::fromValue 存入；先辨元类型，再读枚举值，空字符串赋值仍是 QString。设备通过 `packetReceived(RxTime at, const Tcm232::Packet &packet)` 从收包线程直接上传每条有效回报。方向异常的查询、保存及解析错误记录日志；合法的设备错误应答同样上传。

界面端链接 `Lidar::Tcm232Qml` 并导入 `Lidar.Tcm232`（静态应用导入 Tcm232TypesPlugin）。`tcm232Packet` 只读开放 module、command、value、address。未带地址在 QML 中为 undefined，显式 @0 为数字 0。以 `packet.module === Tcm232Protocol.CMD && packet.command === Tcm232Protocol.REPLY` 识别命令应答；温度可通过 TC1 / TC2 与 TCACTTEMP 分流。类型登记在可选 QML 目标，`TCM232_WITH_QML=OFF` 只构建设备后端，`TCM232_WITH_DEVICE=OFF` 只构建串口传输。`Tcm232Module` 自持一个串口后端。温控1和风扇1分区仅显示各自回报，温控开关、实际输出状态与故障分开显示。连接成功立即查型号、SN 和完整状态集合。首轮结束时仅对缺失身份项补查一次；AboutToRelease 停表，Released 清除回报。连接后启动 1 秒定时器，每次同步提交温控1七项、风扇1三项查询；手动设定、保存与开关直接调用设备，不等待、不重启轮询，也不插入页面队列。串口 Handle 对所有命令按接收顺序统一发送。断开停止轮询并清空回报，未发命令由 Handle 清理。调节温度和风扇启动温度的输入均为独立草稿；设定只发送赋值，由正常轮询取得新值；保存只发 `!`，不会先写入草稿。

下行使用 `Protocol::pack(const Packet &packet)`；结果为可直接交给 Handle 的命令主体，失败返回空 QByteArray。ValueType::Query / ValueType::Save 枚举值分别产生 `?` / `!`，其他值按命令配置编码为 ASCII 赋值。地址省略与显式 0 分别生成无后缀与 `@0`。例如 `pack({0, Module::TC1, Command::TCSW, 1})` 返回 `TC1:TCSW=1@0`。封装不主动加可选异或校验，串口层负责追加 CR。收发使用同一命令描述表；`nameOf(Command)` 可单独取协议拼写。

## 验证

配置 Qt 6 和 libserialport 的搜索路径后，在仓库根目录执行。公开仓库使用 MIT 许可证，版权为 2026 周涛 (ZHOU TAO)。构建从 GitHub 拉取固定提交：

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

测试覆盖半帧、多帧同读、残留半帧、空行、超长恢复，以及生产 Handle 经过模拟串口的收发边界，无需连接实物。

2026-11-05：补充首条立即发送、相邻写入不少于 100 ms、末条冷却期间重新入队、冷却结束后立即发送、断开/重连清队列、未连接拒收、短写/错误日志及析构后不发送的验证。Windows Qt 6.11.2/MSVC Release 与 CTest 1/1 通过（包含设备和 QML 测试）；Linux Qt 6.8.3/GCC 串口专项 1/1 通过。Bridge 集成以突发 UDP 输入验证实际串口节奏，Linux 和 ARM64 均通过。

板端已部署此 Handle：Windows 一次突发发送 12 条只读查询，型号、SN 和 10 项状态全部返回，回报间隔约 100 ms（接收时间含网络抖动，不作为串口写入精确间隔）。未下发设定、保存或输出开关命令。部署与备份记录见相邻 BridgeCore/VALIDATION.md。本轮仅本地提交，尚未推送；独立构建本地版本需同时指定 `FETCHCONTENT_SOURCE_DIR_LIDAR_HANDLE_CORE` 为相邻 HandleCore 源码目录。

启用 QML 时，测试用 QQmlEngine 的 Connections 接收生产信号并按枚举分流，在主线程逐包核对原始 RxTime、地址和值。

页面右栏依次装配设备连接、温控1、风扇1和日志。左侧沿用激光器的三联趋势图：温控1和风扇1实际温度共用一图，实际输出电流和电压各一图。官邸逐包暂存本轮测量，在下一轮开始前以最后一条测量的接收时间向三图一起追加一次；本轮缺项留空，整轮无测量时跳过；转速、开关、故障和设定值仅更新右栏。

网口版 `Tcm232UdpModule` 与串口版 `Tcm232Module` 共用 `Tcm232Page`。Windows 工作台默认装配网口版，使用 Pcap 网卡枚举、Windows 192.168.100.200 与板端 192.168.100.100，两端 UDP 55300。每个数据报携带一条 ASCII 主体，串口回车由板上的 Handle 处理。


2026-11-05 后续：删除页面 pending、queueManual 和逐条 sendNext；改为 1 秒 queryRound。页面专项用可记录提交的后端验证每次整组十条、按钮同步提交且不插入额外查询、约 1 秒周期、三图统一更新及断开停表。真实串口节奏仍由已有 Handle 测试覆盖。Windows 增量构建及 CTest 1/1（5.38 秒）通过；未修改或重新部署板端 Handle/Bridge。本轮暂不推送。

2026-11-05 冷却调整（用户定）：串口写入后的冷却从 100 ms 缩短为 75 ms，QML 仍每秒提交十项查询，手动命令独立提交。首条立即、尾条冷却、断开和重连清队列的机制保持。上述 100 ms 验证是调整前记录。Windows 完整专项 1/1（5.39 秒）、Linux 串口专项 1/1、Bridge 4/4，以及板上 ARM64 模拟转发与 DMA 验证通过。新日志为 build/{build,test}-paced75.log，部署和连续查询证据见 BridgeCore/VALIDATION.md。

2026-11-05 表单统一（用户定）：温控及风扇读数接入 Readout，设定/保存与开/关接入 DoubleButtonRow，保存提示保留。型号及 SN 使用 FormLabel。输入草稿、查询轮次、缓存及整轮绘图不变。 公共视觉规格由 UICore 提供，不增加业务状态。Windows 主程序增量构建、八秒离屏冒烟通过；固定依赖在本地更新，暂不推送。


### 2026-11-05 公共视觉与连接布局

串口与网口连接区使用 UICore ConnectionSection，型号/SN 使用 InfoRow；查询轮次、暂存值、整轮绘图和 75 ms 发送间隔不变。 本轮仅本地整理，未推送；Windows 集成验证记录见主程序维护操作。

### 2026-11-05 错误正文限宽

错误状态使用纵向 Readout，与其他只读字段同样式；长文字按公共规则省略并悬停显示全文。错误解析、回报缓存、查询轮次、发送间隔和整轮绘图均未改。主程序 Windows 增量构建及八秒冒烟通过；仅本地提交，未推送。


### 2026-11-05 按钮气泡与布局复用

温控1的温度设定与输出开关双列直接复用 ReadoutRow，删除等宽、填充和列间距的重复声明。Temperature 业务组件、草稿、回报、设定/保存、发送间隔与整轮绘图保持。

主程序Windows Release增量构建、八秒桌面冒烟与四窗口装配检查通过，运行stderr为空。未连接硬件，仅本地提交、未推送。

## 字段分类与开机默认值（2026-11-05 用户定）

型号/SN 在首排使用纵向 Readout；温度设定和输出开关使用 UICore ReadoutField。温控调节温度与风扇启动温度在合法编辑完成且值改变时立即下发，删除“设定”按钮。设备回报只更新读数，不覆盖草稿；查询轮次、整轮绘图与75 ms发送间隔保持。

原“保存”改为“设为开机默认”，仍发送对应参数的 ! 命令，将设备当前值写入非易失存储，重启后恢复。按钮不读取草稿、不追加设定命令；与设置后设备是否接受的判断相互独立。

验证：Windows Release 增量构建、serial_delivery 1/1通过（包括真实QML及模拟串口）。覆盖编辑完成设定、同值不重复、回报与草稿独立、保存仅发送 !、查询和整轮绘图不变。测试中的成组按钮改为触发实际 Action，避免仅手工发 clicked 信号却漏掉动作。未操作实物设备、未推送。

### 2026-11-05 温控与风扇三列排布（用户定）

型号/SN仍在温控1首排，不新增设备信息分区。调节温度、输出开关、错误状态共用一个ReadoutRow；错误仍为纵向Readout，删除原底部独立分隔行。风扇1按启动温度、实际温度、实际转速三列排列，启动温度保留编辑完成设定和设为开机默认。查询、回报、错误解释与发送间隔不变。

验证：Windows Release增量构建、serial_delivery 1/1、主程序四窗口装配及八秒冒烟通过；截图核查三列对齐和按钮显示。未连接实物、未推送。

2026-11-05 输出开关按钮改为上下排列：复用ReadoutField自带纵向内容区，开启在上、关闭在下，均占满该列。命令与回报不变；Release增量构建、serial_delivery 1/1及主程序八秒装配冒烟通过，未推送。

## 表单分隔与提示统一（2026-11-05 用户定）

温控横线使用 UICore FormSeparator，开机默认提示使用 FormToolTip；原提示文字、按钮上下排布、组间距、查询与整轮绘图保持。

验证：Windows 主程序 Release 增量构建、四窗装配和八秒桌面冒烟通过，运行 stderr 为空；UICore CTest 3/3通过。未连接硬件，未推送。
