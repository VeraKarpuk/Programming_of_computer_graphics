#include "colormodel.h"

ColorModel::ColorModel() {}

double ColorModel::gammaCorrectionForward(double x) {
    if (x >= 0.04045) {
        return pow((x + 0.055) / 1.055, 2.4);
    }
    return x / 12.92;
}

double ColorModel::gammaCorrectionReverse(double x) {
    if (x >= 0.0031308) {
        return 1.055 * pow(x, 1.0/2.4) - 0.055;
    }
    return 12.92 * x;
}

QMatrix4x4 ColorModel::getRgbToXyzMatrix(Illuminant illuminant) {
    QMatrix4x4 mat;
    mat.setToIdentity();

    switch (illuminant) {
    case Illuminant::D65:
        mat(0,0) = 0.412453; mat(0,1) = 0.357580; mat(0,2) = 0.180423;
        mat(1,0) = 0.212671; mat(1,1) = 0.715160; mat(1,2) = 0.072169;
        mat(2,0) = 0.019334; mat(2,1) = 0.119193; mat(2,2) = 0.950227;
        break;
    case Illuminant::D50:
        mat(0,0) = 0.4360747; mat(0,1) = 0.3850649; mat(0,2) = 0.1430804;
        mat(1,0) = 0.2225045; mat(1,1) = 0.7168786; mat(1,2) = 0.0606169;
        mat(2,0) = 0.0139322; mat(2,1) = 0.0971045; mat(2,2) = 0.7141733;
        break;
    case Illuminant::E:
        mat(0,0) = 0.333333; mat(0,1) = 0.333333; mat(0,2) = 0.333333;
        mat(1,0) = 0.333333; mat(1,1) = 0.333333; mat(1,2) = 0.333333;
        mat(2,0) = 0.333333; mat(2,1) = 0.333333; mat(2,2) = 0.333333;
        break;
    }

    return mat;
}

QMatrix4x4 ColorModel::getXyzToRgbMatrix(Illuminant illuminant) {
    QMatrix4x4 mat = getRgbToXyzMatrix(illuminant);
    return mat.inverted();
}

QVector3D ColorModel::rgbToXyz(const QVector3D& rgb, Illuminant illuminant) {
    double rn = gammaCorrectionForward(rgb.x() / 255.0) * 100.0;
    double gn = gammaCorrectionForward(rgb.y() / 255.0) * 100.0;
    double bn = gammaCorrectionForward(rgb.z() / 255.0) * 100.0;

    QMatrix4x4 mat = getRgbToXyzMatrix(illuminant);

    QVector4D rgbVec(rn, gn, bn, 1.0);
    QVector4D xyzVec = mat * rgbVec;

    return QVector3D(xyzVec.x(), xyzVec.y(), xyzVec.z());
}

QVector3D ColorModel::xyzToRgb(const QVector3D& xyz, Illuminant illuminant) {
    QMatrix4x4 mat = getXyzToRgbMatrix(illuminant);

    QVector4D xyzVec(xyz.x() / 100.0, xyz.y() / 100.0, xyz.z() / 100.0, 1.0);
    QVector4D rgbVec = mat * xyzVec;

    double r = gammaCorrectionReverse(rgbVec.x()) * 255.0;
    double g = gammaCorrectionReverse(rgbVec.y()) * 255.0;
    double b = gammaCorrectionReverse(rgbVec.z()) * 255.0;

    return QVector3D(r, g, b);
}

QVector3D ColorModel::rgbToHsv(const QVector3D& rgb) {
    double r = rgb.x() / 255.0;
    double g = rgb.y() / 255.0;
    double b = rgb.z() / 255.0;

    double max = std::max({r, g, b});
    double min = std::min({r, g, b});
    double delta = max - min;

    double h = 0, s = 0, v = max;

    if (delta > 0.0001) {
        s = delta / max;
        if (max == r) {
            h = 60.0 * (fmod((g - b) / delta, 6.0));
        } else if (max == g) {
            h = 60.0 * ((b - r) / delta + 2.0);
        } else {
            h = 60.0 * ((r - g) / delta + 4.0);
        }
        if (h < 0) h += 360.0;
    }

    return QVector3D(h, s * 100.0, v * 100.0);
}

QVector3D ColorModel::hsvToRgb(const QVector3D& hsv) {
    double h = hsv.x();
    double s = hsv.y() / 100.0;
    double v = hsv.z() / 100.0;

    if (s < 0.0001) {
        return QVector3D(v * 255.0, v * 255.0, v * 255.0);
    }

    h = fmod(h, 360.0);
    if (h < 0) h += 360.0;

    double c = v * s;
    double x = c * (1.0 - fabs(fmod(h / 60.0, 2.0) - 1.0));
    double m = v - c;

    double r, g, b;
    if (h < 60) { r = c; g = x; b = 0; }
    else if (h < 120) { r = x; g = c; b = 0; }
    else if (h < 180) { r = 0; g = c; b = x; }
    else if (h < 240) { r = 0; g = x; b = c; }
    else if (h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    return QVector3D((r + m) * 255.0, (g + m) * 255.0, (b + m) * 255.0);
}

QVector3D ColorModel::clampRgb(const QVector3D& rgb, bool& clamped) {
    clamped = false;
    QVector3D result = rgb;

    for (int i = 0; i < 3; i++) {
        if (result[i] < 0) {
            result[i] = 0;
            clamped = true;
        } else if (result[i] > 255.0) {
            result[i] = 255.0;
            clamped = true;
        }
    }

    return result;
}

QVector3D ColorModel::scaleRgb(const QVector3D& rgb, bool& scaled) {
    scaled = false;
    QVector3D result = rgb;

    double maxVal = std::max({rgb.x(), rgb.y(), rgb.z()});

    if (maxVal > 255.0) {
        double scale = 255.0 / maxVal;
        result.setX(result.x() * scale);
        result.setY(result.y() * scale);
        result.setZ(result.z() * scale);
        scaled = true;
    }

    for (int i = 0; i < 3; i++) {
        if (result[i] < 0) {
            result[i] = 0;
            scaled = true;
        }
    }

    return result;
}