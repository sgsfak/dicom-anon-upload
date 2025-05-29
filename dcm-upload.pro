REVISION = $$system(git rev-parse --short HEAD)
DEFINES += APP_REVISION=$$REVISION
VERSION = 0.13.0
DEFINES += APP_VERSION=$$VERSION

QT       += core gui network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11
# CONFIG += console
CONFIG += sdk_no_version_check
# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

UI_DIR = $$PWD
TARGET = "EUCAIM Anonymizer"

macx {
INCLUDEPATH += "/usr/local/Cellar/libzip/1.10.1/include"
QMAKE_LFLAGS += "-L/usr/local/Cellar/libzip/1.10.1/lib -lzip"
}

#win32 {
#INCLUDEPATH += "C:\Users\User\vcpkg\packages\libzip_x64-windows-static\include"
#INCLUDEPATH += "C:\Users\User\vcpkg\packages\zlib_x64-windows-static\include"
#INCLUDEPATH += "C:\Users\User\vcpkg\packages\bzip2_x64-windows-static\include"
#LIBS += -LC:\Users\User\vcpkg\packages\libzip_x64-windows-static\lib -LC:\Users\User\vcpkg\packages\zlib_x64-windows-static\lib -LC:\Users\User\vcpkg\packages\bzip2_x64-windows-static\lib -lbz2 -lzlib -lzip
#}

win32 {
INCLUDEPATH += "C:\Users\User\vcpkg\packages\libzip_x64-windows\include"
INCLUDEPATH += "C:\Users\User\vcpkg\packages\zlib_x64-windows\include"
INCLUDEPATH += "C:\Users\User\vcpkg\packages\bzip2_x64-windows\include"
#LIBS += -LC:\Users\User\vcpkg\packages\libzip_x64-windows\lib -LC:\Users\User\vcpkg\packages\zlib_x64-windows\lib -LC:\Users\User\vcpkg\packages\bzip2_x64-windows\lib -lbz2 -lzlib -lzip
}

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    dicom/dcm.cpp \
    historyform.cpp \
    loginwindow.cpp \
    main.cpp \
    mainwindow.cpp \
    upload_worker.cpp \
    utils.cpp \
    worker.cpp \
    xxhash/xxh_x86dispatch.c \
    xxhash/xxhash.c

HEADERS += \
    config.h \
    dicom/dcm.h \
    historyform.h \
    loginwindow.h \
    mainwindow.h \
    token_data.h \
    upload_worker.h \
    utilities/bigint.hpp \
    utilities/csv.hpp \
    utils.h \
    worker.h \
    xxhash/xxh_x86dispatch.h \
    xxhash/xxhash.h

FORMS += \
    configdialog.ui \
    eucaimwelcome.ui \
    historyform.ui \
    loginwindow.ui \
    mainwindow.ui \
    patientinfo.ui \
    progress_dialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=

RESOURCES += \
    resources.qrc

RC_ICONS = eucaim.ico
ICON = eucaim.icns

#win32: LIBS += -L$$PWD/../build-dcm-upload-Desktop_Qt_5_15_2_MSVC2015_64bit-Release/release/openssl_1_1_1k/ -llibcrypto_static

#win32: LIBS += -L$$PWD/../build-dcm-upload-Desktop_Qt_5_15_2_MSVC2015_64bit-Release/release/openssl_1_1_1k/ -llibssl_static

INCLUDEPATH += $$PWD/../build-dcm-upload-Desktop_Qt_5_15_2_MSVC2015_64bit-Release/release/openssl_1_1_1k
DEPENDPATH += $$PWD/../build-dcm-upload-Desktop_Qt_5_15_2_MSVC2015_64bit-Release/release/openssl_1_1_1k
