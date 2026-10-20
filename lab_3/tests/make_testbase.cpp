// Генерирует базу тестовых изображений в виде PNM-файлов (без Qt):
//   g++ -std=c++11 -O2 -pthread -Isrc tests/make_testbase.cpp src/core/*.cpp -o make_testbase && ./make_testbase outdir
// (В самом приложении та же база сохраняется в PNG: Сервис → Сохранить базу тестовых изображений…)
#include <fstream>
#include <string>
#include "core/TestImages.h"
using namespace ip;
static void writePnm(const Image& im, const std::string& path) {
    std::ofstream f(path.c_str(), std::ios::binary);
    f << (im.channels == 1 ? "P5" : "P6") << "\n" << im.width << " " << im.height << "\n255\n";
    f.write(reinterpret_cast<const char*>(im.data.data()), static_cast<std::streamsize>(im.data.size()));
}
int main(int argc, char** argv) {
    const std::string dir = argc > 1 ? argv[1] : ".";
    const std::vector<TestImageInfo>& cat = testImageCatalog();
    for (size_t i = 0; i < cat.size(); ++i) {
        Image truth;
        Image im = generateTestImage((int)i, 800, 600, &truth);
        writePnm(im, dir + "/" + cat[i].id + ".pnm");
        if (cat[i].hasTruth) writePnm(truth, dir + "/" + cat[i].id + "_truth.pnm");
    }
    return 0;
}
