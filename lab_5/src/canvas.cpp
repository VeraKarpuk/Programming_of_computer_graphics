#include "canvas.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QWheelEvent>
#include <cmath>

#include "viewutil.h"

namespace {

const QColor kWindowColor(230, 126, 34);
const QColor kSourceColor(60, 105, 210);
const QColor kResultColor(22, 160, 80);

bool isPoint(const geo::Segment &s) { return geo::samePoint(s.a, s.b); }

bool sameSeg(const geo::Segment &a, const geo::Segment &b) {
    auto near = [](const geo::Point &p, const geo::Point &q) {
        return std::fabs(p.x - q.x) < 1e-6 && std::fabs(p.y - q.y) < 1e-6;
    };
    return near(a.a, b.a) && near(a.b, b.b);
}

geo::Point centroid(const std::vector<geo::Point> &pts) {
    geo::Point c;
    if (pts.empty()) return c;
    for (const geo::Point &p : pts) { c.x += p.x; c.y += p.y; }
    c.x /= pts.size();
    c.y /= pts.size();
    return c;
}

}  // namespace

Canvas::Canvas(QWidget *parent) : QWidget(parent), rng_(2024) {
    setMouseTracking(true);
    setMinimumSize(480, 360);
    setFocusPolicy(Qt::StrongFocus);

    scene_.window = geo::Rect(-4, -3, 5, 4);
    scene_.segments = {
        geo::Segment(-6, -1, 7, 2),   geo::Segment(-2, -2, 3, 3),  geo::Segment(-7, 5, -1, 6),
        geo::Segment(-5, -4, 6, 5),   geo::Segment(0, -6, 0, 6),   geo::Segment(6, -2, 8, 3),
        geo::Segment(-4, -3, 5, -3),  geo::Segment(-3, 6, 2, 1),   geo::Segment(1, 1, 1, 1),
    };
    for (int i = 0; i < 6; ++i)
        scene_.clipperPoly.push_back(geo::Point(4 * std::cos(i * M_PI / 3), 0.5 + 4 * std::sin(i * M_PI / 3)));

    connect(&timer_, &QTimer::timeout, this, &Canvas::tick);
    timer_.setInterval(16);
    recompute();
}

void Canvas::setMode(Mode m) {
    mode_ = m;
    drag_ = NoDrag;
    recompute();
    refreshTimer();
    fitView();
    update();
}

void Canvas::setScene(const geo::SceneData &s) {
    scene_ = s;
    if (scene_.clipperPoly.empty())
        for (int i = 0; i < 6; ++i)
            scene_.clipperPoly.push_back(geo::Point(4 * std::cos(i * M_PI / 3), 0.5 + 4 * std::sin(i * M_PI / 3)));
    recompute();
    fitView();
    update();
}

void Canvas::showEvent(QShowEvent *) {
    if (needFit_) fitView();
}

QPointF Canvas::toScreen(const geo::Point &p) const {
    return QPointF(origin_.x() + p.x * scale_, origin_.y() - p.y * scale_);
}

geo::Point Canvas::toWorld(const QPointF &s) const {
    return geo::Point((s.x() - origin_.x()) / scale_, (origin_.y() - s.y()) / scale_);
}

void Canvas::fitView() {
    if (!isVisible()) { needFit_ = true; return; }
    needFit_ = false;

    double xmin = 1e300, ymin = 1e300, xmax = -1e300, ymax = -1e300;
    auto add = [&](const geo::Point &p) {
        xmin = std::min(xmin, p.x); xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y);
    };
    if (mode_ == BigData) {
        add(geo::Point(bigArea_.xmin, bigArea_.ymin));
        add(geo::Point(bigArea_.xmax, bigArea_.ymax));
    } else {
        if (usesWindow()) {
            add(geo::Point(win().xmin, win().ymin));
            add(geo::Point(win().xmax, win().ymax));
        } else {
            for (const geo::Point &p : scene_.clipperPoly) add(p);
        }
        if (segmentMode()) {
            for (const geo::Segment &s : scene_.segments) { add(s.a); add(s.b); }
        } else {
            for (const geo::Ring &r : scene_.polygons)
                for (const geo::Point &p : r) add(p);
        }
    }
    if (xmin > xmax) { xmin = ymin = -10; xmax = ymax = 10; }
    double bw = std::max(xmax - xmin, 1e-3), bh = std::max(ymax - ymin, 1e-3);
    bw *= 1.16;
    bh *= 1.16;
    scale_ = std::min(width() / bw, height() / bh);
    double cx = (xmin + xmax) / 2, cy = (ymin + ymax) / 2;
    origin_ = QPointF(width() / 2.0 - cx * scale_, height() / 2.0 + cy * scale_);
    update();
}

