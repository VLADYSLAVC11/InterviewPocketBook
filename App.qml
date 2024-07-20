// Copyright PocketBook - Interview Task

import QtQuick
import PocketBookPlugin

Item {
    id: mainWindow

    width: 640
    height: 480

    CustomDialogManager {
        id: customDialogManager

        width: mainWindow.width
        height: mainWindow.height
    }

    ProgressModel {
        id: customProgressModel
    }

    ListView {
        id: listView

        width: parent.width * 0.9
        height: parent.height

        anchors.horizontalCenter: parent.horizontalCenter

        model: FileListModel {
            folder: initialFolder
        }

        delegate: FileListModelDelegate {
            id: fileDelegate

            dialogManager: customDialogManager
            progressModel: customProgressModel
        }
    }

    CustomProgressBar {
        id: progressBar

        visible: false
        from: customProgressModel.min
        to: customProgressModel.max
        progressText: customProgressModel.text
    }

    Connections {
        target: customProgressModel
        function onProgressChanged(value) {
            progressBar.visible = true;
            if (value < progressBar.to - 1) {
                progressBar.value = value;
            }
            else {
                progressBar.visible = false;
                progressBar.value = 1;
                customDialogManager.showDialog({
                    title: "Message",
                    message: "Operation Success"
                });
            }
        }
    }
}

