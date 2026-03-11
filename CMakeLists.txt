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

# Extract numeric part for project() (e.g. 1.1.3 from v1.1.3-...)
if(GIT_VERSION MATCHES "^v?([0-9]+\\.[0-9]+\\.[0-9]+)")
    set(PROJECT_VERSION_NUMERIC ${CMAKE_MATCH_1})
else()
    set(PROJECT_VERSION_NUMERIC "1.1.3")
endif()

# Strip 'v' prefix from GIT_VERSION for package naming if it exists
if(GIT_VERSION MATCHES "^v")
    string(SUBSTRING "${GIT_VERSION}" 1 -1 GIT_VERSION_CLEAN)
else()
    set(GIT_VERSION_CLEAN "${GIT_VERSION}")
endif()

project(FlashHelper VERSION ${PROJECT_VERSION_NUMERIC} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    resources.qrc
)

add_executable(FlashHelper ${PROJECT_SOURCES})

target_link_libraries(FlashHelper PRIVATE Qt6::Widgets Qt6::Core Qt6::Gui)

# Packaging config (for .deb and .rpm)
set(CPACK_PACKAGE_NAME "flashhelper")
set(CPACK_PACKAGE_VENDOR "Robin")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BIOS Flash Tool GUI")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "flashhelper")
set(CPACK_PACKAGE_VERSION "${GIT_VERSION_CLEAN}")

# DEB specific
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Robin <robin@example.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "flashrom, coreutils, libqt6widgets6, libqt6gui6, libqt6core6")

# RPM specific
# RPM version strings cannot contain dashes in the 'Version' field. 
# CPack handles this by splitting or replacing. 
# We'll keep it as is for now as it seemed to work, or replace - with . if needed.
string(REPLACE "-" "." GIT_VERSION_RPM "${GIT_VERSION_CLEAN}")
set(CPACK_RPM_PACKAGE_VERSION "${GIT_VERSION_RPM}")
set(CPACK_RPM_PACKAGE_REQUIRES "flashrom, coreutils, qt6-qtbase-gui")

include(CPack)

install(TARGETS FlashHelper DESTINATION bin)

# Install desktop entry
install(FILES flashhelper.desktop DESTINATION share/applications)

# Install icon
install(FILES flashhelper.svg DESTINATION share/icons/hicolor/scalable/apps)

# Install Polkit policy (important: this MUST go to this specific path)
install(FILES com.robin.flashhelper.policy DESTINATION share/polkit-1/actions)
