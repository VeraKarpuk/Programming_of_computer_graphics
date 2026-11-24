#ifndef SEGMENTCLIP_H
#define SEGMENTCLIP_H

#include "geometry.h"

namespace geo {

enum OutCode { INSIDE = 0, LEFT = 1, RIGHT = 2, BOTTOM = 4, TOP = 8 };

int outcode(const Point &p, const Rect &w);

// Сазерленд-Коэн: прямая, содержащая отрезок, задана явно (y = kx + b)
bool clipSutherlandCohen(const Segment &s, const Rect &w, Segment &out, OpStats *st = nullptr);

// Лианг-Барски: параметрическое задание отрезка P(t) = A + t*(B - A)
bool clipLiangBarsky(const Segment &s, const Rect &w, Segment &out, OpStats *st = nullptr);

// быстрая отбраковка по габаритному прямоугольнику отрезка (на битовых масках)
bool aabbOutside(const Segment &s, const Rect &w);

bool isConvex(const Ring &pts);

// выпуклый отсекатель; алгоритм Сайруса-Бека
class ConvexClipper {
public:
    // false, если многоугольник вырожден или невыпуклый
    bool set(const Ring &pts);
    static ConvexClipper fromRect(const Rect &r);

    bool valid() const { return v_.size() >= 3; }
    const Ring &vertices() const { return v_; }

    bool clip(const Segment &s, Segment &out, OpStats *st = nullptr) const;

private:
    Ring v_;                 // обход против часовой стрелки
    std::vector<Point> n_;   // единичные внутренние нормали рёбер
};

}  // namespace geo

#endif
