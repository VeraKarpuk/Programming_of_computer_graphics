#include "Algorithms.h"

namespace ip {

// ============================================================================
//  Ядра и свёртки
// ============================================================================
std::vector<float> gaussianKernel(double sigma) {
    if (sigma < 0.1) sigma = 0.1;
    int r = static_cast<int>(std::ceil(3.0 * sigma));
    if (r < 1) r = 1;
    std::vector<float> k(static_cast<size_t>(2 * r + 1));
    double sum = 0;
    for (int i = -r; i <= r; ++i) {
        const double v = std::exp(-(static_cast<double>(i) * i) / (2.0 * sigma * sigma));
        k[static_cast<size_t>(i + r)] = static_cast<float>(v);
        sum += v;
    }
    for (size_t i = 0; i < k.size(); ++i) k[i] = static_cast<float>(k[i] / sum);
    return k;
}

std::vector<float> boxKernel(int radius) {
    if (radius < 1) radius = 1;
    const size_t n = static_cast<size_t>(2 * radius + 1);
    return std::vector<float>(n, 1.0f / static_cast<float>(n));
}

PlaneF convolve3x3(const PlaneF& src, const float k[9], EdgeMode edge, Progress* prog) {
    const int w = src.width, h = src.height;
    PlaneF dst(w, h);
    const std::vector<int> xm = buildIndexMap(w, 1, edge), ym = buildIndexMap(h, 1, edge);
    parallelFor(0, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* r0 = src.row(ym[y]);      // строка y-1
            const float* r1 = src.row(ym[y + 1]);  // строка y
            const float* r2 = src.row(ym[y + 2]);  // строка y+1
            float* o = dst.row(y);
            for (int x = 0; x < w; ++x) {
                const int a = xm[x], b = xm[x + 1], c = xm[x + 2];  // x-1, x, x+1
                o[x] = k[0] * r0[a] + k[1] * r0[b] + k[2] * r0[c] +
                       k[3] * r1[a] + k[4] * r1[b] + k[5] * r1[c] +
                       k[6] * r2[a] + k[7] * r2[b] + k[8] * r2[c];
            }
            stepProgress(prog);
        }
    });
    return dst;
}

PlaneF separableConvolve(const PlaneF& src, const std::vector<float>& k, EdgeMode edge, Progress* prog) {
    const int w = src.width, h = src.height;
    const int K = static_cast<int>(k.size()), r = K / 2;
    PlaneF tmp(w, h), dst(w, h);
    const std::vector<int> xm = buildIndexMap(w, r, edge), ym = buildIndexMap(h, r, edge);

    // Проход 1: по строкам.
    parallelFor(0, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* s = src.row(y);
            float* t = tmp.row(y);
            for (int x = 0; x < w; ++x) {
                float acc = 0.f;
                if (x >= r && x < w - r) {            // внутренняя часть: без обращения к таблице краёв
                    const float* p = s + (x - r);
                    for (int i = 0; i < K; ++i) acc += k[i] * p[i];
                } else {                              // у края: индексы через таблицу Clamp/Reflect
                    for (int i = 0; i < K; ++i) acc += k[i] * s[xm[x + i]];
                }
                t[x] = acc;
            }
            stepProgress(prog);
        }
    });
    // Проход 2: по столбцам (строка за строкой — кеш-дружелюбно).
    parallelFor(0, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            float* d = dst.row(y);
            for (int x = 0; x < w; ++x) d[x] = 0.f;
            for (int i = 0; i < K; ++i) {
                const float* s = tmp.row(ym[y + i]);  // строка y + i - r
                const float ki = k[i];
                for (int x = 0; x < w; ++x) d[x] += ki * s[x];
            }
            stepProgress(prog);
        }
    });
    return dst;
}

PlaneF convolve2DFull(const PlaneF& src, const std::vector<float>& kRow, const std::vector<float>& kCol,
                      EdgeMode edge) {
    const int w = src.width, h = src.height;
    const int KX = static_cast<int>(kRow.size()), KY = static_cast<int>(kCol.size());
    const int rx = KX / 2, ry = KY / 2;
    PlaneF dst(w, h);
    const std::vector<int> xm = buildIndexMap(w, rx, edge), ym = buildIndexMap(h, ry, edge);
    parallelFor(0, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y)
            for (int x = 0; x < w; ++x) {
                float acc = 0.f;
                for (int j = 0; j < KY; ++j) {
                    const float* s = src.row(ym[y + j]);
                    for (int i = 0; i < KX; ++i) acc += kCol[j] * kRow[i] * s[xm[x + i]];
                }
                dst.row(y)[x] = acc;
            }
    });
    return dst;
}

