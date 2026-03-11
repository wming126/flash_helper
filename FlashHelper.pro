QT       += core gui widgets

TARGET = FlashHelper
TEMPLATE = app

# Setting C++ standard
CONFIG += c++17

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp

HEADERS += \
    src/mainwindow.h

FORMS += \
    src/mainwindow.ui

# Add packaging info (simplified for qmake)
target.path = /usr/bin
INSTALLS += target
