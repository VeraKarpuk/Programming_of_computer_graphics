#include <QApplication>
#include <QDebug>
#include "mainwindow.h"
#include "colormodel.h"

void runQuickTests() {
    qDebug() << "БЫСТРЫЕ ТЕСТЫ";

    // Тест 1: RGB(255,0,0) -> XYZ
    QVector3D rgb(255, 0, 0);
    QVector3D xyz = ColorModel::rgbToXyz(rgb, Illuminant::D65);
    qDebug() << "RGB(255,0,0) -> XYZ:" << xyz;
    qDebug() << "Ожидается: (41.245, 21.267, 1.933)";
    qDebug() << "Результат:" << (std::abs(xyz.x() - 41.245) < 0.01 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН");

    // Тест 2: XYZ -> RGB
    QVector3D result = ColorModel::xyzToRgb(xyz, Illuminant::D65);
    qDebug() << "XYZ -> RGB:" << result;
    qDebug() << "Ожидается: (255, 0, 0)";
    qDebug() << "Результат:" << (std::abs(result.x() - 255) < 1.0 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН");

    // Тест 3: RGB(255,0,0) -> HSV
    QVector3D hsv = ColorModel::rgbToHsv(rgb);
    qDebug() << "RGB(255,0,0) -> HSV:" << hsv;
    qDebug() << "Ожидается: (0, 100, 100)";
    qDebug() << "Результат:" << (std::abs(hsv.y() - 100) < 0.01 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН");

    // Тест 4: HSV(0,100,100) -> RGB
    QVector3D hsvInput(0, 100, 100);
    QVector3D rgbFromHsv = ColorModel::hsvToRgb(hsvInput);
    qDebug() << "HSV(0,100,100) -> RGB:" << rgbFromHsv;
    qDebug() << "Ожидается: (255, 0, 0)";
    qDebug() << "Результат:" << (std::abs(rgbFromHsv.x() - 255) < 0.1 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН");

    // Тест 5: Полный цикл RGB -> XYZ -> RGB
    QVector3D original(100, 150, 200);
    QVector3D xyzCycle = ColorModel::rgbToXyz(original, Illuminant::D65);
    QVector3D rgbCycle = ColorModel::xyzToRgb(xyzCycle, Illuminant::D65);
    qDebug() << "Цикл RGB(100,150,200) -> XYZ -> RGB:" << rgbCycle;
    qDebug() << "Результат:" << (std::abs(original.x() - rgbCycle.x()) < 0.5 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН");

    // Тест 6: Полный цикл RGB -> HSV -> RGB
    QVector3D hsvCycle = ColorModel::rgbToHsv(original);
    QVector3D rgbFromHsvCycle = ColorModel::hsvToRgb(hsvCycle);
    qDebug() << "Цикл RGB(100,150,200) -> HSV -> RGB:" << rgbFromHsvCycle;
    qDebug() << "Результат:" << (std::abs(original.x() - rgbFromHsvCycle.x()) < 0.5 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН");

    // Тест 7: RGB(255,255,255) -> XYZ (белый)
    QVector3D white(255, 255, 255);
    QVector3D whiteXyz = ColorModel::rgbToXyz(white, Illuminant::D65);
    qDebug() << "RGB(255,255,255) -> XYZ:" << whiteXyz;
    qDebug() << "Ожидается: (95.047, 100.000, 108.883)";
    qDebug() << "Результат:" << (std::abs(whiteXyz.x() - 95.047) < 0.01 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН");

    // Тест 8: Clipping
    QVector3D testRgb(300, -50, 400);
    bool flag;
    QVector3D clipped = ColorModel::clampRgb(testRgb, flag);
    qDebug() << "Clipping (300,-50,400) ->" << clipped;
    qDebug() << "Ожидается: (255, 0, 255)";
    qDebug() << "Результат:" << (std::abs(clipped.x() - 255) < 0.01 &&
                                         std::abs(clipped.y() - 0) < 0.01 &&
                                         std::abs(clipped.z() - 255) < 0.01 ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН");

    // Тест 9: Scaling
    QVector3D scaled = ColorModel::scaleRgb(testRgb, flag);
    qDebug() << "Scaling (300,-50,400) ->" << scaled;
    qDebug() << "Результат:" << (flag ? "ПРОЙДЕН" : "НЕ ПРОЙДЕН");

    qDebug() << "ТЕСТЫ ЗАВЕРШЕНЫ";
}

int main(int argc, char *argv[])
{
    runQuickTests();

    QApplication app(argc, argv);
    MainWindow window;
    window.show();
    return app.exec();
}