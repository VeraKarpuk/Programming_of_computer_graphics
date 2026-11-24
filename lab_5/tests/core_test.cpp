#include <chrono>
#include <cstdio>
#include <random>

#include "../src/polygonclip.h"
#include "../src/quadtree.h"
#include "../src/sceneio.h"
#include "../src/segmentclip.h"
#include "../src/testcases.h"

using namespace geo;

int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { ++fails; printf("FAIL: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main() {
    Rect w = testWindow();
    ConvexClipper cb = ConvexClipper::fromRect(w);

    for (const SegTest &t : segmentTests()) {
        Segment a, b, c;
        bool v1 = clipLiangBarsky(t.seg, w, a);
        bool v2 = clipSutherlandCohen(t.seg, w, b);
        bool v3 = cb.clip(t.seg, c);
        CHECK(v1 == t.visible, "LB visible %s (%d)", t.name.c_str(), v1);
        CHECK(v2 == t.visible, "SC visible %s (%d)", t.name.c_str(), v2);
        CHECK(v3 == t.visible, "CB visible %s (%d)", t.name.c_str(), v3);
        if (t.visible) {
            if (v1) CHECK(sameSegment(a, t.expected), "LB geom %s: %g %g %g %g", t.name.c_str(), a.a.x, a.a.y, a.b.x, a.b.y);
            if (v2) CHECK(sameSegment(b, t.expected), "SC geom %s: %g %g %g %g", t.name.c_str(), b.a.x, b.a.y, b.b.x, b.b.y);
            if (v3) CHECK(sameSegment(c, t.expected), "CB geom %s: %g %g %g %g", t.name.c_str(), c.a.x, c.a.y, c.b.x, c.b.y);
        }
    }
    printf("segment tests done, fails=%d\n", fails);

    // случайные отрезки: все три алгоритма должны согласоваться
    {
        std::mt19937 rng(7);
        std::uniform_real_distribution<double> u(-15, 25);
        OpStats sLB, sSC, sCB;
        int diff = 0, vis = 0;
        for (int i = 0; i < 200000; ++i) {
            Segment s(u(rng), u(rng), u(rng), u(rng));
            // часть отрезков делаем вертикальными/горизонтальными
            if (i % 7 == 0) s.b.x = s.a.x;
            if (i % 11 == 0) s.b.y = s.a.y;
            Segment a, b, c;
            bool v1 = clipLiangBarsky(s, w, a, &sLB);
            bool v2 = clipSutherlandCohen(s, w, b, &sSC);
            bool v3 = cb.clip(s, c, &sCB);
            vis += v1;
            if (v1 != v2 || v1 != v3 || (v1 && (!sameSegment(a, b) || !sameSegment(a, c)))) {
                if (++diff <= 5) printf("diff: %g %g %g %g  %d %d %d\n", s.a.x, s.a.y, s.b.x, s.b.y, v1, v2, v3);
            }
        }
        printf("random: visible=%d diff=%d\n LB mul=%lld div=%lld\n SC mul=%lld div=%lld\n CB mul=%lld div=%lld\n",
               vis, diff, sLB.mul, sLB.div, sSC.mul, sSC.div, sCB.mul, sCB.div);
        CHECK(diff == 0, "random disagreement");
    }

    // выпуклый многоугольник: сравнение с обрезкой по параметрам напрямую
    {
        ConvexClipper hex;
        Ring h;
        for (int i = 0; i < 6; ++i) h.push_back(Point(5 + 4 * std::cos(i * M_PI / 3), 5 + 4 * std::sin(i * M_PI / 3)));
        CHECK(hex.set(h), "hex convex");
        Segment o;
        CHECK(hex.clip(Segment(-5, 5, 15, 5), o), "hex crossing");
        CHECK(std::fabs(o.a.x - 1) < 1e-6 && std::fabs(o.b.x - 9) < 1e-6, "hex x %g %g", o.a.x, o.b.x);
        Ring star{Point(5, 0), Point(8, 10), Point(0, 4), Point(10, 4), Point(2, 10)};
        CHECK(!ConvexClipper().set(star), "star not convex");
        Ring concave{Point(0, 0), Point(10, 0), Point(10, 4), Point(4, 4), Point(4, 10), Point(0, 10)};
        CHECK(!ConvexClipper().set(concave), "L not convex");
    }

    for (const PolyTest &t : polygonTests()) {
        WAResult r = weilerAtherton(t.subject, t.clipper);
        int bad = polygonMismatches(t.subject, t.clipper, r.contours);
        printf("poly %-45s contours=%zu (want %d) mismatches=%d entries=%zu exits=%zu\n", t.name.c_str(),
               r.contours.size(), t.contours, bad, r.entryPoints.size(), r.exitPoints.size());
        CHECK(bad == 0, "poly mismatch %s", t.name.c_str());
        CHECK((int)r.contours.size() == t.contours, "poly contours %s", t.name.c_str());
    }

    // случайные выпуклые отсекатели и случайные многоугольники
    {
        std::mt19937 rng(11);
        std::uniform_real_distribution<double> u(-2, 12);
        int bad = 0, runs = 0;
        for (int it = 0; it < 300; ++it) {
            Ring sub;
            int n = 3 + rng() % 8;
            for (int i = 0; i < n; ++i) sub.push_back(Point(u(rng), u(rng)));  // возможны самопересечения
            Ring cl{Point(0, 0), Point(10, 0), Point(10, 10), Point(0, 10)};
            WAResult r = weilerAtherton({sub}, cl);
            int m = polygonMismatches({sub}, cl, r.contours);
            ++runs;
            if (m > 3) { ++bad; if (bad < 4) printf("random poly mismatch %d at iter %d\n", m, it); }
        }
        printf("random polygons: %d/%d bad\n", bad, runs);
        CHECK(bad == 0, "random polygons");
    }

    // квадродерево против перебора
    {
        Rect area(0, 0, 1000, 1000);
        auto data = randomSegments(200000, area, 15, 5);
        for (int depth : {3, 6, 9}) {
            Quadtree qt;
            auto t0 = std::chrono::steady_clock::now();
            qt.build(&data, area, depth, 32);
            double bt = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
            std::mt19937 rng(3);
            std::uniform_real_distribution<double> u(0, 900);
            double tq = 0, tb = 0;
            int mism = 0;
            for (int k = 0; k < 50; ++k) {
                double x = u(rng), y = u(rng);
                Rect q(x, y, x + 100, y + 80);
                std::vector<Segment> v1, v2;
                QueryStats s1, s2;
                auto a0 = std::chrono::steady_clock::now();
                qt.clipWindow(q, v1, nullptr, s1);
                auto a1 = std::chrono::steady_clock::now();
                clipBruteForce(data, q, v2, s2);
                auto a2 = std::chrono::steady_clock::now();
                tq += std::chrono::duration<double>(a1 - a0).count();
                tb += std::chrono::duration<double>(a2 - a1).count();
                if (v1.size() != v2.size()) ++mism;
            }
            printf("depth %d: build %.2fs nodes=%d reached=%d  query %.3f ms vs brute %.3f ms mism=%d\n", depth, bt,
                   qt.nodeCount(), qt.maxDepthReached(), tq / 50 * 1e3, tb / 50 * 1e3, mism);
            CHECK(mism == 0, "quadtree mismatch depth %d", depth);
        }
        // динамическое обновление
        Quadtree qt;
        qt.build(&data, area, 8, 32);
        std::mt19937 rng(9);
        auto fresh = randomSegments(5000, area, 15, 99);
        for (int i = 0; i < 5000; ++i) {
            int id = rng() % data.size();
            Segment old = data[id];
            data[id] = fresh[i];
            qt.update(id, old, fresh[i]);
        }
        Rect q(300, 300, 450, 400);
        std::vector<Segment> v1, v2;
        QueryStats s1, s2;
        qt.clipWindow(q, v1, nullptr, s1);
        clipBruteForce(data, q, v2, s2);
        CHECK(v1.size() == v2.size(), "after updates %zu vs %zu", v1.size(), v2.size());
    }

    // разбор файла
    {
        SceneData sc;
        std::string err;
        CHECK(parseScene("2 *число отрезков*\n0 0 1 1\n2 2 3 3\n-1 -1 5 5 # окно\n", sc, err), "parse classic %s", err.c_str());
        CHECK(sc.segments.size() == 2 && sc.window.xmax == 5, "classic content");
        SceneData sc2;
        CHECK(parseScene(serializeScene(sc), sc2, err), "parse own");
        CHECK(sc2.segments.size() == 2, "roundtrip");
        CHECK(!parseScene("3\n1 2 3\n", sc, err), "bad file rejected");
    }

    printf("TOTAL FAILS: %d\n", fails);
    return fails != 0;
}
