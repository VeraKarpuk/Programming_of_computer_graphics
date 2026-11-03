#include "Benchmark.h"

#include <QElapsedTimer>
#include <algorithm>
#include <cmath>

namespace
{
const double kPi = 3.14159265358979323846;
const int kGridSize = 2048;   // логические координаты от -1024 до 1023
const int kLineAlgos = 5;
const int kSteps = 10;

typedef void (*Fn)(int, int, int, int, IRasterTarget &, Trace *);
const Fn kLineFns[kLineAlgos] = {
    &RasterEngine::stepLine, &RasterEngine::ddaLine, &RasterEngine::bresenhamLine,
    &RasterEngine::wuLine, &RasterEngine::castlePitteway
};
}

Benchmark::Benchmark(int maxLines, int maxShapes, int repeats, QObject *parent)
    : QObject(parent), m_maxLines(maxLines), m_maxShapes(maxShapes), m_repeats(repeats),
      m_done(0), m_total(1), m_cancel(false)
{
}

QStringList Benchmark::lineNames()
{
    return QStringList() << "Пошаговый" << "ЦДА" << "Брезенхем" << "Ву" << "Кастл–Питвей";
}

QStringList Benchmark::shapeNames()
{
    return QStringList() << "Брезенхем (окружность)" << "Питвей (эллипс)";
}

// «Солнце»: все отрезки выходят из центра под случайным углом, значит попадают во все 8 октантов
std::vector<Benchmark::Seg> Benchmark::makeFan(int count, int minLen, int maxLen, std::mt19937 &rng)
{
    std::uniform_real_distribution<double> ang(0.0, 2 * kPi);
    std::uniform_int_distribution<int> len(minLen, maxLen);
    std::vector<Seg> v(count);
    for (int i = 0; i < count; ++i) {
        double a = ang(rng);
        int l = len(rng);
        v[i].x0 = 0;
        v[i].y0 = 0;
        v[i].x1 = (int)std::floor(l * std::cos(a) + 0.5);
        v[i].y1 = (int)std::floor(l * std::sin(a) + 0.5);
    }
    return v;
}

std::vector<Benchmark::Shape> Benchmark::makeShapes(int count, int rMax, int spread, std::mt19937 &rng)
{
    std::uniform_int_distribution<int> pos(-spread, spread);
    std::uniform_int_distribution<int> rad(1, rMax);
    std::vector<Shape> v(count);
    for (int i = 0; i < count; ++i) {
        v[i].cx = pos(rng);
        v[i].cy = pos(rng);
        v[i].a = rad(rng);
        v[i].b = rad(rng);
    }
    return v;
}

void Benchmark::tick()
{
    ++m_done;
    emit progress(m_done * 100 / m_total);
}

// из нескольких прогонов берём лучший, чтобы убрать выбросы из-за планировщика ОС
double Benchmark::timeLines(LineFn fn, const std::vector<Seg> &s, int n)
{
    double best = 1e30;
    for (int rep = 0; rep < m_repeats && !m_cancel; ++rep) {
        m_grid.clear();
        QElapsedTimer timer;
        timer.start();
        for (int i = 0; i < n; ++i)
            fn(s[i].x0, s[i].y0, s[i].x1, s[i].y1, m_grid, 0);
        best = std::min(best, timer.nsecsElapsed() / 1e6);
        tick();
    }
    return best;
}

double Benchmark::timeShapes(const std::vector<Shape> &s, int n, bool ellipse)
{
    double best = 1e30;
    for (int rep = 0; rep < m_repeats && !m_cancel; ++rep) {
        m_grid.clear();
        QElapsedTimer timer;
        timer.start();
        if (ellipse) {
            for (int i = 0; i < n; ++i)
                RasterEngine::pittewayEllipse(s[i].cx, s[i].cy, s[i].a, s[i].b, m_grid, 0);
        } else {
            for (int i = 0; i < n; ++i)
                RasterEngine::bresenhamCircle(s[i].cx, s[i].cy, s[i].a, m_grid, 0);
        }
        best = std::min(best, timer.nsecsElapsed() / 1e6);
        tick();
    }
    return best;
}

