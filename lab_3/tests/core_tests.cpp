// Автономные тесты ядра (без Qt):  g++ -std=c++11 -O2 -pthread -Isrc tests/core_tests.cpp src/core/*.cpp -o core_tests
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include "core/Algorithms.h"
#include "core/TestImages.h"

using namespace ip;

static int failures = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("  FAIL: %s\n", msg); ++failures; } else std::printf("  ok:   %s\n", msg); } while (0)

static double nowMs() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
static double accuracy(const Image& bin, const Image& truth, bool* inverted = nullptr) {
    size_t same = 0; const size_t n = bin.data.size();
    for (size_t i = 0; i < n; ++i) if ((bin.data[i] > 127) == (truth.data[i] > 127)) ++same;
    double a = double(same) / n;
    if (inverted) *inverted = a < 0.5;
    return a < 0.5 ? 1.0 - a : a;
}
static void writePgm(const Image& im, const char* path) {
    std::ofstream f(path, std::ios::binary);
    f << (im.channels == 1 ? "P5" : "P6") << "\n" << im.width << " " << im.height << "\n255\n";
    f.write(reinterpret_cast<const char*>(im.data.data()), im.data.size());
}

int main(int argc, char** argv) {
    std::printf("== Края: Clamp / Reflect\n");
    CHECK(mapIndex(-1, 5, EdgeMode::Clamp) == 0 && mapIndex(7, 5, EdgeMode::Clamp) == 4, "Clamp: -1->0, 7->4");
    CHECK(mapIndex(-1, 5, EdgeMode::Reflect) == 0 && mapIndex(-2, 5, EdgeMode::Reflect) == 1, "Reflect: -1->0, -2->1");
    CHECK(mapIndex(5, 5, EdgeMode::Reflect) == 4 && mapIndex(6, 5, EdgeMode::Reflect) == 3, "Reflect: 5->4, 6->3");
    CHECK(mapIndex(-23, 5, EdgeMode::Reflect) >= 0 && mapIndex(23, 5, EdgeMode::Reflect) < 5, "Reflect: большой радиус в пределах");

    std::printf("== Разделяемая свёртка == полная 2D свёртка\n");
    {
        Image im = generateTestImage(1, 123, 97, nullptr);
        PlaneF f = extractChannel(im, 0);
        std::vector<float> k = gaussianKernel(2.0);
        EdgeMode modes[2] = { EdgeMode::Clamp, EdgeMode::Reflect };
        for (int m = 0; m < 2; ++m) {
            PlaneF a = separableConvolve(f, k, modes[m]), b = convolve2DFull(f, k, k, modes[m]);
            double mx = 0;
            for (size_t i = 0; i < a.data.size(); ++i) mx = std::max(mx, (double)std::fabs(a.data[i] - b.data[i]));
            std::printf("  max|sep - full| = %.2e (%s)\n", mx, m ? "Reflect" : "Clamp");
            CHECK(mx < 1e-3, "совпадение с эталоном до 1e-3");
        }
        // постоянное изображение остаётся постоянным (края обработаны корректно, нет чёрной рамки)
        PlaneF c(40, 30); std::fill(c.data.begin(), c.data.end(), 100.f);
        PlaneF r = separableConvolve(c, k, EdgeMode::Reflect);
        double mx = 0; for (size_t i = 0; i < r.data.size(); ++i) mx = std::max(mx, (double)std::fabs(r.data[i] - 100.f));
        CHECK(mx < 1e-3, "константа -> константа (включая края)");
    }

    std::printf("== Лапласиан: константа даёт 0, ступенька усиливается\n");
    {
        Image flat(32, 32, 1); std::fill(flat.data.begin(), flat.data.end(), 90);
        SharpenParams sp; sp.method = SharpenParams::Laplacian8; sp.strength = 1.0;
        Image o = sharpen(flat, sp);
        bool same = std::equal(o.data.begin(), o.data.end(), flat.data.begin());
        CHECK(same, "плоское изображение не меняется");
        Image step(32, 8, 1);
        for (int y = 0; y < 8; ++y) for (int x = 0; x < 32; ++x) step.row(y)[x] = x < 16 ? 80 : 160;
        sp.method = SharpenParams::Laplacian4;
        Image s2 = sharpen(step, sp);
        CHECK(s2.row(3)[15] < 80 && s2.row(3)[16] > 160, "перерегулирование на краю ступеньки (резкость выросла)");
    }

    std::printf("== Пороги Оцу / итерационный\n");
    {
        std::vector<int> h(256, 0); h[50] = 5000; h[200] = 5000;
        int to = otsuThreshold(h), ti = iterativeThreshold(h);
        std::printf("  идеальная бимодальная: Otsu=%d iter=%d\n", to, ti);
        CHECK(to >= 50 && to < 200 && ti >= 50 && ti < 200, "порог между модами");
    }

    std::printf("== Интегральное изображение == наивный перебор окна\n");
    {
        Image im = generateTestImage(7, 160, 120, nullptr);
        AdaptiveParams ap; ap.window = 17;
        for (int m = 0; m < 2; ++m) {
            ap.method = m ? AdaptiveParams::Sauvola : AdaptiveParams::LocalMean;
            ap.useIntegral = true;  Image a = adaptiveThreshold(im, ap);
            ap.useIntegral = false; Image b = adaptiveThreshold(im, ap);
            size_t diff = 0; for (size_t i = 0; i < a.data.size(); ++i) diff += a.data[i] != b.data[i];
            std::printf("  %s: различающихся пикселей: %zu\n", m ? "Sauvola" : "LocalMean", diff);
            CHECK(diff == 0, "результаты идентичны");
        }
    }

    std::printf("== Сравнение методов на тестовой базе (точность по эталону)\n");
    const int W = 800, H = 600;
    for (int idx = 4; idx <= 9; ++idx) {
        if (idx == 8) continue;
        Image truth; Image im = generateTestImage(idx, W, H, &truth);
        int tO = 0, tI = 0, tT = 0;
        GlobalThresholdParams g; g.method = GlobalThresholdParams::Otsu;      Image o = globalThreshold(im, g, &tO);
        g.method = GlobalThresholdParams::Iterative;                           Image it = globalThreshold(im, g, &tI);
        g.method = GlobalThresholdParams::Triangle; Image tri = globalThreshold(im, g, &tT);
        AdaptiveParams ap; ap.window = 25; ap.C = 10; Image ad = adaptiveThreshold(im, ap);
        bool inv = false;
        double aa = accuracy(ad, truth, &inv);
        if (idx == 6 || idx == 7) { ap.invert = true; ad = adaptiveThreshold(im, ap); aa = accuracy(ad, truth); }
        // истина: объекты = 255; у документа объекты тёмные -> инвертируем сравнение
        double ao, ai, at = accuracy(tri, truth);
        if (idx == 6 || idx == 7) {
            g.invert = true; g.method = GlobalThresholdParams::Otsu; ao = accuracy(globalThreshold(im, g), truth);
            g.method = GlobalThresholdParams::Iterative;             ai = accuracy(globalThreshold(im, g), truth);
            g.method = GlobalThresholdParams::Triangle;              at = accuracy(globalThreshold(im, g), truth);
        } else { ao = accuracy(o, truth); ai = accuracy(it, truth); }
        std::printf("  #%d %-22s Otsu T=%3d acc=%.3f | Iter T=%3d acc=%.3f | Triangle T=%3d acc=%.3f | Adaptive(25,C=10) acc=%.3f\n",
                    idx, testImageCatalog()[idx].id, tO, ao, tI, ai, tT, at, aa);
    }

    std::printf("== Скорость (800x600), мс\n");
    {
        Image im = generateTestImage(7, 1600, 1200, nullptr);
        AdaptiveParams ap; ap.window = 51;
        double t0 = nowMs(); adaptiveThreshold(im, ap); double tSat = nowMs() - t0;
        ap.useIntegral = false;
        Image small = generateTestImage(7, 400, 300, nullptr);
        t0 = nowMs(); adaptiveThreshold(small, ap); double tNaive = (nowMs() - t0) * 16.0;  // экстраполяция на 1600x1200
        std::printf("  Adaptive 1600x1200, окно 51: интегральное = %.1f мс; наивное ≈ %.0f мс (экстраполяция), ускорение ≈ %.0fx\n",
                    tSat, tNaive, tNaive / tSat);
        PlaneF f = extractChannel(generateTestImage(0, 1600, 1200, nullptr), 0);
        std::vector<float> k = gaussianKernel(3.0);
        t0 = nowMs(); separableConvolve(f, k, EdgeMode::Reflect); double ts = nowMs() - t0;
        Image sm = generateTestImage(0, 400, 300, nullptr);
        PlaneF fs = extractChannel(sm, 0);
        t0 = nowMs(); convolve2DFull(fs, k, k, EdgeMode::Reflect); double tf = (nowMs() - t0) * 16.0;
        std::printf("  Гаусс σ=3 (K=%d) 1600x1200: разделяемая = %.1f мс; полная K×K ≈ %.0f мс, ускорение ≈ %.1fx\n",
                    (int)k.size(), ts, tf, tf / ts);
        setThreadCount(1);
        SharpenParams sp; sp.method = SharpenParams::Unsharp; sp.sigma = 2;
        Image big = generateTestImage(1, 3000, 2000, nullptr);
        t0 = nowMs(); sharpen(big, sp); double t1 = nowMs() - t0;
        setThreadCount(0);
        t0 = nowMs(); sharpen(big, sp); double tn = nowMs() - t0;
        std::printf("  Unsharp 3000x2000: 1 поток = %.0f мс, авто (%d потоков) = %.0f мс\n", t1, effectiveThreads(), tn);
    }

    if (argc > 1) {
        std::printf("== Дамп результатов в %s\n", argv[1]);
        std::string d = argv[1];
        Image im = generateTestImage(1, 800, 600, nullptr); writePgm(im, (d + "/in_blur_noisy.pgm").c_str());
        SharpenParams sp; sp.method = SharpenParams::Unsharp; sp.strength = 1.5; sp.sigma = 2; sp.threshold = 6;
        writePgm(sharpen(im, sp), (d + "/out_unsharp.pgm").c_str());
        sp.method = SharpenParams::Laplacian8; sp.strength = 0.5; writePgm(sharpen(im, sp), (d + "/out_lap8.pgm").c_str());
        Image doc = generateTestImage(7, 800, 600, nullptr); writePgm(doc, (d + "/in_doc.pgm").c_str());
        GlobalThresholdParams g; writePgm(globalThreshold(doc, g), (d + "/out_doc_otsu.pgm").c_str());
        AdaptiveParams ap; ap.window = 31; ap.C = 12; writePgm(adaptiveThreshold(doc, ap), (d + "/out_doc_adaptive.pgm").c_str());
    }
    std::printf("\n%s (%d провал(ов))\n", failures ? "ТЕСТЫ НЕ ПРОЙДЕНЫ" : "ВСЕ ТЕСТЫ ПРОЙДЕНЫ", failures);
    return failures ? 1 : 0;
}
