#include <QTest>
#include <QVector3D>
#include <cmath>
#include "colormodel.h"
#include "colorcontroller.h"

class TestColorModel : public QObject
{
    Q_OBJECT

private slots:
    void testGammaCorrection();
    void testRgbToXyz();
    void testXyzToRgb();
    void testRgbToHsv();
    void testHsvToRgb();
    void testRoundTripRgbXyz();
    void testRoundTripRgbHsv();
    void testIlluminants();
    void testClippingAndScaling();
    void testOutOfGamutWarning();
};

void TestColorModel::testGammaCorrection()
{
    QCOMPARE(ColorModel::gammaCorrectionForward(0.0), 0.0);
    QCOMPARE(ColorModel::gammaCorrectionForward(0.04045), 0.04045 / 12.92);
    double val = 0.5;
    double expected = pow((val + 0.055) / 1.055, 2.4);
    QCOMPARE(ColorModel::gammaCorrectionForward(val), expected);

    QCOMPARE(ColorModel::gammaCorrectionReverse(0.0), 0.0);
    QCOMPARE(ColorModel::gammaCorrectionReverse(0.0031308), 12.92 * 0.0031308);
}

void TestColorModel::testRgbToXyz()
{
    QVector3D rgb(255, 0, 0);
    QVector3D xyz = ColorModel::rgbToXyz(rgb, Illuminant::D65);
    QVERIFY(std::abs(xyz.x() - 41.245) < 0.01);
    QVERIFY(std::abs(xyz.y() - 21.267) < 0.01);
    QVERIFY(std::abs(xyz.z() - 1.933) < 0.01);

    rgb = QVector3D(255, 255, 255);
    xyz = ColorModel::rgbToXyz(rgb, Illuminant::D65);
    QVERIFY(std::abs(xyz.x() - 95.047) < 0.01);
    QVERIFY(std::abs(xyz.y() - 100.000) < 0.01);
    QVERIFY(std::abs(xyz.z() - 108.883) < 0.01);
}

void TestColorModel::testXyzToRgb()
{
    QVector3D xyz(41.245, 21.267, 1.933);
    QVector3D rgb = ColorModel::xyzToRgb(xyz, Illuminant::D65);
    QVERIFY(std::abs(rgb.x() - 255) < 1.0);
    QVERIFY(std::abs(rgb.y()) < 1.0);
    QVERIFY(std::abs(rgb.z()) < 1.0);
}

void TestColorModel::testRgbToHsv()
{
    QVector3D rgb(255, 0, 0);
    QVector3D hsv = ColorModel::rgbToHsv(rgb);
    QVERIFY(std::abs(hsv.x() - 0) < 0.01 || std::abs(hsv.x() - 360) < 0.01);
    QVERIFY(std::abs(hsv.y() - 100) < 0.01);
    QVERIFY(std::abs(hsv.z() - 100) < 0.01);

    rgb = QVector3D(0, 255, 0);
    hsv = ColorModel::rgbToHsv(rgb);
    QVERIFY(std::abs(hsv.x() - 120) < 0.01);
    QVERIFY(std::abs(hsv.y() - 100) < 0.01);
    QVERIFY(std::abs(hsv.z() - 100) < 0.01);
}

void TestColorModel::testHsvToRgb()
{
    QVector3D hsv(0, 100, 100);
    QVector3D rgb = ColorModel::hsvToRgb(hsv);
    QVERIFY(std::abs(rgb.x() - 255) < 0.1);
    QVERIFY(std::abs(rgb.y()) < 0.1);
    QVERIFY(std::abs(rgb.z()) < 0.1);

    hsv = QVector3D(120, 100, 100);
    rgb = ColorModel::hsvToRgb(hsv);
    QVERIFY(std::abs(rgb.x()) < 0.1);
    QVERIFY(std::abs(rgb.y() - 255) < 0.1);
    QVERIFY(std::abs(rgb.z()) < 0.1);
}

void TestColorModel::testRoundTripRgbXyz()
{
    QVector3D original(100, 150, 200);
    QVector3D xyz = ColorModel::rgbToXyz(original, Illuminant::D65);
    QVector3D result = ColorModel::xyzToRgb(xyz, Illuminant::D65);
    QVERIFY(std::abs(original.x() - result.x()) < 0.5);
    QVERIFY(std::abs(original.y() - result.y()) < 0.5);
    QVERIFY(std::abs(original.z() - result.z()) < 0.5);
}

void TestColorModel::testRoundTripRgbHsv()
{
    QVector3D original(100, 150, 200);
    QVector3D hsv = ColorModel::rgbToHsv(original);
    QVector3D result = ColorModel::hsvToRgb(hsv);
    QVERIFY(std::abs(original.x() - result.x()) < 0.5);
    QVERIFY(std::abs(original.y() - result.y()) < 0.5);
    QVERIFY(std::abs(original.z() - result.z()) < 0.5);
}

void TestColorModel::testIlluminants()
{
    QMatrix4x4 d65 = ColorModel::getRgbToXyzMatrix(Illuminant::D65);
    QMatrix4x4 d50 = ColorModel::getRgbToXyzMatrix(Illuminant::D50);
    QMatrix4x4 e   = ColorModel::getRgbToXyzMatrix(Illuminant::E);

    bool diff = false;
    for (int i=0; i<3; ++i)
        for (int j=0; j<3; ++j)
            if (d65(i,j) != d50(i,j)) diff = true;
    QVERIFY(diff);

    diff = false;
    for (int i=0; i<3; ++i)
        for (int j=0; j<3; ++j)
            if (d65(i,j) != e(i,j)) diff = true;
    QVERIFY(diff);
}

void TestColorModel::testClippingAndScaling()
{
    QVector3D rgb(300, -50, 400);
    bool flag;
    QVector3D clipped = ColorModel::clampRgb(rgb, flag);
    QVERIFY(flag);
    QVERIFY(std::abs(clipped.x() - 255) < 0.01);
    QVERIFY(std::abs(clipped.y() - 0) < 0.01);
    QVERIFY(std::abs(clipped.z() - 255) < 0.01);

    QVector3D scaled = ColorModel::scaleRgb(rgb, flag);
    QVERIFY(flag);
    QVERIFY(std::abs(scaled.x() - 300 * 255 / 400) < 0.01);
    QVERIFY(std::abs(scaled.y() - 0) < 0.01);
    QVERIFY(std::abs(scaled.z() - 400 * 255 / 400) < 0.01);
}

void TestColorModel::testOutOfGamutWarning()
{
    ColorController controller;
    controller.setRgb(QVector3D(100, 150, 200));
    QVERIFY(controller.getWarningMessage().isEmpty());

    controller.setXyz(QVector3D(200, 150, 100));
    QVERIFY(!controller.getWarningMessage().isEmpty());
    QVERIFY(controller.getWarningMessage().contains("выходит за границы"));

    controller.setRgb(QVector3D(100, 150, 200));
    QVERIFY(controller.getWarningMessage().isEmpty());
}

QTEST_MAIN(TestColorModel)
#include "test_colormodel.moc"