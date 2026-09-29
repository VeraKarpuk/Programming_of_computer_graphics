#ifndef COLORMODEL_H
#define COLORMODEL_H

#include <QVector3D>
#include <QMatrix4x4>
#include <cmath>
#include <algorithm>

enum class Illuminant { D65, D50, E };

class ColorModel
{
public:
    ColorModel();

    static QVector3D rgbToXyz(const QVector3D& rgb, Illuminant illuminant);
    static QVector3D xyzToRgb(const QVector3D& xyz, Illuminant illuminant);
    static QVector3D rgbToHsv(const QVector3D& rgb);
    static QVector3D hsvToRgb(const QVector3D& hsv);

    static QVector3D clampRgb(const QVector3D& rgb, bool& clamped);
    static QVector3D scaleRgb(const QVector3D& rgb, bool& scaled);

    static QMatrix4x4 getRgbToXyzMatrix(Illuminant illuminant);
    static QMatrix4x4 getXyzToRgbMatrix(Illuminant illuminant);

    static double gammaCorrectionForward(double x);
    static double gammaCorrectionReverse(double x);
};

#endif