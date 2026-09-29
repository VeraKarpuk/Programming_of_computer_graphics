#include "gifparser.h"

bool GifParser::parse(QFile &file, ImageMetadata &meta) {
    meta.format = "GIF";
    meta.compression = "LZW";
    meta.resolution = 96.0;

    file.seek(6);
    quint16 width = readU16(file, false);
    quint16 height = readU16(file, false);
    quint8 packed = readU8(file);
    readU8(file);
    readU8(file);

    int gctSize = 2 << (packed & 0x07);
    int bitDepth = (packed & 0x07) + 1;
    meta.size = QSize(width, height);
    meta.bitDepth = bitDepth;

    if (packed & 0x80)
        file.seek(file.pos() + gctSize * 3);

    return true;
}