#ifndef BENCHMARK_H
#define BENCHMARK_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>
#include <random>
#include <vector>

#include "RasterEngine.h"
#include "VirtualGrid.h"

// Стресс-тест: рисует тысячи фигур в виртуальный растр без участия интерфейса и замеряет время
class Benchmark : public QObject
{
    Q_OBJECT
public:
    Benchmark(int maxLines, int maxShapes, int repeats, QObject *parent = 0);

    static QStringList lineNames();
    static QStringList shapeNames();
    void cancel() { m_cancel = true; }

public slots:
    void run();

signals:
    void point(int chart, int series, double x, double ms);
    void progress(int percent);
    void summary(const QString &text);
    void finished();

private:
    struct Seg { int x0, y0, x1, y1; };
    struct Shape { int cx, cy, a, b; };
    typedef void (*LineFn)(int, int, int, int, IRasterTarget &, Trace *);

    static std::vector<Seg> makeFan(int count, int minLen, int maxLen, std::mt19937 &rng);
    static std::vector<Shape> makeShapes(int count, int rMax, int spread, std::mt19937 &rng);
    double timeLines(LineFn fn, const std::vector<Seg> &s, int n);
    double timeShapes(const std::vector<Shape> &s, int n, bool ellipse);
    void tick();

    int m_maxLines, m_maxShapes, m_repeats;
    int m_done, m_total;
    std::atomic<bool> m_cancel;
    VirtualGrid m_grid;
};

#endif
