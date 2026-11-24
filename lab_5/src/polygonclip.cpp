#include "polygonclip.h"

#include <map>

namespace geo {

namespace {

const double PROBE = 1e-4;  // на сколько сдвигаем пробную точку от границы

Ring cleanRing(const Ring &r) {
    Ring out;
    for (const Point &p : r)
        if (out.empty() || !samePoint(out.back(), p)) out.push_back(p);
    while (out.size() > 1 && samePoint(out.front(), out.back())) out.pop_back();
    return out;
}

double paramOnEdge(const Point &c, const Point &a, const Point &b) {
    Point d = b - a;
    return dot(c - a, d) / dot(d, d);
}

// точки пересечения ab и pq; для коллинеарных рёбер - концы общего участка
void edgeIntersections(const Point &a, const Point &b, const Point &p, const Point &q,
                       std::vector<Point> &out) {
    Point d1 = b - a, d2 = q - p;
    double len1 = length(d1), len2 = length(d2);
    if (len1 <= EPS || len2 <= EPS) return;

    double cr = cross(d1, d2);
    if (std::fabs(cr) / (len1 * len2) > 1e-9) {
        Point ap = p - a;
        double t = cross(ap, d2) / cr;
        double u = cross(ap, d1) / cr;
        double et = EPS / len1, eu = EPS / len2;
        if (t >= -et && t <= 1 + et && u >= -eu && u <= 1 + eu) {
            t = std::max(0.0, std::min(1.0, t));
            out.push_back(a + d1 * t);
        }
        return;
    }
    // параллельны: смотрим, лежат ли на одной прямой
    if (distToSegment(a, p, q) > EPS && distToSegment(b, p, q) > EPS &&
        distToSegment(p, a, b) > EPS && distToSegment(q, a, b) > EPS)
        return;
    const Point cand[4] = {a, b, p, q};
    for (const Point &c : cand)
        if (distToSegment(c, a, b) <= EPS && distToSegment(c, p, q) <= EPS) out.push_back(c);
}

struct Node {
    Point p;
    bool inter = false;
    int id = -1;
};

// вставляет точки пересечения в контур; marks - вершины, которые сами оказались пересечением
std::vector<Node> buildList(const Ring &ring, std::vector<std::vector<Point>> &cuts,
                            const std::vector<char> &marks) {
    std::vector<Node> list;
    size_t n = ring.size();
    for (size_t i = 0; i < n; ++i) {
        Node v;
        v.p = ring[i];
        v.inter = marks[i];
        list.push_back(v);

        const Point &a = ring[i], &b = ring[(i + 1) % n];
        std::vector<std::pair<double, Point>> ts;
        for (const Point &c : cuts[i]) ts.push_back({paramOnEdge(c, a, b), c});
        std::sort(ts.begin(), ts.end(),
                  [](const std::pair<double, Point> &l, const std::pair<double, Point> &r) {
                      return l.first < r.first;
                  });
        for (const auto &tp : ts) {
            if (samePoint(tp.second, list.back().p)) continue;
            Node c;
            c.p = tp.second;
            c.inter = true;
            list.push_back(c);
        }
    }
    // последняя вставка могла совпасть с первой вершиной
    while (list.size() > 1 && samePoint(list.front().p, list.back().p)) {
        if (list.back().inter) list.front().inter = true;
        list.pop_back();
    }
    return list;
}

struct PieceEdge {
    int a, b;
    int type;  // 0 - от subject, 1 - от clipper
};

}  // namespace

int classifyEvenOdd(const std::vector<Ring> &rings, const Point &p, int *ringIdx, int *edgeIdx) {
    for (size_t r = 0; r < rings.size(); ++r) {
        size_t n = rings[r].size();
        for (size_t i = 0; i < n; ++i) {
            if (distToSegment(p, rings[r][i], rings[r][(i + 1) % n]) <= EPS) {
                if (ringIdx) *ringIdx = (int)r;
                if (edgeIdx) *edgeIdx = (int)i;
                return 0;
            }
        }
    }
    bool inside = false;
    for (const Ring &ring : rings) {
        size_t n = ring.size();
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            const Point &a = ring[i], &b = ring[j];
            if ((a.y > p.y) != (b.y > p.y)) {
                double x = (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x;
                if (p.x < x) inside = !inside;
            }
        }
    }
    return inside ? 1 : -1;
}

WAResult weilerAtherton(const std::vector<Ring> &subjectIn, const Ring &clipperIn) {
    WAResult res;

    std::vector<Ring> subject;
    for (const Ring &r : subjectIn) {
        Ring c = cleanRing(r);
        if (c.size() >= 3) subject.push_back(c);
    }
    Ring clipper = cleanRing(clipperIn);
    if (subject.empty() || clipper.size() < 3) return res;
    if (signedArea(clipper) < 0) std::reverse(clipper.begin(), clipper.end());
    std::vector<Ring> clipperRings(1, clipper);

    // 1. пересечения рёбер subject и clipper
    std::vector<std::vector<std::vector<Point>>> cutsS(subject.size());
    std::vector<std::vector<char>> marksS(subject.size());
    for (size_t r = 0; r < subject.size(); ++r) {
        cutsS[r].resize(subject[r].size());
        marksS[r].assign(subject[r].size(), 0);
    }
    std::vector<std::vector<Point>> cutsC(clipper.size());
    std::vector<char> marksC(clipper.size(), 0);

    auto addCut = [](const Ring &ring, std::vector<std::vector<Point>> &cuts,
                     std::vector<char> &marks, size_t edge, const Point &c) {
        size_t n = ring.size();
        if (samePoint(c, ring[edge])) { marks[edge] = 1; return; }
        if (samePoint(c, ring[(edge + 1) % n])) { marks[(edge + 1) % n] = 1; return; }
        for (const Point &o : cuts[edge])
            if (samePoint(o, c)) return;
        cuts[edge].push_back(c);
    };

    std::vector<Point> found;
    for (size_t r = 0; r < subject.size(); ++r) {
        size_t n = subject[r].size();
        for (size_t i = 0; i < n; ++i) {
            const Point &a = subject[r][i], &b = subject[r][(i + 1) % n];
            for (size_t j = 0; j < clipper.size(); ++j) {
                found.clear();
                edgeIntersections(a, b, clipper[j], clipper[(j + 1) % clipper.size()], found);
                for (const Point &c : found) {
                    addCut(subject[r], cutsS[r], marksS[r], i, c);
                    addCut(clipper, cutsC, marksC, j, c);
                }
            }
        }
    }

    // 2. списки вершин с вставленными пересечениями
    std::vector<std::vector<Node>> listS;
    for (size_t r = 0; r < subject.size(); ++r)
        listS.push_back(buildList(subject[r], cutsS[r], marksS[r]));
    std::vector<Node> listC = buildList(clipper, cutsC, marksC);

    // 3. общие номера точек: пересечения в обоих списках получают один id
    std::vector<Point> idPoints;
    std::vector<int> interIds;
    auto assignId = [&](Node &nd) {
        if (nd.inter) {
            for (int id : interIds)
                if (samePoint(idPoints[id], nd.p)) { nd.id = id; return; }
            idPoints.push_back(nd.p);
            nd.id = (int)idPoints.size() - 1;
            interIds.push_back(nd.id);
        } else {
            idPoints.push_back(nd.p);
            nd.id = (int)idPoints.size() - 1;
        }
    };
    for (auto &lst : listS) for (Node &nd : lst) assignId(nd);
    for (Node &nd : listC) assignId(nd);
    res.intersections = (int)interIds.size();

    // 4. какие участки границы попадают в результат
    std::vector<PieceEdge> edges;

    // участок subject внутри отсекателя; если он лежит на границе отсекателя,
    // оставляем его только когда область subject примыкает к отсекателю изнутри
    auto subjectKept = [&](const Point &a, const Point &b) {
        Point mid((a.x + b.x) / 2, (a.y + b.y) / 2);
        int ri = 0, ei = 0;
        int c = classifyEvenOdd(clipperRings, mid, &ri, &ei);
        if (c != 0) return c == 1;
        const Point &p = clipper[ei], &q = clipper[(ei + 1) % clipper.size()];
        Point d = q - p;
        double len = length(d);
        Point inward(-d.y / len, d.x / len);
        Point probe = mid + inward * PROBE;
        return classifyEvenOdd(subject, probe) == 1;
    };

    std::vector<std::vector<char>> keptS(listS.size());
    for (size_t r = 0; r < listS.size(); ++r) {
        size_t n = listS[r].size();
        keptS[r].assign(n, 0);
        for (size_t i = 0; i < n; ++i) {
            const Node &a = listS[r][i], &b = listS[r][(i + 1) % n];
            if (a.id == b.id) continue;
            if (subjectKept(a.p, b.p)) {
                keptS[r][i] = 1;
                edges.push_back({a.id, b.id, 0});
            }
        }
    }
    // участок отсекателя берём, если он строго внутри области subject
    for (size_t i = 0; i < listC.size(); ++i) {
        const Node &a = listC[i], &b = listC[(i + 1) % listC.size()];
        if (a.id == b.id) continue;
        Point mid((a.p.x + b.p.x) / 2, (a.p.y + b.p.y) / 2);
        if (classifyEvenOdd(subject, mid) == 1) edges.push_back({a.id, b.id, 1});
    }

    // точки входа/выхода для показа на экране
    for (size_t r = 0; r < listS.size(); ++r) {
        size_t n = listS[r].size();
        for (size_t i = 0; i < n; ++i) {
            if (!listS[r][i].inter) continue;
            bool prevIn = keptS[r][(i + n - 1) % n], nextIn = keptS[r][i];
            if (!prevIn && nextIn) res.entryPoints.push_back(listS[r][i].p);
            else if (prevIn && !nextIn) res.exitPoints.push_back(listS[r][i].p);
        }
    }

    // 5. сборка контуров: идём по subject, на границе переходим на clipper и обратно
    std::vector<std::vector<int>> adj(idPoints.size());
    for (size_t e = 0; e < edges.size(); ++e) {
        adj[edges[e].a].push_back((int)e);
        adj[edges[e].b].push_back((int)e);
    }
    std::vector<char> used(edges.size(), 0);
    for (size_t e0 = 0; e0 < edges.size(); ++e0) {
        if (used[e0]) continue;
        used[e0] = 1;
        Ring loop;
        int start = edges[e0].a, cur = edges[e0].b, lastType = edges[e0].type;
        loop.push_back(idPoints[start]);
        bool closed = false;
        while (true) {
            if (cur == start) { closed = true; break; }
            loop.push_back(idPoints[cur]);
            int pick = -1;
            for (int e : adj[cur]) {
                if (used[e]) continue;
                if (pick < 0) pick = e;
                if (edges[e].type != lastType) { pick = e; break; }
            }
            if (pick < 0) break;
            used[pick] = 1;
            cur = (edges[pick].a == cur) ? edges[pick].b : edges[pick].a;
            lastType = edges[pick].type;
        }
        if (closed && loop.size() >= 3) res.contours.push_back(loop);
    }
    return res;
}

double evenOddArea(const std::vector<Ring> &contours) {
    double total = 0;
    for (size_t i = 0; i < contours.size(); ++i) {
        // пробная точка внутри контура: середина первого ребра, сдвинутая внутрь
        const Ring &c = contours[i];
        Point m((c[0].x + c[1].x) / 2, (c[0].y + c[1].y) / 2);
        Point d = c[1] - c[0];
        double len = length(d);
        double side = signedArea(c) > 0 ? 1 : -1;
        Point probe = m + Point(-d.y / len, d.x / len) * (side * 1e-4);
        int depth = 0;
        for (size_t j = 0; j < contours.size(); ++j) {
            if (j == i) continue;
            if (classifyEvenOdd({contours[j]}, probe) == 1) ++depth;
        }
        total += (depth % 2 == 0 ? 1 : -1) * std::fabs(signedArea(c));
    }
    return total;
}

}  // namespace geo
