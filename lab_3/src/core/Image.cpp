#include "Image.h"
#include "Parallel.h"

namespace ip {

PlaneF extractChannel(const Image& src, int c) {
    PlaneF p(src.width, src.height);
    const int C = src.channels, w = src.width;
    parallelFor(0, src.height, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const uint8_t* s = src.row(y) + c;
            float* d = p.row(y);
            for (int x = 0; x < w; ++x) d[x] = static_cast<float>(s[x * C]);
        }
    });
    return p;
}

void storeChannel(const PlaneF& plane, Image& dst, int c) {
    const int C = dst.channels, w = dst.width;
    parallelFor(0, dst.height, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const float* s = plane.row(y);
            uint8_t* d = dst.row(y) + c;
            for (int x = 0; x < w; ++x) d[x * C] = clamp8(s[x]);
        }
    });
}

Image toGray(const Image& src) {
    if (src.channels == 1) return src;
    Image g(src.width, src.height, 1);
    const int w = src.width;
    parallelFor(0, src.height, [&](int y0, int y1) {
        for (int y = y0; y < y1; ++y) {
            const uint8_t* s = src.row(y);
            uint8_t* d = g.row(y);
            for (int x = 0; x < w; ++x) {
                const uint8_t* p = s + 3 * x;
                d[x] = static_cast<uint8_t>((299 * p[0] + 587 * p[1] + 114 * p[2] + 500) / 1000);
            }
        }
    });
    return g;
}

std::vector<int> grayHistogram(const Image& gray) {
    std::vector<int> h(256, 0);
    const size_t n = static_cast<size_t>(gray.width) * gray.height;
    const uint8_t* p = gray.data.data();
    for (size_t i = 0; i < n; ++i) ++h[p[i]];
    return h;
}

Histograms computeHistograms(const Image& im) {
    Histograms h;
    h.color = (im.channels == 3);
    h.lum.assign(256, 0);
    if (h.color) { h.r.assign(256, 0); h.g.assign(256, 0); h.b.assign(256, 0); }
    if (im.empty()) return h;
    double sum = 0, sum2 = 0;
    int mn = 255, mx = 0;
    for (int y = 0; y < im.height; ++y) {
        const uint8_t* s = im.row(y);
        for (int x = 0; x < im.width; ++x) {
            int lum;
            if (h.color) {
                const uint8_t* p = s + 3 * x;
                ++h.r[p[0]]; ++h.g[p[1]]; ++h.b[p[2]];
                lum = (299 * p[0] + 587 * p[1] + 114 * p[2] + 500) / 1000;
            } else {
                lum = s[x];
            }
            ++h.lum[lum];
            sum += lum; sum2 += static_cast<double>(lum) * lum;
            if (lum < mn) mn = lum;
            if (lum > mx) mx = lum;
        }
    }
    const double n = static_cast<double>(im.width) * im.height;
    h.mean = sum / n;
    h.stddev = std::sqrt(std::max(0.0, sum2 / n - h.mean * h.mean));
    h.minVal = mn; h.maxVal = mx;
    return h;
}

}  // namespace ip
