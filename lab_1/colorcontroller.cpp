#include "colorcontroller.h"

ColorController::ColorController(QObject *parent)
    : QObject(parent)
    , m_illuminant(Illuminant::D65)
    , m_strategy(OutOfGamutStrategy::Clipping)
    , m_updating(false)
{
    m_rgb = QVector3D(255, 0, 0);
    updateAllFromRgb();
}

void ColorController::setRgb(const QVector3D& rgb) {
    if (m_updating) return;
    m_updating = true;

    m_rgb = rgb;
    updateAllFromRgb();

    m_updating = false;
    emit rgbChanged();
}

void ColorController::setXyz(const QVector3D& xyz) {
    if (m_updating) return;
    m_updating = true;

    m_xyz = xyz;
    updateAllFromXyz();

    m_updating = false;
    emit xyzChanged();
}

void ColorController::setHsv(const QVector3D& hsv) {
    if (m_updating) return;
    m_updating = true;

    m_hsv = hsv;
    updateAllFromHsv();

    m_updating = false;
    emit hsvChanged();
}

void ColorController::setIlluminant(Illuminant illuminant) {
    if (m_illuminant == illuminant) return;
    m_illuminant = illuminant;
    updateAllFromRgb();
    emit illuminantChanged();
}

void ColorController::setStrategy(OutOfGamutStrategy strategy) {
    if (m_strategy == strategy) return;
    m_strategy = strategy;
    updateAllFromRgb();
    emit strategyChanged();
}

void ColorController::updateAllFromRgb() {
    m_xyz = ColorModel::rgbToXyz(m_rgb, m_illuminant);
    m_hsv = ColorModel::rgbToHsv(m_rgb);
    checkRgbOutOfGamut();

    emit xyzChanged();
    emit hsvChanged();
    emit warningMessageChanged();
}

void ColorController::updateAllFromXyz() {
    QVector3D rgb = ColorModel::xyzToRgb(m_xyz, m_illuminant);

    bool outOfGamut = false;
    if (m_strategy == OutOfGamutStrategy::Clipping) {
        m_rgb = ColorModel::clampRgb(rgb, outOfGamut);
    } else {
        m_rgb = ColorModel::scaleRgb(rgb, outOfGamut);
    }

    if (outOfGamut) {
        m_warningMessage = "Цвет выходит за границы RGB. Применена стратегия: " +
                           QString(m_strategy == OutOfGamutStrategy::Clipping ? "Clipping" : "Scaling");
    } else {
        m_warningMessage.clear();
    }

    m_hsv = ColorModel::rgbToHsv(m_rgb);

    emit rgbChanged();
    emit hsvChanged();
    emit warningMessageChanged();
}

void ColorController::updateAllFromHsv() {
    m_rgb = ColorModel::hsvToRgb(m_hsv);
    m_xyz = ColorModel::rgbToXyz(m_rgb, m_illuminant);
    checkRgbOutOfGamut();

    emit rgbChanged();
    emit xyzChanged();
    emit warningMessageChanged();
}

void ColorController::checkRgbOutOfGamut() {
    bool outOfGamut = false;
    for (int i = 0; i < 3; i++) {
        if (m_rgb[i] < 0 || m_rgb[i] > 255.0) {
            outOfGamut = true;
            break;
        }
    }

    if (outOfGamut) {
        m_warningMessage = "Внимание: Цвет выходит за границы RGB!";
    } else {
        m_warningMessage.clear();
    }
}