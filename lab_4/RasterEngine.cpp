#include "RasterEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace
{
const size_t kTraceLimit = 1500;

void note(Trace *tr, const char *fmt, ...)
{
    if (tr->size() > kTraceLimit)
        return;
    if (tr->size() == kTraceLimit) {
        tr->push_back("... протокол сокращён ...");
        return;
    }
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    tr->push_back(buf);
}

int roundi(double v)
{
    return (int)std::floor(v + 0.5);
}

std::string repeat(const std::string &s, int k)
{
    std::string r;
    r.reserve(s.size() * k);
    for (int i = 0; i < k; ++i)
        r += s;
    return r;
}

std::string brief(const std::string &s)
{
    return s.size() <= 48 ? s : s.substr(0, 45) + "...";
}
}

void RasterEngine::stepLine(int x0, int y0, int x1, int y1, IRasterTarget &t, Trace *tr) //пошаговый алгоритм
{
    int dx = x1 - x0, dy = y1 - y0;
    if (dx == 0 && dy == 0) {
        t.setPixel(x0, y0, 1.0);
        if (tr) note(tr, "Отрезок вырожден в точку (%d; %d)", x0, y0);
        return;
    }

    // шагаем по той оси, где отрезок длиннее, иначе в крутых октантах появятся разрывы
    if (std::abs(dx) >= std::abs(dy)) {
        double k = (double)dy / dx;
        double b = y0 - k * x0;
        int sx = dx > 0 ? 1 : -1;
        if (tr) note(tr, "Шаг по x. k = dy/dx = %d/%d = %.4f, b = y0 - k*x0 = %.4f", dy, dx, k, b);
        for (int x = x0; x != x1 + sx; x += sx) {
            double y = k * x + b;
            int yr = roundi(y);
            t.setPixel(x, yr, 1.0);
            if (tr) note(tr, "x = %d: y = k*x + b = %.4f, округляем -> %d", x, y, yr);
        }
    } else {
        double k = (double)dx / dy;
        double b = x0 - k * y0;
        int sy = dy > 0 ? 1 : -1;
        if (tr) note(tr, "Шаг по y. k = dx/dy = %d/%d = %.4f, b = x0 - k*y0 = %.4f", dx, dy, k, b);
        for (int y = y0; y != y1 + sy; y += sy) {
            double x = k * y + b;
            int xr = roundi(x);
            t.setPixel(xr, y, 1.0);
            if (tr) note(tr, "y = %d: x = k*y + b = %.4f, округляем -> %d", y, x, xr);
        }
    }
}

void RasterEngine::ddaLine(int x0, int y0, int x1, int y1, IRasterTarget &t, Trace *tr) //ЦДА
{
    int dx = x1 - x0, dy = y1 - y0;
    int len = std::max(std::abs(dx), std::abs(dy));
    if (len == 0) {
        t.setPixel(x0, y0, 1.0);
        if (tr) note(tr, "Отрезок вырожден в точку (%d; %d)", x0, y0);
        return;
    }

    // длина берётся по большей проекции, поэтому по ней приращение ровно 1, по другой - не больше 1
    double sx = (double)dx / len, sy = (double)dy / len;
    double x = x0, y = y0;
    if (tr) note(tr, "L = max(|dx|, |dy|) = %d, приращения: sx = dx/L = %.4f, sy = dy/L = %.4f", len, sx, sy);
    for (int i = 0; i <= len; ++i) {
        int xr = roundi(x), yr = roundi(y);
        t.setPixel(xr, yr, 1.0);
        if (tr) note(tr, "i = %d: (%.4f; %.4f) -> (%d; %d)", i, x, y, xr, yr);
        x += sx;
        y += sy;
    }
}

