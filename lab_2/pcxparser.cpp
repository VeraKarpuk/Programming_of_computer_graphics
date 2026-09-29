#include "pcxparser.h"

bool PcxParser::parse(QFile &file, ImageMetadata &meta) {
    meta.format = "PCX";
    meta.resolution = 72.0;

    file.seek(0);
    quint8 manufacturer = readU8(file);
    if (manufacturer != 0x0A) {
        meta.status = "Corrupted";
        meta.errorMessage = "Invalid PCX magic";
        return false;
    }

    quint8 version = readU8(file);
    quint8 encoding = readU8(file);
    quint8 bitsPerPixel = readU8(file);
    quint16 xMin = readU16(file, false);
    quint16 yMin = readU16(file, false);
    quint16 xMax = readU16(file, false);
    quint16 yMax = readU16(file, false);
    quint16 hDpi = readU16(file, false);
    quint16 vDpi = readU16(file, false);

    Q_UNUSED(version);
    Q_UNUSED(yMin);
    Q_UNUSED(vDpi);

    int width = xMax - xMin + 1;
    int height = yMax - yMin + 1;
    meta.size = QSize(width, height);
    meta.resolution = hDpi;
    if (meta.resolution <= 0) meta.resolution = 72.0;

    file.seek(65);
    quint8 numPlanes = readU8(file);
    readU16(file, false);

    meta.bitDepth = bitsPerPixel * numPlanes;
    meta.compression = (encoding == 1) ? "RLE" : "None";
    return true;
}