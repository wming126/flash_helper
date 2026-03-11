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
    set(GIT_VERSION "1.1.3")
endif()

# Extract numeric part for project()
if(GIT_VERSION MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(PROJECT_VERSION_NUMERIC ${CMAKE_MATCH_1})
else()
    set(PROJECT_VERSION_NUMERIC "1.1.3")
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

# --- Fat Packaging Logic ---
# Set binary installation to /opt/flashhelper/bin
set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui DBus)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

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
target_link_libraries(FlashHelper PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui Qt6::DBus)

# 1. Install Binary
install(TARGETS FlashHelper DESTINATION bin)

# 2. Bundle Runtime Dependencies (Libraries)
install(CODE [[
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES $<TARGET_FILE:FlashHelper>
        RESOLVED_DEPENDENCIES_VAR _resolved_deps
        UNRESOLVED_DEPENDENCIES_VAR _unresolved_deps
    )
    foreach(_file ${_resolved_deps})
        # Bundle non-standard libs (Skip core OS libs)
        if(NOT _file MATCHES "libc\\.so|libm\\.so|libpthread\\.so|libdl\\.so|librt\\.so|libstdc\\+\\+\\.so|libgcc_s\\.so|libX11\\.so|libxcb\\.so|libGL\\.so|libGLX\\.so|libEGL\\.so|libfontconfig\\.so|libexpat\\.so|libdbus-1\\.so|libz\\.so|libsystemd\\.so|libselinux\\.so|libcap\\.so|libgpg-error\\.so|libgcrypt\\.so|libmount\\.so|libblkid\\.so|libuuid\\.so")
            file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "${_file}")
        endif()
    endforeach()
]])

# 3. Bundle Mandatory Qt Plugins (platforms)
set(QT_PLUGINS_DIR "/usr/lib/x86_64-linux-gnu/qt6/plugins")
install(DIRECTORY "${QT_PLUGINS_DIR}/platforms" DESTINATION lib/qt-plugins)

# 4. Create qt.conf
install(CODE [[
    file(WRITE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/qt.conf" "[Paths]\nPlugins=../lib/qt-plugins\n")
]])

# 5. Packaging config (for .deb and .rpm)
set(CPACK_PACKAGE_NAME "flashhelper")
set(CPACK_PACKAGE_VENDOR "Robin")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BIOS Flash Tool GUI (Self-contained)")
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/flashhelper")
set(CPACK_PACKAGE_VERSION "${GIT_VERSION_CLEAN}")

# DEB specific
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Robin <robin@example.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "flashrom, coreutils, libxkbcommon0, libpci3, libftdi1-2")

# RPM specific
string(REPLACE "-" "." GIT_VERSION_RPM "${GIT_VERSION_CLEAN}")
set(CPACK_RPM_PACKAGE_VERSION "${GIT_VERSION_RPM}")
set(CPACK_RPM_PACKAGE_REQUIRES "flashrom, coreutils, libxkbcommon, pciutils-libs, libftdi")

include(CPack)

# 6. Install desktop entry (Standard path)
install(CODE "
    file(READ \"${CMAKE_SOURCE_DIR}/deploy/flashhelper.desktop\" DESKTOP_CONTENT)
    string(REPLACE \"Exec=FlashHelper\" \"Exec=/opt/flashhelper/bin/FlashHelper\" DESKTOP_CONTENT \"\${DESKTOP_CONTENT}\")
    string(REPLACE \"Icon=flashhelper\" \"Icon=/usr/share/icons/hicolor/scalable/apps/flashhelper.svg\" DESKTOP_CONTENT \"\${DESKTOP_CONTENT}\")
    file(WRITE \"\$ENV{DESTDIR}/usr/share/applications/flashhelper.desktop\" \"\${DESKTOP_CONTENT}\")
")

# 7. Install icon (Standard path)
install(FILES res/flashhelper.svg DESTINATION /usr/share/icons/hicolor/scalable/apps)

# 8. Install Polkit policy (Standard path)
install(FILES deploy/com.robin.flashhelper.policy DESTINATION /usr/share/polkit-1/actions)
