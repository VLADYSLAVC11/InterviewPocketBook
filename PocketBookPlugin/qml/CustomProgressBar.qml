// Copyright PocketBook - Interview Task

import QtQuick
import QtQuick.Controls

ProgressBar {
    id: progressBar
    value: 1
    padding: 2

    property var progressText: ""

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 10

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 6
        color: "#e6e6e6"
        radius: 3
    }

    contentItem: Item {
        implicitWidth: 200
        implicitHeight: 4

        Text {
            id: messageLabel
            text: progressText
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Rectangle {
            width: progressBar.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: "black"
            visible: !progressBar.indeterminate
        }

        Item {
            anchors.fill: parent
            visible: progressBar.indeterminate
            clip: true

            Row {
                spacing: 20

                Repeater {
                    model: progressBar.width / 40 + 1

                    Rectangle {
                        color: "black"
                        width: 20
                        height: progressBar.height
                    }
                }
                XAnimator on x {
                    from: 0
                    to: -40
                    loops: Animation.Infinite
                    running: progressBar.indeterminate
                }
            }
        }
    }
}