// ============================================================================
//  Повышение резкости
// ============================================================================
Image sharpen(const Image& src, const SharpenParams& p, Progress* prog) {
    const int w = src.width, h = src.height, C = src.channels;
    Image out(w, h, C);
    const bool blurBased = (p.method == SharpenParams::Unsharp || p.method == SharpenParams::HighBoost);
    if (prog) prog->reset(C * h * (blurBased ? 3 : 1));

    std::vector<float> kern;
    if (blurBased) kern = p.boxBlur ? boxKernel(p.boxRadius) : gaussianKernel(p.sigma);

    // ВЧ-ядра 3x3 (центр положителен): HP = f - среднее соседей, умноженное на число соседей.
    static const float hp4[9] = { 0, -1, 0, -1, 4, -1, 0, -1, 0 };
    static const float hp8[9] = { -1, -1, -1, -1, 8, -1, -1, -1, -1 };

    for (int c = 0; c < C; ++c) {
        const PlaneF f = extractChannel(src, c);
        // aux: для лапласианов — сама ВЧ-составляющая, для остальных — НЧ-составляющая (размытие).
        const PlaneF aux = blurBased
            ? separableConvolve(f, kern, p.edge, prog)
            : convolve3x3(f, p.method == SharpenParams::Laplacian4 ? hp4 : hp8, p.edge, prog);

        const float k = static_cast<float>(p.strength);
        const float A = static_cast<float>(p.boostA);
        const float thr = static_cast<float>(p.threshold);
        parallelFor(0, h, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y) {
                const float* fr = f.row(y);
                const float* ar = aux.row(y);
                uint8_t* o = out.row(y) + c;
                for (int x = 0; x < w; ++x) {
                    float v;
                    switch (p.method) {
                    case SharpenParams::Laplacian4:
                    case SharpenParams::Laplacian8: {
                        const float d = ar[x];
                        v = p.detailOnly ? 128.f + k * d : fr[x] + k * d;
                        break;
                    }
                    case SharpenParams::Unsharp: {
                        float d = fr[x] - ar[x];
                        if (std::fabs(d) < thr) d = 0.f;
                        v = p.detailOnly ? 128.f + k * d : fr[x] + k * d;
                        break;
                    }
                    default: {  // HighBoost: A*f - LP = (A-1)*f + HP
                        const float d = fr[x] - ar[x];
                        v = p.detailOnly ? 128.f + d : (A - 1.f) * fr[x] + d;
                        break;
                    }
                    }
                    o[x * C] = clamp8(v);
                }
                if (blurBased) stepProgress(prog);
            }
        });
    }
    return out;
}

// ============================================================================
//  Глобальные пороги
// ============================================================================
int otsuThreshold(const std::vector<int>& hist) {
    double total = 0, sumAll = 0;
    for (int i = 0; i < 256; ++i) { total += hist[i]; sumAll += static_cast<double>(i) * hist[i]; }
    double wB = 0, sumB = 0, best = -1;
    int t1 = 0, t2 = 0;
    for (int t = 0; t < 256; ++t) {
        wB += hist[t];                       // вес класса «фон» (≤ t)
        if (wB == 0) continue;
        const double wF = total - wB;        // вес класса «объект» (> t)
        if (wF == 0) break;
        sumB += static_cast<double>(t) * hist[t];
        const double mB = sumB / wB, mF = (sumAll - sumB) / wF;
        const double between = wB * wF * (mB - mF) * (mB - mF);  // межклассовая дисперсия (× N²)
        if (between > best) { best = between; t1 = t2 = t; }
        else if (between == best) t2 = t;    // плато максимума — берём середину
    }
    return (t1 + t2) / 2;
}

