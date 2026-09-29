#ifndef BMPPARSER_H
#define BMPPARSER_H

#include "imageparser.h"

class BmpParser : public ImageParser {
public:
    bool parse(QFile &file, ImageMetadata &meta) override;
};

#endif