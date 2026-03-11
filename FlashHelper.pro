QT       += core gui widgets

TARGET = FlashHelper
TEMPLATE = app

# Setting C++ standard
CONFIG += c++17

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui

# Add packaging info (simplified for qmake)
target.path = /usr/bin
INSTALLS += target
