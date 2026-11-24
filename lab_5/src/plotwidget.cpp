#include "plotwidget.h"

#include <QPainter>
#include <QPolygonF>
#include <algorithm>

#include "viewutil.h"

PlotWidget::PlotWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(520, 320);
}

void PlotWidget::setData(const QString &title, const QString &xLabel, const QString &yLabel,
                         const std::vector<PlotSeries> &series) {
    title_ = title;
    xLabel_ = xLabel;
    yLabel_ = yLabel;
    series_ = series;
    update();
}

void PlotWidget::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), Qt::white);

    p.setPen(Qt::black);
    QFont bold = font();
    bold.setBold(true);
    p.setFont(bold);
    p.drawText(QRectF(0, 6, width(), 20), Qt::AlignHCenter, title_);
    p.setFont(font());

    double xmin = 1e300, xmax = -1e300, ymax = 0;
    for (const PlotSeries &s : series_) {
        for (double v : s.x) { xmin = std::min(xmin, v); xmax = std::max(xmax, v); }
        for (double v : s.y) ymax = std::max(ymax, v);
    }
    if (xmin > xmax || ymax <= 0) return;
    if (xmax - xmin < 1e-12) xmax = xmin + 1;

    const double left = 64, right = 20, top = 36, bottom = 52;
    QRectF area(left, top, width() - left - right, height() - top - bottom);
    double ystep = viewutil::niceStep(ymax / 5);
    double ytop = std::ceil(ymax / ystep) * ystep;

    auto sx = [&](double v) { return area.left() + (v - xmin) / (xmax - xmin) * area.width(); };
    auto sy = [&](double v) { return area.bottom() - v / ytop * area.height(); };

    // сетка и подписи по y
    for (double v = 0; v <= ytop + ystep / 2; v += ystep) {
        p.setPen(QPen(QColor(225, 225, 225), 1));
        p.drawLine(QPointF(area.left(), sy(v)), QPointF(area.right(), sy(v)));
        p.setPen(Qt::black);
        p.drawText(QRectF(0, sy(v) - 8, left - 6, 16), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'g', 5));
    }
    // подписи по x - в точках первой серии
    if (!series_.empty()) {
        const std::vector<double> &xs = series_[0].x;
        size_t every = std::max<size_t>(1, xs.size() / 10);
        for (size_t i = 0; i < xs.size(); i += every) {
            p.setPen(QPen(QColor(225, 225, 225), 1));
            p.drawLine(QPointF(sx(xs[i]), area.top()), QPointF(sx(xs[i]), area.bottom()));
            p.setPen(Qt::black);
            p.drawText(QRectF(sx(xs[i]) - 30, area.bottom() + 4, 60, 16), Qt::AlignHCenter,
                       QString::number(xs[i], 'g', 6));
        }
    }
    p.setPen(QPen(Qt::black, 1.4));
    p.drawRect(area);
    p.drawText(QRectF(area.left(), height() - 22, area.width(), 18), Qt::AlignHCenter, xLabel_);
    p.save();
    p.translate(14, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF(-area.height() / 2, -8, area.height(), 16), Qt::AlignHCenter, yLabel_);
    p.restore();

    // линии и легенда
    double ly = area.top() + 8;
    for (const PlotSeries &s : series_) {
        QPolygonF poly;
        for (size_t i = 0; i < s.x.size(); ++i) poly << QPointF(sx(s.x[i]), sy(s.y[i]));
        p.setPen(QPen(s.color, 2.2));
        p.drawPolyline(poly);
        p.setBrush(s.color);
        for (const QPointF &pt : poly) p.drawEllipse(pt, 3.2, 3.2);
        p.setBrush(Qt::NoBrush);

        p.setPen(QPen(s.color, 3));
        p.drawLine(QPointF(area.right() - 190, ly), QPointF(area.right() - 168, ly));
        p.setPen(Qt::black);
        p.drawText(QPointF(area.right() - 162, ly + 4), s.name);
        ly += 16;
    }
}
