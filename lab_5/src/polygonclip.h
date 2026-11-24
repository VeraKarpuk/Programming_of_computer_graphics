#ifndef POLYGONCLIP_H
#define POLYGONCLIP_H

#include "geometry.h"

namespace geo {

struct WAResult {
    std::vector<Ring> contours;       // контуры результата (внешние и дыры)
    std::vector<Point> entryPoints;   // точки входа границы subject в отсекатель
    std::vector<Point> exitPoints;    // точки выхода
    int intersections = 0;            // число найденных точек пересечения
};

// 1 - внутри (чётно-нечётное правило), 0 - на границе, -1 - снаружи
int classifyEvenOdd(const std::vector<Ring> &rings, const Point &p,
                    int *ringIdx = nullptr, int *edgeIdx = nullptr);

// Уайлер-Атертон: subject - набор контуров (внешние границы, дыры, самопересечения),
// clipper - простой многоугольник (выпуклый или нет)
WAResult weilerAtherton(const std::vector<Ring> &subject, const Ring &clipper);

// площадь области, заданной набором контуров по чётно-нечётному правилу
// (контуры не должны пересекаться между собой)
double evenOddArea(const std::vector<Ring> &contours);

}  // namespace geo

#endif
