#include "ImageConvert.h"
#include <cstring>

ip::Image qimageToImage(const QImage& src) {
    if (src.isNull()) return ip::Image();
    const QImage q = src.convertToFormat(QImage::Format_RGB888);  // только смена формата хранения
    const int w = q.width(), h = q.height();
    ip::Image rgb(w, h, 3);
    bool gray = true;
    for (int y = 0; y < h; ++y) {
        const uchar* line = q.constScanLine(y);
        uint8_t* d = rgb.row(y);
        std::memcpy(d, line, static_cast<size_t>(w) * 3);
        if (gray)
            for (int x = 0; x < w; ++x)
                if (d[3 * x] != d[3 * x + 1] || d[3 * x] != d[3 * x + 2]) { gray = false; break; }
    }
    if (!gray) return rgb;
    ip::Image g(w, h, 1);  // изображение серое — храним один канал (в 3 раза меньше работы)
    for (int y = 0; y < h; ++y) {
        const uint8_t* s = rgb.row(y);
        uint8_t* d = g.row(y);
        for (int x = 0; x < w; ++x) d[x] = s[3 * x];
    }
    return g;
}

QImage imageToQImage(const ip::Image& img) {
    if (img.empty()) return QImage();
    const int w = img.width, h = img.height;
    QImage q(w, h, img.channels == 1 ? QImage::Format_Grayscale8 : QImage::Format_RGB888);
    for (int y = 0; y < h; ++y)
        std::memcpy(q.scanLine(y), img.row(y), static_cast<size_t>(w) * img.channels);
    return q;
}
