#ifndef VIRTUALGRID_H
#define VIRTUALGRID_H

#include "RasterEngine.h"
#include <vector>

// Виртуальный растр: числовой массив яркостей, начало координат лежит в центре массива
class VirtualGrid : public IRasterTarget
{
public:
    VirtualGrid(int width = 61, int height = 41);

    void resize(int width, int height);
    void clear();
    void setPixel(int x, int y, double intensity);
    double at(int x, int y) const;
    long long litCount() const;

    int width() const { return m_w; }
    int height() const { return m_h; }
    int minX() const { return -m_ox; }  //границы логических координат
    int maxX() const { return m_w - 1 - m_ox; }
    int minY() const { return -m_oy; }
    int maxY() const { return m_h - 1 - m_oy; }

private:
    int m_w, m_h;
    int m_ox, m_oy;   // индекс клетки с координатами (0; 0)
    std::vector<float> m_cells;
};

#endif
