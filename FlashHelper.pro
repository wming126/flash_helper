QT       += core gui widgets

TARGET = FlashHelper
TEMPLATE = app

# Setting C++ standard
CONFIG += c++17

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/hexeditor.cpp \
    src/chippreviewwidget.cpp \
    src/localspidriver.cpp

HEADERS += \
    src/mainwindow.h \
    src/hexeditor.h \
    src/chippreviewwidget.h \
    src/localspidriver.h

RESOURCES += \
    res/resources.qrc

FORMS += \
    src/mainwindow.ui

TRANSLATIONS += \
    translations/FlashHelper_zh_CN.ts

# Add packaging info (simplified for qmake)
target.path = /usr/bin
INSTALLS += target
