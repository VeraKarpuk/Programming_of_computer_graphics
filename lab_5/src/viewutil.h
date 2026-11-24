#ifndef VIEWUTIL_H
#define VIEWUTIL_H

#include <cmath>

namespace viewutil {

// "круглый" шаг сетки 1, 2 или 5 * 10^k, не меньше raw
inline double niceStep(double raw) {
    double e = std::pow(10.0, std::floor(std::log10(raw)));
    double f = raw / e;
    double n = f <= 1 ? 1 : (f <= 2 ? 2 : (f <= 5 ? 5 : 10));
    return n * e;
}

}  // namespace viewutil

#endif
