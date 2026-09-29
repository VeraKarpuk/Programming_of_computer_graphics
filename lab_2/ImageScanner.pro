QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    bmpparser.cpp \
    gifparser.cpp \
    imageparser.cpp \
    jpegparser.cpp \
    main.cpp \
    mainwindow.cpp \
    parserfactory.cpp \
    pcxparser.cpp \
    pngparser.cpp \
    scanmanager.cpp \
    scanworker.cpp \
    tiffparser.cpp

HEADERS += \
    bmpparser.h \
    gifparser.h \
    imagemetadata.h \
    imageparser.h \
    jpegparser.h \
    mainwindow.h \
    parserfactory.h \
    pcxparser.h \
    pngparser.h \
    scanmanager.h \
    scanworker.h \
    tiffparser.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
