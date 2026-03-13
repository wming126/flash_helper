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
find_package(Qt6 COMPONENTS Widgets Core Gui QUIET)
if(Qt6_FOUND)
    message(STATUS "Found Qt6")
    set(QT_VERSION_MAJOR 6)
    set(QT_LIBRARIES Qt6::Widgets Qt6::Core Qt6::Gui)
    find_package(Qt6 COMPONENTS LinguistTools QUIET)
else()
    find_package(Qt5 COMPONENTS Widgets Core Gui REQUIRED)
    message(STATUS "Found Qt5")
    set(QT_VERSION_MAJOR 5)
    set(QT_LIBRARIES Qt5::Widgets Qt5::Core Qt5::Gui)
    find_package(Qt5 COMPONENTS LinguistTools QUIET)
endif()

# Translations
set(TS_FILES translations/FlashHelper_zh_CN.ts)
if(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
    message(STATUS "Qt${QT_VERSION_MAJOR} LinguistTools found, enabling automatic translation updates")
    if(QT_VERSION_MAJOR EQUAL 6)
        qt_add_translations(FlashHelper TS_FILES ${TS_FILES})
    else()
        qt5_add_translation(QM_FILES ${TS_FILES})
        list(APPEND PROJECT_SOURCES ${QM_FILES})
    endif()
else()
    message(WARNING "Qt${QT_VERSION_MAJOR} LinguistTools NOT found. Translations will not be updated automatically.")
endif()


set(PROJECT_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/mainwindow.ui
    src/hexeditor.cpp
    src/hexeditor.h
    src/chippreviewwidget.cpp
    src/chippreviewwidget.h
    src/localspidriver.cpp
    src/localspidriver.h
    res/resources.qrc
)

add_executable(FlashHelper ${PROJECT_SOURCES})
target_include_directories(FlashHelper PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(FlashHelper PRIVATE ${QT_LIBRARIES})

# Build the helper (for root MMIO access)
add_executable(flashhelper-helper 
    src/helper.cpp 
    src/localspidriver.cpp 
    src/localspidriver.h
)
target_include_directories(flashhelper-helper PRIVATE ${CMAKE_SOURCE_DIR})

# --- Packaging Logic ---
# CPack doesn't work well with absolute paths in install commands.
# Let's use relative paths.

# 1. Install Binary
install(TARGETS FlashHelper flashhelper-helper DESTINATION bin)

# Install bundled flashrom binary
# Note: This assumes flashrom has been built in flashrom/build/
if(EXISTS "${CMAKE_SOURCE_DIR}/flashrom/build/flashrom")
    install(FILES "${CMAKE_SOURCE_DIR}/flashrom/build/flashrom" 
            DESTINATION bin
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    message(STATUS "Including bundled flashrom in the package")
endif()

# 2. Packaging config
set(CPACK_PACKAGE_NAME "flashhelper")
set(CPACK_PACKAGE_VENDOR "Robin")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BIOS Flash Tool GUI (Bundled flashrom)")
set(CPACK_PACKAGE_VERSION "${GIT_VERSION_CLEAN}")
set(CPACK_STRIP_FILES TRUE)

# DEB specific
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Robin <robin@example.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "coreutils, libxkbcommon0, libpci3, libusb-1.0-0, libftdi1-2")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# RPM specific
set(CPACK_RPM_PACKAGE_LICENSE "GPLv2")
set(CPACK_RPM_PACKAGE_GROUP "Applications/System")
set(CPACK_RPM_PACKAGE_REQUIRES "coreutils, pciutils-libs, libusb1, libftdi")

# Include CPack after variables are set
include(CPack)

# 4. Install desktop entry
# Replace hardcoded paths in desktop file during installation if needed, 
# but better to have a clean desktop file or handle it differently.
# For now, install it to the standard location relative to CMAKE_INSTALL_PREFIX.
install(FILES deploy/flashhelper.desktop DESTINATION share/applications)

# 5. Install icon
install(FILES res/flashhelper.svg DESTINATION share/icons/hicolor/scalable/apps)

# 6. Install Polkit policy
install(FILES deploy/com.robin.flashhelper.policy DESTINATION share/polkit-1/actions)