// ---------------------------------------------------------------- вычисления

void Canvas::recompute() {
    visible_.clear();
    stLB_.reset();
    stSC_.reset();
    stCB_.reset();
    mismatches_ = 0;
    rejected_ = 0;
    wa_ = geo::WAResult();
    convexOk_ = true;

    switch (mode_) {
    case LiangBarsky:
    case SutherlandCohen:
        for (const geo::Segment &s : scene_.segments) {
            if (prefilter_ && geo::aabbOutside(s, scene_.window)) { ++rejected_; continue; }
            geo::Segment a, b;
            bool v1 = geo::clipLiangBarsky(s, scene_.window, a, &stLB_);
            bool v2 = geo::clipSutherlandCohen(s, scene_.window, b, &stSC_);
            if (v1 != v2 || (v1 && !sameSeg(a, b))) ++mismatches_;
            if (mode_ == LiangBarsky && v1) visible_.push_back(a);
            if (mode_ == SutherlandCohen && v2) visible_.push_back(b);
        }
        break;
    case CyrusBeck:
        convexOk_ = convex_.set(scene_.clipperPoly);
        if (convexOk_) {
            geo::Rect box = geo::Rect(convex_.vertices()[0].x, convex_.vertices()[0].y,
                                      convex_.vertices()[0].x, convex_.vertices()[0].y);
            for (const geo::Point &pt : convex_.vertices())
                box = geo::Rect(std::min(box.xmin, pt.x), std::min(box.ymin, pt.y),
                                std::max(box.xmax, pt.x), std::max(box.ymax, pt.y));
            for (const geo::Segment &s : scene_.segments) {
                if (prefilter_ && geo::aabbOutside(s, box)) { ++rejected_; continue; }
                geo::Segment a;
                if (convex_.clip(s, a, &stCB_)) visible_.push_back(a);
            }
        }
        break;
    case PolyWindow: {
        const geo::Rect &w = scene_.window;
        geo::Ring r = {geo::Point(w.xmin, w.ymin), geo::Point(w.xmax, w.ymin),
                       geo::Point(w.xmax, w.ymax), geo::Point(w.xmin, w.ymax)};
        wa_ = geo::weilerAtherton(scene_.polygons, r);
        break;
    }
    case PolyConvex:
        convexOk_ = convex_.set(scene_.clipperPoly);
        if (convexOk_) wa_ = geo::weilerAtherton(scene_.polygons, scene_.clipperPoly);
        break;
    case BigData:
        break;
    }

    if (mode_ == BigData) {
        update();
        return;
    }
    if (!convexOk_) emit notice(QString::fromUtf8("Отсекатель должен быть выпуклым многоугольником"));

    QString text;
    if (segmentMode()) {
        text = segmentStatsText();
    } else {
        text += QString::fromUtf8("Контуров в subject: %1\n").arg(scene_.polygons.size());
        text += QString::fromUtf8("Точек пересечения: %1\n").arg(wa_.intersections);
        text += QString::fromUtf8("Точек входа: %1, точек выхода: %2\n")
                    .arg(wa_.entryPoints.size()).arg(wa_.exitPoints.size());
        text += QString::fromUtf8("Контуров результата: %1\n").arg(wa_.contours.size());
        text += QString::fromUtf8("Площадь результата: %1\n")
                    .arg(geo::evenOddArea(wa_.contours), 0, 'f', 4);
        text += QString::fromUtf8("(зелёные точки - вход, красные - выход)");
    }
    emit statsChanged(text);
    update();
}

