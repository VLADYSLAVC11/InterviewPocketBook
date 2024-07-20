// Copyright PocketBook - Interview Task

import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control

    required property var controlCallback

    contentItem: Text {
        id: controlText

        text: control.text
        font: control.font
        opacity: enabled ? 1.0 : 0.3
        color: "black"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        id: controlBackground

        anchors.fill: parent
        implicitWidth: 100
        implicitHeight: 40
        color: "white"
        opacity: enabled ? 1 : 0.3
        border.color: "grey"
        border.width: 1
        radius: 10
    }

    Timer {
        id: delayTimer

        interval: 200
        repeat: false

        onTriggered: {
            if (controlCallback) {
                controlCallback();
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true

        onPressed: {
            controlBackground.color = "black"
            controlText.color = "white"
            delayTimer.start();
        }

        onReleased: {
            controlBackground.color = "#E0E0E0"
            controlText.color = "black"
        }

        onEntered: {
            controlBackground.color = "#E0E0E0"
        }

        onExited: {
            controlBackground.color = "white"
        }
    }

    anchors.horizontalCenter: parent.horizontalCenter
}