int iterativeThreshold(const std::vector<int>& hist) {
    double total = 0, sumAll = 0;
    for (int i = 0; i < 256; ++i) { total += hist[i]; sumAll += static_cast<double>(i) * hist[i]; }
    if (total == 0) return 128;
    double T = sumAll / total;
    for (int iter = 0; iter < 200; ++iter) {
        double s1 = 0, n1 = 0, s2 = 0, n2 = 0;
        for (int i = 0; i < 256; ++i) {
            if (i <= T) { s1 += static_cast<double>(i) * hist[i]; n1 += hist[i]; }
            else        { s2 += static_cast<double>(i) * hist[i]; n2 += hist[i]; }
        }
        if (n1 == 0 || n2 == 0) break;
        const double Tn = 0.5 * (s1 / n1 + s2 / n2);
        if (std::fabs(Tn - T) < 0.5) { T = Tn; break; }
        T = Tn;
    }
    return static_cast<int>(std::floor(T + 0.5));
}

int triangleThreshold(const std::vector<int>& hist) {
    // лёгкое сглаживание гистограммы (окно 5), чтобы шум не давал ложных пиков
    std::vector<double> s(256, 0.0);
    for (int i = 0; i < 256; ++i) {
        double acc = 0; int cnt = 0;
        for (int j = i - 2; j <= i + 2; ++j) if (j >= 0 && j < 256) { acc += hist[j]; ++cnt; }
        s[i] = acc / cnt;
    }
    int lo = 0, hi = 255, pk = 0;
    while (lo < 255 && hist[lo] == 0) ++lo;
    while (hi > 0 && hist[hi] == 0) --hi;
    if (lo >= hi) return lo;
    for (int i = lo; i <= hi; ++i) if (s[i] > s[pk]) pk = i;
    const int e = (pk - lo >= hi - pk) ? lo : hi;   // конец более длинного хвоста
    if (e == pk) return pk;
    // расстояние (с точностью до общего множителя) от точки (i, s[i]) до прямой пик—конец
    const double dx = static_cast<double>(pk - e), dy = s[pk] - s[e];
    double bestD = -1; int bestI = pk;
    const int a = std::min(e, pk), b = std::max(e, pk);
    for (int i = a; i <= b; ++i) {
        const double d = std::fabs(dy * (i - e) - dx * (s[i] - s[e]));
        if (d > bestD) { bestD = d; bestI = i; }
    }
    return bestI;
}

Image globalThreshold(const Image& src, const GlobalThresholdParams& p, int* usedThreshold, Progress* prog) {
    const Image g = toGray(src);
    const std::vector<int> hist = grayHistogram(g);
    int T = p.manual;
    if (p.method == GlobalThresholdParams::Otsu) T = otsuThreshold(hist);
    else if (p.method == GlobalThresholdParams::Iterative) T = iterativeThreshold(hist);
    else if (p.method == GlobalThresholdParams::Triangle) T = triangleThreshold(hist);
    if (usedThreshold) *usedThreshold = T;

    const int w = g.width, h = g.height;
    const uint8_t hi = p.invert ? 0 : 255, lo = p.invert ? 255 : 0;
    Image out(w, h, 1);
    if (prog) prog->reset(h);
    parallelFor(0, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const uint8_t* s = g.row(y);
            uint8_t* o = out.row(y);
            for (int x = 0; x < w; ++x) o[x] = (s[x] > T) ? hi : lo;
            stepProgress(prog);
        }
    });
    return out;
}

