#include "TestImages.h"
#include "Algorithms.h"
#include <random>

namespace ip {
namespace {

typedef std::mt19937 Rng;

// Холст из float-значений с «эталонной маской» нарисованных объектов.
struct Scene {
    PlaneF img, mask;
    Scene(int w, int h, float bg) : img(w, h), mask(w, h) {
        std::fill(img.data.begin(), img.data.end(), bg);
    }
    void blend(int x, int y, float a, float v) {
        if (x < 0 || y < 0 || x >= img.width || y >= img.height || a <= 0.f) return;
        float& p = img.row(y)[x];
        p = p * (1.f - a) + v * a;
        if (a > 0.5f) mask.row(y)[x] = 255.f;
    }
    static float cov(float d) { return std::min(1.f, std::max(0.f, 0.5f + d)); }  // покрытие пикселя (AA)

    void rect(float x0, float y0, float x1, float y1, float v) {
        for (int y = (int)std::floor(y0) - 1; y <= (int)std::ceil(y1) + 1; ++y)
            for (int x = (int)std::floor(x0) - 1; x <= (int)std::ceil(x1) + 1; ++x) {
                const float dx = std::min(x + 0.5f - x0, x1 - (x + 0.5f));
                const float dy = std::min(y + 0.5f - y0, y1 - (y + 0.5f));
                blend(x, y, cov(std::min(dx, dy)), v);
            }
    }
    void circle(float cx, float cy, float r, float v) {
        for (int y = (int)(cy - r) - 1; y <= (int)(cy + r) + 1; ++y)
            for (int x = (int)(cx - r) - 1; x <= (int)(cx + r) + 1; ++x) {
                const float d = std::sqrt((x + 0.5f - cx) * (x + 0.5f - cx) + (y + 0.5f - cy) * (y + 0.5f - cy));
                blend(x, y, cov(r - d), v);
            }
    }
    void line(float x0, float y0, float x1, float y1, float thick, float v) {
        const float minx = std::min(x0, x1) - thick - 1, maxx = std::max(x0, x1) + thick + 1;
        const float miny = std::min(y0, y1) - thick - 1, maxy = std::max(y0, y1) + thick + 1;
        const float vx = x1 - x0, vy = y1 - y0, len2 = std::max(1e-6f, vx * vx + vy * vy);
        for (int y = (int)miny; y <= (int)maxy; ++y)
            for (int x = (int)minx; x <= (int)maxx; ++x) {
                float t = ((x + 0.5f - x0) * vx + (y + 0.5f - y0) * vy) / len2;
                t = std::min(1.f, std::max(0.f, t));
                const float px = x0 + t * vx, py = y0 + t * vy;
                const float d = std::sqrt((x + 0.5f - px) * (x + 0.5f - px) + (y + 0.5f - py) * (y + 0.5f - py));
                blend(x, y, cov(thick * 0.5f - d), v);
            }
    }
};

// «Текст» из случайных штрихов: строки глифов с пробелами. Шрифты не нужны.
void pseudoText(Scene& s, float x, float y, float w, float h, float lineH, float v, Rng& rng) {
    const float gw = lineH * 0.55f, th = std::max(1.2f, lineH * 0.13f);
    for (float ly = y; ly + lineH <= y + h; ly += lineH * 1.7f) {
        float cx = x;
        while (cx + gw <= x + w) {
            if (rng() % 7 == 0) { cx += gw * 1.3f; continue; }
            const int strokes = 2 + static_cast<int>(rng() % 3);
            int pa = static_cast<int>(rng() % 9);
            for (int i = 0; i < strokes; ++i) {
                int pb = static_cast<int>(rng() % 9);
                if (pb == pa) pb = (pb + 4) % 9;
                s.line(cx + (pa % 3) * gw * 0.4f, ly + (pa / 3) * lineH * 0.5f,
                       cx + (pb % 3) * gw * 0.4f, ly + (pb / 3) * lineH * 0.5f, th, v);
                pa = pb;
            }
            cx += gw * 1.2f;
        }
    }
}

void addNoise(PlaneF& p, float sigma, Rng& rng) {
    std::normal_distribution<float> n(0.f, sigma);
    for (size_t i = 0; i < p.data.size(); ++i) p.data[i] += n(rng);
}

void blur(PlaneF& p, double sigma) {
    p = separableConvolve(p, gaussianKernel(sigma), EdgeMode::Reflect);
}

Image fromPlane(const PlaneF& p) {
    Image im(p.width, p.height, 1);
    storeChannel(p, im, 0);
    return im;
}

Image truthFrom(const Scene& s) {
    Image t(s.mask.width, s.mask.height, 1);
    storeChannel(s.mask, t, 0);
    return t;
}

// Сцена из геометрических фигур (для проверки резкости).
void drawShapes(Scene& s, int W, int H, Rng& rng) {
    const float w = (float)W, h = (float)H;
    s.rect(0.06f * w, 0.08f * h, 0.30f * w, 0.35f * h, 200.f);
    s.rect(0.10f * w, 0.12f * h, 0.26f * w, 0.31f * h, 120.f);
    s.circle(0.50f * w, 0.22f * h, 0.12f * h, 230.f);
    s.circle(0.50f * w, 0.22f * h, 0.06f * h, 90.f);
    for (int i = 0; i < 6; ++i)  // линии разной толщины
        s.line(0.66f * w, (0.08f + 0.05f * i) * h, 0.94f * w, (0.12f + 0.05f * i) * h, 1.f + i * 1.2f, 220.f);
    pseudoText(s, 0.06f * w, 0.45f * h, 0.42f * w, 0.28f * h, 0.045f * h, 235.f, rng);
    s.rect(0.54f * w, 0.45f * h, 0.94f * w, 0.73f * h, 40.f);
    pseudoText(s, 0.56f * w, 0.47f * h, 0.36f * w, 0.24f * h, 0.04f * h, 210.f, rng);
    for (int i = 0; i < 12; ++i)  // сетка точек
        for (int j = 0; j < 3; ++j) s.circle((0.08f + 0.07f * i) * w, (0.80f + 0.05f * j) * h, 0.012f * h, 245.f);
}

PlaneF illumination(int w, int h, int kind) {
    PlaneF f(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const float u = (float)x / w, v = (float)y / h;
            float k;
            if (kind == 0) {            // линейный градиент слева направо
                k = 1.0f - 0.68f * u;
            } else {                    // диагональная тень + виньетка
                const float d = (u + v) * 0.5f;
                float t = std::min(1.f, std::max(0.f, (d - 0.35f) / 0.25f));
                t = t * t * (3.f - 2.f * t);
                const float cx = u - 0.5f, cy = v - 0.5f;
                k = (1.0f - 0.55f * t) * (1.0f - 0.35f * (cx * cx + cy * cy) * 2.f);
            }
            f.row(y)[x] = k;
        }
    return f;
}

}  // namespace

const std::vector<TestImageInfo>& testImageCatalog() {
    static const std::vector<TestImageInfo> list = {
        { "01_blurred_shapes", "Размытые фигуры и текст",
          "Для повышения резкости: гауссово размытие σ=2.2, чистое (без шума).", false },
        { "02_blurred_noisy", "Размытые + шум",
          "Для повышения резкости: размытие + гауссов шум σ=7. Видно усиление шума лапласианами; порог в unsharp его подавляет.", false },
        { "03_siemens_star", "Звезда Сименса (размытая)",
          "Тест-мира для оценки разрешения до/после повышения резкости.", false },
        { "04_color_blurred", "Цветное размытое",
          "Повышение резкости цветного изображения (обработка каждого канала RGB).", false },
        { "05_bimodal_clean", "Бимодальное, равномерный фон",
          "Глобальные методы работают хорошо (Оцу, итерационный, ручной). Есть эталон.", true },
        { "06_small_object", "Малый объект на фоне + сильный шум",
          "Объект занимает ~1% кадра, гистограмма почти унимодальна: Оцу и итерационный метод ошибаются (≈53%), метод треугольника справляется (≈98%).", true },
        { "07_doc_gradient", "Документ, градиент освещённости",
          "Глобальные пороги не справляются, адаптивный (локальное среднее) работает. Есть эталон.", true },
        { "08_doc_shadow_noise", "Документ с тенью и шумом",
          "Диагональная тень + виньетка + шум: только адаптивная обработка даёт читаемый результат.", true },
        { "09_low_contrast", "Малоконтрастное",
          "Узкий диапазон яркостей: пример для CLAHE и для проверки порогов.", false },
        { "10_noisy_bimodal", "Бимодальное, сильный шум",
          "Сильный шум σ=35: сравнение устойчивости порогов, видна ошибка бинаризации.", true },
    };
    return list;
}

Image generateTestImage(int index, int W, int H, Image* truth) {
    Rng rng(1000u + static_cast<unsigned>(index) * 7919u);
    switch (index) {
    case 0: case 1: {
        Scene s(W, H, 70.f);
        drawShapes(s, W, H, rng);
        blur(s.img, index == 0 ? 2.2 : 1.8);
        if (index == 1) addNoise(s.img, 7.f, rng);
        return fromPlane(s.img);
    }
    case 2: {
        Scene s(W, H, 128.f);
        const float cx = W * 0.5f, cy = H * 0.5f, R = std::min(W, H) * 0.46f;
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x) {
                const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                if (dx * dx + dy * dy <= R * R)
                    s.img.row(y)[x] = (std::sin(36.f * std::atan2(dy, dx)) > 0.f) ? 225.f : 30.f;
            }
        blur(s.img, 1.5);
        return fromPlane(s.img);
    }
    case 3: {
        Scene s(W, H, 70.f);
        drawShapes(s, W, H, rng);
        Image im(W, H, 3);
        PlaneF R(W, H), G(W, H), B(W, H);
        for (size_t i = 0; i < s.img.data.size(); ++i) {
            const float v = s.img.data[i];
            R.data[i] = v;
            G.data[i] = 255.f - 0.8f * v;
            B.data[i] = 40.f + 1.7f * std::fabs(v - 120.f);
        }
        blur(R, 2.0); blur(G, 2.0); blur(B, 2.0);
        storeChannel(R, im, 0); storeChannel(G, im, 1); storeChannel(B, im, 2);
        return im;
    }
    case 4: case 9: {
        Scene s(W, H, 70.f);
        s.rect(0.08f * W, 0.10f * H, 0.40f * W, 0.45f * H, 180.f);
        s.circle(0.65f * W, 0.30f * H, 0.17f * H, 180.f);
        s.line(0.10f * W, 0.75f * H, 0.90f * W, 0.60f * H, 0.07f * H, 180.f);
        s.circle(0.25f * W, 0.75f * H, 0.08f * H, 180.f);
        s.rect(0.70f * W, 0.62f * H, 0.92f * W, 0.90f * H, 180.f);
        if (index == 4) { blur(s.img, 1.0); addNoise(s.img, 12.f, rng); }
        else            { blur(s.img, 1.0); addNoise(s.img, 35.f, rng); }
        if (truth) *truth = truthFrom(s);
        return fromPlane(s.img);
    }
    case 5: {
        Scene s(W, H, 90.f);
        s.circle(0.62f * W, 0.40f * H, 0.055f * H, 135.f);
        s.rect(0.20f * W, 0.62f * H, 0.25f * W, 0.68f * H, 135.f);
        s.circle(0.80f * W, 0.75f * H, 0.035f * H, 135.f);
        blur(s.img, 1.0);
        addNoise(s.img, 22.f, rng);
        if (truth) *truth = truthFrom(s);
        return fromPlane(s.img);
    }
    case 6: case 7: {
        Scene s(W, H, 225.f);
        pseudoText(s, 0.05f * W, 0.06f * H, 0.90f * W, 0.88f * H, H / 26.f, 45.f, rng);
        PlaneF ill = illumination(W, H, index == 6 ? 0 : 1);
        for (size_t i = 0; i < s.img.data.size(); ++i) s.img.data[i] *= ill.data[i];
        blur(s.img, 0.8);
        addNoise(s.img, index == 6 ? 4.f : 8.f, rng);
        if (truth) *truth = truthFrom(s);
        return fromPlane(s.img);
    }
    case 8: {
        Scene s(W, H, 112.f);
        drawShapes(s, W, H, rng);
        for (size_t i = 0; i < s.img.data.size(); ++i) s.img.data[i] = 100.f + (s.img.data[i] - 40.f) * 0.14f;
        blur(s.img, 1.0);
        addNoise(s.img, 2.5f, rng);
        return fromPlane(s.img);
    }
    default: break;
    }
    return Image(W, H, 1);
}

}  // namespace ip