void Benchmark::run()
{
    m_grid.resize(kGridSize, kGridSize);
    m_total = kSteps * (kLineAlgos * 2 + 2 * 2) * m_repeats;
    m_done = 0;
    std::mt19937 rng(20241);   // фиксированное зерно: все алгоритмы получают одни и те же фигуры

    double lastLines[kLineAlgos] = {0, 0, 0, 0, 0};
    double lastShapes[2] = {0, 0};
    int nLines = 0, nShapes = 0;

    // график 0: время от числа отрезков, длина случайная от 1 до 500
    std::vector<Seg> fan = makeFan(m_maxLines, 1, 500, rng);
    for (int i = 1; i <= kSteps && !m_cancel; ++i) {
        int n = std::max(1, (int)((long long)m_maxLines * i / kSteps));
        for (int k = 0; k < kLineAlgos && !m_cancel; ++k) {
            double ms = timeLines((LineFn)kLineFns[k], fan, n);
            if (m_cancel) break;
            emit point(0, k, n, ms);
            lastLines[k] = ms;
        }
        nLines = n;
    }

    // график 1: фиксированное число отрезков, меняется их длина
    int fixedLines = std::min(m_maxLines, 20000);
    for (int i = 1; i <= kSteps && !m_cancel; ++i) {
        int len = 100 * i;
        std::vector<Seg> f = makeFan(fixedLines, len, len, rng);
        for (int k = 0; k < kLineAlgos && !m_cancel; ++k) {
            double ms = timeLines((LineFn)kLineFns[k], f, fixedLines);
            if (m_cancel) break;
            emit point(1, k, len, ms);
        }
    }

    // график 2: время от числа окружностей и эллипсов, радиусы от 1 до 500
    std::vector<Shape> shapes = makeShapes(m_maxShapes, 500, 256, rng);
    for (int i = 1; i <= kSteps && !m_cancel; ++i) {
        int n = std::max(1, (int)((long long)m_maxShapes * i / kSteps));
        for (int k = 0; k < 2 && !m_cancel; ++k) {
            double ms = timeShapes(shapes, n, k == 1);
            if (m_cancel) break;
            emit point(2, k, n, ms);
            lastShapes[k] = ms;
        }
        nShapes = n;
    }

    // график 3: фиксированное число фигур, меняется радиус (у эллипса полуоси R и R/2)
    int fixedShapes = std::min(m_maxShapes, 5000);
    for (int i = 1; i <= kSteps && !m_cancel; ++i) {
        int r = 100 * i;
        std::vector<Shape> f = makeShapes(fixedShapes, 1, 20, rng);
        for (size_t j = 0; j < f.size(); ++j) {
            f[j].a = r;
            f[j].b = r / 2;
        }
        for (int k = 0; k < 2 && !m_cancel; ++k) {
            double ms = timeShapes(f, fixedShapes, k == 1);
            if (m_cancel) break;
            emit point(3, k, r, ms);
        }
    }

    if (m_cancel) {
        emit summary("Тест остановлен.");
    } else {
        QStringList ln = lineNames(), sn = shapeNames();
        QString s = QString("Суммарное время генерации виртуального растра 2048x2048 (без интерфейса)\n\n");
        s += QString("Отрезки: %1 шт., веер из центра, длина 1-500 пикселей\n").arg(nLines);
        for (int k = 0; k < kLineAlgos; ++k)
            s += QString("  %1 %2 мс\n").arg(ln[k], -14).arg(lastLines[k], 9, 'f', 2);
        s += QString("  ЦДА / Брезенхем = %1,  Ву / Брезенхем = %2,  Кастл-Питвей / Брезенхем = %3\n\n")
                 .arg(lastLines[1] / lastLines[2], 0, 'f', 2)
                 .arg(lastLines[3] / lastLines[2], 0, 'f', 2)
                 .arg(lastLines[4] / lastLines[2], 0, 'f', 2);
        s += QString("Фигуры: %1 шт., радиусы 1-500 пикселей\n").arg(nShapes);
        for (int k = 0; k < 2; ++k)
            s += QString("  %1 %2 мс\n").arg(sn[k], -24).arg(lastShapes[k], 9, 'f', 2);
        emit summary(s);
    }
    emit finished();
}