void RasterEngine::bresenhamLine(int x0, int y0, int x1, int y1, IRasterTarget &t, Trace *tr) //Брезенхем для отрезка, обобщён на все 8 октантов
{
    // приводим любой октант к первому: крутой отрезок отражаем по диагонали, затем идём слева направо
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    int dx = x1 - x0, dy = std::abs(y1 - y0);
    int ystep = y0 < y1 ? 1 : -1;
    int d = 2 * dy - dx;   // знак d говорит, какой пиксель ближе к идеальной линии
    int y = y0;
    if (tr) {
        note(tr, "%s", steep ? "Отрезок крутой: меняем местами x и y, результат переставим обратно" : "Отрезок пологий: оси не меняем");
        note(tr, "После приведения: dx = %d, dy = %d, шаг по y = %d, d0 = 2*dy - dx = %d", dx, dy, ystep, d);
    }
    for (int x = x0; x <= x1; ++x) {
        if (steep)
            t.setPixel(y, x, 1.0);
        else
            t.setPixel(x, y, 1.0);
        if (tr) {
            if (d > 0)
                note(tr, "(%d; %d), d = %d > 0: поднимаемся по y, d += 2*(dy - dx)", steep ? y : x, steep ? x : y, d);
            else
                note(tr, "(%d; %d), d = %d <= 0: y остаётся, d += 2*dy", steep ? y : x, steep ? x : y, d);
        }
        if (d > 0) {
            y += ystep;
            d -= 2 * dx;
        }
        d += 2 * dy;
    }
}

void RasterEngine::wuLine(int x0, int y0, int x1, int y1, IRasterTarget &t, Trace *tr) //сглаженный Ву
{
    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep) {
        std::swap(x0, y0);
        std::swap(x1, y1);
    }
    if (x0 > x1) {
        std::swap(x0, x1);
        std::swap(y0, y1);
    }

    int dx = x1 - x0, dy = y1 - y0;
    if (dx == 0) {
        t.setPixel(x0, y0, 1.0);
        if (tr) note(tr, "Отрезок вырожден в точку");
        return;
    }

    // ордината идеальной линии хранится как y + r/dx, так дробная часть считается точно, без накопления ошибки
    double inv = 1.0 / dx;
    int y = y0, r = 0;
    if (tr) note(tr, "dx = %d, dy = %d, наклон = %.4f. На каждом x закрашиваем два пикселя: 1 - f и f, f = дробная часть y", dx, dy, (double)dy / dx);
    for (int x = x0; x <= x1; ++x) {
        double f = r * inv;
        if (steep) {
            t.setPixel(y, x, 1.0 - f);
            if (r != 0) t.setPixel(y + 1, x, f);
        } else {
            t.setPixel(x, y, 1.0 - f);
            if (r != 0) t.setPixel(x, y + 1, f);
        }
        if (tr) {
            if (steep)
                note(tr, "x = %d: y = %d + %d/%d = %.4f, f = %.4f -> I(%d; %d) = %.4f, I(%d; %d) = %.4f",
                     x, y, r, dx, y + f, f, y, x, 1.0 - f, y + 1, x, f);
            else
                note(tr, "x = %d: y = %d + %d/%d = %.4f, f = %.4f -> I(%d; %d) = %.4f, I(%d; %d) = %.4f",
                     x, y, r, dx, y + f, f, x, y, 1.0 - f, x, y + 1, f);
        }
        r += dy;
        if (r >= dx) {
            r -= dx;
            ++y;
        } else if (r < 0) {
            r += dx;
            --y;
        }
    }
}

