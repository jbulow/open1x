# -----------------------------------------------------------
# This file is generated by the Qt Visual Studio Integration.
# -----------------------------------------------------------

# This is a reminder that you are using a generated .pro file.
# Remove it when you are finished editing this file.
message("You are running qmake on a generated .pro file. This may not work!")


TEMPLATE = app
TARGET = XSupplicantUI
DESTDIR = ../build-debug
QT += xml
CONFIG += debug
INCLUDEPATH += ./../../xsupplicant/src/eap_types/tnc \
    ./../../xsupplicant/lib \
    ./../../xsupplicant/lib/libsupdetect \
    ./../../xsupplicant/lib/libxsupconfig \
    ./../../xsupplicant/lib/libxsupgui \
    ./../../xsupplicant/src \
    /usr/include \
    /usr/include/libxml2 \
    /usr/local/include \
    . \
    ./debug \
    ./GeneratedFiles
LIBS += -L"./../../xsupplicant/lib/libsupdetect" \
    -L"./../../xsupplicant/lib/libxsupconfig" \
    -L"./../../xsupplicant/lib/libxsupgui" \
    -L"./../../xsupplicant/lib/libxsupconfwrite" \
    -L"./../../xsupplicant/lib/liblist" \
    -L"./../../xsupplicant/lib/libsupdetect" \
    -L"./../../xsupplicant/lib/libxsupconfcheck" \
    -lQtUiTools \
    -lxsupgui \
    -lxml2 \
    -lxsupconfig \
    -lxsupconfwrite \
    -llist \
    -lxsupconfcheck \
    -lsupdetect 
DEPENDPATH += .
MOC_DIR += debug
OBJECTS_DIR += debug
UI_DIR += ./GeneratedFiles
RCC_DIR += ./release

#Include file(s)
include(XSupplicantUI.pri)