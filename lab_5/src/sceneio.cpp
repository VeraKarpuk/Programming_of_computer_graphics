#include "sceneio.h"

#include <cstdlib>
#include <random>
#include <sstream>

namespace geo {

namespace {

std::string stripComments(const std::string &text) {
    std::string out;
    bool star = false, hash = false;
    for (char ch : text) {
        if (hash) {
            if (ch == '\n') hash = false;
            continue;
        }
        if (ch == '#' && !star) { hash = true; continue; }
        if (ch == '*') { star = !star; out += ' '; continue; }
        if (!star) out += ch;
    }
    return out;
}

bool toDouble(const std::string &s, double &v) {
    char *end = nullptr;
    v = std::strtod(s.c_str(), &end);
    return end && *end == '\0' && !s.empty();
}

}  // namespace

bool parseScene(const std::string &text, SceneData &out, std::string &error) {
    std::istringstream in(stripComments(text));
    std::vector<std::string> tok;
    std::string t;
    while (in >> t) tok.push_back(t);
    if (tok.empty()) { error = "файл пуст"; return false; }

    SceneData sc;
    size_t pos = 0;
    auto num = [&](double &v) {
        if (pos >= tok.size() || !toDouble(tok[pos], v)) return false;
        ++pos;
        return true;
    };
    auto readPoints = [&](int count, Ring &dst) {
        for (int i = 0; i < count; ++i) {
            double x, y;
            if (!num(x) || !num(y)) return false;
            dst.push_back(Point(x, y));
        }
        return true;
    };
    auto readSegments = [&](int n) {
        for (int i = 0; i < n; ++i) {
            double a, b, c, d;
            if (!num(a) || !num(b) || !num(c) || !num(d)) return false;
            sc.segments.push_back(Segment(a, b, c, d));
        }
        return true;
    };
    auto readWindow = [&]() {
        double a, b, c, d;
        if (!num(a) || !num(b) || !num(c) || !num(d)) return false;
        sc.window = Rect(a, b, c, d);
        return true;
    };

    double first;
    if (toDouble(tok[0], first)) {
        int n = (int)first;
        if (n < 0 || first != n) { error = "неверное число отрезков"; return false; }
        pos = 1;
        if (!readSegments(n)) { error = "не хватает координат отрезков"; return false; }
        if (!readWindow()) { error = "не хватает координат окна"; return false; }
    } else {
        while (pos < tok.size()) {
            std::string key = tok[pos++];
            double cnt = 0;
            if (key == "segments") {
                if (!num(cnt) || cnt < 0 || !readSegments((int)cnt)) { error = "ошибка в блоке segments"; return false; }
            } else if (key == "window") {
                if (!readWindow()) { error = "ошибка в блоке window"; return false; }
            } else if (key == "clipper") {
                if (!num(cnt) || cnt < 0 || !readPoints((int)cnt, sc.clipperPoly)) { error = "ошибка в блоке clipper"; return false; }
            } else if (key == "polygon") {
                Ring r;
                if (!num(cnt) || cnt < 0 || !readPoints((int)cnt, r)) { error = "ошибка в блоке polygon"; return false; }
                sc.polygons.push_back(r);
            } else {
                error = "неизвестное слово: " + key;
                return false;
            }
        }
    }
    out = sc;
    return true;
}

std::string serializeScene(const SceneData &s) {
    std::ostringstream o;
    o.precision(10);
    o << "segments " << s.segments.size() << "\n";
    for (const Segment &g : s.segments)
        o << g.a.x << " " << g.a.y << " " << g.b.x << " " << g.b.y << "\n";
    o << "window " << s.window.xmin << " " << s.window.ymin << " " << s.window.xmax << " "
      << s.window.ymax << "\n";
    if (!s.clipperPoly.empty()) {
        o << "clipper " << s.clipperPoly.size() << "\n";
        for (const Point &p : s.clipperPoly) o << p.x << " " << p.y << "\n";
    }
    for (const Ring &r : s.polygons) {
        o << "polygon " << r.size() << "\n";
        for (const Point &p : r) o << p.x << " " << p.y << "\n";
    }
    return o.str();
}

std::vector<Segment> randomSegments(int n, const Rect &area, double maxLen, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> ux(area.xmin, area.xmax), uy(area.ymin, area.ymax);
    std::uniform_real_distribution<double> ua(0, 2 * M_PI), ul(0, 1);
    std::vector<Segment> v;
    v.reserve(n);
    for (int i = 0; i < n; ++i) {
        double x = ux(rng), y = uy(rng), a = ua(rng), l = maxLen * ul(rng);
        v.push_back(Segment(x, y, x + l * std::cos(a), y + l * std::sin(a)));
    }
    return v;
}

}  // namespace geo
