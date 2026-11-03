#include "ChartWidget.h"

#include <QPainter>
#include <QPolygonF>
#include <cmath>

namespace
{
double niceStep(double range, int ticks)
{
    double raw = range / ticks;
    double mag = std::pow(10.0, std::floor(std::log10(raw)));
    double n = raw / mag;
    double k = n < 1.5 ? 1 : n < 3 ? 2 : n < 7 ? 5 : 10;
    return k * mag;
}

QColor seriesColor(int i)
{
    static const QColor c[] = {QColor(31, 119, 180), QColor(214, 39, 40), QColor(44, 160, 44),
                               QColor(255, 127, 14), QColor(148, 103, 189), QColor(23, 190, 207)};
    return c[i % 6];
}
}

ChartWidget::ChartWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(380, 260);
}

void ChartWidget::setup(const QString &title, const QString &xLabel, const QStringList &names)
{
    m_title = title;
    m_xLabel = xLabel;
    m_series.clear();
    for (int i = 0; i < names.size(); ++i) {
        Series s;
        s.name = names[i];
        m_series.push_back(s);
    }
    update();
}

void ChartWidget::addPoint(int series, double x, double ms)
{
    if (series >= 0 && series < m_series.size()) {
        m_series[series].pts.push_back(QPointF(x, ms));
        update();
    }
}

void ChartWidget::clearData()
{
    for (int i = 0; i < m_series.size(); ++i)
        m_series[i].pts.clear();
    update();
}

QString ChartWidget::csv() const
{
    QString s;
    for (int i = 0; i < m_series.size(); ++i) {
        for (int j = 0; j < m_series[i].pts.size(); ++j) {
            QString ms = QString::number(m_series[i].pts[j].y(), 'f', 3).replace('.', ',');
            s += m_title + ";" + m_series[i].name + ";" + QString::number(m_series[i].pts[j].x(), 'f', 0) + ";" + ms + "\n";
        }
    }
    return s;
}

void ChartWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);
    p.setPen(Qt::black);

    const int L = 64, R = 16, T = 30, B = 46;
    const QRect plot(L, T, width() - L - R, height() - T - B);

    double xmax = 0, ymax = 0;
    for (int i = 0; i < m_series.size(); ++i) {
        for (int j = 0; j < m_series[i].pts.size(); ++j) {
            xmax = qMax(xmax, m_series[i].pts[j].x());
            ymax = qMax(ymax, m_series[i].pts[j].y());
        }
    }
    if (xmax <= 0) xmax = 1;
    if (ymax <= 0) ymax = 1;
    double xs = niceStep(xmax, 5), ys = niceStep(ymax, 5);
    int nx = (int)std::ceil(xmax / xs - 1e-9), ny = (int)std::ceil(ymax / ys - 1e-9);
    xmax = nx * xs;
    ymax = ny * ys;

    p.drawText(QRect(0, 5, width(), 20), Qt::AlignCenter, m_title);

    for (int i = 0; i <= ny; ++i) {
        int y = plot.bottom() - (int)(plot.height() * i / (double)ny);
        p.setPen(QColor(225, 225, 225));
        p.drawLine(plot.left(), y, plot.right(), y);
        p.setPen(Qt::black);
        p.drawText(QRect(0, y - 8, L - 6, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(i * ys, 'g', 5));
    }
    for (int i = 0; i <= nx; ++i) {
        int x = plot.left() + (int)(plot.width() * i / (double)nx);
        p.setPen(QColor(225, 225, 225));
        p.drawLine(x, plot.top(), x, plot.bottom());
        p.setPen(Qt::black);
        p.drawText(QRect(x - 30, plot.bottom() + 4, 60, 16), Qt::AlignHCenter | Qt::AlignTop, QString::number(i * xs, 'g', 6));
    }
    p.setPen(Qt::black);
    p.drawRect(plot);
    p.drawText(QRect(plot.left(), height() - 22, plot.width(), 18), Qt::AlignCenter, m_xLabel);
    p.save();
    p.translate(14, plot.center().y());
    p.rotate(-90);
    p.drawText(QRect(-60, -10, 120, 20), Qt::AlignCenter, "время, мс");
    p.restore();

    for (int i = 0; i < m_series.size(); ++i) {
        QPolygonF poly;
        for (int j = 0; j < m_series[i].pts.size(); ++j) {
            const QPointF &pt = m_series[i].pts[j];
            poly << QPointF(plot.left() + pt.x() / xmax * plot.width(), plot.bottom() - pt.y() / ymax * plot.height());
        }
        p.setPen(QPen(seriesColor(i), 2));
        p.drawPolyline(poly);
        p.setBrush(seriesColor(i));
        for (int j = 0; j < poly.size(); ++j)
            p.drawEllipse(poly[j], 3, 3);
    }

    // легенда в левом верхнем углу области графика
    int ly = plot.top() + 10;
    for (int i = 0; i < m_series.size(); ++i) {
        p.setPen(QPen(seriesColor(i), 3));
        p.drawLine(plot.left() + 10, ly, plot.left() + 30, ly);
        p.setPen(Qt::black);
        p.drawText(plot.left() + 36, ly + 4, m_series[i].name);
        ly += 16;
    }
}