void RasterEngine::castlePitteway(int x0, int y0, int x1, int y1, IRasterTarget &t, Trace *tr) //Кастл–Питвей для отрезка
{
    int dx = x1 - x0, dy = y1 - y0;
    int adx = std::abs(dx), ady = std::abs(dy);
    int n = std::max(adx, ady), m = std::min(adx, ady);
    int sx = dx >= 0 ? 1 : -1, sy = dy >= 0 ? 1 : -1;
    bool steep = ady > adx;

    t.setPixel(x0, y0, 1.0);
    if (n == 0) {
        if (tr) note(tr, "Отрезок вырожден в точку");
        return;
    }

    // S - шаг только по главной оси, D - шаг по главной и по второй оси сразу
    std::string period;
    int times;
    if (m == 0) {
        period = "S";
        times = n;
    } else if (m == n) {
        period = "D";
        times = n;
    } else {
        // алгоритм Евклида над строками: считаем a копий S и b копий D и склеиваем их как можно равномернее
        int a = n - m, b = m;
        std::string s = "S", d = "D";
        if (tr) note(tr, "Нужно %d шагов S и %d шагов D (n = %d, m = %d)", a, b, n, m);
        while (a != b) {
            if (a > b) {
                int k = (a - 1) / b;
                d = repeat(s, k) + d;
                a -= k * b;
                if (tr) note(tr, "a > b: к каждому D добавляем %d S слева, a = %d, D = %s", k, a, brief(d).c_str());
            } else {
                int k = (b - 1) / a;
                s = s + repeat(d, k);
                b -= k * a;
                if (tr) note(tr, "b > a: к каждому S добавляем %d D справа, b = %d, S = %s", k, b, brief(s).c_str());
            }
        }
        period = s + d;
        times = a;
        if (tr) note(tr, "a = b = %d = НОД(n, m). Период = S + D = %s, повторяем его %d раз(а)", a, brief(period).c_str(), times);
    }

    int x = x0, y = y0;
    for (int rep = 0; rep < times; ++rep) {
        for (size_t i = 0; i < period.size(); ++i) {
            bool diag = period[i] == 'D';
            if (steep) {
                y += sy;
                if (diag) x += sx;
            } else {
                x += sx;
                if (diag) y += sy;
            }
            t.setPixel(x, y, 1.0);
            if (tr) note(tr, "шаг %c -> (%d; %d)", period[i], x, y);
        }
    }
}

void RasterEngine::bresenhamCircle(int cx, int cy, int r, IRasterTarget &t, Trace *tr) //Брезенхем для окружности
{
    r = std::abs(r);
    int x = 0, y = r;
    int d = 1 - r;   // значение функции окружности в средней точке между двумя кандидатами
    if (tr) note(tr, "Центр (%d; %d), R = %d. Считаем один октант, остальные семь получаем симметрией. x = 0, y = R, d0 = 1 - R = %d", cx, cy, r, d);
    while (x <= y) {
        t.setPixel(cx + x, cy + y, 1.0);
        t.setPixel(cx - x, cy + y, 1.0);
        t.setPixel(cx + x, cy - y, 1.0);
        t.setPixel(cx - x, cy - y, 1.0);
        t.setPixel(cx + y, cy + x, 1.0);
        t.setPixel(cx - y, cy + x, 1.0);
        t.setPixel(cx + y, cy - x, 1.0);
        t.setPixel(cx - y, cy - x, 1.0);
        if (d < 0) {
            if (tr) note(tr, "(x; y) = (%d; %d), d = %d < 0: идём вправо, d += 2x + 3", x, y, d);
            d += 2 * x + 3;
        } else {
            if (tr) note(tr, "(x; y) = (%d; %d), d = %d >= 0: вправо и вниз, d += 2(x - y) + 5", x, y, d);
            d += 2 * (x - y) + 5;
            --y;
        }
        ++x;
    }
}

namespace
{
// для полного эллипса фильтр пропускает всё и компилятор его выбрасывает
struct AllAngles
{
    bool operator()(int, int) const { return true; }
};

// сектор от луча u до луча v против часовой стрелки, проверка точки через векторные произведения
struct Sector
{
    double ux, uy, vx, vy;
    bool wide;   // сектор не меньше 180 градусов

