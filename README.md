## 2026-11-17：网口连接参数收紧（用户定）

Tcm232UdpDevice 构参时仅从所选网卡取 pcapDeviceName 与 hostMAC；名称/描述仍用于原 QML 选项，不随连接快照传递。本机 MAC 参与 PcapParams 默认比较。固定 IP、端口、业务命令与既有收发/ARP/关闭链保持，验证见主程序维护操作。本地源码覆盖，不推送、不更新板端。

## 2026-11-17：onTX 接入 vector 只读引用（用户定）

onTX 输入统一为 const std::vector<unsigned char>&，同步传参不复制调用方数组；RX 仍用只读 span。命令检查、补回车和异步队列所有权保持；首条立即发送、75ms 间隔、末条冷却及断开清队列不变。 不新增成员状态；使用相邻本地 HandleCore 源码覆盖，远端锁定版本暂不变，不推送。Windows 完整配置 Release 与串口/设备/QML 综合测试 1/1 通过。 证据见主程序 docs/maintenance/validation.md 本轮记录。

## 2026-11-17：网口地址迁移（用户定）

Tcm232UdpDevice 的固定 IP 改为 PacketProtocol::IPv4Header::Address，从四字节 array 构造；业务地址、端口、75 ms 发送间隔、每秒查询及整轮绘图保持。采用相邻 HandleCore 的新协议，不增加旧地址兼容层。

Windows 完整 QML 配置 Release 构建与 CTest 1/1 通过（串口、设备、QML 验证在同一入口），主程序集成及 8 秒离屏冒烟通过；Linux 桥接转发测试通过。证据为相邻主程序 build/pcap-vector-packet-tcm-{build,test}.log 及本轮主程序验证记录。未操作温控实物或更新板端；本地源码覆盖，不推送。

## 2026-11-15：网卡列表接入 vector（用户定）

UDP 官邸直接缓存 const std::vector<NetworkInterfaceInfo>；拒绝负下标后按 size_t 比较和访问。选项顺序、首次枚举与字符串边界保持；75 ms 发送、每秒查询、整轮绘图、重连及关闭不变。

验证：Windows 完整 QML 配置 Release 与 serial_delivery 1/1 通过，主程序构建及 8 秒冒烟通过。证据见相邻主程序 build/pcap-vector-TCM232Device-qml-*.log；不推送。


## 2026-11-15：UDP 网卡信息适配（用户定）

Tcm232UdpDevice 接入标准字符串网卡信息，只在 QML 选项边界将 UTF-8 名称与描述转为 QString；pcap 标识原样进入连接参数。串口元数据仍按其本地编码显示。75 ms 发送队列、每秒查询、整轮绘图、重连与关闭保持，不推送。

验证：Windows 完整 QML 配置的 Release 构建与 serial_delivery 1/1 通过（含设备及 QML 验证），主程序增量构建、8 秒离屏冒烟及配置的 qmllint 目标通过。证据见相邻主程序 build/pcap-string-TCM232Device-qml-*.log。


## 2026-11-15：按半个最小帧长分块上传（用户定）

本轮验证：Windows Release、公开头与 serial_delivery 1/1 通过，温控继续按一字节交付；半帧、多帧、75 ms 发送、末条冷却和重连清理验证通过。主程序 Release、8 秒冒烟和温控配置的 qmllint 目标通过，Linux BridgeCore 集成 4/4 通过。日志见相邻主程序 build/serial-chunk-TCM232Device-*.log、serial-chunk-qmllint.log 和 serial-chunk-linux.log。未操作硬件。

SerialHandle<MaxRxBytes> 的编译期参数只表示上传上限，不恢复 Framer 类型或回调对象。各协议按最小帧长的一半向下取整：激光器 13 → 6 字节，Pelco-D 7 → 3 字节；温控成帧层接受非空主体加 CR，最短 2 → 1 字节，业务报文合法性仍由上层检查。SerialConn 仍最多一次读取 256 字节，然后顺序切为不超过上限的非空 span 同步上传；短块立即交付，不等凑满，同一读取块共用原 RxTime。

