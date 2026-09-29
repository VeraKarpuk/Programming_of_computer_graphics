#include "jpegparser.h"

bool JpegParser::parse(QFile &file, ImageMetadata &meta) {
    meta.format = "JPEG";
    meta.compression = "JPEG (Lossy)";
    meta.resolution = 72.0;
    meta.bitDepth = 24;
    file.seek(2);

    while (!file.atEnd()) {
        quint8 prefix = readU8(file);
        if (prefix != 0xFF) continue;
        quint8 marker = readU8(file);
        while (marker == 0xFF && !file.atEnd())
            marker = readU8(file);
        if (marker == 0xD8 || marker == 0xD9) continue;
        if (marker == 0xDA) break;

        quint16 length = readU16(file, false);

        if (marker >= 0xC0 && marker <= 0xCF
            && marker != 0xC4 && marker != 0xC8 && marker != 0xCC) {
            quint8 precision = readU8(file);
            quint16 height = readU16(file, false);
            quint16 width = readU16(file, false);
            quint8 components = readU8(file);
            meta.size = QSize(width, height);
            meta.bitDepth = precision * components;
            file.seek(file.pos() + length - 8);
        } else if (marker == 0xE0) {
            file.seek(file.pos() + 5);
            quint8 units = readU8(file);
            quint16 xDensity = readU16(file, false);
            readU16(file, false);
            if (units == 1) meta.resolution = xDensity;
            else if (units == 2) meta.resolution = xDensity * 2.54;
            file.seek(file.pos() + length - 12);
        } else {
            file.seek(file.pos() + length - 2);
        }

        if (meta.size.isValid()) {
            bool hasEOI = false;
            qint64 endPos = file.size() - 2;
            if (endPos > 0) {
                file.seek(endPos);
                quint8 b1 = readU8(file);
                quint8 b2 = readU8(file);
                if (b1 == 0xFF && b2 == 0xD9) hasEOI = true;
            }
            if (!hasEOI) {
                meta.status = "Corrupted";
                meta.errorMessage = "Missing EOI";
                return false;
            }
            return true;
        }
    }
    return meta.size.isValid();
}