    bool operator()(int px, int py) const
    {
        const double eps = 1e-9;
        double cu = ux * py - uy * px;   // знак: точка слева от u
        double cv = px * vy - py * vx;   // знак: точка справа от v
        if (!wide)
            return cu >= -eps && cv >= -eps;
        // широкий сектор - это всё, кроме узкого сектора от v до u
        double cvu = vx * py - vy * px;
        return !(cvu > eps && -cu > eps);
    }
};

double snap(double v)
{
    return std::fabs(v) < 1e-12 ? 0.0 : v;
}

template <class Filter>
void ellipseCore(int cx, int cy, int a, int b, IRasterTarget &t, Trace *tr, const Filter &ok)
{
    long long a2 = (long long)a * a, b2 = (long long)b * b;
    long long x = 0, y = b;
    long long p = 4 * b2 - 4 * a2 * b + a2;   // решающая величина, умноженная на 4, чтобы остаться в целых

    // четыре точки по симметрии относительно осей, каждая проверяется на попадание в дугу
    auto plot4 = [&](long long px, long long py) {
        int ix = (int)px, iy = (int)py;
        if (ok(ix, iy))   t.setPixel(cx + ix, cy + iy, 1.0);
        if (ok(-ix, iy))  t.setPixel(cx - ix, cy + iy, 1.0);
        if (ok(ix, -iy))  t.setPixel(cx + ix, cy - iy, 1.0);
        if (ok(-ix, -iy)) t.setPixel(cx - ix, cy - iy, 1.0);
    };

    if (tr) note(tr, "Центр (%d; %d), a = %d, b = %d. Область 1 (наклон < 1): идём по x, p0 = 4b^2 - 4a^2*b + a^2 = %lld", cx, cy, a, b, p);
    while (b2 * x < a2 * y) {
        plot4(x, y);
        if (tr) note(tr, "обл.1: (%lld; %lld), p = %lld %s", x, y, p, p < 0 ? "< 0: только x+1" : ">= 0: x+1 и y-1");
        if (p < 0) {
            p += 4 * b2 * (2 * x + 3);
        } else {
            p += 4 * b2 * (2 * x + 3) - 8 * a2 * (y - 1);
            --y;
        }
        ++x;
    }

    p = b2 * (2 * x + 1) * (2 * x + 1) + 4 * a2 * (y - 1) * (y - 1) - 4 * a2 * b2;
    if (tr) note(tr, "Область 2 (наклон >= 1): идём по y, p0 = %lld", p);
    while (y >= 0) {
        plot4(x, y);
        if (tr) note(tr, "обл.2: (%lld; %lld), p = %lld %s", x, y, p, p > 0 ? "> 0: только y-1" : "<= 0: x+1 и y-1");
        if (p > 0) {
            p += 4 * a2 * (3 - 2 * y);
        } else {
            p += 8 * b2 * (x + 1) + 4 * a2 * (3 - 2 * y);
            ++x;
        }
        --y;
    }
}
}

//Питвей для эллипса через метод средней точки
void RasterEngine::pittewayEllipse(int cx, int cy, int a, int b, IRasterTarget &t, Trace *tr)
{
    a = std::abs(a);
    b = std::abs(b);
    if (a == 0 || b == 0) {
        bresenhamLine(cx - a, cy - b, cx + a, cy + b, t, tr);
        return;
    }
    ellipseCore(cx, cy, a, b, t, tr, AllAngles());
}

//закрашивает только точки, попадающие в сектор от startDeg до endDeg
void RasterEngine::pittewayArc(int cx, int cy, int a, int b, int startDeg, int endDeg, IRasterTarget &t, Trace *tr)
{
    a = std::abs(a);
    b = std::abs(b);
    int span = endDeg - startDeg;
    if (span >= 360 || span <= -360 || span == 0 || a == 0 || b == 0) {
        pittewayEllipse(cx, cy, a, b, t, tr);
        return;
    }
    if (span < 0)
        span += 360;

    const double kPi = 3.14159265358979323846;
    Sector sec;
    sec.ux = snap(std::cos(startDeg * kPi / 180.0));
    sec.uy = snap(std::sin(startDeg * kPi / 180.0));
    sec.vx = snap(std::cos(endDeg * kPi / 180.0));
    sec.vy = snap(std::sin(endDeg * kPi / 180.0));
    sec.wide = span >= 180;
    if (tr) {
        note(tr, "Дуга от %d до %d градусов (%d град.), угол идёт от оси x против часовой стрелки.", startDeg, endDeg, span);
        note(tr, "Эллипс строится целиком, но точки вне сектора не закрашиваются (проверка знаком векторного произведения).");
    }
    ellipseCore(cx, cy, a, b, t, tr, sec);
}
