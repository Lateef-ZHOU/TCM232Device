pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Lidar.UI

Tcm232Page {
    id: root
    objectName: "Tcm232UdpModule"
    deviceState: internals.device
    QtObject {
        id: internals
        readonly property Tcm232UdpDevice device: root.Tcm232UdpDevice
        readonly property var parameters: device.connectionParameters
    }
    connection: ConnectionSection {
        title: qsTr("设备连接")

        label: qsTr("网卡")
        ChoiceField {
            id: interfaceSelector
            objectName: "networkInterfaceSelector"
            Layout.fillWidth: true
            model: internals.parameters.networkInterfaceOptions
            tips: internals.parameters.networkInterfaceTips
            onCommitted: i => value = i
        }

        firstAction: Action {
            text: qsTr("连接")
            onTriggered: {
                if (internals.device.open(interfaceSelector.value))
                    root.logMessage(qsTr("连接请求已提交"));
            }
        }
        secondAction: Action {
            text: qsTr("断开")
            onTriggered: internals.device.close()
        }
    }
}
