cmake_minimum_required(VERSION 3.16)
project(BackupTool_GUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)
if (MSVC)
    add_compile_options(/utf-8 /Zc:__cplusplus)
endif()

# Core library sources
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

# Main executable (Win32 GUI, no Qt)
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
    src/RealBackend.cpp
    src/RealBackend.h
    src/Theme.cpp
    src/Theme.h
    src/PathPicker.cpp
    src/PathPicker.h
     src/ProgressDialog.cpp
     src/ProgressDialog.h
     src/SessionHistory.cpp
     src/SessionHistory.h
     src/MockBackend.cpp
     src/MockBackend.h
    ${BACKUP_CORE_SOURCES}
)

target_include_directories(BackupTool PRIVATE src)
if (MSVC)
    set_target_properties(BackupTool PROPERTIES WIN32_EXECUTABLE TRUE)
    target_link_libraries(BackupTool
        PRIVATE
            Qt6::Widgets
            Qt6::EntryPointPrivate
            advapi32
            bcrypt
            comctl32
            comdlg32
            gdi32
            kernel32
            ole32
            shell32
            user32
            uuid
    )
else()
    target_link_libraries(BackupTool
        PRIVATE
            -mwindows
            Qt6::Widgets
            Qt6::Widgets
            Qt6::EntryPointPrivate
            advapi32
            bcrypt
            comctl32
            comdlg32
            gdi32
            kernel32
            ole32
            shell32
            user32
            uuid
            mingw32
    )
endif()

if (WIN32)
    target_compile_definitions(BackupTool PRIVATE _UNICODE UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endif()





