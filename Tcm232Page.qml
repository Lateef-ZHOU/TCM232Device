pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Lidar.UI
import Lidar.Charts

ModulePage {
    id: root
    required property Section connection
    readonly property string kind: "device"
    readonly property string name: deviceState.displayName
    readonly property string subtitle: deviceState.subtitle
    required property var deviceState
    readonly property list<int> tcCommands: [Tcm232Protocol.TCADJTEMP, Tcm232Protocol.TCACTTEMP, Tcm232Protocol.TCSW, Tcm232Protocol.TCOE, Tcm232Protocol.TCACTVOL, Tcm232Protocol.TCACTCUR, Tcm232Protocol.TCERRORSTATUS]
    readonly property list<int> fanCommands: [Tcm232Protocol.FANSTARTTEMP, Tcm232Protocol.FANACTTEMP, Tcm232Protocol.FANSPEED]
    property var reports: ({})
    property var samples: ({})
    property bool firstRound: false
    property var lastSampleTime
    function finishRound(): void {
        if (lastSampleTime !== undefined) {
            chart.append(lastSampleTime, {
                tcTemperature: samples[Tcm232Protocol.TCACTTEMP] ?? NaN,
                fanTemperature: samples[Tcm232Protocol.FANACTTEMP] ?? NaN,
                outputVoltage: samples[Tcm232Protocol.TCACTVOL] ?? NaN,
                outputCurrent: samples[Tcm232Protocol.TCACTCUR] ?? NaN
            });
        }
        samples = ({});
        lastSampleTime = undefined;
    }
    function queryRound(): void {
        if (!deviceState.isOpen)
            return;
        finishRound();
        // ARP 首次探测可能丢失身份查询；只在首个定时周期补查缺失项。
        if (firstRound) {
            firstRound = false;
            for (const command of [Tcm232Protocol.SYSMODEL, Tcm232Protocol.SYSSN])
                if (reports[command] === undefined)
                    deviceState.query(Tcm232Protocol.SYSTEM, command);
        }
        for (const command of tcCommands)
            deviceState.query(Tcm232Protocol.TC1, command);
        for (const command of fanCommands)
            deviceState.query(Tcm232Protocol.FAN1, command);
    }
    function accept(at, packet: tcm232Packet): void {
        if (packet.module === Tcm232Protocol.CMD && packet.command === Tcm232Protocol.REPLY) {
            const messages = ["模块或参数名错误", "执行成功", "参数名错误", "命令被禁止", "值超出范围", "未知错误", "语法错误", "校验错误", "保存成功"];
            logMessage(messages[packet.value]);
            return;
        }
        const commands = packet.module === Tcm232Protocol.TC1 ? tcCommands : packet.module === Tcm232Protocol.FAN1 ? fanCommands : [];
        const identity = packet.module === Tcm232Protocol.SYSTEM && (packet.command === Tcm232Protocol.SYSMODEL || packet.command === Tcm232Protocol.SYSSN);
        if (!identity && !commands.includes(packet.command))
            return;
        if (reports[packet.command] !== packet.value)
            reports = Object.assign({}, reports, {
                [packet.command]: packet.value
            });
        switch (packet.command) {
        case Tcm232Protocol.TCACTTEMP:
        case Tcm232Protocol.FANACTTEMP:
        case Tcm232Protocol.TCACTVOL:
        case Tcm232Protocol.TCACTCUR:
            samples[packet.command] = packet.value;
            lastSampleTime = at;
        }
    }
    function valueText(command: int, unit: string): string {
        const value = reports[command];
        return value === undefined ? "—" : value + (unit ? " " + unit : "");
    }
    function switchText(command: int): string {
        const value = reports[command];
        return value === 0 ? "关" : value === 1 ? "开" : "—";
    }
    function errorText(): string {
        const bits = reports[Tcm232Protocol.TCERRORSTATUS];
        if (bits === undefined)
            return "—";
        const names = ["外部错误", "过压", "过流", "过温", "温度传感器开路", "负载短路", "负载开路"];
        return names.filter((name, bit) => !(bits & (1 << bit))).join("、") || "正常";
    }
    // 每秒提交完整查询集合；手动命令独立提交，实际串口间隔由 Handle 保证。
    Timer {
        id: poll
        interval: 1000
        repeat: true
        onTriggered: root.queryRound()
    }
    Connections {
        target: root.deviceState as QtObject
        function onLogMessage(text: string): void {
            root.logMessage(text);
        }
        function onConnectionChanged(phase): void {
            if (phase === ResourcePhase.Acquired && root.deviceState.isOpen) {
                root.firstRound = false;
                root.samples = ({});
                root.lastSampleTime = undefined;
                root.reports = ({});
                root.deviceState.query(Tcm232Protocol.SYSTEM, Tcm232Protocol.SYSMODEL);
                root.deviceState.query(Tcm232Protocol.SYSTEM, Tcm232Protocol.SYSSN);
                chart.clear();
                root.queryRound();
                root.firstRound = true;
                poll.restart();
            }
            if (phase === ResourcePhase.AboutToRelease || phase === ResourcePhase.Released) {
                poll.stop();
            }
            if (phase === ResourcePhase.Released && !root.deviceState.isOpen) {
                root.reports = ({});
            }
        }
        function onPacketReceived(at, packet: tcm232Packet): void {
            if (root.deviceState.isOpen)
                root.accept(at, packet);
        }
    }
    component Temperature: ColumnLayout {
        id: editor
        required property int module
        required property int command
        required property string caption
        required property string key
        Layout.fillWidth: true
        Layout.preferredWidth: FormStyle.fieldColumnWidth
        Layout.alignment: Qt.AlignTop
        spacing: FormStyle.rowSpacing
        Readout {
            caption: editor.caption
            text: root.valueText(editor.command, "°C")
        }
        ValueField {
            id: draft
            objectName: editor.key + "Draft"
            Layout.fillWidth: true
            value: ""
            placeholderText: qsTr("输入温度（°C）")
            validator: DoubleValidator {
                notation: DoubleValidator.StandardNotation
                locale: "C"
            }
            onCommitted: v => value = v
        }
        DoubleButtonRow {
            secondToolTip: qsTr("保存设备当前值")
            firstAction: Action {
                objectName: editor.key + "Set"
                text: qsTr("设定")
                enabled: draft.acceptableInput && draft.text.length > 0
                onTriggered: root.deviceState.setParameter(editor.module, editor.command, Number(draft.text))
            }
            secondAction: Action {
                objectName: editor.key + "Save"
                text: qsTr("保存")
                onTriggered: root.deviceState.saveParameter(editor.module, editor.command)
            }
        }
    }

    sections: [connection, tcSection, fanSection]
    readonly property Section tcSection: Section {
        title: qsTr("温控1")
        InfoRow {
            caption: qsTr("型号")
            text: root.valueText(Tcm232Protocol.SYSMODEL, "")
        }
        InfoRow {
            caption: qsTr("SN")
            text: root.valueText(Tcm232Protocol.SYSSN, "")
        }
        ReadoutRow {
            Readout {
                caption: qsTr("实际温度")
                text: root.valueText(Tcm232Protocol.TCACTTEMP, "°C")
            }
            Readout {
                caption: qsTr("输出电压")
                text: root.valueText(Tcm232Protocol.TCACTVOL, "V")
            }
            Readout {
                caption: qsTr("输出电流")
                text: root.valueText(Tcm232Protocol.TCACTCUR, "A")
            }
            Readout {
                caption: qsTr("实际输出")
                text: root.switchText(Tcm232Protocol.TCOE)
            }
        }
        MenuSeparator {
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.fillWidth: true
            uniformCellSizes: true
            spacing: FormStyle.columnSpacing
            Temperature {
                key: "tc"
                caption: qsTr("调节温度")
                module: Tcm232Protocol.TC1
                command: Tcm232Protocol.TCADJTEMP
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                Layout.preferredWidth: FormStyle.fieldColumnWidth
                spacing: FormStyle.rowSpacing
                Readout {
                    caption: qsTr("输出开关")
                    text: root.switchText(Tcm232Protocol.TCSW)
                }
                DoubleButtonRow {
                    firstAction: Action {
                        objectName: "outputOn"
                        text: qsTr("开启")
                        onTriggered: root.deviceState.setParameter(Tcm232Protocol.TC1, Tcm232Protocol.TCSW, 1)
                    }
                    secondAction: Action {
                        text: qsTr("关闭")
                        onTriggered: root.deviceState.setParameter(Tcm232Protocol.TC1, Tcm232Protocol.TCSW, 0)
                    }
                }
            }
        }
        MenuSeparator {
            Layout.fillWidth: true
        }
        FieldRow {
            label: qsTr("错误状态")
            FormLabel {
                objectName: "errors"
                multiline: true
                text: root.errorText()
            }
        }
    }
    readonly property Section fanSection: Section {
        title: qsTr("风扇1")
        ReadoutRow {
            Readout {
                caption: qsTr("实际温度")
                text: root.valueText(Tcm232Protocol.FANACTTEMP, "°C")
            }
            Readout {
                caption: qsTr("实际转速")
                text: root.valueText(Tcm232Protocol.FANSPEED, "rpm")
            }
        }
        Temperature {
            key: "fan"
            caption: qsTr("启动温度")
            module: Tcm232Protocol.FAN1
            command: Tcm232Protocol.FANSTARTTEMP
        }
    }
    mainPages: [
        Page {
            title: qsTr("温度、电流与电压")
            contentItem: LaserTelemetryChart {
                id: chart
                configuration: ({
                        windowSeconds: 120,
                        capacity: 600,
                        temperature: {
                            minimum: -10,
                            maximum: 120,
                            channels: [
                                {
                                    key: "tcTemperature",
                                    name: qsTr("温控1实际温度")
                                },
                                {
                                    key: "fanTemperature",
                                    name: qsTr("风扇1实际温度")
                                }
                            ]
                        },
                        current: {
                            minimum: -2,
                            maximum: 2,
                            channels: [
                                {
                                    key: "outputCurrent",
                                    name: qsTr("实际输出电流")
                                }
                            ]
                        },
                        voltage: {
                            minimum: -10,
                            maximum: 10,
                            channels: [
                                {
                                    key: "outputVoltage",
                                    name: qsTr("实际输出电压")
                                }
                            ]
                        }
                    })
            }
        }
    ]
}
