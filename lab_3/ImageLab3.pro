# ImageLab3 — лабораторная работа 3 (вариант 8). Сборка: qmake && make (Qt 5.x / 6.x)
QT       += core gui widgets
TARGET    = ImageLab3
TEMPLATE  = app
CONFIG   += c++11
INCLUDEPATH += src

# std::thread: для MinGW нужна сборка с posix-потоками; для gcc/clang под Linux/macOS — -pthread
unix:QMAKE_CXXFLAGS += -pthread
unix:LIBS           += -pthread
msvc:QMAKE_CXXFLAGS += /utf-8

SOURCES += \
    src/main.cpp \
    src/core/Image.cpp \
    src/core/Algorithms.cpp \
    src/core/TestImages.cpp \
    src/ui/MainWindow.cpp \
    src/ui/ImageView.cpp \
    src/ui/HistogramWidget.cpp \
    src/ui/ImageConvert.cpp

HEADERS += \
    src/core/Image.h \
    src/core/Parallel.h \
    src/core/Algorithms.h \
    src/core/TestImages.h \
    src/ui/MainWindow.h \
    src/ui/ImageView.h \
    src/ui/HistogramWidget.h \
    src/ui/ImageConvert.h \
    src/ui/SliderSpin.h
