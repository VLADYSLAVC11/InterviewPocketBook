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
    anchors.bottomMargin: 15

    background: Rectangle {
        implicitWidth: 200
        implicitHeight: 6
        color: "#e6e6e6"
        radius: 3
    }

    contentItem: Item {
        id: _contentItem

        implicitWidth: 200
        implicitHeight: 4

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

        Text {
            id: messageLabel

            anchors.horizontalCenter: parent.horizontalCenter
            horizontalAlignment: Text.AlignHCenter
            text: progressText
            wrapMode: Text.WordWrap
            anchors.top: _contentItem.bottom
        }
    }
}