// ============================================================================
//  Адаптивный порог в скользящем окне (+ интегральное изображение)
// ============================================================================
Image adaptiveThreshold(const Image& src, const AdaptiveParams& p, Progress* prog) {
    const Image g = toGray(src);
    const int w = g.width, h = g.height;
    int win = p.window < 3 ? 3 : p.window;
    if (win % 2 == 0) ++win;
    const int r = win / 2;
    const double area = static_cast<double>(win) * win;
    const bool needSq = (p.method == AdaptiveParams::Sauvola);
    const uint8_t hi = p.invert ? 0 : 255, lo = p.invert ? 255 : 0;

    const std::vector<int> xm = buildIndexMap(w, r, p.edge), ym = buildIndexMap(h, r, p.edge);
    Image out(w, h, 1);

    auto decide = [&](double sum, double sum2, int v) -> uint8_t {
        const double mean = sum / area;
        double T;
        if (!needSq) {
            T = mean - p.C;                                   // метод из лекции: T = mean - C
        } else {
            double var = sum2 / area - mean * mean;
            if (var < 0) var = 0;
            T = mean * (1.0 + p.k * (std::sqrt(var) / p.R - 1.0));  // Савола
        }
        return v > T ? hi : lo;
    };

    if (!p.useIntegral) {
        // Наивный вариант: каждый раз суммируем все win² пикселей окна. O(win²) на пиксель.
        if (prog) prog->reset(h);
        parallelFor(0, h, [&](int y0, int y1) {
            for (int y = y0; y < y1; ++y) {
                uint8_t* o = out.row(y);
                for (int x = 0; x < w; ++x) {
                    long long s = 0, s2 = 0;
                    for (int dy = 0; dy < win; ++dy) {
                        const uint8_t* row = g.row(ym[y + dy]);
                        for (int dx = 0; dx < win; ++dx) {
                            const int v = row[xm[x + dx]];
                            s += v; s2 += v * v;
                        }
                    }
                    o[x] = decide(static_cast<double>(s), static_cast<double>(s2), g.row(y)[x]);
                }
                stepProgress(prog);
            }
        });
        return out;
    }

    // Интегральное изображение расширенного (с краями) кадра: S[y][x] = Σ I(i<y, j<x).
    const int pw = w + 2 * r, ph = h + 2 * r, W = pw + 1;
    std::vector<long long> S(static_cast<size_t>(ph + 1) * W, 0), S2;
    if (needSq) S2.assign(static_cast<size_t>(ph + 1) * W, 0);
    if (prog) prog->reset(ph + h);
    for (int y = 0; y < ph; ++y) {
        const uint8_t* s = g.row(ym[y]);
        long long rs = 0, rs2 = 0;
        long long* cur = &S[static_cast<size_t>(y + 1) * W];
        const long long* prev = &S[static_cast<size_t>(y) * W];
        for (int x = 0; x < pw; ++x) {
            const int v = s[xm[x]];
            rs += v;
            cur[x + 1] = prev[x + 1] + rs;
        }
        if (needSq) {
            long long* cur2 = &S2[static_cast<size_t>(y + 1) * W];
            const long long* prev2 = &S2[static_cast<size_t>(y) * W];
            for (int x = 0; x < pw; ++x) {
                const int v = s[xm[x]];
                rs2 += static_cast<long long>(v) * v;
                cur2[x + 1] = prev2[x + 1] + rs2;
            }
        }
        stepProgress(prog);
    }
    // Окно пикселя (x,y) в расширенных координатах: [x, x+win) × [y, y+win) — сумма за 4 обращения.
    parallelFor(0, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const long long* a = &S[static_cast<size_t>(y) * W];
            const long long* b = &S[static_cast<size_t>(y + win) * W];
            const long long* a2 = needSq ? &S2[static_cast<size_t>(y) * W] : nullptr;
            const long long* b2 = needSq ? &S2[static_cast<size_t>(y + win) * W] : nullptr;
            const uint8_t* s = g.row(y);
            uint8_t* o = out.row(y);
            for (int x = 0; x < w; ++x) {
                const double sum = static_cast<double>(b[x + win] - a[x + win] - b[x] + a[x]);
                const double sum2 = needSq ? static_cast<double>(b2[x + win] - a2[x + win] - b2[x] + a2[x]) : 0.0;
                o[x] = decide(sum, sum2, s[x]);
            }
            stepProgress(prog);
        }
    });
    return out;
}

