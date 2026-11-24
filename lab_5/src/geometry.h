#ifndef GEOMETRY_H
#define GEOMETRY_H

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <algorithm>
#include <cmath>
#include <vector>

namespace geo {

// все сравнения вещественных чисел идут через эту точность
constexpr double EPS = 1e-7;

inline bool eq(double a, double b) { return std::fabs(a - b) <= EPS; }
inline bool lt(double a, double b) { return a < b - EPS; }
inline bool gt(double a, double b) { return a > b + EPS; }
inline bool le(double a, double b) { return a <= b + EPS; }
inline bool ge(double a, double b) { return a >= b - EPS; }
inline bool isZero(double a) { return std::fabs(a) <= EPS; }

struct Point {
    double x = 0, y = 0;
    Point() {}
    Point(double x_, double y_) : x(x_), y(y_) {}
};

inline bool samePoint(const Point &a, const Point &b) { return eq(a.x, b.x) && eq(a.y, b.y); }

struct Segment {
    Point a, b;
    Segment() {}
    Segment(Point a_, Point b_) : a(a_), b(b_) {}
    Segment(double x1, double y1, double x2, double y2) : a(x1, y1), b(x2, y2) {}
};

struct Rect {
    double xmin = 0, ymin = 0, xmax = 0, ymax = 0;
    Rect() {}
    Rect(double x1, double y1, double x2, double y2)
        : xmin(std::min(x1, x2)), ymin(std::min(y1, y2)), xmax(std::max(x1, x2)), ymax(std::max(y1, y2)) {}
    double width() const { return xmax - xmin; }
    double height() const { return ymax - ymin; }
};

using Ring = std::vector<Point>;

// счётчик арифметических операций для сравнения алгоритмов
struct OpStats {
    long long mul = 0, div = 0, add = 0, cmp = 0;
    void reset() { mul = div = add = cmp = 0; }
    long long heavy() const { return mul + div; }
    OpStats &operator+=(const OpStats &o) {
        mul += o.mul; div += o.div; add += o.add; cmp += o.cmp;
        return *this;
    }
};

inline double cross(const Point &a, const Point &b) { return a.x * b.y - a.y * b.x; }
inline double dot(const Point &a, const Point &b) { return a.x * b.x + a.y * b.y; }
inline Point operator-(const Point &a, const Point &b) { return Point(a.x - b.x, a.y - b.y); }
inline Point operator+(const Point &a, const Point &b) { return Point(a.x + b.x, a.y + b.y); }
inline Point operator*(const Point &a, double k) { return Point(a.x * k, a.y * k); }
inline double length(const Point &a) { return std::sqrt(a.x * a.x + a.y * a.y); }

inline double signedArea(const Ring &r) {
    double s = 0;
    for (size_t i = 0, n = r.size(); i < n; ++i)
        s += cross(r[i], r[(i + 1) % n]);
    return s / 2;
}

// расстояние от точки до отрезка ab
inline double distToSegment(const Point &p, const Point &a, const Point &b) {
    Point ab = b - a, ap = p - a;
    double len2 = dot(ab, ab);
    if (len2 <= EPS * EPS) return length(ap);
    double t = std::max(0.0, std::min(1.0, dot(ap, ab) / len2));
    return length(p - (a + ab * t));
}

}  // namespace geo

#endif