QString Canvas::segmentStatsText() const {
    QString t;
    t += QString::fromUtf8("Отрезков: %1, видимых: %2\n").arg(scene_.segments.size()).arg(visible_.size());
    t += QString::fromUtf8("Точность сравнений ε = 1e-7\n");
    if (prefilter_) t += QString::fromUtf8("Отброшено габаритом (AABB) до отсечения: %1\n").arg(rejected_);
    t += "\n";
    t += QString::fromUtf8("Арифметические операции на всех отрезках:\n");
    if (mode_ == CyrusBeck) {
        t += QString::fromUtf8("Сайрус-Бек (%1 рёбер): умн. %2, дел. %3, всего %4\n")
                 .arg(convex_.vertices().size()).arg(stCB_.mul).arg(stCB_.div).arg(stCB_.heavy());
        return t;
    }
    t += QString::fromUtf8("Сазерленд-Коэн: умн. %1, дел. %2, всего %3\n")
             .arg(stSC_.mul).arg(stSC_.div).arg(stSC_.heavy());
    t += QString::fromUtf8("Лианг-Барски:   умн. %1, дел. %2, всего %3\n")
             .arg(stLB_.mul).arg(stLB_.div).arg(stLB_.heavy());
    if (stSC_.heavy() > 0 && stLB_.heavy() > 0)
        t += QString::fromUtf8("Отношение (ЛБ / СК): %1\n")
                 .arg((double)stLB_.heavy() / stSC_.heavy(), 0, 'f', 2);
    t += QString::fromUtf8("Расхождений результатов ЛБ и СК: %1").arg(mismatches_);
    return t;
}

// ---------------------------------------------------------------- рисование

void Canvas::drawGrid(QPainter &p) {
    double step = viewutil::niceStep(80.0 / scale_);
    geo::Point lo = toWorld(QPointF(0, height())), hi = toWorld(QPointF(width(), 0));
    long long i0 = (long long)std::floor(lo.x / step), i1 = (long long)std::ceil(hi.x / step);
    long long j0 = (long long)std::floor(lo.y / step), j1 = (long long)std::ceil(hi.y / step);
    if (i1 - i0 > 400 || j1 - j0 > 400) return;

    QPen grid(QColor(228, 228, 228), 1);
    p.setPen(grid);
    for (long long i = i0; i <= i1; ++i) {
        double sx = toScreen(geo::Point(i * step, 0)).x();
        p.drawLine(QPointF(sx, 0), QPointF(sx, height()));
    }
    for (long long j = j0; j <= j1; ++j) {
        double sy = toScreen(geo::Point(0, j * step)).y();
        p.drawLine(QPointF(0, sy), QPointF(width(), sy));
    }

    QPen axis(QColor(60, 60, 60), 1.6);
    p.setPen(axis);
    double ax = origin_.x(), ay = origin_.y();
    if (ay >= 0 && ay <= height()) {
        p.drawLine(QPointF(0, ay), QPointF(width(), ay));
        p.drawLine(QPointF(width() - 9, ay - 4), QPointF(width(), ay));
        p.drawLine(QPointF(width() - 9, ay + 4), QPointF(width(), ay));
        p.drawText(QPointF(width() - 14, ay - 8), "x");
    }
    if (ax >= 0 && ax <= width()) {
        p.drawLine(QPointF(ax, 0), QPointF(ax, height()));
        p.drawLine(QPointF(ax - 4, 9), QPointF(ax, 0));
        p.drawLine(QPointF(ax + 4, 9), QPointF(ax, 0));
        p.drawText(QPointF(ax + 8, 12), "y");
    }

    // подписи делений прижимаем к оси или к краю окна
    p.setPen(QColor(70, 70, 70));
    double labelY = std::max(14.0, std::min(ay + 14, (double)height() - 6));
    double labelX = std::max(4.0, std::min(ax + 5, (double)width() - 56));
    for (long long i = i0; i <= i1; ++i) {
        double sx = toScreen(geo::Point(i * step, 0)).x();
        p.drawLine(QPointF(sx, labelY - 12), QPointF(sx, labelY - 8));
        p.drawText(QPointF(sx + 3, labelY), QString::number(i * step, 'g', 8));
    }
    for (long long j = j0; j <= j1; ++j) {
        if (j == 0) continue;
        double sy = toScreen(geo::Point(0, j * step)).y();
        p.drawText(QPointF(labelX, sy - 3), QString::number(j * step, 'g', 8));
    }
    p.drawText(QPointF(8, height() - 8),
               QString::fromUtf8("1 клетка = %1").arg(step, 0, 'g', 6));
}

void Canvas::drawLegend(QPainter &p) {
    struct Item { QColor c; QString text; };
    std::vector<Item> items;
    items.push_back({kWindowColor, usesWindow() ? QString::fromUtf8("отсекающее окно")
                                                : QString::fromUtf8("выпуклый отсекатель")});
    items.push_back({kSourceColor, QString::fromUtf8("исходные")});
    items.push_back({kResultColor, QString::fromUtf8("видимые части")});
    int y = 16;
    for (const Item &it : items) {
        p.setPen(QPen(it.c, 3));
        p.drawLine(10, y - 4, 30, y - 4);
        p.setPen(QColor(40, 40, 40));
        p.drawText(36, y, it.text);
        y += 16;
    }
}

