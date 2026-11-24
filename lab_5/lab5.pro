QT += core gui widgets

CONFIG += c++14
TARGET = lab5
TEMPLATE = app

msvc: QMAKE_CXXFLAGS += /utf-8

INCLUDEPATH += src

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/canvas.cpp \
    src/plotwidget.cpp \
    src/testdialog.cpp \
    src/benchmarkdialog.cpp \
    src/segmentclip.cpp \
    src/polygonclip.cpp \
    src/quadtree.cpp \
    src/sceneio.cpp \
    src/testcases.cpp

HEADERS += \
    src/mainwindow.h \
    src/canvas.h \
    src/plotwidget.h \
    src/testdialog.h \
    src/benchmarkdialog.h \
    src/viewutil.h \
    src/geometry.h \
    src/segmentclip.h \
    src/polygonclip.h \
    src/quadtree.h \
    src/sceneio.h \
    src/testcases.h
