#include "GridPainter.h"

#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QWheelEvent>

namespace
{
const int kLeft = 46, kTop = 22, kRight = 26, kBottom = 30;

QColor shade(double v)
{
    // яркость пикселя - это доля цвета пикселя в смеси с белым фоном
    const double r = 30, g = 90, b = 200;
    return QColor(int(255 + (r - 255) * v), int(255 + (g - 255) * v), int(255 + (b - 255) * v));
}

int labelStep(int cell)
{
    static const int steps[] = {1, 2, 5, 10, 20, 50, 100};
    for (int i = 0; i < 7; ++i)
        if (steps[i] * cell >= 32)
            return steps[i];
    return 100;
}
}

GridPainter::GridPainter(const VirtualGrid *grid, QWidget *parent)
    : QWidget(parent), m_grid(grid), m_cell(18), m_hx(0), m_hy(0), m_hover(false)
{
    setMouseTracking(true);
    refresh();
}

void GridPainter::setCellSize(int px)
{
    m_cell = qBound(3, px, 80);
    refresh();
}

void GridPainter::refresh()
{
    setFixedSize(kLeft + m_grid->width() * m_cell + kRight, kTop + m_grid->height() * m_cell + kBottom);
    update();
}

bool GridPainter::cellAt(const QPoint &p, int &x, int &y) const
{
    int px = p.x() - kLeft, py = p.y() - kTop;
    if (px < 0 || py < 0)
        return false;
    int col = px / m_cell, row = py / m_cell;
    if (col >= m_grid->width() || row >= m_grid->height())
        return false;
    x = m_grid->minX() + col;
    y = m_grid->maxY() - row;   // строки на экране идут сверху вниз, а ось y - снизу вверх
    return true;
}

void GridPainter::paintEvent(QPaintEvent *ev)
{
    QPainter p(this);
    p.fillRect(ev->rect(), QColor(245, 245, 245));

    const int w = m_grid->width(), h = m_grid->height(), cs = m_cell;
    const QRect area(kLeft, kTop, w * cs, h * cs);
    const QRect vis = area.intersected(ev->rect());
    p.fillRect(vis, Qt::white);

    if (!vis.isEmpty()) {
        int c0 = (vis.left() - kLeft) / cs;
        int c1 = qMin(w - 1, (vis.right() - kLeft) / cs);
        int r0 = (vis.top() - kTop) / cs;
        int r1 = qMin(h - 1, (vis.bottom() - kTop) / cs);

        for (int r = r0; r <= r1; ++r) {
            for (int c = c0; c <= c1; ++c) {
                double v = m_grid->at(m_grid->minX() + c, m_grid->maxY() - r);
                if (v > 0.0)
                    p.fillRect(kLeft + c * cs, kTop + r * cs, cs, cs, shade(v));
            }
        }

        if (cs >= 6) {
            p.setPen(QColor(200, 200, 200));
            for (int c = c0; c <= c1 + 1; ++c)
                p.drawLine(kLeft + c * cs, kTop + r0 * cs, kLeft + c * cs, kTop + (r1 + 1) * cs);
            for (int r = r0; r <= r1 + 1; ++r)
                p.drawLine(kLeft + c0 * cs, kTop + r * cs, kLeft + (c1 + 1) * cs, kTop + r * cs);
        }

        // целая координата - центр клетки, поэтому оси идут через середины клеток x = 0 и y = 0
        if (cs >= 14) {
            p.setPen(QPen(QColor(110, 110, 110), 3));
            for (int r = r0; r <= r1; ++r)
                for (int c = c0; c <= c1; ++c)
                    p.drawPoint(kLeft + c * cs + cs / 2, kTop + r * cs + cs / 2);
        }

        if (cs >= 34) {
            QFont f = font();
            f.setPointSize(8);
            p.setFont(f);
            for (int r = r0; r <= r1; ++r) {
                for (int c = c0; c <= c1; ++c) {
                    double v = m_grid->at(m_grid->minX() + c, m_grid->maxY() - r);
                    if (v > 0.0 && v < 0.995) {
                        p.setPen(v > 0.5 ? Qt::white : QColor(40, 40, 40));
                        p.drawText(QRect(kLeft + c * cs, kTop + r * cs, cs, cs), Qt::AlignCenter, QString::number(v, 'f', 2));
                    }
                }
            }
            p.setFont(font());
        }
    }

    p.setPen(QPen(QColor(220, 40, 40), 1));
    int axisY = kTop + (m_grid->maxY() - 0) * cs + cs / 2;
    int axisX = kLeft + (0 - m_grid->minX()) * cs + cs / 2;
    bool hasX = m_grid->minY() <= 0 && m_grid->maxY() >= 0;
    bool hasY = m_grid->minX() <= 0 && m_grid->maxX() >= 0;
    if (hasX) {
        p.drawLine(kLeft, axisY, kLeft + w * cs, axisY);
        p.drawText(kLeft + w * cs + 8, axisY + 4, "x");
    }
    if (hasY) {
        p.drawLine(axisX, kTop, axisX, kTop + h * cs);
        p.drawText(axisX - 3, kTop - 7, "y");
    }

    // подписи целых координат
    p.setPen(Qt::black);
    int step = labelStep(cs);
    for (int x = m_grid->minX(); x <= m_grid->maxX(); ++x) {
        if (x % step != 0)
            continue;
        int cx = kLeft + (x - m_grid->minX()) * cs + cs / 2;
        p.drawLine(cx, kTop + h * cs, cx, kTop + h * cs + 3);
        p.drawText(QRect(cx - 22, kTop + h * cs + 5, 44, 16), Qt::AlignHCenter | Qt::AlignTop, QString::number(x));
    }
    for (int y = m_grid->minY(); y <= m_grid->maxY(); ++y) {
        if (y % step != 0)
            continue;
        int cy = kTop + (m_grid->maxY() - y) * cs + cs / 2;
        p.drawLine(kLeft - 3, cy, kLeft, cy);
        p.drawText(QRect(0, cy - 8, kLeft - 6, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(y));
    }

    if (m_hover) {
        p.setPen(QPen(QColor(255, 140, 0), 2));
        p.setBrush(Qt::NoBrush);
        int c = m_hx - m_grid->minX(), r = m_grid->maxY() - m_hy;
        p.drawRect(kLeft + c * cs, kTop + r * cs, cs, cs);
    }
}

void GridPainter::mouseMoveEvent(QMouseEvent *ev)
{
    int x = 0, y = 0;
    m_hover = cellAt(ev->pos(), x, y);
    if (m_hover) {
        m_hx = x;
        m_hy = y;
    }
    emit hovered(x, y, m_hover);
    update();
}

void GridPainter::mousePressEvent(QMouseEvent *ev)
{
    int x = 0, y = 0;
    if (cellAt(ev->pos(), x, y))
        emit picked(x, y, ev->button());
}

void GridPainter::leaveEvent(QEvent *)
{
    m_hover = false;
    emit hovered(0, 0, false);
    update();
}

void GridPainter::wheelEvent(QWheelEvent *ev)
{
    if (ev->modifiers() & Qt::ControlModifier) {
        emit zoomRequested(ev->angleDelta().y() > 0 ? 1 : -1);
        ev->accept();
    } else {
        ev->ignore();
    }
}