void Canvas::drawWindowOrClipper(QPainter &p) {
    QBrush fill(QColor(kWindowColor.red(), kWindowColor.green(), kWindowColor.blue(), 28));
    if (usesWindow()) {
        QPointF tl = toScreen(geo::Point(win().xmin, win().ymax));
        QPointF br = toScreen(geo::Point(win().xmax, win().ymin));
        p.setPen(QPen(kWindowColor, 2.5));
        p.setBrush(fill);
        p.drawRect(QRectF(tl, br));
    } else {
        QPolygonF poly;
        for (const geo::Point &pt : scene_.clipperPoly) poly << toScreen(pt);
        p.setPen(QPen(convexOk_ ? kWindowColor : QColor(200, 30, 30), 2.5));
        p.setBrush(fill);
        p.drawPolygon(poly);
    }
    p.setBrush(Qt::NoBrush);
}

void Canvas::drawHandles(QPainter &p) {
    p.setBrush(Qt::white);
    p.setPen(QPen(kWindowColor, 2));
    if (usesWindow()) {
        const geo::Rect &w = win();
        geo::Point c[4] = {{w.xmin, w.ymin}, {w.xmax, w.ymin}, {w.xmax, w.ymax}, {w.xmin, w.ymax}};
        for (const geo::Point &pt : c) {
            QPointF s = toScreen(pt);
            p.drawRect(QRectF(s.x() - 4, s.y() - 4, 8, 8));
        }
    } else {
        for (const geo::Point &pt : scene_.clipperPoly) p.drawEllipse(toScreen(pt), 5, 5);
    }
    if (mode_ == BigData) return;

    p.setPen(QPen(kSourceColor, 1.5));
    if (segmentMode()) {
        if (scene_.segments.size() > 300) return;
        for (const geo::Segment &s : scene_.segments) {
            p.drawEllipse(toScreen(s.a), 3.5, 3.5);
            p.drawEllipse(toScreen(s.b), 3.5, 3.5);
        }
    } else {
        for (const geo::Ring &r : scene_.polygons)
            for (const geo::Point &pt : r) p.drawEllipse(toScreen(pt), 3.5, 3.5);
    }
}

void Canvas::drawSegmentsScene(QPainter &p) {
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(kSourceColor, 1.3, Qt::DashLine));
    for (const geo::Segment &s : scene_.segments) {
        if (isPoint(s)) {
            p.drawEllipse(toScreen(s.a), 2.5, 2.5);
        } else {
            p.drawLine(toScreen(s.a), toScreen(s.b));
        }
    }
    drawWindowOrClipper(p);

    QPen res(kResultColor, 3.4);
    res.setCapStyle(Qt::RoundCap);
    p.setPen(res);
    for (const geo::Segment &s : visible_) {
        if (isPoint(s)) {
            p.setBrush(kResultColor);
            p.drawEllipse(toScreen(s.a), 4.5, 4.5);
            p.setBrush(Qt::NoBrush);
        } else {
            p.drawLine(toScreen(s.a), toScreen(s.b));
        }
    }
    drawHandles(p);
}

void Canvas::drawRings(QPainter &p, const std::vector<geo::Ring> &rings, const QPen &pen,
                       const QBrush &brush) {
    QPainterPath path;
    path.setFillRule(Qt::OddEvenFill);
    for (const geo::Ring &r : rings) {
        if (r.size() < 2) continue;
        QPolygonF poly;
        for (const geo::Point &pt : r) poly << toScreen(pt);
        path.addPolygon(poly);
        path.closeSubpath();
    }
    p.setPen(pen);
    p.setBrush(brush);
    p.drawPath(path);
    p.setBrush(Qt::NoBrush);
}

