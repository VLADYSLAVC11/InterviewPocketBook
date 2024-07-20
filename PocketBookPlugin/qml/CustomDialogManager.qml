// Copyright PocketBook - Interview Task

import QtQuick
import QtQuick.Controls

Item {
    id: dialogManager

    property var dialogConfig: ({})

    function showDialog(config) {
        dialogLoader.source = "";
        dialogConfig = config;
        dialogLoader.source = "CustomDialog.qml";
    }

    Loader {
        id: dialogLoader

        active: dialogConfig !== null

        onLoaded: {
            item.custom_title = dialogConfig.title || ""
            item.message = dialogConfig.message || ""
            item.okButtonText = dialogConfig.okButtonText || "OK"
            item.windowWidth = dialogManager.width
            item.windowHeight = dialogManager.height
            item.accepted.connect(dialogConfig.onAccepted || function() {})
            item.open()
        }
    }
}