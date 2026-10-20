// Гистограмма яркости (+ R, G, B для цветных) с маркером порога и статистикой.
#pragma once
#include <QString>
#include <QWidget>
#include "core/Image.h"

class HistogramWidget : public QWidget {
    Q_OBJECT
public:
    explicit HistogramWidget(QWidget* parent = 0);
    void setData(const ip::Histograms& h, const QString& title);
    void setThreshold(int t) { m_threshold = t; update(); }  // -1 = не показывать
    void clearData();

protected:
    void paintEvent(QPaintEvent*);

private:
    ip::Histograms m_hist;
    QString m_title;
    int m_threshold;
    bool m_has;
};
