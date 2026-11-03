#ifndef RASTERENGINE_H
#define RASTERENGINE_H

#include <string>
#include <vector>

// Алгоритмы ничего не рисуют, а только сообщают цели, какой пиксель и с какой яркостью закрашен
class IRasterTarget
{
public:
    virtual ~IRasterTarget() {}
    virtual void setPixel(int x, int y, double intensity) = 0;
};

// Текстовый протокол вычислений для демонстрационного примера (nullptr - не вести)
typedef std::vector<std::string> Trace;

class RasterEngine
{
public:
    static void stepLine(int x0, int y0, int x1, int y1, IRasterTarget &t, Trace *tr = 0);
    static void ddaLine(int x0, int y0, int x1, int y1, IRasterTarget &t, Trace *tr = 0);
    static void bresenhamLine(int x0, int y0, int x1, int y1, IRasterTarget &t, Trace *tr = 0);
    static void wuLine(int x0, int y0, int x1, int y1, IRasterTarget &t, Trace *tr = 0);
    static void castlePitteway(int x0, int y0, int x1, int y1, IRasterTarget &t, Trace *tr = 0);
    static void bresenhamCircle(int cx, int cy, int r, IRasterTarget &t, Trace *tr = 0);
    static void pittewayEllipse(int cx, int cy, int a, int b, IRasterTarget &t, Trace *tr = 0);
    // дуга эллипса: углы в градусах от оси x против часовой стрелки, от startDeg до endDeg
    static void pittewayArc(int cx, int cy, int a, int b, int startDeg, int endDeg, IRasterTarget &t, Trace *tr = 0);
};

#endif
