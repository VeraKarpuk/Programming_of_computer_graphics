QT += core gui widgets

greaterThan(QT_MAJOR_VERSION, 5): CONFIG += c++17
else: CONFIG += c++11

TARGET = lab4_raster
TEMPLATE = app

win32-msvc* {
    QMAKE_CXXFLAGS += /utf-8
}

SOURCES += \
    main.cpp \
    MainWindow.cpp \
    GridPainter.cpp \
    ChartWidget.cpp \
    Benchmark.cpp \
    VirtualGrid.cpp \
    RasterEngine.cpp

HEADERS += \
    MainWindow.h \
    GridPainter.h \
    ChartWidget.h \
    Benchmark.h \
    VirtualGrid.h \
    RasterEngine.h