// ============================================================================
//  CLAHE
// ============================================================================
static void claheGray(const uint8_t* Y, int w, int h, const ClaheParams& p, uint8_t* outY, Progress* prog) {
    const int tx = std::max(1, std::min(p.tilesX, w)), ty = std::max(1, std::min(p.tilesY, h));
    const int tw = (w + tx - 1) / tx, th = (h + ty - 1) / ty;
    const int nx = (w + tw - 1) / tw, ny = (h + th - 1) / th;
    std::vector<uint8_t> lut(static_cast<size_t>(nx) * ny * 256);

    // 1) гистограмма, отсечение по clipLimit, перераспределение избытка, CDF -> LUT для каждой плитки
    parallelFor(0, nx * ny, [&](int a, int b) {
        for (int t = a; t < b; ++t) {
            const int tyi = t / nx, txi = t % nx;
            const int x0 = txi * tw, x1 = std::min(w, x0 + tw), y0 = tyi * th, y1 = std::min(h, y0 + th);
            double hist[256] = {0};
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x) hist[Y[static_cast<size_t>(y) * w + x]] += 1.0;
            const double area = static_cast<double>(x1 - x0) * (y1 - y0);
            const double clip = std::max(1.0, p.clipLimit * area / 256.0);
            double excess = 0;
            for (int i = 0; i < 256; ++i)
                if (hist[i] > clip) { excess += hist[i] - clip; hist[i] = clip; }
            const double add = excess / 256.0;
            double cum = 0;
            uint8_t* L = &lut[static_cast<size_t>(t) * 256];
            for (int i = 0; i < 256; ++i) {
                cum += hist[i] + add;
                L[i] = clamp8(static_cast<float>(cum * 255.0 / area));
            }
            stepProgress(prog);
        }
    });

    // 2) билинейная интерполяция между LUT четырёх ближайших плиток (убирает швы между плитками)
    std::vector<int> x0v(w), x1v(w);
    std::vector<float> wxv(w);
    for (int x = 0; x < w; ++x) {
        const float fx = (x + 0.5f) / tw - 0.5f;
        int i0 = static_cast<int>(std::floor(fx));
        wxv[x] = fx - i0;
        x0v[x] = std::max(0, std::min(nx - 1, i0));
        x1v[x] = std::max(0, std::min(nx - 1, i0 + 1));
    }
    parallelFor(0, h, [&](int ya, int yb) {
        for (int y = ya; y < yb; ++y) {
            const float fy = (y + 0.5f) / th - 0.5f;
            const int j0 = static_cast<int>(std::floor(fy));
            const float wy = fy - j0;
            const int r0 = std::max(0, std::min(ny - 1, j0)), r1 = std::max(0, std::min(ny - 1, j0 + 1));
            const uint8_t* in = Y + static_cast<size_t>(y) * w;
            uint8_t* o = outY + static_cast<size_t>(y) * w;
            for (int x = 0; x < w; ++x) {
                const int v = in[x];
                const float l00 = lut[(static_cast<size_t>(r0) * nx + x0v[x]) * 256 + v];
                const float l01 = lut[(static_cast<size_t>(r0) * nx + x1v[x]) * 256 + v];
                const float l10 = lut[(static_cast<size_t>(r1) * nx + x0v[x]) * 256 + v];
                const float l11 = lut[(static_cast<size_t>(r1) * nx + x1v[x]) * 256 + v];
                const float top = l00 + wxv[x] * (l01 - l00), bot = l10 + wxv[x] * (l11 - l10);
                o[x] = clamp8(top + wy * (bot - top));
            }
            stepProgress(prog);
        }
    });
}

Image clahe(const Image& src, const ClaheParams& p, Progress* prog) {
    const int w = src.width, h = src.height;
    const Image gray = toGray(src);
    const int tx = std::max(1, std::min(p.tilesX, w)), ty = std::max(1, std::min(p.tilesY, h));
    if (prog) prog->reset(tx * ty + h);
    Image eq(w, h, 1);
    claheGray(gray.data.data(), w, h, p, eq.data.data(), prog);
    if (src.channels == 1) return eq;
    // Цветное изображение: выравниваем только яркость, сдвигая каналы на ΔY (цветность сохраняется).
    Image out(w, h, 3);
    parallelFor(0, h, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const uint8_t* s = src.row(y);
            const uint8_t* g = gray.row(y);
            const uint8_t* e = eq.row(y);
            uint8_t* o = out.row(y);
            for (int x = 0; x < w; ++x) {
                const int d = static_cast<int>(e[x]) - static_cast<int>(g[x]);
                for (int c = 0; c < 3; ++c) {
                    const int v = s[3 * x + c] + d;
                    o[3 * x + c] = static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
                }
            }
        }
    });
    return out;
}

}  // namespace ip
