#ifndef TIFFPARSER_H
#define TIFFPARSER_H

#include "imageparser.h"

class TiffParser : public ImageParser {
public:
    bool parse(QFile &file, ImageMetadata &meta) override;
};

#endif