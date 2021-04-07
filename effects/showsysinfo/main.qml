import QtQuick 2.4
import QtQuick.Layouts 1.1
import QtQuick.Window 2.2
import QtGraphicalEffects 1.12

    Rectangle {
        id: taskHandle
        anchors.centerIn: parent

        width: 420
        height: 15
        color: "#d8d8d8"
        radius:  height / 2
        Behavior on scale {
            NumberAnimation { duration: 100 }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton

            onClicked: {
                if(mouse.button == Qt.LeftButton) {
                    showSysInfo.showDesktop();
                }
            }

            onEntered: {
                showSysInfo.enterControlBar()
            }

            onExited: {
                showSysInfo.leaveControlBar()
            }

            onCanceled: {
                showSysInfo.leaveControlBar()
            }
        }
    }