void Canvas::drawPolygonsScene(QPainter &p) {
    drawRings(p, scene_.polygons, QPen(kSourceColor, 1.6),
              QBrush(QColor(kSourceColor.red(), kSourceColor.green(), kSourceColor.blue(), 40)));
    drawWindowOrClipper(p);
    drawRings(p, wa_.contours, QPen(kResultColor, 3.2),
              QBrush(QColor(kResultColor.red(), kResultColor.green(), kResultColor.blue(), 110)));

    p.setPen(QPen(Qt::white, 1));
    p.setBrush(QColor(30, 150, 60));
    for (const geo::Point &pt : wa_.entryPoints) p.drawEllipse(toScreen(pt), 5, 5);
    p.setBrush(QColor(210, 40, 40));
    for (const geo::Point &pt : wa_.exitPoints) p.drawEllipse(toScreen(pt), 5, 5);
    p.setBrush(Qt::NoBrush);
    drawHandles(p);
}

void Canvas::drawBig(QPainter &p) {
    // запрос к базе: дерево или перебор
    QElapsedTimer qt;
    qt.start();
    if (useTree_) tree_.clipWindow(bigWindow_, bigVisible_, nullptr, qs_);
    else geo::clipBruteForce(big_, bigWindow_, bigVisible_, qs_);
    queryMs_ = qt.nsecsElapsed() / 1e6;

    p.setRenderHint(QPainter::Antialiasing, false);
    if (showAll_) {
        std::vector<QLineF> lines;
        lines.reserve(big_.size());
        for (const geo::Segment &s : big_) lines.push_back(QLineF(toScreen(s.a), toScreen(s.b)));
        p.setPen(QPen(QColor(185, 200, 235), 1));
        p.drawLines(lines.data(), (int)lines.size());
    }
    drawWindowOrClipper(p);

    std::vector<QLineF> vis;
    vis.reserve(bigVisible_.size());
    for (const geo::Segment &s : bigVisible_) vis.push_back(QLineF(toScreen(s.a), toScreen(s.b)));
    p.setPen(QPen(kResultColor, 1.6));
    p.drawLines(vis.data(), (int)vis.size());

    // граница области данных
    p.setPen(QPen(QColor(120, 120, 120), 1, Qt::DotLine));
    p.drawRect(QRectF(toScreen(geo::Point(bigArea_.xmin, bigArea_.ymax)),
                      toScreen(geo::Point(bigArea_.xmax, bigArea_.ymin))));
    drawHandles(p);
}

void Canvas::paintEvent(QPaintEvent *) {
    QElapsedTimer total;
    total.start();

    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    p.setRenderHint(QPainter::Antialiasing, mode_ != BigData);
    p.setFont(font());
    drawGrid(p);
    if (mode_ == BigData) drawBig(p);
    else if (segmentMode()) drawSegmentsScene(p);
    else drawPolygonsScene(p);
    drawLegend(p);
    p.end();

    drawMs_ = total.nsecsElapsed() / 1e6;
    if (mode_ != BigData) return;

    if (paintClock_.isValid()) {
        double dt = paintClock_.nsecsElapsed() / 1e6;
        if (animate_ && dt > 0) fps_ = fps_ <= 0 ? 1000.0 / dt : 0.9 * fps_ + 0.1 * (1000.0 / dt);
    }
    paintClock_.restart();

    if (!statClock_.isValid() || statClock_.elapsed() > 150) {
        statClock_.restart();
        QString t;
        t += QString::fromUtf8("Отрезков в базе: %1\n").arg(big_.size());
        if (useTree_)
            t += QString::fromUtf8("Метод: квадродерево (глубина %1, лист %2, узлов %3, реально %4)\n")
                     .arg(treeDepth_).arg(treeCap_).arg(tree_.nodeCount()).arg(tree_.maxDepthReached());
        else
            t += QString::fromUtf8("Метод: полный перебор O(N)\n");
        t += QString::fromUtf8("Построение дерева: %1 мс\n").arg(buildMs_, 0, 'f', 0);
        t += QString::fromUtf8("Запрос (отсечение): %1 мс\n").arg(queryMs_, 0, 'f', 3);
        t += QString::fromUtf8("Посещено узлов: %1\n").arg(qs_.nodesVisited);
        t += QString::fromUtf8("Отброшено по габариту (AABB): %1\n").arg(qs_.aabbRejected);
        t += QString::fromUtf8("Дошло до отсечения: %1, видимых: %2\n").arg(qs_.candidates).arg(qs_.visible);
        t += QString::fromUtf8("Отрисовка: %1 мс\n").arg(drawMs_ - queryMs_, 0, 'f', 2);
        double frame = drawMs_;
        if (dynPerFrame_ > 0)
            t += QString::fromUtf8("Обновление %1 отрезков/кадр: %2 мс\n").arg(dynPerFrame_).arg(updateMs_, 0, 'f', 2);
        t += QString::fromUtf8("Кадр: %1 мс, запас до %2 FPS\n").arg(frame, 0, 'f', 2).arg(frame > 0 ? 1000.0 / frame : 0, 0, 'f', 0);
        if (animate_) t += QString::fromUtf8("Измеренный FPS: %1").arg(fps_, 0, 'f', 1);
        emit statsChanged(t);
    }
}

