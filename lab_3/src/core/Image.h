// ============================================================================
//  Memory & Pixel Access Layer
//  Обёртка над «сырыми» байтами изображения + обработка краёв (Clamp / Reflect).
//  Не зависит от Qt. Алгоритмы не знают ни про UI, ни про указатели на QImage.
// ============================================================================
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace ip {

// ---------------------------------------------------------------- края ------
enum class EdgeMode { Clamp = 0, Reflect = 1 };

// Преобразование «виртуальной» координаты i (может быть <0 или >=n) в реальную.
//  Clamp  : дублирование граничных пикселей      ... a a | a b c d | d d ...
//  Reflect: зеркальное отражение (с повтором края) ... b a | a b c d | d c ...
// Работает для любого радиуса (в т.ч. больше размера изображения).
inline int mapIndex(int i, int n, EdgeMode mode) {
    if (i >= 0 && i < n) return i;
    if (n <= 1) return 0;
    if (mode == EdgeMode::Clamp) return i < 0 ? 0 : n - 1;
    const int period = 2 * n;
    i %= period;
    if (i < 0) i += period;
    return i < n ? i : period - 1 - i;
}

// Таблица для «расширенной» оси: map[i + r] = реальный индекс для i in [-r, n + r).
// Строится один раз на проход, поэтому во внутренних циклах нет ветвлений по краям.
inline std::vector<int> buildIndexMap(int n, int r, EdgeMode mode) {
    std::vector<int> map(static_cast<size_t>(n + 2 * r));
    for (int i = -r; i < n + r; ++i) map[static_cast<size_t>(i + r)] = mapIndex(i, n, mode);
    return map;
}

// ---------------------------------------------------------------- буферы ----
// 8-битное изображение, чередующиеся каналы (1 = градации серого, 3 = RGB).
struct Image {
    int width = 0, height = 0, channels = 1;
    std::vector<uint8_t> data;

    Image() {}
    Image(int w, int h, int c) : width(w), height(h), channels(c), data(static_cast<size_t>(w) * h * c, 0) {}

    bool empty() const { return data.empty(); }
    uint8_t* row(int y) { return data.data() + static_cast<size_t>(y) * width * channels; }
    const uint8_t* row(int y) const { return data.data() + static_cast<size_t>(y) * width * channels; }
};

// Один канал во float (для свёрток без промежуточного округления).
struct PlaneF {
    int width = 0, height = 0;
    std::vector<float> data;

    PlaneF() {}
    PlaneF(int w, int h) : width(w), height(h), data(static_cast<size_t>(w) * h, 0.f) {}
    float* row(int y) { return data.data() + static_cast<size_t>(y) * width; }
    const float* row(int y) const { return data.data() + static_cast<size_t>(y) * width; }
};

inline uint8_t clamp8(float v) {
    const int i = static_cast<int>(std::floor(v + 0.5f));
    return static_cast<uint8_t>(i < 0 ? 0 : (i > 255 ? 255 : i));
}

PlaneF extractChannel(const Image& src, int c);
void storeChannel(const PlaneF& plane, Image& dst, int c);

// Яркость Y = 0.299R + 0.587G + 0.114B (целочисленно). Для 1 канала — копия.
Image toGray(const Image& src);

// ------------------------------------------------------------ гистограммы ---
struct Histograms {
    std::vector<int> lum, r, g, b;  // по 256 корзин (r,g,b — только для цветных)
    bool color = false;
    double mean = 0, stddev = 0;    // статистики яркости
    int minVal = 0, maxVal = 0;
};
Histograms computeHistograms(const Image& img);
std::vector<int> grayHistogram(const Image& gray);  // gray.channels == 1

}  // namespace ip
