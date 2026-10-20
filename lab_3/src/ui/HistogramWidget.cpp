#include "HistogramWidget.h"
#include <QPainter>
#include <QPolygonF>
#include <algorithm>

HistogramWidget::HistogramWidget(QWidget* parent) : QWidget(parent), m_threshold(-1), m_has(false) {
    setMinimumHeight(130);
}

void HistogramWidget::setData(const ip::Histograms& h, const QString& title) {
    m_hist = h; m_title = title; m_has = true; update();
}
void HistogramWidget::clearData() { m_has = false; m_threshold = -1; update(); }

void HistogramWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), QColor(250, 250, 250));
    p.setPen(QColor(200, 200, 200));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
    if (!m_has || m_hist.lum.empty()) {
        p.setPen(QColor(120, 120, 120));
        p.drawText(rect(), Qt::AlignCenter, QString::fromUtf8("гистограмма появится после обработки"));
        return;
    }
    const QRect plot = rect().adjusted(10, 22, -10, -18);
    p.setPen(Qt::black);
    p.drawText(10, 15, m_title + QString::fromUtf8("   μ=%1  σ=%2  мин=%3  макс=%4")
                          .arg(m_hist.mean, 0, 'f', 1).arg(m_hist.stddev, 0, 'f', 1)
                          .arg(m_hist.minVal).arg(m_hist.maxVal));

    int mx = 1;
    for (int i = 0; i < 256; ++i) mx = std::max(mx, m_hist.lum[i]);
    const double bw = static_cast<double>(plot.width()) / 256.0;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(120, 120, 120));
    for (int i = 0; i < 256; ++i) {
        const int hgt = static_cast<int>(plot.height() * static_cast<double>(m_hist.lum[i]) / mx + 0.5);
        p.drawRect(QRectF(plot.left() + i * bw, plot.bottom() - hgt, bw + 0.6, hgt));
    }
    if (m_hist.color) {
        int mc = 1;
        for (int i = 0; i < 256; ++i) mc = std::max(mc, std::max(m_hist.r[i], std::max(m_hist.g[i], m_hist.b[i])));
        const std::vector<int>* ch[3] = { &m_hist.r, &m_hist.g, &m_hist.b };
        const QColor col[3] = { QColor(220, 30, 30, 200), QColor(30, 170, 30, 200), QColor(40, 60, 230, 200) };
        for (int c = 0; c < 3; ++c) {
            QPolygonF poly;
            for (int i = 0; i < 256; ++i)
                poly << QPointF(plot.left() + (i + 0.5) * bw,
                                plot.bottom() - plot.height() * static_cast<double>((*ch[c])[i]) / mc);
            p.setPen(QPen(col[c], 1));
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(poly);
        }
    }
    p.setPen(Qt::black);
    p.drawLine(plot.left(), plot.bottom(), plot.right(), plot.bottom());
    p.drawText(plot.left(), height() - 4, "0");
    p.drawText(plot.left() + plot.width() / 2 - 8, height() - 4, "128");
    p.drawText(plot.right() - 20, height() - 4, "255");
    if (m_threshold >= 0) {
        const double x = plot.left() + (m_threshold + 0.5) * bw;
        p.setPen(QPen(QColor(220, 0, 0), 2));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.drawText(QPointF(std::min<double>(x + 4, plot.right() - 50), plot.top() + 12),
                   QString::fromUtf8("T = %1").arg(m_threshold));
    }
}