每次 onRX 至多返回一帧。具体 Handle 持有接收状态和连续结果，结果只在同步派发期间借用，异步使用须取得所有权；每次 connOpen 清空旧半帧。激光器和 Pelco-D 先追加整块，再查找第一份有效帧；环形容量为最大帧长加上传上限减一，避免追加末块时覆盖尚未校验的头部。激光/温控使用触发交付块的时刻，Pelco-D 保留帧头块时刻。TX、温控 75 ms 节奏、重连和关闭链不变。

设计权衡：分块减少逐字节的回调和虚函数调用次数，并不是“永远只有一帧”的证明。变长协议的错误长度可能阻塞一个长候选，候选失败后重新找头会一次发现多个既有完整帧；单字节同样可能积压，只是后续同量输入提供更多交付机会。这里接受分块恢复更慢：剩余完整帧严格等待后续非空输入，没有新输入就保持到重连清空，不加空输入排空或定时补发。一次片段缺失或长度字段损坏也可能触发，不能据此假设整体丢包率很高。详细示例及验证见 HandleCore/docs/reference.md 和各库验证记录。下方带日期的旧方案仅供历史追溯。

## 历史阶段：2026-11-15 单字节 onRX 成帧

验证：Windows Release、公开头编译与 serial_delivery 1/1 通过，覆盖半帧、多帧、校验、超长丢弃和自动重连清除旧半帧；原 75 ms 发送队列、首条即发及末条冷却验证保留。主程序 Release 与 8 秒离屏冒烟、Linux BridgeCore 集成 4/4 通过。证据见相邻主程序 build/serial-byte-TCM232Device-*.log、serial-byte-linux.log。未操作硬件。

SerialConn/SerialHandle 不再带 Framer 模板参数，SerialFramer concept 与独立 Framer 类删除。串口仍批量读取（最多 256 字节），按序将每个新字节作为长度恰为 1 的只读 span 同步交入 onRX，沿用同一读取块的 RxTime；不调用空输入，不设置按最小帧长推算的分块参数。具体协议 Handle 的 onRX 保存半帧、校验和重同步状态，每次返回 Drop 或至多一帧；不改变 HandleBase 的单项裁决接口。

接收状态仅由工作线程访问。具体 Handle 覆盖 connOpen，在每次打开（含自动重连）前清空旧半帧，再调用 SerialHandle::connOpen；关闭仍由原机制等待收包结束。RXRuling 借用的连续结果存于 Handle，保持至下一次 onRX，onData 同步消费，异步使用须取得所有权。TX vector 只读引用、发送顺序及关闭链保持；温控 75 ms 队列不变。

激光器坏帧仍仅丢一字节重新找头。一次新字节最多交付一帧；若重同步后还剩完整帧，严格等待后续新字节触发，不增设空输入排空、定时补发或后台队列。若不再有输入，剩余帧就留在缓冲中，重新连接时清空。激光/温控结果使用触发交付的读取块时刻，Pelco-D 保留帧头所在块的时刻。 以下带日期的 Framer 记录仅供历史追溯，以本条为准。本轮不推送。

## 2026-11-14：温控发送直接检查 span（用户定）

onTX 直接以 span::empty 和 std::ranges::any_of 检查非空单行主体，不再创建 QByteArray 借用适配；波特率枚举使用 static_cast<int>。仍拒绝包含 CR 或 LF 的命令，再补 CR 构造自有 vector 入队。首条立即发送、75 ms 间隔、末条冷却、重连和关闭清队列均保持。

Windows Release 独立构建、公开头与 serial_delivery 1/1 通过，补齐 LF 拒收验证，保留空命令、CR、队列所有权及启停覆盖。主程序 Release 增量构建与 8 秒离屏冒烟通过，stderr 与原字体基线一致。证据相邻主程序 build/tcm-spancheck-*.log 和 tcm-spancheck-smoke.json。仅本地提交，未推送、未操作板卡。

## 2026-11-14：connSend 引用传输验证

