// Генератор базы тестовых изображений (зашумлённые, размытые, малоконтрастные,
// с неравномерной освещённостью и т.д.). Не зависит от Qt, детерминирован (фиксированный seed).
#pragma once
#include "Image.h"

namespace ip {

struct TestImageInfo {
    const char* id;           // ASCII-имя для файла
    const char* title;        // название (UTF-8)
    const char* description;  // для чего подходит (UTF-8)
    bool hasTruth;            // есть ли эталонная маска объектов
};

const std::vector<TestImageInfo>& testImageCatalog();

// index — номер в каталоге. truth (если не nullptr и hasTruth) получает эталон 0/255.
Image generateTestImage(int index, int width, int height, Image* truth = nullptr);

}  // namespace ip
