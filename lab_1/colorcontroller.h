#ifndef COLORCONTROLLER_H
#define COLORCONTROLLER_H

#include <QObject>
#include <QVector3D>
#include "colormodel.h"

class ColorController : public QObject
{
    Q_OBJECT

public:
    enum class OutOfGamutStrategy { Clipping, Scaling };
    Q_ENUM(OutOfGamutStrategy)

    explicit ColorController(QObject *parent = nullptr);

    QVector3D getRgb() const { return m_rgb; }
    QVector3D getXyz() const { return m_xyz; }
    QVector3D getHsv() const { return m_hsv; }
    Illuminant getIlluminant() const { return m_illuminant; }
    OutOfGamutStrategy getStrategy() const { return m_strategy; }
    QString getWarningMessage() const { return m_warningMessage; }

    void setRgb(const QVector3D& rgb);
    void setXyz(const QVector3D& xyz);
    void setHsv(const QVector3D& hsv);
    void setIlluminant(Illuminant illuminant);
    void setStrategy(OutOfGamutStrategy strategy);

signals:
    void rgbChanged();
    void xyzChanged();
    void hsvChanged();
    void illuminantChanged();
    void strategyChanged();
    void warningMessageChanged();

private:
    void updateAllFromRgb();
    void updateAllFromXyz();
    void updateAllFromHsv();
    void checkRgbOutOfGamut();

    QVector3D m_rgb;
    QVector3D m_xyz;
    QVector3D m_hsv;
    Illuminant m_illuminant;
    OutOfGamutStrategy m_strategy;
    QString m_warningMessage;
    bool m_updating;
};

#endif