// ---------------------------------------------------------------- мышь

Canvas::Hit Canvas::hitTest(const QPointF &pos) const {
    const double R = 9;
    auto near = [&](const geo::Point &pt) {
        QPointF s = toScreen(pt);
        return std::hypot(s.x() - pos.x(), s.y() - pos.y()) <= R;
    };
    Hit h;
    if (usesWindow()) {
        const geo::Rect &w = win();
        geo::Point c[4] = {{w.xmin, w.ymin}, {w.xmax, w.ymin}, {w.xmax, w.ymax}, {w.xmin, w.ymax}};
        for (int i = 0; i < 4; ++i)
            if (near(c[i])) { h.kind = Hit::WinCorner; h.i = i; return h; }
    } else {
        for (size_t i = 0; i < scene_.clipperPoly.size(); ++i)
            if (near(scene_.clipperPoly[i])) { h.kind = Hit::ClipVertex; h.i = (int)i; return h; }
    }
    if (mode_ != BigData) {
        if (segmentMode()) {
            for (size_t i = 0; i < scene_.segments.size(); ++i) {
                if (near(scene_.segments[i].a)) { h.kind = Hit::SegPoint; h.i = (int)i; h.j = 0; return h; }
                if (near(scene_.segments[i].b)) { h.kind = Hit::SegPoint; h.i = (int)i; h.j = 1; return h; }
            }
        } else {
            for (size_t r = 0; r < scene_.polygons.size(); ++r)
                for (size_t k = 0; k < scene_.polygons[r].size(); ++k)
                    if (near(scene_.polygons[r][k])) {
                        h.kind = Hit::RingPoint; h.i = (int)r; h.j = (int)k; return h;
                    }
        }
    }
    geo::Point wp = toWorld(pos);
    if (usesWindow()) {
        const geo::Rect &w = win();
        if (wp.x >= w.xmin && wp.x <= w.xmax && wp.y >= w.ymin && wp.y <= w.ymax) h.kind = Hit::WinBody;
    } else if (scene_.clipperPoly.size() >= 3 &&
               geo::classifyEvenOdd({scene_.clipperPoly}, wp) >= 0) {
        h.kind = Hit::ClipBody;
    }
    return h;
}

