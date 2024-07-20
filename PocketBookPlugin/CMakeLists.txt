qt_policy(SET QTP0001 NEW)

set(SRC_QML_FILES
    qml/FileListModelDelegate.qml
    qml/CustomDialogManager.qml
    qml/CustomDialog.qml
    qml/CustomButton.qml
    qml/CustomProgressBar.qml
)

set(SRC_FILES
    filelistmodel.cpp
    filelistmodel.h
    compressionmodel.cpp
    compressionmodel.h
    progressmodel.cpp
    progressmodel.h
)

qt_add_qml_module(pocketbookplugin
    URI "PocketBookPlugin"
    PLUGIN_TARGET pocketbookplugin
    DEPENDENCIES QtQuick
    QML_FILES ${SRC_QML_FILES}
)

target_sources(pocketbookplugin PRIVATE
    ${SRC_FILES}
)

target_link_libraries(pocketbookplugin PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Qml
    Qt::Quick
    Bmp
)

install(TARGETS pocketbookplugin
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}/PocketBookPlugin"
    LIBRARY DESTINATION "${CMAKE_INSTALL_BINDIR}/PocketBookPlugin"
)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/qmldir
    DESTINATION "${CMAKE_INSTALL_BINDIR}/PocketBookPlugin"
)
