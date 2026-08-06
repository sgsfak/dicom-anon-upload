REVISION = $$system(git rev-parse --short HEAD)
DEFINES += APP_REVISION=$$REVISION
VERSION = 0.14.2
DEFINES += APP_VERSION=$$VERSION

QT       += core gui network sql
QT       += core5compat

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
TARGET = "EUCAIM-Anonymizer"

# Use pkg-config to pull libzip + transitive libs (zlib, bzip2)
CONFIG += link_pkgconfig
PKGCONFIG += libzip

# Tell qmake to copy the entire ctp folder next to the binary
ctp.files = $$PWD/ctp
# On Windows: copy into release folder (same as exe)
win32:ctp.path = $$OUT_PWD/release
# On macOS: copy inside the .app bundle's folder
macx:ctp.path = $$OUT_PWD/$${TARGET}.app/Contents/MacOS
INSTALLS += ctp

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
    worker.cpp

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
    worker.h

FORMS += \
    configdialog.ui \
    eucaimwelcome.ui \
    historyform.ui \
    loginwindow.ui \
    mainwindow.ui \
    patientinfo.ui \
    progress_dialog.ui

# Default rules for deployment.
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
