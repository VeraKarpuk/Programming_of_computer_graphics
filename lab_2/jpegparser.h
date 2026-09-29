#ifndef JPEGPARSER_H
#define JPEGPARSER_H

#include "imageparser.h"

class JpegParser : public ImageParser {
public:
    bool parse(QFile &file, ImageMetadata &meta) override;
};

#endif