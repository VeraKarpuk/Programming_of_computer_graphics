#ifndef PNGPARSER_H
#define PNGPARSER_H

#include "imageparser.h"

class PngParser : public ImageParser {
public:
    bool parse(QFile &file, ImageMetadata &meta) override;
};

#endif