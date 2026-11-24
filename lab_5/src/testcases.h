#ifndef TESTCASES_H
#define TESTCASES_H

#include <string>

#include "geometry.h"

namespace geo {

// окно для всех тестов отрезков: (0,0)-(10,10)
struct SegTest {
    std::string name;
    std::string marker;  // вырожденный случай, который проверяет тест
    Segment seg;
    bool visible;
    Segment expected;    // ожидаемая видимая часть (если visible)
};

struct PolyTest {
    std::string name;
    std::string marker;
    std::vector<Ring> subject;
    Ring clipper;
    int contours;        // ожидаемое число контуров результата
};

Rect testWindow();
std::vector<SegTest> segmentTests();
std::vector<PolyTest> polygonTests();

// сравнение по сетке пробных точек: область результата должна совпасть
// с пересечением subject (чётно-нечётное правило) и отсекателя
int polygonMismatches(const std::vector<Ring> &subject, const Ring &clipper,
                      const std::vector<Ring> &result);

bool sameSegment(const Segment &a, const Segment &b);

}  // namespace geo

#endif
