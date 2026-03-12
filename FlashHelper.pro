QT       += core gui widgets

TARGET = FlashHelper
TEMPLATE = app

# Setting C++ standard
CONFIG += c++17

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/hexeditor.cpp

HEADERS += \
    src/mainwindow.h \
    src/hexeditor.h

RESOURCES += \
    res/resources.qrc

FORMS += \
    src/mainwindow.ui

# Add packaging info (simplified for qmake)
target.path = /usr/bin
INSTALLS += target
