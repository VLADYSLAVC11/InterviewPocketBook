// Copyright PocketBook - Interview Task

import QtQuick
import QtQuick.Controls

Dialog {
    id: customDialog

    width: 300
    height: 150

    property int windowWidth: 0
    property int windowHeight: 0

    x: (windowWidth - width) / 2;
    y: (windowHeight - height) / 2;

    modal: true
    standardButtons: Dialog.NoButton

    property string custom_title: ""
    property string message: ""
    property string okButtonText: "OK"

    contentItem: Item {

        anchors.fill: parent

        Rectangle {

            anchors.fill: parent
            color: "white"
            border.color: "gray"
            radius: 10

            Column {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 10

                Text {
                    id: titleLabel
                    text: customDialog.custom_title
                    font.bold: true
                    font.pixelSize: 20
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                Text {
                    id: messageLabel
                    text: customDialog.message
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                CustomButton {
                    id: okButton

                    text: customDialog.okButtonText
                    controlCallback: () => { customDialog.accepted() }
                }
            }
        }
    }

    onAccepted: {
        customDialog.close();
    }
}