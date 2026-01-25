cmake_minimum_required(VERSION 3.16)
project(GXTStudio LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Concurrent
        Network
)
qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    GXTStudio.ui
    GXTStudio.h
    GXTStudio.cpp
    GXTParser.h
    GXTParser.cpp
    GXTEditor.h
    GXTEditor.cpp
    TranslateConfigDialog.h
    TranslateConfigDialog.cpp
    SmartTranslator.h
    SmartTranslator.cpp
    CodeTableConverter.h
    CodeTableConverter.cpp
    TextRenderWidget.h
    TextRenderWidget.cpp
    CharTableParser.h
    CharTableParser.cpp
    CharTableWidget.h
    CharTableWidget.cpp
    AboutDialog.h
    AboutDialog.cpp
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        WIN32_EXECUTABLE TRUE
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Qt::Core
        Qt::Gui
        Qt::Widgets
        Qt::Concurrent
        Qt::Network
)

# 复制字体文件到构建输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/font"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/font/Font Awesome 7 Free-Solid-900.otf"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/font/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/font/Font Awesome 7 Free-Regular-400.otf"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/font/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/font/Font Awesome 7 Brands-Regular-400.otf"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/font/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/font/ViceCitySans.otf"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/font/"
    COMMENT "Copying font files to build directory"
)

