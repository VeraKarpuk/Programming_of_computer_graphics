#include "segmentclip.h"

namespace geo {

int outcode(const Point &p, const Rect &w) {
    int c = INSIDE;
    if (lt(p.x, w.xmin)) c |= LEFT;
    else if (gt(p.x, w.xmax)) c |= RIGHT;
    if (lt(p.y, w.ymin)) c |= BOTTOM;
    else if (gt(p.y, w.ymax)) c |= TOP;
    return c;
}

bool aabbOutside(const Segment &s, const Rect &w) {
    // углы габарита (min,min) и (max,max): общий бит значит, что весь
    // габарит лежит по одну сторону от окна
    Point lo(std::min(s.a.x, s.b.x), std::min(s.a.y, s.b.y));
    Point hi(std::max(s.a.x, s.b.x), std::max(s.a.y, s.b.y));
    return (outcode(lo, w) & outcode(hi, w)) != 0;
}

bool clipSutherlandCohen(const Segment &s, const Rect &w, Segment &out, OpStats *st) {
    Point p1 = s.a, p2 = s.b;

    // явное уравнение прямой считаем только тогда, когда оно понадобится
    bool lineReady = false, vertical = false;
    double k = 0, b = 0;
    auto prepareLine = [&]() {
        lineReady = true;
        double dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
        if (st) st->add += 2;
        if (isZero(dx)) {
            vertical = true;
            return;
        }
        k = dy / dx;
        b = s.a.y - k * s.a.x;
        if (st) { st->div += 1; st->mul += 1; st->add += 1; }
    };

    for (int iter = 0; iter < 10; ++iter) {
        int c1 = outcode(p1, w), c2 = outcode(p2, w);
        if (st) st->cmp += 8;
        if ((c1 | c2) == 0) {
            out = Segment(p1, p2);
            return true;
        }
        if (c1 & c2) return false;

        Point &p = c1 ? p1 : p2;
        int c = c1 ? c1 : c2;
        if (!lineReady) prepareLine();

        if (c & (LEFT | RIGHT)) {
            if (vertical) return false;
            double X = (c & LEFT) ? w.xmin : w.xmax;
            p = Point(X, k * X + b);
            if (st) { st->mul += 1; st->add += 1; }
        } else {
            if (!vertical && isZero(k)) return false;
            double Y = (c & BOTTOM) ? w.ymin : w.ymax;
            if (vertical) p = Point(s.a.x, Y);
            else {
                p = Point((Y - b) / k, Y);
                if (st) { st->div += 1; st->add += 1; }
            }
        }
    }
    return false;
}

bool clipLiangBarsky(const Segment &s, const Rect &w, Segment &out, OpStats *st) {
    double dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
    double p[4] = {-dx, dx, -dy, dy};
    double q[4] = {s.a.x - w.xmin, w.xmax - s.a.x, s.a.y - w.ymin, w.ymax - s.a.y};
    if (st) st->add += 6;

    double t0 = 0, t1 = 1;
    for (int i = 0; i < 4; ++i) {
        if (st) st->cmp += 1;
        if (isZero(p[i])) {
            // параллельно границе: либо целиком снаружи, либо целиком внутри полосы
            if (q[i] < -EPS) return false;
            continue;
        }
        double r = q[i] / p[i];
        if (st) { st->div += 1; st->cmp += 2; }
        if (p[i] < 0) {
            if (r > t1 + EPS) return false;
            if (gt(r, t0)) t0 = r;
        } else {
            if (r < t0 - EPS) return false;
            if (lt(r, t1)) t1 = r;
        }
    }
    if (t0 > t1 + EPS) return false;

    out = s;
    if (gt(t0, 0)) {
        out.a = Point(s.a.x + t0 * dx, s.a.y + t0 * dy);
        if (st) { st->mul += 2; st->add += 2; }
    }
    if (lt(t1, 1)) {
        out.b = Point(s.a.x + t1 * dx, s.a.y + t1 * dy);
        if (st) { st->mul += 2; st->add += 2; }
    }
    return true;
}

bool isConvex(const Ring &pts) {
    Ring r;
    for (const Point &p : pts)
        if (r.empty() || !samePoint(r.back(), p)) r.push_back(p);
    while (r.size() > 1 && samePoint(r.front(), r.back())) r.pop_back();
    size_t n = r.size();
    if (n < 3) return false;

    int sign = 0;
    double turning = 0;
    for (size_t i = 0; i < n; ++i) {
        Point e1 = r[(i + 1) % n] - r[i];
        Point e2 = r[(i + 2) % n] - r[(i + 1) % n];
        double cr = cross(e1, e2);
        double cn = cr / (length(e1) * length(e2));
        if (std::fabs(cn) > 1e-9) {
            int sg = cr > 0 ? 1 : -1;
            if (sign == 0) sign = sg;
            else if (sign != sg) return false;
        }
        turning += std::atan2(cr, dot(e1, e2));
    }
    // у звезды (самопересечение) полный поворот больше 2*pi
    return sign != 0 && std::fabs(std::fabs(turning) - 2 * M_PI) < 1e-6;
}

bool ConvexClipper::set(const Ring &pts) {
    Ring r;
    for (const Point &p : pts)
        if (r.empty() || !samePoint(r.back(), p)) r.push_back(p);
    while (r.size() > 1 && samePoint(r.front(), r.back())) r.pop_back();
    if (!isConvex(r)) return false;
    if (signedArea(r) < 0) std::reverse(r.begin(), r.end());

    // убираем лишние вершины на прямой, чтобы нормали были определены
    Ring clean;
    size_t n = r.size();
    for (size_t i = 0; i < n; ++i) {
        const Point &prev = r[(i + n - 1) % n], &cur = r[i], &next = r[(i + 1) % n];
        Point e1 = cur - prev, e2 = next - cur;
        if (std::fabs(cross(e1, e2)) / (length(e1) * length(e2)) > 1e-9) clean.push_back(cur);
    }
    if (clean.size() < 3) return false;

    v_ = clean;
    n_.clear();
    for (size_t i = 0; i < v_.size(); ++i) {
        Point e = v_[(i + 1) % v_.size()] - v_[i];
        double len = length(e);
        n_.push_back(Point(-e.y / len, e.x / len));  // левая нормаль = внутренняя
    }
    return true;
}

ConvexClipper ConvexClipper::fromRect(const Rect &r) {
    ConvexClipper c;
    c.set({Point(r.xmin, r.ymin), Point(r.xmax, r.ymin), Point(r.xmax, r.ymax), Point(r.xmin, r.ymax)});
    return c;
}

bool ConvexClipper::clip(const Segment &s, Segment &out, OpStats *st) const {
    double dx = s.b.x - s.a.x, dy = s.b.y - s.a.y;
    if (st) st->add += 2;

    double tIn = 0, tOut = 1;
    for (size_t i = 0; i < v_.size(); ++i) {
        // условие видимости: n * (P(t) - F) >= 0  ->  num + t * den >= 0
        double num = n_[i].x * (s.a.x - v_[i].x) + n_[i].y * (s.a.y - v_[i].y);
        double den = n_[i].x * dx + n_[i].y * dy;
        if (st) { st->mul += 4; st->add += 4; st->cmp += 1; }
        if (isZero(den)) {
            if (num < -EPS) return false;
            continue;
        }
        double t = -num / den;
        if (st) { st->div += 1; st->cmp += 2; }
        if (den > 0) {
            if (gt(t, tIn)) tIn = t;
        } else {
            if (lt(t, tOut)) tOut = t;
        }
        if (tIn > tOut + EPS) return false;
    }

    out = s;
    if (gt(tIn, 0)) {
        out.a = Point(s.a.x + tIn * dx, s.a.y + tIn * dy);
        if (st) { st->mul += 2; st->add += 2; }
    }
    if (lt(tOut, 1)) {
        out.b = Point(s.a.x + tOut * dx, s.a.y + tOut * dy);
        if (st) { st->mul += 2; st->add += 2; }
    }
    return true;
}

}  // namespace geo
