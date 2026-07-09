cmake_minimum_required(VERSION 3.16)
project(BackupTool_GUI LANGUAGES CXX)

# Qt 5，与课程示例保持一致；若用户环境为 Qt 6，把下面 5 改成 6 即可。
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 常规桌面字号。可用环境变量 BACKUP_FONT_SIZE 覆盖。
set(APP_DEFAULT_FONT_SIZE 10)

# MSVC 默认按系统代码页（GBK）读源文件，而本项目源码为 UTF-8。
# 必须显式指定 /utf-8，否则中文注释/字符串里的字节会被误读，导致大量 C2001/C1057。
if (MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt5 5.12 REQUIRED COMPONENTS Widgets Concurrent)
include(CTest)

set(BACKUP_CORE_SOURCES
    src/core/archive.cpp
    src/core/archive.h
    src/core/backup_task.cpp
    src/core/backup_task.h
    src/core/binary_io.cpp
    src/core/binary_io.h
    src/core/checksum.cpp
    src/core/checksum.h
    src/core/crypto.cpp
    src/core/crypto.h
    src/core/encoding.cpp
    src/core/encoding.h
    src/core/filter.cpp
    src/core/filter.h
    src/core/huffman.cpp
    src/core/huffman.h
    src/core/metadata.cpp
    src/core/metadata.h
    src/core/scanner.cpp
    src/core/scanner.h
    src/core/types.cpp
    src/core/types.h
)

# GUI target
add_executable(BackupTool
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/BackupTab.cpp
    src/BackupTab.h
    src/RestoreTab.cpp
    src/RestoreTab.h
    src/FilterTab.cpp
    src/FilterTab.h
    src/LogPanel.cpp
    src/LogPanel.h
    src/BackendAdapter.cpp
    src/BackendAdapter.h
    src/MockBackend.cpp
    src/MockBackend.h
    src/RealBackend.cpp
    src/RealBackend.h
    src/Theme.cpp
    src/Theme.h
    src/PathPicker.cpp
    src/PathPicker.h
    ${BACKUP_CORE_SOURCES}
)

target_include_directories(BackupTool PRIVATE src)

target_link_libraries(BackupTool
    PRIVATE
        Qt5::Widgets
        Qt5::Concurrent
        bcrypt
        Advapi32
)

# Windows 下复制必要的 Qt DLL
if (WIN32)
    add_custom_command(TARGET BackupTool POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "GUI 构建完成；运行时需 Qt5Widgets.dll 等。"
    )
endif()

# 默认字号；也可通过环境变量 BACKUP_FONT_SIZE 覆盖。
target_compile_definitions(BackupTool PRIVATE
    APP_DEFAULT_FONT_SIZE=${APP_DEFAULT_FONT_SIZE}
)

if (BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        TLS_VERIFY FALSE
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    add_executable(backup_tests
        tests/core_tests.cpp
        ${BACKUP_CORE_SOURCES}
    )
    target_include_directories(backup_tests PRIVATE src)
    target_link_libraries(backup_tests
        PRIVATE
            GTest::gtest_main
            bcrypt
            Advapi32
    )
    include(GoogleTest)
    gtest_discover_tests(backup_tests)

    add_executable(real_backend_smoke
        tests/real_backend_smoke.cpp
        src/BackendAdapter.cpp
        src/BackendAdapter.h
        src/MockBackend.cpp
        src/MockBackend.h
        src/RealBackend.cpp
        src/RealBackend.h
        ${BACKUP_CORE_SOURCES}
    )
    target_include_directories(real_backend_smoke PRIVATE src)
    target_link_libraries(real_backend_smoke
        PRIVATE
            GTest::gtest
            Qt5::Core
            Qt5::Concurrent
            Qt5::Widgets
            bcrypt
            Advapi32
    )
    gtest_discover_tests(real_backend_smoke)

    add_executable(gui_signal_smoke
        tests/gui_signal_smoke.cpp
        src/MainWindow.cpp
        src/MainWindow.h
        src/BackupTab.cpp
        src/BackupTab.h
        src/RestoreTab.cpp
        src/RestoreTab.h
        src/FilterTab.cpp
        src/FilterTab.h
        src/LogPanel.cpp
        src/LogPanel.h
        src/PathPicker.cpp
        src/PathPicker.h
        src/Theme.cpp
        src/Theme.h
        src/BackendAdapter.cpp
        src/BackendAdapter.h
        src/MockBackend.cpp
        src/MockBackend.h
        src/RealBackend.cpp
        src/RealBackend.h
        ${BACKUP_CORE_SOURCES}
    )
    target_include_directories(gui_signal_smoke PRIVATE src)
    target_link_libraries(gui_signal_smoke
        PRIVATE
            GTest::gtest
            Qt5::Widgets
            Qt5::Concurrent
            bcrypt
            Advapi32
    )
    gtest_discover_tests(gui_signal_smoke)
endif()
