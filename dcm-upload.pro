REVISION = $$system(git rev-parse --short HEAD)
DEFINES += APP_REVISION=$$REVISION
VERSION = 0.5.0
DEFINES += APP_VERSION=$$VERSION

QT       += core gui network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

macx {
INCLUDEPATH += "/usr/local/Cellar/libzip/1.9.2/include"
QMAKE_LFLAGS += "-L/usr/local/Cellar/libzip/1.9.2/lib -lzip"
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
LIBS += -LC:\Users\User\vcpkg\packages\libzip_x64-windows\lib -LC:\Users\User\vcpkg\packages\zlib_x64-windows\lib -LC:\Users\User\vcpkg\packages\bzip2_x64-windows\lib -lbz2 -lzlib -lzip
}

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    loginwindow.cpp \
    main.cpp \
    mainwindow.cpp \
    worker.cpp \
    xxhash/xxh_x86dispatch.c \
    xxhash/xxhash.c

HEADERS += \
    loginwindow.h \
    mainwindow.h \
    token_data.h \
    worker.h \
    xxhash/xxh_x86dispatch.h \
    xxhash/xxhash.h

FORMS += \
    loginwindow.ui \
    mainwindow.ui \
    patientinfo.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES +=

RESOURCES += \
    resources.qrc

RC_ICONS = cardiocare-logo.ico
ICON = cardiocare-logo.icns
