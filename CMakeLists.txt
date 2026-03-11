cmake_minimum_required(VERSION 3.16)

project(FlashHelper VERSION 1.0.0 LANGUAGES CXX)

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
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

# DEB specific
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Robin <robin@example.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "flashrom, coreutils, libqt6widgets6, libqt6gui6, libqt6core6")

# RPM specific
set(CPACK_RPM_PACKAGE_REQUIRES "flashrom, coreutils, qt6-qtbase-gui")

include(CPack)

install(TARGETS FlashHelper DESTINATION bin)

# Install desktop entry
install(FILES flashhelper.desktop DESTINATION share/applications)

# Install icon
install(FILES flashhelper.svg DESTINATION share/icons/hicolor/scalable/apps)

# Install Polkit policy (important: this MUST go to this specific path)
install(FILES com.robin.flashhelper.policy DESTINATION share/polkit-1/actions)
