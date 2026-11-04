pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Lidar.UI

ModulePage {
    id: root
    objectName: "Tcm232Module"
    readonly property string kind: "device"
    readonly property string name: deviceState.displayName
    readonly property string subtitle: deviceState.subtitle
    readonly property Tcm232Device deviceState: root.Tcm232Device
    readonly property list<int> tcCommands: [Tcm232Protocol.TCADJTEMP, Tcm232Protocol.TCACTTEMP,
        Tcm232Protocol.TCSW, Tcm232Protocol.TCOE, Tcm232Protocol.TCACTVOL, Tcm232Protocol.TCACTCUR,
        Tcm232Protocol.TCERRORSTATUS]
    readonly property list<int> fanCommands: [Tcm232Protocol.FANSTARTTEMP, Tcm232Protocol.FANACTTEMP, Tcm232Protocol.FANSPEED]
    property var reports: ({})
    function refresh(): void {
        for (const c of tcCommands) deviceState.query(Tcm232Protocol.TC1, c)
        for (const c of fanCommands) deviceState.query(Tcm232Protocol.FAN1, c)
    }
    function accept(packet: tcm232Packet): void {
        if (packet.module === Tcm232Protocol.CMD && packet.command === Tcm232Protocol.REPLY) {
            const messages = ["模块或参数名错误", "执行成功", "参数名错误", "命令被禁止", "值超出范围",
                              "未知错误", "语法错误", "校验错误", "保存成功"]
            logMessage(messages[packet.value])
            return
        }
        const commands = packet.module === Tcm232Protocol.TC1 ? tcCommands
                       : packet.module === Tcm232Protocol.FAN1 ? fanCommands : []
        if (!commands.includes(packet.command) || reports[packet.command] === packet.value) return
        reports = Object.assign({}, reports, { [packet.command]: packet.value })
    }
    function valueText(command: int, unit: string): string {
        const value = reports[command]
        return value === undefined ? "—" : value + (unit ? " " + unit : "")
    }
    function switchText(command: int): string {
        const value = reports[command]
        return value === 0 ? "关" : value === 1 ? "开" : "—"
    }
    function errorText(): string {
        const bits = reports[Tcm232Protocol.TCERRORSTATUS]
        if (bits === undefined) return "—"
        const names = ["外部错误", "过压", "过流", "过温", "温度传感器开路", "负载短路", "负载开路"]
        return names.filter((name, bit) => !(bits & (1 << bit))).join("、") || "正常"
    }
    Timer { id: poll; interval: 1000; repeat: true; triggeredOnStart: true; onTriggered: root.refresh() }
    Connections {
        target: root.deviceState as QtObject
        function onLogMessage(text: string): void { root.logMessage(text) }
        function onConnectionChanged(phase): void {
            if (phase === ResourcePhase.Acquired && root.deviceState.isOpen) poll.restart()
            if (phase === ResourcePhase.Released && !root.deviceState.isOpen) {
                poll.stop()
                root.reports = ({})
            }
        }
        function onPacketReceived(at, packet: tcm232Packet): void {
            if (root.deviceState.isOpen) root.accept(packet)
        }
    }
    component Reading: ColumnLayout {
        property alias caption: heading.text
        property alias reading: result.text
        Layout.fillWidth: true
        Layout.preferredWidth: 120
        Layout.alignment: Qt.AlignTop
        Label { id: heading; opacity: 0.75 }
        Label { id: result; font.bold: true; Layout.fillWidth: true; wrapMode: Text.Wrap }
    }
    component Temperature: ColumnLayout {
        id: editor
        required property int module
        required property int command
        required property string caption
        required property string key
        Layout.fillWidth: true
        Layout.preferredWidth: 160
        Layout.alignment: Qt.AlignTop
        Reading { caption: editor.caption; reading: root.valueText(editor.command, "°C") }
        ValueField {
            id: draft
            objectName: editor.key + "Draft"
            Layout.fillWidth: true
            value: ""
            placeholderText: qsTr("输入温度（°C）")
            validator: DoubleValidator { notation: DoubleValidator.StandardNotation; locale: "C" }
            onCommitted: v => value = v
        }
        RowLayout {
            Layout.fillWidth: true
            Button {
                objectName: editor.key + "Set"
                Layout.fillWidth: true; text: qsTr("设定")
                enabled: draft.acceptableInput && draft.text.length > 0
                onClicked: root.deviceState.setParameter(editor.module, editor.command, Number(draft.text))
            }
            Button {
                objectName: editor.key + "Save"
                Layout.fillWidth: true; text: qsTr("保存")
                ToolTip.text: qsTr("保存设备当前值"); ToolTip.visible: hovered
                onClicked: root.deviceState.saveParameter(editor.module, editor.command)
            }
        }
    }

    sections: [ Section {
        title: qsTr("设备连接")
        FieldRow {
            label: qsTr("串口")
            ChoiceBox { id: serial; Layout.fillWidth: true; model: root.deviceState.ports() }
        }
        RowLayout {
            Layout.fillWidth: true; uniformCellSizes: true
            Button { Layout.fillWidth: true; text: qsTr("连接"); onClicked: root.deviceState.open(serial.currentText) }
            Button { Layout.fillWidth: true; text: qsTr("断开"); onClicked: root.deviceState.close() }
        }
    } ]
    mainPages: [ Page {
        title: qsTr("温控与风扇")
        ScrollView {
            id: scroll
            anchors.fill: parent
            contentWidth: availableWidth
            ColumnLayout {
                width: scroll.availableWidth
                spacing: 12
                Section {
                    title: qsTr("温控1")
                    RowLayout {
                        Layout.fillWidth: true; uniformCellSizes: true
                        Reading { caption: qsTr("实际温度"); reading: root.valueText(Tcm232Protocol.TCACTTEMP, "°C") }
                        Reading { caption: qsTr("输出电压"); reading: root.valueText(Tcm232Protocol.TCACTVOL, "V") }
                        Reading { caption: qsTr("输出电流"); reading: root.valueText(Tcm232Protocol.TCACTCUR, "A") }
                        Reading { caption: qsTr("实际输出"); reading: root.switchText(Tcm232Protocol.TCOE) }
                    }
                    MenuSeparator { Layout.fillWidth: true }
                    RowLayout {
                        Layout.fillWidth: true; uniformCellSizes: true
                        Temperature { key: "tc"; caption: qsTr("调节温度"); module: Tcm232Protocol.TC1; command: Tcm232Protocol.TCADJTEMP }
                        ColumnLayout {
                            Layout.fillWidth: true; Layout.alignment: Qt.AlignTop
                            Reading { caption: qsTr("输出开关"); reading: root.switchText(Tcm232Protocol.TCSW) }
                            RowLayout {
                                Button { objectName: "outputOn"; text: qsTr("开启"); onClicked: root.deviceState.setParameter(Tcm232Protocol.TC1, Tcm232Protocol.TCSW, 1) }
                                Button { text: qsTr("关闭"); onClicked: root.deviceState.setParameter(Tcm232Protocol.TC1, Tcm232Protocol.TCSW, 0) }
                            }
                        }
                    }
                    MenuSeparator { Layout.fillWidth: true }
                    FieldRow { label: qsTr("错误状态"); Label { objectName: "errors"; Layout.fillWidth: true; wrapMode: Text.Wrap; text: root.errorText() } }
                }
                Section {
                    title: qsTr("风扇1")
                    RowLayout {
                        Layout.fillWidth: true; uniformCellSizes: true
                        Reading { caption: qsTr("实际温度"); reading: root.valueText(Tcm232Protocol.FANACTTEMP, "°C") }
                        Reading { caption: qsTr("实际转速"); reading: root.valueText(Tcm232Protocol.FANSPEED, "rpm") }
                        Temperature { key: "fan"; caption: qsTr("启动温度"); module: Tcm232Protocol.FAN1; command: Tcm232Protocol.FANSTARTTEMP }
                    }
                }
            }
        }
    } ]
    mainMargin: 10
}
