// Presentation Layer: преобразование QImage <-> ip::Image.
// Копирование строк идёт через scanLine() — прямой доступ к памяти, без pixel()/setPixel().
#pragma once
#include <QImage>
#include "core/Image.h"

ip::Image qimageToImage(const QImage& src);
QImage imageToQImage(const ip::Image& img);
