#include "ImageView.h"
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

ImageView::ImageView(QWidget* parent)
    : QWidget(parent), m_fit(true), m_scale(1.0), m_center(0, 0), m_drag(false) {
    setMinimumSize(200, 150);
    setMouseTracking(false);
}

void ImageView::setImage(const QImage& img) {
    if (!m_img.isNull() && m_img.size() != img.size()) m_fit = true;
    m_img = img;
    update();
}

void ImageView::setCaption(const QString& text) { m_caption = text; update(); }

double ImageView::fitScale() const {
    if (m_img.isNull()) return 1.0;
    return std::min((width() - 8.0) / m_img.width(), (height() - 8.0) / m_img.height());
}
double ImageView::scale() const { return m_fit ? fitScale() : m_scale; }
QPointF ImageView::center() const {
    return m_fit ? QPointF(m_img.width() / 2.0, m_img.height() / 2.0) : m_center;
}

void ImageView::applyView(double s, QPointF c, bool fit) {
    m_scale = s; m_center = c; m_fit = fit;
    update();
}

void ImageView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(45, 45, 48));
    if (m_img.isNull()) {
        p.setPen(QColor(170, 170, 170));
        p.drawText(rect(), Qt::AlignCenter, QString::fromUtf8("нет изображения"));
    } else {
        const double s = scale();
        const QPointF c = center();
        const QRectF target(width() / 2.0 - c.x() * s, height() / 2.0 - c.y() * s, m_img.width() * s, m_img.height() * s);
        p.setRenderHint(QPainter::SmoothPixmapTransform, s < 1.0);
        p.drawImage(target, m_img);
    }
    if (!m_caption.isEmpty()) {
        QFontMetrics fm(p.font());
#if QT_VERSION >= QT_VERSION_CHECK(5, 11, 0)
        const int tw = fm.horizontalAdvance(m_caption);
#else
        const int tw = fm.width(m_caption);
#endif
        QRect r(6, 6, tw + 12, fm.height() + 6);
        p.fillRect(r, QColor(0, 0, 0, 150));
        p.setPen(Qt::white);
        p.drawText(r, Qt::AlignCenter, m_caption);
    }
}

void ImageView::wheelEvent(QWheelEvent* e) {
    if (m_img.isNull()) return;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QPointF cur = e->position();
#else
    const QPointF cur = e->posF();
#endif
    const double s = scale();
    const QPointF c = center();
    const QPointF off = cur - QPointF(width() / 2.0, height() / 2.0);
    const QPointF imgPt = c + off / s;
    const double ns = std::min(64.0, std::max(0.02, s * std::pow(1.0015, e->angleDelta().y())));
    m_scale = ns;
    m_center = imgPt - off / ns;
    m_fit = false;
    update();
    emit viewChanged(m_scale, m_center, m_fit);
}

void ImageView::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) { m_drag = true; m_last = e->pos(); setCursor(Qt::ClosedHandCursor); }
}
void ImageView::mouseMoveEvent(QMouseEvent* e) {
    if (!m_drag || m_img.isNull()) return;
    const double s = scale();
    const QPointF c = center();
    const QPoint d = e->pos() - m_last;
    m_last = e->pos();
    m_center = c - QPointF(d.x(), d.y()) / s;
    m_scale = s;
    m_fit = false;
    update();
    emit viewChanged(m_scale, m_center, m_fit);
}
void ImageView::mouseReleaseEvent(QMouseEvent*) { m_drag = false; setCursor(Qt::ArrowCursor); }
void ImageView::mouseDoubleClickEvent(QMouseEvent*) {
    m_fit = true;
    update();
    emit viewChanged(m_scale, m_center, m_fit);
}
