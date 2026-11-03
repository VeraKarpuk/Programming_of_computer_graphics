#include "VirtualGrid.h"

#include <algorithm>

VirtualGrid::VirtualGrid(int width, int height)
{
    resize(width, height);
}

void VirtualGrid::resize(int width, int height) //пересоздаёт массив и пересчитывает смещение центра
{
    m_w = width;
    m_h = height;
    m_ox = width / 2;
    m_oy = height / 2;
    m_cells.assign((size_t)width * height, 0.0f);
}

void VirtualGrid::clear() //заполняет всё нулями
{
    std::fill(m_cells.begin(), m_cells.end(), 0.0f);
}

//переводит логические координаты (x; y) в индексы массива и записывает яркость
void VirtualGrid::setPixel(int x, int y, double intensity)
{
    int cx = x + m_ox, cy = y + m_oy;
    if (cx < 0 || cx >= m_w || cy < 0 || cy >= m_h || intensity <= 0.0)
        return;
    // при наложении фигур остаётся наибольшая яркость
    float &c = m_cells[(size_t)cy * m_w + cx];
    float v = (float)intensity;
    if (v > c)
        c = v;
}

double VirtualGrid::at(int x, int y) const //читает яркость по логическим координатам
{
    int cx = x + m_ox, cy = y + m_oy;
    if (cx < 0 || cx >= m_w || cy < 0 || cy >= m_h)
        return 0.0;
    return m_cells[(size_t)cy * m_w + cx];
}

long long VirtualGrid::litCount() const
{
    long long n = 0;
    for (size_t i = 0; i < m_cells.size(); ++i)
        if (m_cells[i] > 0.0f)
            ++n;
    return n;
}
