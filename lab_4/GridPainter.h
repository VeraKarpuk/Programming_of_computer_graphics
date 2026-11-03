#ifndef GRIDPAINTER_H
#define GRIDPAINTER_H

#include <QWidget>
#include "VirtualGrid.h"

// Только показывает содержимое VirtualGrid увеличенными квадратами, ничего не вычисляет
class GridPainter : public QWidget
{
    Q_OBJECT
public:
    explicit GridPainter(const VirtualGrid *grid, QWidget *parent = 0);

    int cellSize() const { return m_cell; }
    void refresh();

public slots:
    void setCellSize(int px);

signals:
    void hovered(int x, int y, bool inside);
    void picked(int x, int y, int button);
    void zoomRequested(int direction);

protected:
    void paintEvent(QPaintEvent *ev);
    void mouseMoveEvent(QMouseEvent *ev);
    void mousePressEvent(QMouseEvent *ev);
    void leaveEvent(QEvent *ev);
    void wheelEvent(QWheelEvent *ev);

private:
    bool cellAt(const QPoint &p, int &x, int &y) const;

    const VirtualGrid *m_grid;
    int m_cell;
    int m_hx, m_hy;
    bool m_hover;
};

#endif
