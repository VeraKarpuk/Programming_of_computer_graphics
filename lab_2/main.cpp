#include <QApplication>
#include "mainwindow.h"
#include "imagemetadata.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    qRegisterMetaType<ImageMetadata>("ImageMetadata");
    MainWindow w;
    w.show();
    return app.exec();
}