发送实现/测试替身适配 connSend 的 vector 只读引用；生产 RX 保持 span。Windows Release 独立构建和 CTest 1/1 通过，主程序增量构建和生命周期 8/8、8 秒离屏冒烟通过。证据相邻主程序 build/connsend-TCM232Device-*.log。无硬件操作，仅本地、不推送。

## 2026-11-14：connSend 使用自有数组的只读引用（用户定）

connSend 统一接受 const std::vector<unsigned char>&，各串口、网口、DMA 和仿真实现保持同一虚接口。Encode 直接引用自有 TXRuling；SerialHandle 直接把同一数组交给 SerialConn::send，删除临时 vector 复制。RX 与 onTX 输入仍为只读 span，异步回复和定时发送队列自行持有字节。发送顺序、失败短路、重连和关闭保持。 温控队列直接持有补回车后的 vector，出队移动所有权后按引用发送；首条立即发送、75 ms 节奏、末条冷却、断开清队列不变。下方旧 span 发送记录为阶段历史，以本条为准。

## 2026-11-14：串口成帧 span 验证

成帧器 feed/deliver 改用只读 span；底层缓冲仍自有，测试留存结果时复制字节。新增输入覆盖后续接半帧验证，保留多帧、校验、超长行恢复和 75 ms 队列测试。Windows Release、公开头及 CTest 1/1 通过。主程序构建和离屏冒烟通过，日志在主程序 build/framerspan-TCM232Device-*.log。未操作硬件、不推送。

## 2026-11-14：成帧器同步借用字节

feed 收只读 span，完整帧经 deliver 只读 span 同步交出；半帧和完整帧底层缓冲仍由成帧器拥有。校验、噪声重同步、帧时间及重连清理行为保持；异步使用必须取得所有权，不保存 span。本轮不推送。

## 2026-11-14：串口标准字符串验证

适配 SerialPortInfo 标准字符串；QML 签名不变，仅显示/输入边界进行本地编码转换。Windows 独立 Release、公开头与 CTest 1/1 通过，主程序增量构建和 8 秒离屏冒烟通过。日志：主程序 build/serialstring-TCM232Device-*.log。本轮未操作硬件、不推送。

## 2026-11-14：串口字符串边界

SerialPortInfo 的名称与描述改用 std::string，保留 libserialport 原始编码。仅在 QML/命令行边界转换 Qt 字符串，线路规格文本转换返回标准字符串。协议、连接参数及关闭行为不变。本轮不推送。

## 2026-11-14：vector 引用发送验证

测试调用适配 const vector& 发送入口；Windows 独立构建、公开头及 CTest 1/1 通过。温控原有 75 ms 节流、首条立即、末条冷却和断开清队列验证保持。证据主程序 build/vectorsend-TCM232Device-{build,test}.log。未操作硬件、不推送。

## 2026-11-14：Handle 发送接口适配

协议生成的 QByteArray 在发送入口转换为自有 vector<unsigned char>；Handle 的单包和批量入口按 const 引用接收，内部 onTX 与 connSend 保留只读 span。入队处独立持有报文；发送间隔、协议和关闭行为保持。仅本地，不推送。

## 2026-11-14：onData 只读 span 验证

上传/测试替身适配 span；只在现有 Qt 解析或断言边界借用适配。Windows 独立构建、公开头和 CTest 1/1 通过；日志在主程序 build/ondata-TCM232Device-{build,test}.log。仅本地，不推送，无真实硬件操作。

## 2026-11-14：接入只读上传视图

onData(std::span<const unsigned char>, RxTime) 在收包线程同步借用字节，视图本身不保活。HandleBase 不再包含或构造 QByteArray；Decode 直接交出 RXRuling 的视图。Device/具体设备仅在现有 Qt 解析入口构造零拷贝适配，异步使用必须先取得字节所有权。发送、重连、关闭与取消链不变。 本轮不推送。

## 2026-11-14：发送裁决所有权与收发 span（用户定）

适配拥有型 TXRuling 与只读 span 收发口。Windows Release 独立构建及 CTest 1/1 通过；原输入寿命、排队所有权和协议字节回归通过。日志：相邻主程序 build/wire-TCM232Device-{build,test}.log。重连、关闭与业务时序保持，仅本地提交，不推送；不涉及实物测试。

