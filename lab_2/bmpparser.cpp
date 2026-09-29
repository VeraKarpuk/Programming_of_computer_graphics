#include "bmpparser.h"

bool BmpParser::parse(QFile &file, ImageMetadata &meta) {
    meta.format = "BMP";
    file.seek(14);
    quint32 headerSize = readU32(file, false);
    if (headerSize < 40) {
        meta.status = "Corrupted";
        meta.errorMessage = "Invalid header size";
        return false;
    }
    qint32 width = readU32(file, false);
    qint32 height = readU32(file, false);
    readU16(file, false);
    quint16 bitCount = readU16(file, false);
    quint32 compression = readU32(file, false);
    meta.size = QSize(width, qAbs(height));
    meta.bitDepth = bitCount;

    switch (compression) {
    case 0: meta.compression = "BI_RGB"; break;
    case 1: meta.compression = "BI_RLE8"; break;
    case 2: meta.compression = "BI_RLE4"; break;
    case 3: meta.compression = "BI_BITFIELDS"; break;
    default: meta.compression = "Unknown";
    }

    qint32 xppm = readU32(file, false);
    meta.resolution = xppm * 0.0254;
    if (meta.resolution <= 0) meta.resolution = 96.0;
    return true;
}