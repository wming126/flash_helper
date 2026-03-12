cmake_minimum_required(VERSION 3.16)

# Get version from Git
execute_process(
    COMMAND git describe --tags --always --dirty --long
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    RESULT_VARIABLE GIT_RESULT
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Fallback version if git fails
if(NOT GIT_RESULT EQUAL 0 OR GIT_VERSION STREQUAL "")
    set(GIT_VERSION "1.2.0")
endif()

# Extract numeric part for project()
if(GIT_VERSION MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(PROJECT_VERSION_NUMERIC ${CMAKE_MATCH_1})
else()
    set(PROJECT_VERSION_NUMERIC "1.2.0")
endif()

# Strip 'v' prefix from GIT_VERSION
if(GIT_VERSION MATCHES "^v")
    string(SUBSTRING "${GIT_VERSION}" 1 -1 GIT_VERSION_CLEAN)
else()
    set(GIT_VERSION_CLEAN "${GIT_VERSION}")
endif()

project(FlashHelper VERSION ${PROJECT_VERSION_NUMERIC} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt
find_package(Qt6 COMPONENTS Widgets Core Gui DBus Svg QUIET)
if(Qt6_FOUND)
    message(STATUS "Found Qt6")
    set(QT_LIBRARIES Qt6::Widgets Qt6::Core Qt6::Gui Qt6::DBus Qt6::Svg)
    set(QT_VERSION_MAJOR 6)
else()
    find_package(Qt5 COMPONENTS Widgets Core Gui DBus Svg REQUIRED)
    message(STATUS "Found Qt5")
    set(QT_LIBRARIES Qt5::Widgets Qt5::Core Qt5::Gui Qt5::DBus Qt5::Svg)
    set(QT_VERSION_MAJOR 5)
endif()

set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/mainwindow.ui
    src/hexeditor.cpp
    src/hexeditor.h
    res/resources.qrc
)

add_executable(FlashHelper ${PROJECT_SOURCES})
target_include_directories(FlashHelper PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(FlashHelper PRIVATE ${QT_LIBRARIES})

# --- Packaging Logic ---
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# 1. Install Binary
install(TARGETS FlashHelper DESTINATION bin)

# 2. Bundle Runtime Dependencies (Libraries) - 仅在非 CPack 模式下由脚本处理或由 CPack 自动处理
# 这里简化，不使用硬编码路径

# 3. Packaging config
set(CPACK_PACKAGE_NAME "flashhelper")
set(CPACK_PACKAGE_VENDOR "Robin")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BIOS Flash Tool GUI")
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/flashhelper")
set(CPACK_PACKAGE_VERSION "${GIT_VERSION_CLEAN}")

# DEB specific
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Robin <robin@example.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "flashrom, coreutils, libxkbcommon0")

include(CPack)

# 4. Install desktop entry
install(CODE "
    file(READ \"${CMAKE_SOURCE_DIR}/deploy/flashhelper.desktop\" DESKTOP_CONTENT)
    string(REPLACE \"Exec=FlashHelper\" \"Exec=/opt/flashhelper/bin/FlashHelper\" DESKTOP_CONTENT \"\${DESKTOP_CONTENT}\")
    string(REPLACE \"Icon=flashhelper\" \"Icon=/usr/share/icons/hicolor/scalable/apps/flashhelper.svg\" DESKTOP_CONTENT \"\${DESKTOP_CONTENT}\")
    file(WRITE \"\$ENV{DESTDIR}/usr/share/applications/flashhelper.desktop\" \"\${DESKTOP_CONTENT}\")
")

# 5. Install icon
install(FILES res/flashhelper.svg DESTINATION /usr/share/icons/hicolor/scalable/apps)

# 6. Install Polkit policy
install(FILES deploy/com.robin.flashhelper.policy DESTINATION /usr/share/polkit-1/actions)
