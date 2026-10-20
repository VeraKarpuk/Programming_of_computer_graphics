// Слайдер + числовое поле, синхронизированные друг с другом.
#pragma once
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QSlider>
#include <QWidget>
#include <cmath>

class SliderSpin : public QWidget {
    Q_OBJECT
public:
    SliderSpin(double minV, double maxV, double step, double value, int decimals = 0, QWidget* parent = 0)
        : QWidget(parent), m_min(minV), m_step(step) {
        m_slider = new QSlider(Qt::Horizontal);
        m_slider->setRange(0, static_cast<int>(std::floor((maxV - minV) / step + 0.5)));
        m_spin = new QDoubleSpinBox;
        m_spin->setRange(minV, maxV);
        m_spin->setSingleStep(step);
        m_spin->setDecimals(decimals);
        m_spin->setValue(value);
        m_slider->setValue(static_cast<int>(std::floor((value - minV) / step + 0.5)));
        QHBoxLayout* l = new QHBoxLayout(this);
        l->setContentsMargins(0, 0, 0, 0);
        l->addWidget(m_slider, 1);
        l->addWidget(m_spin);
        connect(m_slider, SIGNAL(valueChanged(int)), this, SLOT(onSlider(int)));
        connect(m_spin, SIGNAL(valueChanged(double)), this, SLOT(onSpin(double)));
    }
    double value() const { return m_spin->value(); }
    void setValue(double v) { m_spin->setValue(v); }

signals:
    void valueChanged(double v);

private slots:
    void onSlider(int i) { m_spin->setValue(m_min + i * m_step); }
    void onSpin(double v) {
        m_slider->blockSignals(true);
        m_slider->setValue(static_cast<int>(std::floor((v - m_min) / m_step + 0.5)));
        m_slider->blockSignals(false);
        emit valueChanged(v);
    }

private:
    double m_min, m_step;
    QSlider* m_slider;
    QDoubleSpinBox* m_spin;
};
