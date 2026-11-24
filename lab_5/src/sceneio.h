#ifndef SCENEIO_H
#define SCENEIO_H

#include <string>

#include "geometry.h"

namespace geo {

struct SceneData {
    std::vector<Segment> segments;
    Rect window = Rect(-5, -5, 5, 5);
    Ring clipperPoly;               // выпуклый отсекатель для части 2
    std::vector<Ring> polygons;     // контуры отсекаемых многоугольников
};

// Формат 1 (как в задании):  n, n строк "x1 y1 x2 y2", строка "xmin ymin xmax ymax".
// Формат 2 (расширенный, ключевые слова в любом порядке):
//   segments n / window xmin ymin xmax ymax / clipper m / polygon k
// Комментарии: после '#' до конца строки и текст между звёздочками *...*
bool parseScene(const std::string &text, SceneData &out, std::string &error);
std::string serializeScene(const SceneData &s);

std::vector<Segment> randomSegments(int n, const Rect &area, double maxLen, unsigned seed);

}  // namespace geo

#endif