void Canvas::mousePressEvent(QMouseEvent *e) {
    QPointF pos = e->pos();
    lastPos_ = pos;
    if (e->button() == Qt::MiddleButton || e->button() == Qt::RightButton) {
        drag_ = Pan;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (e->button() != Qt::LeftButton) return;

    geo::Point wp = toWorld(pos);
    Hit h = hitTest(pos);

    if (h.kind == Hit::None && mode_ != BigData) {
        bool shift = e->modifiers().testFlag(Qt::ShiftModifier);
        bool ctrl = e->modifiers().testFlag(Qt::ControlModifier);
        if (segmentMode() && shift) {
            scene_.segments.push_back(geo::Segment(wp, wp));
            h.kind = Hit::SegPoint;
            h.i = (int)scene_.segments.size() - 1;
            h.j = 1;
        } else if (!segmentMode() && (shift || ctrl)) {
            // Shift: ещё одна вершина в последний контур, Ctrl: новый контур
            if (scene_.polygons.empty() || ctrl) scene_.polygons.push_back(geo::Ring());
            scene_.polygons.back().push_back(wp);
            h.kind = Hit::RingPoint;
            h.i = (int)scene_.polygons.size() - 1;
            h.j = (int)scene_.polygons.back().size() - 1;
        }
    }

    if (h.kind == Hit::None) {
        drag_ = Pan;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    drag_ = MoveObject;
    hit_ = h;
    pressWorld_ = wp;
    savedWindow_ = win();
    savedClipper_ = scene_.clipperPoly;
    if (h.kind == Hit::WinCorner) {
        const geo::Rect &w = win();
        geo::Point c[4] = {{w.xmin, w.ymin}, {w.xmax, w.ymin}, {w.xmax, w.ymax}, {w.xmin, w.ymax}};
        fixedCorner_ = c[(h.i + 2) % 4];
    }
    setCursor(h.kind == Hit::WinBody || h.kind == Hit::ClipBody ? Qt::SizeAllCursor : Qt::PointingHandCursor);
    applyDrag(wp);
}

void Canvas::applyDrag(const geo::Point &wp) {
    double dx = wp.x - pressWorld_.x, dy = wp.y - pressWorld_.y;
    switch (hit_.kind) {
    case Hit::WinCorner:
        win() = geo::Rect(fixedCorner_.x, fixedCorner_.y, wp.x, wp.y);
        break;
    case Hit::WinBody:
        win() = geo::Rect(savedWindow_.xmin + dx, savedWindow_.ymin + dy, savedWindow_.xmax + dx,
                          savedWindow_.ymax + dy);
        break;
    case Hit::ClipVertex: {
        geo::Ring r = scene_.clipperPoly;
        r[hit_.i] = wp;
        geo::ConvexClipper test;
        if (test.set(r)) scene_.clipperPoly = r;
        else emit notice(QString::fromUtf8("Отсекатель должен оставаться выпуклым"));
        break;
    }
    case Hit::ClipBody:
        for (size_t i = 0; i < savedClipper_.size(); ++i)
            scene_.clipperPoly[i] = geo::Point(savedClipper_[i].x + dx, savedClipper_[i].y + dy);
        break;
    case Hit::SegPoint: {
        geo::Segment &s = scene_.segments[hit_.i];
        (hit_.j == 0 ? s.a : s.b) = wp;
        break;
    }
    case Hit::RingPoint:
        scene_.polygons[hit_.i][hit_.j] = wp;
        break;
    case Hit::None:
        break;
    }
    recompute();
}

void Canvas::mouseMoveEvent(QMouseEvent *e) {
    QPointF pos = e->pos();
    geo::Point wp = toWorld(pos);
    emit cursorMoved(QString("x = %1,  y = %2").arg(wp.x, 0, 'f', 3).arg(wp.y, 0, 'f', 3));

    if (drag_ == Pan) {
        origin_ += pos - lastPos_;
        lastPos_ = pos;
        update();
    } else if (drag_ == MoveObject) {
        applyDrag(wp);
    } else {
        Hit h = hitTest(pos);
        if (h.kind == Hit::None) setCursor(Qt::ArrowCursor);
        else if (h.kind == Hit::WinBody || h.kind == Hit::ClipBody) setCursor(Qt::SizeAllCursor);
        else setCursor(Qt::PointingHandCursor);
    }
}

void Canvas::mouseReleaseEvent(QMouseEvent *) {
    if (drag_ == MoveObject && mode_ == BigData) {
        // после ручного перемещения анимация продолжается от новой позиции
        double t = animT_;
        geo::Point c((bigWindow_.xmin + bigWindow_.xmax) / 2, (bigWindow_.ymin + bigWindow_.ymax) / 2);
        animBase_ = geo::Point(c.x - 260 * std::sin(0.6 * t), c.y - 200 * std::sin(0.9 * t));
    }
    drag_ = NoDrag;
    setCursor(Qt::ArrowCursor);
}

void Canvas::mouseDoubleClickEvent(QMouseEvent *e) {
    if (mode_ == BigData || e->button() != Qt::LeftButton) return;
    Hit h = hitTest(e->pos());
    if (h.kind == Hit::SegPoint) {
        scene_.segments.erase(scene_.segments.begin() + h.i);
    } else if (h.kind == Hit::RingPoint) {
        geo::Ring &r = scene_.polygons[h.i];
        r.erase(r.begin() + h.j);
        if (r.size() < 3) scene_.polygons.erase(scene_.polygons.begin() + h.i);
    } else {
        return;
    }
    drag_ = NoDrag;
    recompute();
}

void Canvas::scaleClipper(double f) {
    if (usesWindow()) {
        geo::Rect &w = win();
        double cx = (w.xmin + w.xmax) / 2, cy = (w.ymin + w.ymax) / 2;
        double hw = (w.xmax - w.xmin) / 2 * f, hh = (w.ymax - w.ymin) / 2 * f;
        w = geo::Rect(cx - hw, cy - hh, cx + hw, cy + hh);
    } else {
        geo::Point c = centroid(scene_.clipperPoly);
        for (geo::Point &pt : scene_.clipperPoly)
            pt = geo::Point(c.x + (pt.x - c.x) * f, c.y + (pt.y - c.y) * f);
    }
}

void Canvas::scaleSubject(double f) {
    std::vector<geo::Point> all;
    for (const geo::Segment &s : scene_.segments) { all.push_back(s.a); all.push_back(s.b); }
    for (const geo::Ring &r : scene_.polygons) all.insert(all.end(), r.begin(), r.end());
    geo::Point c = centroid(all);
    auto sc = [&](geo::Point &pt) { pt = geo::Point(c.x + (pt.x - c.x) * f, c.y + (pt.y - c.y) * f); };
    for (geo::Segment &s : scene_.segments) { sc(s.a); sc(s.b); }
    for (geo::Ring &r : scene_.polygons)
        for (geo::Point &pt : r) sc(pt);
}

void Canvas::wheelEvent(QWheelEvent *e) {
    int delta = e->angleDelta().y();
    if (delta == 0) delta = e->angleDelta().x();  // Shift+колесо на некоторых системах
    double f = std::pow(1.1, delta / 120.0);

    if (e->modifiers().testFlag(Qt::ControlModifier)) {
        scaleClipper(f);
        recompute();
    } else if (e->modifiers().testFlag(Qt::ShiftModifier) && mode_ != BigData) {
        scaleSubject(f);
        recompute();
    } else {
        QPointF pos = e->position();
        geo::Point wp = toWorld(pos);
        scale_ = std::max(1e-4, std::min(1e7, scale_ * f));
        origin_ = QPointF(pos.x() - wp.x * scale_, pos.y() + wp.y * scale_);
        update();
    }
}

// ---------------------------------------------------------------- большие данные

void Canvas::generateBigData(int n, int depth, int capacity) {
    big_ = geo::randomSegments(n, bigArea_, 20.0, 12345);
    geo::sortSpatially(big_, bigArea_);
    rebuildTree(depth, capacity);
    emit notice(QString::fromUtf8("База из %1 отрезков создана, дерево построено за %2 мс")
                    .arg(n).arg(buildMs_, 0, 'f', 0));
}

void Canvas::rebuildTree(int depth, int capacity) {
    treeDepth_ = depth;
    treeCap_ = capacity;
    QElapsedTimer t;
    t.start();
    tree_.build(&big_, bigArea_, depth, capacity);
    buildMs_ = t.nsecsElapsed() / 1e6;
    update();
}

void Canvas::setAnimate(bool on) {
    animate_ = on;
    fps_ = 0;
    refreshTimer();
    update();
}

void Canvas::setDynamicPerFrame(int k) {
    dynPerFrame_ = k;
    refreshTimer();
}

void Canvas::refreshTimer() {
    bool need = mode_ == BigData && (animate_ || dynPerFrame_ > 0);
    if (need && !timer_.isActive()) {
        clock_.restart();
        timer_.start();
    } else if (!need) {
        timer_.stop();
    }
}

void Canvas::animatePlace() {
    double w = bigWindow_.width(), h = bigWindow_.height();
    double cx = animBase_.x + 260 * std::sin(0.6 * animT_);
    double cy = animBase_.y + 200 * std::sin(0.9 * animT_);
    bigWindow_ = geo::Rect(cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2);
}

void Canvas::tick() {
    if (mode_ != BigData) return;
    double dt = clock_.nsecsElapsed() * 1e-9;
    clock_.restart();
    if (animate_ && drag_ != MoveObject) {
        animT_ += dt;
        animatePlace();
    }
    if (dynPerFrame_ > 0 && !big_.empty()) {
        QElapsedTimer t;
        t.start();
        std::uniform_real_distribution<double> shift(-6, 6);
        auto clampCoord = [&](double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); };
        for (int i = 0; i < dynPerFrame_; ++i) {
            int id = (int)(rng_() % big_.size());
            geo::Segment old = big_[id], ns = old;
            double dx = shift(rng_), dy = shift(rng_);
            ns.a = geo::Point(clampCoord(old.a.x + dx, bigArea_.xmin, bigArea_.xmax),
                              clampCoord(old.a.y + dy, bigArea_.ymin, bigArea_.ymax));
            ns.b = geo::Point(clampCoord(old.b.x + dx, bigArea_.xmin, bigArea_.xmax),
                              clampCoord(old.b.y + dy, bigArea_.ymin, bigArea_.ymax));
            big_[id] = ns;
            tree_.update(id, old, ns);
        }
        updateMs_ = t.nsecsElapsed() / 1e6;
    }
    update();
}
