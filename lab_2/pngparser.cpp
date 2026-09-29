#include "pngparser.h"

bool PngParser::parse(QFile &file, ImageMetadata &meta) {
    meta.format = "PNG";
    meta.compression = "Deflate";
    meta.resolution = 96.0;

    file.seek(8);
    if (file.read(4) != "IHDR") {
        meta.status = "Corrupted";
        meta.errorMessage = "Missing IHDR";
        return false;
    }

    quint32 width = readU32(file);
    quint32 height = readU32(file);
    quint8 bitDepth = readU8(file);
    quint8 colorType = readU8(file);
    quint8 compression = readU8(file);
    file.seek(file.pos() + 2);

    if (compression != 0) {
        meta.status = "Corrupted";
        meta.errorMessage = "Unsupported compression";
        return false;
    }

    meta.size = QSize(width, height);

    int channels = 1;
    switch (colorType) {
    case 0: channels = 1; break;
    case 2: channels = 3; break;
    case 3: channels = 1; break;
    case 4: channels = 2; break;
    case 6: channels = 4; break;
    default:
        meta.status = "Corrupted";
        meta.errorMessage = "Invalid color type";
        return false;
    }
    meta.bitDepth = bitDepth * channels;

    bool hasIEND = false;
    bool hasIDAT = false;
    int chunkGuard = 0;

    while (!file.atEnd() && chunkGuard++ < 100000) {
        if (file.pos() + 8 > file.size()) break;

        quint32 length = readU32(file);
        QByteArray type = file.read(4);

        if (length > quint32(file.size())) {
            meta.status = "Corrupted";
            meta.errorMessage = "Invalid chunk length";
            return false;
        }

        if (type == "pHYs") {
            quint32 xppu = readU32(file);
            readU32(file);
            quint8 unit = readU8(file);
            if (unit == 1) meta.resolution = xppu * 0.0254;
            file.seek(file.pos() + 4);
        } else if (type == "IDAT") {
            hasIDAT = true;
            file.seek(file.pos() + length + 4);
        } else if (type == "IEND") {
            hasIEND = true;
            break;
        } else {
            file.seek(file.pos() + length + 4);
        }

        if (file.pos() < 0) break;
    }

    if (!hasIEND) {
        meta.status = "Corrupted";
        meta.errorMessage = "Missing IEND";
        return false;
    }
    if (!hasIDAT) {
        meta.status = "Corrupted";
        meta.errorMessage = "Missing IDAT";
        return false;
    }
    return true;
}