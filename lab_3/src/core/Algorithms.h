// ============================================================================
//  Processing Layer — чистые алгоритмы. Принимают буфер, возвращают новый буфер.
//  Ничего не знают про UI и Qt.
// ============================================================================
#pragma once
#include "Image.h"
#include "Parallel.h"

namespace ip {

// ===================== Высокочастотные фильтры (повышение резкости) ==========
struct SharpenParams {
    enum Method { Laplacian4, Laplacian8, Unsharp, HighBoost };
    Method method = Unsharp;
    double strength = 1.0;   // k: g = f + k * HP(f)       (Laplacian4/8, Unsharp)
    double boostA = 2.0;     // A: g = A*f - LP(f)         (HighBoost, A >= 1)
    bool boxBlur = false;    // тип НЧ-фильтра для Unsharp/HighBoost: Гаусс или бокс
    double sigma = 1.5;      // σ Гаусса
    int boxRadius = 2;       // радиус бокс-фильтра (окно 2r+1)
    int threshold = 0;       // порог |HP| для Unsharp (подавление шума), 0..255
    bool detailOnly = false; // показать только ВЧ-составляющую (+128)
    EdgeMode edge = EdgeMode::Reflect;
};
Image sharpen(const Image& src, const SharpenParams& p, Progress* prog = nullptr);

// Строительные блоки (используются и тестами):
std::vector<float> gaussianKernel(double sigma);
std::vector<float> boxKernel(int radius);
// Свёртка 2D ядром 3x3 (k[9], построчно) — «в лоб», два вложенных цикла по пикселям.
PlaneF convolve3x3(const PlaneF& src, const float k[9], EdgeMode edge, Progress* prog = nullptr);
// Разделяемая свёртка: проход по строкам, затем по столбцам. O(K*N^2) вместо O(K^2*N^2).
PlaneF separableConvolve(const PlaneF& src, const std::vector<float>& k, EdgeMode edge, Progress* prog = nullptr);
// Эталонная неразделяемая свёртка K×K (для проверки корректности/сравнения скорости).
PlaneF convolve2DFull(const PlaneF& src, const std::vector<float>& kRow, const std::vector<float>& kCol,
                      EdgeMode edge);

// ===================== Глобальная пороговая обработка ========================
struct GlobalThresholdParams {
    enum Method { Otsu, Iterative, Triangle, Manual };
    Method method = Otsu;
    int manual = 128;
    bool invert = false;
};
// Оцу: автоматический поиск порога по максимуму межклассовой дисперсии.
int otsuThreshold(const std::vector<int>& hist);
// Итерационный (Ridler–Calvard / ISODATA): T = (m1 + m2)/2 до сходимости.
int iterativeThreshold(const std::vector<int>& hist);
// Треугольник (Zack): порог в точке гистограммы, максимально удалённой от прямой «пик — дальний хвост».
// Хорошо работает, когда объект мал и гистограмма почти унимодальна (там, где Оцу ошибается).
int triangleThreshold(const std::vector<int>& hist);
// Возвращает бинарное изображение (0/255). *usedThreshold — найденный порог.
Image globalThreshold(const Image& src, const GlobalThresholdParams& p, int* usedThreshold = nullptr,
                      Progress* prog = nullptr);

// ===================== Адаптивная пороговая обработка ========================
struct AdaptiveParams {
    enum Method { LocalMean, Sauvola };
    Method method = LocalMean;  // LocalMean: T = mean(окно) - C  (метод из лекции)
    int window = 25;            // сторона окна (нечётная)
    double C = 10;              // вычитаемая константа (LocalMean)
    double k = 0.2, R = 128;    // параметры Саволы: T = m * (1 + k * (s / R - 1))
    bool invert = false;
    bool useIntegral = true;    // true: интегральное изображение O(1)/пиксель; false: перебор окна
    EdgeMode edge = EdgeMode::Reflect;
};
Image adaptiveThreshold(const Image& src, const AdaptiveParams& p, Progress* prog = nullptr);

// ===================== CLAHE (дополнительный режим) ==========================
struct ClaheParams {
    int tilesX = 8, tilesY = 8;
    double clipLimit = 3.0;  // во сколько раз можно превысить среднюю высоту корзины
};
Image clahe(const Image& src, const ClaheParams& p, Progress* prog = nullptr);

}  // namespace ip
