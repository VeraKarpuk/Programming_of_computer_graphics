#include "tiffparser.h"

bool TiffParser::parse(QFile &file, ImageMetadata &meta) {
    meta.format = "TIFF";
    meta.resolution = 72.0;
    meta.bitDepth = 1;
    meta.compression = "None";

    file.seek(0);
    QByteArray magic = file.read(4);
    bool bigEndian = (magic == "MM\x00*");
    if (magic != "II\x2A\x00" && magic != "MM\x00\x2A") {
        meta.status = "Corrupted";
        meta.errorMessage = "Invalid TIFF magic";
        return false;
    }

    quint32 ifdOffset = readU32(file, bigEndian);
    if (ifdOffset == 0 || ifdOffset >= quint32(file.size())) {
        meta.status = "Corrupted";
        meta.errorMessage = "Invalid IFD offset";
        return false;
    }

    file.seek(ifdOffset);
    quint16 entryCount = readU16(file, bigEndian);
    quint32 width = 0, height = 0;

    for (int i = 0; i < entryCount && i < 512; ++i) {
        if (file.pos() + 12 > file.size()) break;

        quint16 tag = readU16(file, bigEndian);
        quint16 type = readU16(file, bigEndian);
        quint32 count = readU32(file, bigEndian);
        quint32 value = readU32(file, bigEndian);

        auto readScalar = [&](quint32 v) -> quint32 {
            if (count == 1) return v;
            qint64 saved = file.pos();
            if (v + 4 <= quint32(file.size())) {
                file.seek(v);
                quint32 r = readU32(file, bigEndian);
                file.seek(saved);
                return r;
            }
            file.seek(saved);
            return 0;
        };

        auto readRational = [&](quint32 v) -> double {
            qint64 saved = file.pos();
            if (v + 8 <= quint32(file.size())) {
                file.seek(v);
                quint32 num = readU32(file, bigEndian);
                quint32 den = readU32(file, bigEndian);
                file.seek(saved);
                if (den != 0) return double(num) / double(den);
            }
            file.seek(saved);
            return 0.0;
        };

        if (tag == 0x0100) width = readScalar(value);
        else if (tag == 0x0101) height = readScalar(value);
        else if (tag == 0x0102) meta.bitDepth = static_cast<int>(readScalar(value));
        else if (tag == 0x0103) {
            quint32 c = readScalar(value);
            switch (c) {
            case 1: meta.compression = "None"; break;
            case 2: meta.compression = "CCITT RLE"; break;
            case 3: meta.compression = "CCITT G3"; break;
            case 4: meta.compression = "CCITT G4"; break;
            case 5: meta.compression = "LZW"; break;
            case 6: case 7: meta.compression = "JPEG"; break;
            case 8: case 32946: meta.compression = "Deflate"; break;
            case 32773: meta.compression = "PackBits"; break;
            default: meta.compression = QString("Code %1").arg(c);
            }
        }
        else if (tag == 0x011A) {
            double r = readRational(value);
            if (r > 0) meta.resolution = r;
        }
        else if (tag == 0x011B) {
            double r = readRational(value);
            if (r > 0 && meta.resolution == 72.0) meta.resolution = r;
        }
    }

    meta.size = QSize(width, height);
    if (width == 0 || height == 0) {
        meta.status = "Corrupted";
        meta.errorMessage = "Invalid dimensions";
        return false;
    }
    return true;
}