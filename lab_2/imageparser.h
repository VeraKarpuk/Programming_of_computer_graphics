#ifndef IMAGEPARSER_H
#define IMAGEPARSER_H

#include <QFile>
#include "imagemetadata.h"

class ImageParser {
public:
    virtual ~ImageParser() = default;
    virtual bool parse(QFile &file, ImageMetadata &meta) = 0;

    static quint8 readU8(QFile &file);
    static quint16 readU16(QFile &file, bool bigEndian = true);
    static quint32 readU32(QFile &file, bool bigEndian = true);
};

#endif