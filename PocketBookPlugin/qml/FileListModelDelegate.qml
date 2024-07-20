// Copyright PocketBook - Interview Task

import QtQuick
import PocketBookPlugin

Item {
    id: root

    property var dialogManager: null
    property var progressModel: null

    function updateColor(color) {
        nameText.color = color;
        sizeText.color = color;
    }

    height: 50
    width: ListView.view.width

    CompressionModel {
        id: compressionModel

        progressModel: root.progressModel
    }
    Rectangle {
        id: background

        anchors.fill: parent
        border.color: "gray"
        border.width: 1
        color: ListView.isCurrentItem ? "#f0f0f0" : "white"
        radius: 5
        scale: 1.0
        transformOrigin: Item.Center

        MouseArea {
            id: mouseArea

            acceptedButtons: Qt.LeftButton
            anchors.fill: parent
            hoverEnabled: true

            drag.target: Item {
            }

            onClicked: {
                if (fileName.endsWith(".barch")) {
                    compressionModel.decompress(filePath);
                } else if(fileName.endsWith(".bmp")) {
                    compressionModel.compress(filePath);
                } else {
                    if (dialogManager === null) return;
                    dialogManager.showDialog({
                        title: "Message",
                        message: "Unsupported file format!",
                    });
                }
            }
            onEntered: {
                parent.color = "#E0E0E0";
                hoverAnimation.start();
            }
            onExited: {
                parent.color = ListView.isCurrentItem ? "#F0F0F0" : "white";
                updateColor("black");
                unhoverAnimation.start();
            }
            onPressed: {
                parent.color = "black";
                updateColor("white");
            }
            onReleased: {
                parent.color = "#E0E0E0";
                updateColor("black");
            }
        }
        Row {
            anchors.centerIn: parent
            spacing: 10

            Text {
                id: nameText

                color: "black"
                font.pixelSize: 16
                text: fileName
            }
            Rectangle {
                color: "gray"
                height: parent.height
                width: 1
            }
            Text {
                id: sizeText

                color: "black"
                font.pixelSize: 16
                text: fileSize + " kb"
            }
        }
        PropertyAnimation {
            id: hoverAnimation

            duration: 200
            easing.type: Easing.OutQuad
            property: "scale"
            target: background
            to: 1.1
        }
        PropertyAnimation {
            id: unhoverAnimation

            duration: 200
            easing.type: Easing.OutQuad
            property: "scale"
            target: background
            to: 1.0
        }
    }
    Connections {
         target: progressModel
        function onProgressChanged(value) {
            if (value < progressModel.max) {
                mouseArea.enabled = false;
            }
        }
    }

    Connections {
        target: compressionModel
        function onErrorOccured(text) {
            if (dialogManager === null)
                return;

            dialogManager.showDialog({
                title: "Error",
                message: text
            });
        }
    }
}
