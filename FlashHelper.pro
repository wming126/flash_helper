QT       += core gui widgets
qtHaveModule(svg): QT += svg


TARGET = FlashHelper
TEMPLATE = app
VERSION = $$system(cat $$shell_quote($$PWD/VERSION))
isEmpty(VERSION): VERSION = 1.4.1

# Setting C++ standard
CONFIG += c++17
DEFINES += APP_VERSION=\\\"$$VERSION\\\"

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

# Build helper
QMAKE_EXTRA_TARGETS += helper
helper.target = flashhelper-helper
helper.commands = g++ -O2 -std=c++17 ../src/helper.cpp ../src/localspidriver.cpp -I../src -o flashhelper-helper
helper.depends = ../src/helper.cpp ../src/localspidriver.cpp
PRE_TARGETDEPS += flashhelper-helper
QMAKE_DISTCLEAN += flashhelper-helper