## 2026-11-14：Handle 收发边界

温控串口 onRX/onTX 改收只读 span；Queued 继续把补回车后的自有报文放入队列，定时器从队列字节借出 span 交 connSend。75 ms 间隔、首条立即发送、末条冷却、断开清队列保持。 发送顺序、重连和关闭不变；本轮不部署硬件、不推送。统一契约见 HandleCore/docs/reference.md。

## 2026-11-14：批量发送队列寿命验证

串口测试将两条查询/保存报文组成嵌套 span 批次，发送后立即覆盖并销毁描述符和输入；第一条立即发送，第二条仍按 75 ms 节奏发送原始内容。生产 onTX 的补回车、队列所有权、冷却及关闭行为不变。Windows build-rxspan 构建与 CTest 1/1 通过，证据 batchspan-{build,test}.log。本轮不推送。

## 2026-11-14：适配只读发送视图

查询、设定和保存先在当前调用构造主体，再以只读 span 交 Handle。串口 onTX 补回车生成自有报文后入队，75 ms 间隔、首条立即发送、末条冷却和断开清队列保持。

Windows build-rxspan 串口/后端测试 1/1 通过；新增 vector 借用入队后覆盖原字节的断言，确认队列持有独立报文。日志 sendspan-{build,test}.log。QML 随主程序构建及 8 秒离屏冒烟通过。 本轮不推送。

## 2026-11-14：UTF-8 日志接口（用户定）

温控 Handle、串口/网口设备与协议异常日志统一为 UTF-8 std::string；QML 的文本转换由 UICore 登记，不添加官邸转发。75 ms 发送节奏、首条立即发送、末条冷却、断开清队列、每秒查询和整轮绘图保持。

需要支持 C++20 std::format 的标准库。本轮仅本地修改与验证，不推送。

验证：串口/业务/QML 联合 CTest 1/1 通过（build/logstring-*.log）。串口专项首跑出现一次 burst interval 断言失败；不改测试阈值或发送代码，随后连续 5 次通过（主程序 build/logstring-tcm-repeat.log）。本轮不据复跑推断严格定时保证；未做实物操作。

## 历史：2026-11-14 TXRuling 只读 span（已被上方拥有型裁决取代）

onTX 适配可写缓冲引用签名，Queued 仍由原队列持有完整报文；75 ms 间隔、首条立即、末条冷却和断开清队列不变。 重连和关闭保持，与本地 HandleCore 同批使用，暂不推送。

Windows build-rxspan 的 serial_delivery 1/1 通过；Linux Bridge 温控转发通过。 Windows 日志为上述构建目录的 txspan-{build,test}.log。

## 2026-11-14：只读 span 接收裁决（用户定）

2026-11-14 接收裁决适配：onRX 返回 `std::span<const unsigned char>` 借用接收数据，onData 同步借用只读 span，现有 Qt 解析器在消费处零拷贝适配。留存或异步投递前必须复制取得字节所有权，不能仅浅拷贝视图。协议、重连和关闭逻辑不变；与本地 HandleCore 同批使用，尚未推送。

Windows/MSVC：build-rxspan 后端/串口构建及 serial_delivery 1/1 通过，75 ms 节流与关闭清队列保持。验证日志为相应构建目录下 rxspan-build.log / rxspan-test.log（Linux 使用上述命名）。

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

## 编译警告级别与 qmlcache 豁免（2026-11-09 用户定）

Tcm232Types 与其他官邸一致，MSVC 下开启 `/W4`，并对 `.rcc/qmlcache/` 生成文件局部豁免 C4702（Qt 头在优化阶段报，`/external:W0` 管不到）。开启后发现 Tcm232Protocol.h 一处有符号/无符号比较（C4018），改为显式转换，取值上限检查不变。

验证：主程序原 build 以相邻源码覆盖做 Release 整编，零警告；本仓 build 目录 Release 构建零警告，serial_delivery 1/1（5.34 秒）。未连接实物、未推送。
