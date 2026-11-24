#include "testcases.h"

#include "polygonclip.h"

namespace geo {

Rect testWindow() { return Rect(0, 0, 10, 10); }

bool sameSegment(const Segment &a, const Segment &b) {
    const double tol = 1e-6;
    auto near = [&](const Point &p, const Point &q) {
        return std::fabs(p.x - q.x) < tol && std::fabs(p.y - q.y) < tol;
    };
    return near(a.a, b.a) && near(a.b, b.b);
}

std::vector<SegTest> segmentTests() {
    std::vector<SegTest> t;
    auto add = [&](const char *name, const char *marker, Segment s, bool vis, Segment e) {
        t.push_back({name, marker, s, vis, e});
    };
    Segment none;
    add("целиком внутри", "", Segment(2, 2, 8, 8), true, Segment(2, 2, 8, 8));
    add("целиком снаружи", "", Segment(-5, 2, -1, 8), false, none);
    add("пересекает две стороны", "", Segment(-5, 5, 15, 5), true, Segment(0, 5, 10, 5));
    add("диагональ через две вершины окна", "вершины", Segment(-5, -5, 15, 15), true, Segment(0, 0, 10, 10));
    add("вертикаль через окно", "", Segment(5, -5, 5, 15), true, Segment(5, 0, 5, 10));
    add("на левой границе", "граница", Segment(0, 2, 0, 8), true, Segment(0, 2, 0, 8));
    add("на границе, выходит за окно", "граница", Segment(0, -3, 0, 13), true, Segment(0, 0, 0, 10));
    add("вдоль верхней границы", "граница", Segment(2, 10, 8, 10), true, Segment(2, 10, 8, 10));
    add("вдоль верхней границы с выходом", "граница", Segment(-4, 10, 14, 10), true, Segment(0, 10, 10, 10));
    add("точка в вершине (0,0)", "нулевая длина", Segment(0, 0, 0, 0), true, Segment(0, 0, 0, 0));
    add("точка в вершине (10,0)", "нулевая длина", Segment(10, 0, 10, 0), true, Segment(10, 0, 10, 0));
    add("точка в вершине (10,10)", "нулевая длина", Segment(10, 10, 10, 10), true, Segment(10, 10, 10, 10));
    add("точка в вершине (0,10)", "нулевая длина", Segment(0, 10, 0, 10), true, Segment(0, 10, 0, 10));
    add("точка на стороне", "нулевая длина", Segment(5, 0, 5, 0), true, Segment(5, 0, 5, 0));
    add("точка снаружи", "нулевая длина", Segment(11, 5, 11, 5), false, none);
    add("касание вершины (0,0) снаружи", "вершины", Segment(-5, 5, 5, -5), true, Segment(0, 0, 0, 0));
    add("касание вершины (0,10) снаружи", "вершины", Segment(-5, 5, 5, 15), true, Segment(0, 10, 0, 10));
    add("параллельно границе снаружи", "", Segment(-1, 0, -1, 10), false, none);
    add("в пределах eps от границы", "граница", Segment(-1e-9, 2, -1e-9, 8), true, Segment(-1e-9, 2, -1e-9, 8));
    add("наклонный, один конец внутри", "", Segment(5, 5, 15, 10), true, Segment(5, 5, 10, 7.5));
    return t;
}

std::vector<PolyTest> polygonTests() {
    std::vector<PolyTest> t;
    auto sq = [](double x1, double y1, double x2, double y2) {
        return Ring{Point(x1, y1), Point(x2, y1), Point(x2, y2), Point(x1, y2)};
    };
    auto sqCW = [](double x1, double y1, double x2, double y2) {
        return Ring{Point(x1, y1), Point(x1, y2), Point(x2, y2), Point(x2, y1)};
    };
    Ring win = sq(0, 0, 10, 10);
    auto add = [&](const char *name, const char *marker, std::vector<Ring> s, Ring c, int n) {
        t.push_back({name, marker, s, c, n});
    };

    add("внутри окна", "", {sq(2, 2, 8, 8)}, win, 1);
    add("окно целиком внутри многоугольника", "", {sq(-5, -5, 15, 15)}, win, 1);
    add("вне окна", "", {sq(20, 20, 30, 30)}, win, 0);
    add("перекрытие углом", "", {sq(5, 5, 15, 15)}, win, 1);
    add("касание стороной снаружи", "граница", {sq(10, 0, 20, 10)}, win, 0);
    add("ребро лежит на границе окна", "граница", {sq(0, -5, 10, 5)}, win, 1);
    add("вершины на сторонах окна", "граница", {Ring{Point(5, 0), Point(10, 5), Point(5, 10), Point(0, 5)}}, win, 1);
    add("ромб: рёбра через вершины окна", "вершины",
        {Ring{Point(5, -5), Point(15, 5), Point(5, 15), Point(-5, 5)}}, win, 1);
    add("вогнутая буква П распадается на 2 части", "вогнутый",
        {Ring{Point(1, 1), Point(9, 1), Point(9, 9), Point(7, 9), Point(7, 3), Point(3, 3), Point(3, 9), Point(1, 9)}},
        sq(0, 5, 10, 10), 2);
    add("вогнутая: окно режет перемычку", "вогнутый",
        {Ring{Point(1, 1), Point(9, 1), Point(9, 9), Point(7, 9), Point(7, 3), Point(3, 3), Point(3, 9), Point(1, 9)}},
        sq(0, 2, 10, 10), 1);
    add("дыра целиком в окне", "дыра", {sq(0, 0, 8, 8), sqCW(2, 2, 6, 6)}, sq(-1, -1, 9, 9), 2);
    add("окно задевает дыру", "дыра", {sq(0, 0, 8, 8), sqCW(2, 2, 6, 6)}, sq(3, 3, 12, 12), 1);
    add("окно внутри дыры", "дыра", {sq(-10, -10, 20, 20), sqCW(-5, -5, 15, 15)}, win, 0);
    add("самопересекающийся бантик", "самопересечение",
        {Ring{Point(0, 0), Point(8, 8), Point(8, 0), Point(0, 8)}}, sq(0, 0, 6, 10), 1);
    add("звезда (самопересечение)", "самопересечение",
        {Ring{Point(5, 0), Point(8, 10), Point(0, 4), Point(10, 4), Point(2, 10)}}, sq(1, 1, 9, 7), 1);
    add("вогнутый отсекатель (Г-образный)", "вогнутый отсекатель",
        {sq(-5, -5, 15, 15)}, Ring{Point(0, 0), Point(10, 0), Point(10, 4), Point(4, 4), Point(4, 10), Point(0, 10)}, 1);
    return t;
}

int polygonMismatches(const std::vector<Ring> &subject, const Ring &clipper,
                      const std::vector<Ring> &result) {
    Ring c = clipper;
    if (signedArea(c) < 0) std::reverse(c.begin(), c.end());
    double xmin = 1e18, ymin = 1e18, xmax = -1e18, ymax = -1e18;
    for (const Ring &r : subject)
        for (const Point &p : r) {
            xmin = std::min(xmin, p.x); xmax = std::max(xmax, p.x);
            ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y);
        }
    for (const Point &p : c) {
        xmin = std::min(xmin, p.x); xmax = std::max(xmax, p.x);
        ymin = std::min(ymin, p.y); ymax = std::max(ymax, p.y);
    }
    int bad = 0;
    const int N = 80;
    for (int i = 0; i < N; ++i)
        for (int j = 0; j < N; ++j) {
            // дробные сдвиги, чтобы пробные точки не попадали на границы
            Point p(xmin + (xmax - xmin) * (i + 0.4137) / N, ymin + (ymax - ymin) * (j + 0.5731) / N);
            bool expect = classifyEvenOdd(subject, p) == 1 && classifyEvenOdd({c}, p) == 1;
            bool actual = classifyEvenOdd(result, p) == 1;
            if (expect != actual) ++bad;
        }
    return bad;
}

}  // namespace geo
