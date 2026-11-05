pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Lidar.UI
import Lidar.Charts

Tcm232Page {
    id: root
    objectName: "Tcm232Module"
    deviceState: root.Tcm232Device
    connection: ConnectionSection {
        title: qsTr("设备连接")

        label: qsTr("串口")
        ChoiceBox {
            id: serial
            Layout.fillWidth: true
            model: root.deviceState.ports()
        }

        firstAction: Action {
            text: qsTr("连接")
            onTriggered: root.deviceState.open(serial.currentText)
        }
        secondAction: Action {
            text: qsTr("断开")
            onTriggered: root.deviceState.close()
        }
    }
}
