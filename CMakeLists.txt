cmake_minimum_required(VERSION 3.16)
project(BackupTool LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (MSVC)
    add_compile_options(/utf-8 /Zc:__cplusplus)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
include(CTest)

set(QT_WIDGETS_TARGET Qt${QT_VERSION_MAJOR}::Widgets)

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

set(BACKUP_APP_SOURCES
    src/BackendAdapter.cpp
    src/BackendAdapter.h
    src/BackupTab.cpp
    src/BackupTab.h
    src/FilterTab.cpp
    src/FilterTab.h
    src/LogPanel.cpp
    src/LogPanel.h
    src/MainWindow.cpp
    src/MainWindow.h
    src/PathPicker.cpp
    src/PathPicker.h
    src/ProgressDialog.cpp
    src/ProgressDialog.h
    src/RealBackend.cpp
    src/RealBackend.h
    src/RestoreTab.cpp
    src/RestoreTab.h
    src/SessionHistory.cpp
    src/SessionHistory.h
    src/Theme.cpp
    src/Theme.h
)

add_executable(BackupTool WIN32
    src/main.cpp
    ${BACKUP_APP_SOURCES}
    ${BACKUP_CORE_SOURCES}
)

target_include_directories(BackupTool PRIVATE src)
target_link_libraries(BackupTool
    PRIVATE
        ${QT_WIDGETS_TARGET}
        bcrypt
        Advapi32
)

if (WIN32 AND TARGET Qt${QT_VERSION_MAJOR}::WinMain)
    target_link_libraries(BackupTool PRIVATE Qt${QT_VERSION_MAJOR}::WinMain)
endif()
if (WIN32 AND TARGET Qt${QT_VERSION_MAJOR}::EntryPointPrivate)
    target_link_libraries(BackupTool PRIVATE Qt${QT_VERSION_MAJOR}::EntryPointPrivate)
endif()

if (BUILD_TESTING)
    option(BACKUP_ALLOW_GTEST_DOWNLOAD "Allow CMake to download GoogleTest when it is not installed" OFF)
    find_package(GTest QUIET)
    set(BACKUP_HAS_GTEST OFF)

    if (GTest_FOUND)
        if (TARGET GTest::gtest)
            set(BACKUP_GTEST_LIB GTest::gtest)
        elseif (TARGET GTest::GTest)
            set(BACKUP_GTEST_LIB GTest::GTest)
        endif()
        if (TARGET GTest::gtest_main)
            set(BACKUP_GTEST_MAIN_LIB GTest::gtest_main)
        elseif (TARGET GTest::Main)
            set(BACKUP_GTEST_MAIN_LIB GTest::Main)
        endif()
        if (BACKUP_GTEST_LIB AND BACKUP_GTEST_MAIN_LIB)
            set(BACKUP_HAS_GTEST ON)
        endif()
    elseif (BACKUP_ALLOW_GTEST_DOWNLOAD)
        include(FetchContent)
        FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            TLS_VERIFY TRUE
        )
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
        set(BACKUP_HAS_GTEST ON)
        set(BACKUP_GTEST_LIB GTest::gtest)
        set(BACKUP_GTEST_MAIN_LIB GTest::gtest_main)
    else()
        message(WARNING "GoogleTest not found; tests are skipped. Install GTest or pass -DBACKUP_ALLOW_GTEST_DOWNLOAD=ON.")
    endif()

    if (BACKUP_HAS_GTEST)
        add_executable(backup_tests
            tests/core_tests.cpp
            ${BACKUP_CORE_SOURCES}
        )
        target_include_directories(backup_tests PRIVATE src src/core)
        target_link_libraries(backup_tests
            PRIVATE
                ${BACKUP_GTEST_MAIN_LIB}
                bcrypt
                Advapi32
        )
        include(GoogleTest)
        gtest_add_tests(TARGET backup_tests)

        add_executable(real_backend_smoke
            tests/real_backend_smoke.cpp
            src/BackendAdapter.cpp
            src/BackendAdapter.h
            src/RealBackend.cpp
            src/RealBackend.h
            ${BACKUP_CORE_SOURCES}
        )
        target_include_directories(real_backend_smoke PRIVATE src src/core)
        target_link_libraries(real_backend_smoke
            PRIVATE
                ${BACKUP_GTEST_MAIN_LIB}
                ${QT_WIDGETS_TARGET}
                bcrypt
                Advapi32
        )
        gtest_add_tests(TARGET real_backend_smoke)

        add_executable(gui_signal_smoke
            tests/gui_signal_smoke.cpp
            ${BACKUP_APP_SOURCES}
            ${BACKUP_CORE_SOURCES}
        )
        target_include_directories(gui_signal_smoke PRIVATE src src/core)
        target_link_libraries(gui_signal_smoke
            PRIVATE
                ${BACKUP_GTEST_LIB}
                ${QT_WIDGETS_TARGET}
                bcrypt
                Advapi32
        )
        gtest_add_tests(TARGET gui_signal_smoke)
    endif()
endif()
