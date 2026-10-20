#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("ImageLab3");
    MainWindow w;
    w.show();
    return app.exec();
}
