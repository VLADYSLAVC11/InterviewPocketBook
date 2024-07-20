cmake_minimum_required(VERSION 3.16)
project(PocketBook LANGUAGES CXX)

find_package(Qt6 REQUIRED COMPONENTS Qml Quick QuickControls2)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(pocketbook
    main.cpp
)

set_target_properties(pocketbook PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(pocketbook PRIVATE
    Qt::Qml
    Qt::Quick
    Qt::QuickControls2
)

qt_add_qml_module(pocketbook
    URI PocketBookApp
    QML_FILES App.qml
)

add_subdirectory(PocketBookPlugin)
add_subdirectory(BmpLib)

install(TARGETS pocketbook
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_qml_app_script(
    TARGET pocketbook
    OUTPUT_SCRIPT deploy_script
    MACOS_BUNDLE_POST_BUILD
    NO_UNSUPPORTED_PLATFORM_ERROR
    DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
)
install(SCRIPT ${deploy_script})


