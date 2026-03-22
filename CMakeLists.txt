cmake_minimum_required(VERSION 3.16)
project(GXTStudio LANGUAGES CXX)

include(qt.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置 x64 架构
set(CMAKE_GENERATOR_PLATFORM x64)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        Concurrent
        Network
)

# 查找 zlib (x64) - 优先使用静态链接避免 DLL 依赖
set(ZLIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/zlib" CACHE PATH "zlib root directory")

# 尝试多种方式查找 zlib
find_package(ZLIB QUIET)

if(NOT ZLIB_FOUND)
    # 优先查找静态库 (避免 DLL 架构不匹配问题)
    find_library(ZLIB_LIBRARY NAMES zlibstatic zlibstat z
        PATHS
            "${ZLIB_ROOT}/lib"
            "${ZLIB_ROOT}/lib/x64"
            "${ZLIB_ROOT}/build/Release"
            "${ZLIB_ROOT}/build/Debug"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib"
            "${CMAKE_CURRENT_SOURCE_DIR}/lib/x64"
        PATH_SUFFIXES lib lib/x64
    )
    
    # 如果没找到静态库，再找动态库
    if(NOT ZLIB_LIBRARY)
        find_library(ZLIB_LIBRARY NAMES zlib z zlib1
            PATHS
                "${ZLIB_ROOT}/bin"
                "${ZLIB_ROOT}/lib"
                "${CMAKE_CURRENT_SOURCE_DIR}/lib"
            PATH_SUFFIXES lib lib/x64 bin
        )
    endif()
    
    find_path(ZLIB_INCLUDE_DIR zlib.h
        PATHS
            "${ZLIB_ROOT}/include"
            "${ZLIB_ROOT}/build"
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
        PATH_SUFFIXES include
    )
    
    if(ZLIB_LIBRARY AND ZLIB_INCLUDE_DIR)
        set(ZLIB_FOUND TRUE)
        set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
        set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})
        message(STATUS "Found zlib library: ${ZLIB_LIBRARY}")
        message(STATUS "Found zlib include: ${ZLIB_INCLUDE_DIR}")
    endif()
endif()

if(ZLIB_FOUND)
    message(STATUS "zlib found - x64 support enabled")
    add_definitions(-DHAVE_ZLIB)
else()
    message(WARNING "zlib not found - compression features will be disabled")
endif()
qt_standard_project_setup()

set(PROJECT_SOURCES
    main.cpp
    GXTStudio.ui
    GXTStudio.h
    GXTStudio.cpp
    FontAwesome.h
    WelcomeWidget.h
    WelcomeWidget.cpp
    MultiThreadProgressDialog.h
    MultiThreadProgressDialog.cpp
    GXTTableModel.h
    GXTTableModel.cpp
    ParseWorker.h
    ParseWorker.cpp
    SaveWorker.h
    SaveWorker.cpp
    ItemPool.h
    ItemPool.cpp
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
    CharTableExportWorker.h
    CharTableExportWorker.cpp
    ReplaceDialog.h
    ReplaceDialog.cpp
    ReplaceWorker.h
    ReplaceWorker.cpp
    AboutDialog.h
    AboutDialog.cpp
    BackgroundConfigDialog.h
    BackgroundConfigDialog.cpp
)

# 设置 Windows 资源文件（用于应用程序图标）
set(WINDOWS_RESOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/app.rc")
file(WRITE ${WINDOWS_RESOURCE_FILE} "IDI_ICON1 ICON DISCARDABLE \"${CMAKE_CURRENT_SOURCE_DIR}/icon/favicon.ico\"\n")

qt_add_executable(${PROJECT_NAME} ${PROJECT_SOURCES} ${WINDOWS_RESOURCE_FILE})

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

# 链接 zlib (x64)
if(ZLIB_FOUND)
    target_include_directories(${PROJECT_NAME} PRIVATE ${ZLIB_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${ZLIB_LIBRARIES})
    message(STATUS "Linked zlib: ${ZLIB_LIBRARIES}")
endif()

# 复制 zlib DLL 到输出目录 (x64)
if(ZLIB_FOUND AND EXISTS "${ZLIB_ROOT}/bin/zlib1.dll")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${ZLIB_ROOT}/bin/zlib1.dll"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying zlib1.dll to build directory"
    )
endif()

# 复制字体文件到构建输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/font"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/font/Font Awesome 7 Free-Solid-900.otf"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/font/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/font/ViceCitySans.otf"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/font/"
    COMMENT "Copying font files to build directory"
)

# 复制图标文件到构建输出目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/icon"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/icon/favicon.ico"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/icon/"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/icon/favicon.png"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/icon/"
    COMMENT "Copying icon files to build directory"
)

